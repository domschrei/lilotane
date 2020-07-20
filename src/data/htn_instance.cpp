
#include <regex>
#include <algorithm>
#include <getopt.h>

#include "data/htn_instance.h"

 
void HtnInstance::parse(std::string domainFile, std::string problemFile, ParsedProblem& pp) {

    const char* firstArg = "pandaPIparser";
    const char* domainStr = domainFile.c_str();
    const char* problemStr = problemFile.c_str();

    char* args[3];
    args[0] = (char*)firstArg;
    args[1] = (char*)domainStr;
    args[2] = (char*)problemStr;

    optind = 1;
    run_pandaPIparser(3, args, pp);
}

HtnInstance::HtnInstance(Parameters& params, ParsedProblem& p) : _params(params), _p(p), 
            _q_db([this](int arg) {return _q_constants.count(arg);}), 
            _remove_rigid_predicates(_params.isSet("rrp")), 
            _use_q_constant_mutexes(_params.getIntParam("qcm") > 0) {

    Names::init(_name_back_table);
    _instantiator = new Instantiator(params, *this);

    for (const predicate_definition& p : predicate_definitions)
        extractPredSorts(p);
    for (const task& t : primitive_tasks)
        extractTaskSorts(t);
    for (const task& t : abstract_tasks)
        extractTaskSorts(t);
    for (const method& m : methods)
        extractMethodSorts(m);
    
    extractConstants();

    Log::i("Sorts extracted.\n");
    for (const auto& sort_pair : _p.sorts) {
        Log::d(" %s : ", sort_pair.first.c_str());
        for (const std::string& c : sort_pair.second) {
            Log::d("%s ", c.c_str());
        }
        Log::d("\n");
    }

    // Create actions
    for (const task& t : primitive_tasks) {
        createAction(t);
    }
    
    // Create blank action without any preconditions or effects
    int blankId = getNameId("__BLANK___");
    _action_blank = Action(blankId, std::vector<int>());
    _actions[blankId] = _action_blank;

    // Create reductions
    for (method& method : methods) {
        createReduction(method);
    }

    // Create replacements for surrogate methods with only one subtask
    for (const auto& entry : _reductions) {
        const Reduction& red = entry.second;
        if (red.getSubtasks().size() == 1) {
            // Surrogate method
            USignature childSig = red.getSubtasks().at(0);
            int childId = childSig._name_id;
            if (_actions.count(childId)) {
                // Primitive subtask
                Substitution s(_actions.at(childId).getArguments(), childSig._args);
                Action childAct = _actions.at(childId).substitute(s);
                std::string name = "__SURROGATE*" + std::string(TOSTR(entry.first)) + "*" + std::string(TOSTR(childId)) + "*";
                int nameId = getNameId(name);
                Log::d("SURROGATE %s %i\n", name.c_str(), entry.first);
                _actions[nameId] = Action(nameId, red.getArguments());
                for (const auto& pre : red.getPreconditions()) _actions[nameId].addPrecondition(pre);
                for (const auto& pre : childAct.getPreconditions()) _actions[nameId].addPrecondition(pre);
                for (const auto& eff : childAct.getEffects()) _actions[nameId].addEffect(eff);
                _reduction_to_surrogate[entry.first] = nameId;
                _signature_sorts_table[nameId] = _signature_sorts_table[entry.first];
                //Log::d("SURROGATE par: %s\n", TOSTR(red));
                //Log::d("SURROGATE src: %s\n", TOSTR(_actions[childId]));
                //Log::d("SURROGATE des: %s\n", TOSTR(_actions[nameId]));
                _surrogate_to_orig_parent_and_child[nameId] = std::pair<int, int>(entry.first, childId);
            }
        }
    }

    // If necessary, compile out actions which have some effect predicate
    // in positive AND negative form: create two new actions in these cases
    if (false && (_params.isSet("q") || _params.isSet("qq"))) {

        NodeHashMap<int, Action> newActions;

        for (const auto& pair : _actions) {
            int aId = pair.first;
            std::vector<int> aSorts = _signature_sorts_table[aId];
            const Action& a = pair.second;
            const USignature& aSig = a.getSignature();

            // Find all negative effects to move
            FlatHashSet<int> posEffPreds;
            FlatHashSet<Signature, SignatureHasher> negEffsToMove;
            // Collect positive effect predicates
            for (const Signature& eff : a.getEffects()) {
                if (eff._negated) continue;
                posEffPreds.insert(eff._usig._name_id);
            }
            // Match against negative effects
            for (const Signature& eff : a.getEffects()) {
                if (!eff._negated) continue;
                if (posEffPreds.count(eff._usig._name_id)) {
                    // Must be factorized
                    negEffsToMove.insert(eff);
                }
            }
            
            if (negEffsToMove.empty()) {
                // Nothing to do
                newActions[aId] = a;
                continue;
            }

            // Create two new actions
            std::string oldName = _name_back_table[aId];

            // First action: all preconditions, only the neg. effects to be moved
            int idFirst = getNameId(oldName + "_FIRST");
            Action aFirst = Action(idFirst, aSig._args);
            aFirst.setPreconditions(a.getPreconditions());
            for (const Signature& eff : negEffsToMove) aFirst.addEffect(eff);
            _signature_sorts_table[aFirst.getSignature()._name_id] = aSorts;
            newActions[idFirst] = aFirst;

            // Second action: no preconditions, all other effects
            int idSecond = getNameId(oldName + "_SECOND");
            Action aSecond = Action(idSecond, aSig._args);
            for (const Signature& eff : a.getEffects()) {
                if (!negEffsToMove.count(eff)) aSecond.addEffect(eff);
            }
            _signature_sorts_table[aSecond.getSignature()._name_id] = aSorts;
            newActions[idSecond] = aSecond;

            // Replace all occurrences of the action with BOTH new actions in correct order
            for (auto& rPair : _reductions) {
                Reduction& r = rPair.second;
                bool change = false;
                std::vector<USignature> newSubtasks;
                for (int i = 0; i < r.getSubtasks().size(); i++) {
                    const USignature& subtask = r.getSubtasks()[i];
                    if (subtask._name_id == aId) {
                        // Replace
                        change = true;
                        Substitution s(aSig._args, subtask._args);
                        newSubtasks.push_back(aFirst.getSignature().substitute(s));
                        newSubtasks.push_back(aSecond.getSignature().substitute(s));
                    } else {
                        newSubtasks.push_back(subtask);
                    }
                }
                if (change) {
                    r.setSubtasks(newSubtasks);
                } 
            }

            /*
            log("REPLACE_ACTION %s => \n  <\n    %s,\n    %s\n  >\n", TOSTR(a), 
                    TOSTR(aFirst), 
                    TOSTR(aSecond));
            */
            // Remember original action name ID
            _split_action_from_first[idFirst] = aId;
        }

        _actions = newActions;
    }

    // Instantiate possible "root" / "top" methods
    for (const auto& rPair : _reductions) {
        const Reduction& r = rPair.second;
        if (_name_back_table[r.getSignature()._name_id].rfind("__top_method") == 0) {
            // Initial "top" method
            _init_reduction = r;
        }
    }
    
    Log::i("%i operators and %i methods created.\n", _actions.size(), _reductions.size());
}

int HtnInstance::getNameId(const std::string& name) {
    if (!_name_table.count(name)) {
        _name_table[name] = _name_table_running_id++;
        _name_back_table[_name_table_running_id-1] = name;
        if (name[0] == '?') {
            // variable
            _var_ids.insert(_name_table[name]);
        }
    }
    return _name_table[name];
}

std::vector<int> HtnInstance::getArguments(int predNameId, const std::vector<std::pair<string, string>>& vars) {
    std::vector<int> args;
    for (const auto& var : vars) {
        int id = var.first[0] == '?' ? getNameId(var.first + "_" + std::to_string(predNameId)) : getNameId(var.first);
        args.push_back(id);
    }
    return args;
}
std::vector<int> HtnInstance::getArguments(int predNameId, const std::vector<std::string>& vars) {
    std::vector<int> args;
    for (const auto& var : vars) {
        int id = var[0] == '?' ? getNameId(var + "_" + std::to_string(predNameId)) : getNameId(var);
        args.push_back(id);
    }
    return args;
}

USignature HtnInstance::getSignature(const task& task) {
    return USignature(getNameId(task.name), getArguments(getNameId(task.name), task.vars));
}
USignature HtnInstance::getSignature(const method& method) {
    return USignature(getNameId(method.name), getArguments(getNameId(method.name), method.vars));
}
Signature HtnInstance::getSignature(int parentNameId, const literal& literal) {
    Signature sig = Signature(getNameId(literal.predicate), getArguments(parentNameId, literal.arguments));
    if (!literal.positive) sig.negate();
    return sig;
}
USignature HtnInstance::getInitTaskSignature(int pos) {
    USignature subtask = _init_reduction.getSubtasks()[pos];
    std::vector<int> newArgs;
    for (int arg : subtask._args) {
        std::string name = _name_back_table[arg];
        std::smatch matches;
        if (std::regex_match(name, matches, std::regex("\\?var_for_(.*)_[0-9]+"))) {
            name = matches.str(1);
            newArgs.push_back(getNameId(name));
        } else {
            // Parameter inside initial task
            Log::w("%s was not matched by initial task argname substitution!\n", name.c_str());
            newArgs.push_back(arg);
        }
    }
    assert(newArgs.size() == subtask._args.size());
    subtask = subtask.substitute(Substitution(subtask._args, newArgs));
    return subtask;
}

SigSet HtnInstance::getInitState() {
    SigSet result;
    for (const ground_literal& lit : _p.init) {
        Signature sig(getNameId(lit.predicate), getArguments(getNameId(lit.predicate), lit.args));
        if (!lit.positive) sig.negate();
        result.insert(sig);
    }

    // Insert all necessary equality predicates

    // For each equality predicate:
    for (int eqPredId : _equality_predicates) {

        // For each pair of constants of correct sorts: TODO something more efficient
        std::vector<int> sorts = _signature_sorts_table[eqPredId];
        assert(sorts[0] == sorts[1]);
        for (int c1 : _constants_by_sort[sorts[0]]) {
            for (int c2 : _constants_by_sort[sorts[1]]) {

                // Add equality lit to state if the two are equal
                if (c1 != c2) continue;
                std::vector<int> args;
                args.push_back(c1); args.push_back(c2);
                Signature sig(eqPredId, args);
                result.insert(sig);
                Log::d("EQUALITY %s\n", TOSTR(sig));
            }
        }
    }

    return result;
}

SigSet HtnInstance::getGoals() {
    SigSet result;
    for (const ground_literal& lit : _p.goal) {
        Signature sig(getNameId(lit.predicate), getArguments(getNameId(lit.predicate), lit.args));
        if (!lit.positive) sig.negate();
        result.insert(sig);
    }
    return result;
}

void HtnInstance::extractPredSorts(const predicate_definition& p) {
    int pId = getNameId(p.name);
    std::vector<int> sorts;
    for (const std::string& var : p.argument_sorts) {
        sorts.push_back(getNameId(var));
    }
    assert(!_signature_sorts_table.count(pId));
    _signature_sorts_table[pId] = sorts;
}

void HtnInstance::extractTaskSorts(const task& t) {
    std::vector<int> sorts;
    for (const auto& var : t.vars) {
        int sortId = getNameId(var.second);
        sorts.push_back(sortId);
    }
    int tId = getNameId(t.name);
    assert(!_signature_sorts_table.count(tId));
    _signature_sorts_table[tId] = sorts;
    _original_n_taskvars[tId] = t.number_of_original_vars;
}

void HtnInstance::extractMethodSorts(const method& m) {
    std::vector<int> sorts;
    for (const auto& var : m.vars) {
        int sortId = getNameId(var.second);
        sorts.push_back(sortId);
    }
    int mId = getNameId(m.name);
    assert(!_signature_sorts_table.count(mId));
    _signature_sorts_table[mId] = sorts;
}

void HtnInstance::extractConstants() {
    for (const auto& sortPair : _p.sorts) {
        int sortId = getNameId(sortPair.first);
        _constants_by_sort[sortId];
        FlatHashSet<int>& constants = _constants_by_sort[sortId];
        for (const std::string& c : sortPair.second) {
            constants.insert(getNameId(c));
            //log("constant %s of sort %s\n", c.c_str(), sortPair.first.c_str());
        }
    }
}

Reduction& HtnInstance::createReduction(method& method) {
    int id = getNameId(method.name);
    std::vector<int> args = getArguments(id, method.vars);
    
    int taskId = getNameId(method.at);
    std::vector<int> taskArgs = getArguments(id, method.atargs);
    _task_id_to_reduction_ids[taskId];
    _task_id_to_reduction_ids[taskId].push_back(id);

    assert(_reductions.count(id) == 0);
    _reductions[id] = Reduction(id, args, USignature(taskId, taskArgs));
    Reduction& r = _reductions[id];

    std::vector<literal> condLiterals;

    // Extract (in)equality constraints, put into preconditions to process later
    for (const literal& lit : method.constraints) {            
        assert(lit.predicate == "__equal" || Log::e("Unknown constraint predicate \"\"!\n", lit.predicate.c_str()));
        condLiterals.push_back(lit);
    }

    // Go through expansion of the method
    std::map<std::string, int> subtaskTagToIndex;   
    for (const plan_step& st : method.ps) {
        
        // Normalize task name
        std::string subtaskName = st.task;
        std::smatch matches;
        while (std::regex_match(subtaskName, matches, std::regex("_splitting_method_(.*)_splitted_[0-9]+"))) {
            subtaskName = matches.str(1);
        }
        Log::d("%s\n", subtaskName.c_str());

        if (subtaskName.rfind(method_precondition_action_name) != std::string::npos) {
            // This "subtask" is a method precondition which was compiled out
            
            // Find primitive task belonging to this method precondition
            task precTask;
            int maxSize = 0;
            int numFound = 0;
            for (const task& t : primitive_tasks) {
                
                // Normalize task name
                std::string taskName = t.name;
                std::smatch matches;
                while (std::regex_match(taskName, matches, std::regex("_splitting_method_(.*)_splitted_[0-9]+"))) {
                    taskName = matches.str(1);
                }

                //Log::d(" ~~~ %s\n", taskName.c_str());
                if (subtaskName.rfind(taskName) != std::string::npos) {

                    int size = t.name.size();
                    if (size < maxSize) continue;
                    maxSize = size;

                    numFound++;
                    precTask = t;
                }
            }
            assert(numFound >= 1);
            Log::d("- Using %i preconds of prim. task %s as preconds of method %s\n",
                    precTask.prec.size() + precTask.constraints.size(), precTask.name.c_str(), st.task.c_str());

            // Add its preconditions to the method's preconditions
            for (const literal& lit : precTask.prec) condLiterals.push_back(lit);
            for (const literal& lit : precTask.constraints) condLiterals.push_back(lit);

            // If necessary, (re-)add parameters of the method precondition task
            for (const auto& varPair : precTask.vars) {
                
                if (varPair.first[0] != '?') continue; // not a variable

                int varId = getNameId(varPair.first + "_" + std::to_string(id));
                if (std::find(args.begin(), args.end(), varId) == args.end()) {
                    // Arg is not contained, must be added
                    r.addArgument(varId);
                    args = r.getArguments();
                    _signature_sorts_table[id].push_back(getNameId(varPair.second));
                    method.vars.push_back(varPair);
                }
            }

            // (Do not add the task to the method's subtasks)

        } else {
            
            // Actual subtask
            USignature sig(getNameId(st.task), getArguments(id, st.args));
            _reductions[id].addSubtask(sig);
            subtaskTagToIndex[st.id] = subtaskTagToIndex.size();
        }
    }

    // Process preconditions and constraints of the method
    for (const literal& lit : condLiterals) {

        //Log::d("( %s ", lit.predicate.c_str());
        //for (std::string arg : lit.arguments) Log::d("%s ", arg.c_str());
        //Log::d(")\n");

        if (lit.predicate == dummy_equal_literal) {
            // Equality precondition

            // Find out "type" of this equality predicate
            std::string arg1Str = lit.arguments[0];
            std::string arg2Str = lit.arguments[1];
            //Log::d("%s,%s :: ", arg1Str.c_str(), arg2Str.c_str());
            int sort1 = -1, sort2 = -1;
            for (int argPos = 0; argPos < method.vars.size(); argPos++) {
                //Log::d("(%s,%s) ", method.vars[argPos].first.c_str(), method.vars[argPos].second.c_str());
                if (arg1Str == method.vars[argPos].first)
                    sort1 = getNameId(method.vars[argPos].second);
                if (arg2Str == method.vars[argPos].first)
                    sort2 = getNameId(method.vars[argPos].second);
            }
            //log("\n");
            assert(sort1 >= 0 && sort2 >= 0);
            // Use the "larger" sort as the sort for both argument positions
            int eqSort = (_constants_by_sort[sort1].size() > _constants_by_sort[sort2].size() ? sort1 : sort2);

            // Create equality predicate
            std::string newPredicate = "__equal_" + _name_back_table[eqSort] + "_" + _name_back_table[eqSort];
            int newPredId = getNameId(newPredicate);
            if (!_signature_sorts_table.count(newPredId)) {
                // Predicate is new: remember sorts
                std::vector<int> sorts(2, eqSort);
                _signature_sorts_table[newPredId] = sorts;
                _equality_predicates.insert(newPredId);
            }

            // Add as a precondition to reduction
            std::vector<int> args; args.push_back(getNameId(arg1Str + "_" + std::to_string(id))); args.push_back(getNameId(arg2Str + "_" + std::to_string(id)));
            _reductions[id].addPrecondition(Signature(newPredId, args, !lit.positive));

        } else {
            // Normal precondition
            Signature sig = getSignature(id, lit);
            _reductions[id].addPrecondition(sig);
        }
    }

    // Order subtasks
    if (!method.ordering.empty()) {
        std::map<int, std::vector<int>> orderingNodelist;
        for (const auto& order : method.ordering) {
            int indexLeft = subtaskTagToIndex[order.first];
            int indexRight = subtaskTagToIndex[order.second];
            assert(indexLeft >= 0 && indexLeft < _reductions[id].getSubtasks().size());
            assert(indexRight >= 0 && indexRight < _reductions[id].getSubtasks().size());
            orderingNodelist[indexLeft];
            orderingNodelist[indexLeft].push_back(indexRight);
        }
        _reductions[id].orderSubtasks(orderingNodelist);
    }

    Log::d(" %s : %i preconditions, %i subtasks\n", TOSTR(_reductions[id].getSignature()), 
                _reductions[id].getPreconditions().size(), 
                _reductions[id].getSubtasks().size());
    Log::d("  PRE ");
    for (const Signature& sig : r.getPreconditions()) {
        Log::d("%s ", TOSTR(sig));
    }
    Log::d("\n");

    return _reductions[id];
}
Action& HtnInstance::createAction(const task& task) {
    int id = getNameId(task.name);
    std::vector<int> args = getArguments(id, task.vars);

    assert(_actions.count(id) == 0);
    _actions[id] = Action(id, args);
    for (const auto& p : task.prec) {
        Signature sig = getSignature(id, p);
        _actions[id].addPrecondition(sig);
    }
    for (const auto& p : task.eff) {
        Signature sig = getSignature(id, p);
        _actions[id].addEffect(sig);
        if (_params.isSet("rrp")) _fluent_predicates.insert(sig._usig._name_id);
    }
    _actions[id].removeInconsistentEffects();
    return _actions[id];
}

HtnOp& HtnInstance::getOp(const USignature& opSig) {
    if (_actions.count(opSig._name_id)) return (HtnOp&)_actions.at(opSig._name_id);
    else return (HtnOp&)_reductions.at(opSig._name_id);
}

Action HtnInstance::replaceVariablesWithQConstants(const Action& a, int layerIdx, int pos, const std::function<bool(const Signature&)>& state) {
    std::vector<int> newArgs = replaceVariablesWithQConstants((const HtnOp&)a, layerIdx, pos, state);
    if (newArgs.size() == 1 && newArgs[0] == -1) {
        // No valid substitution.
        return a;
    }
    return Action(a.substitute(Substitution(a.getArguments(), newArgs)));
}
Reduction HtnInstance::replaceVariablesWithQConstants(const Reduction& red, int layerIdx, int pos, const std::function<bool(const Signature&)>& state) {
    std::vector<int> newArgs = replaceVariablesWithQConstants((const HtnOp&)red, layerIdx, pos, state);
    if (newArgs.size() == 1 && newArgs[0] == -1) {
        // No valid substitution.
        return red;
    }
    return red.substituteRed(Substitution(red.getArguments(), newArgs));
}

std::vector<int> HtnInstance::replaceVariablesWithQConstants(const HtnOp& op, int layerIdx, int pos, const std::function<bool(const Signature&)>& state) {

    std::vector<int> vecFailure(1, -1);

    std::vector<int> args = op.getArguments();
    std::vector<int> varargIndices;
    for (int i = 0; i < op.getArguments().size(); i++) {
        const int& arg = op.getArguments()[i];
        if (_var_ids.count(arg)) varargIndices.push_back(i);
    }
    std::vector<bool> occursInPreconditions(op.getArguments().size(), false);
    std::vector<FlatHashSet<int>> domainPerVariable(op.getArguments().size());

    // Check each precondition regarding its valid decodings w.r.t. current state
    for (const auto& preSig : op.getPreconditions()) {

        // Check base condition; if unsatisfied, discard op 
        if (!_instantiator->test(preSig, state)) return vecFailure;

        // Find mapping from precond args to op args
        std::vector<int> opArgIndices;
        for (int arg : preSig._usig._args) {
            opArgIndices.push_back(-1);
            for (int i = 0; i < args.size(); i++) {
                if (args[i] == arg) {
                    opArgIndices.back() = i;
                    occursInPreconditions[i] = true;
                    break;
                }
            }
        }

        // Check possible decodings of precondition
        std::vector<USignature> usigs = getDecodedObjects(preSig._usig, /*checkQConstConds=*/true);
        bool anyValid = usigs.empty();
        for (const auto& decUSig : usigs) {
            Signature decSig(decUSig, preSig._negated);
            //Log::d("------%s\n", TOSTR(decSig));

            // Valid?
            if (!_instantiator->test(decSig, state)) continue;
            
            // Valid precondition decoding found: Increase domain of concerned variables
            anyValid = true;
            for (int i = 0; i < opArgIndices.size(); i++) {
                int opArgIdx = opArgIndices[i];
                if (opArgIdx >= 0) {
                    domainPerVariable[opArgIdx].insert(decSig._usig._args[i]);
                }
            }
        }
        if (!anyValid) return vecFailure;
    }

    // Assemble new operator arguments
    for (int i : varargIndices) {
        int vararg = args[i];
        auto& domain = domainPerVariable[i];
        if (!occursInPreconditions[i]) {
            domain = _constants_by_sort[_signature_sorts_table[op.getSignature()._name_id][i]];
        }
        if (domain.empty()) {
            // No valid constants at this position! The op is impossible.
            Log::d("Empty domain for arg %s of %s\n", TOSTR(vararg), TOSTR(op.getSignature()));
            return vecFailure;
        }
        if (domain.size() == 1) {
            // Only one valid constant here: Replace directly
            int onlyArg = -1; for (int arg : domain) {onlyArg = arg; break;}
            args[i] = onlyArg;
        } else {
            // Several valid constants here: Introduce q-constant
            args[i] = addQConstant(layerIdx, pos, op.getSignature(), i, domain);
            Log::d("QC %s ~> %s\n", TOSTR(vararg), TOSTR(args[i]));
        }
    }

    return args;
}

int HtnInstance::addQConstant(int layerIdx, int pos, const USignature& sig, int argPos, 
                const FlatHashSet<int>& domain) {

    int sort = _signature_sorts_table[sig._name_id][argPos];
    
    // Create / retrieve an ID for the q constant
    std::string qConstName = "Q_" + std::to_string(layerIdx) + "," 
        + std::to_string(pos) + "_" + std::to_string(_q_constants.size())
        + ":" + std::to_string(argPos) + "_" + _name_back_table[sort];
    int qConstId = getNameId(qConstName);

    // Insert q constant into set of q constants
    assert(!_q_constants.count(qConstId));
    _q_constants.insert(qConstId);

    // Create or retrieve the exact sort (= domain of constants) for this q-constant
    std::string qSortName = "qsort_" + std::to_string(sort) + "_"; 
        //+ std::to_string(domain.size()) + "_h";
    for (const auto& d : domain) qSortName += std::to_string(d) + "_";
    int newSortId = getNameId(qSortName);
    if (!_constants_by_sort.count(newSortId)) {
        _constants_by_sort[newSortId] = domain;
    }
    _primary_sort_of_q_constants[qConstId] = newSortId; //sort;

    // CALCULATE ADDITIONAL SORTS OF Q CONSTANT

    // 1. assume that the q-constant is of ALL (super) sorts
    std::set<int> qConstSorts;
    for (const auto& sortPair : _p.sorts) qConstSorts.insert(getNameId(sortPair.first));

    // 2. for each constant of the primary sort:
    //      remove all q-constant sorts NOT containing that constant
    for (int c : _constants_by_sort[newSortId]) {
        std::vector<int> sortsToRemove;
        for (int qsort : qConstSorts) {
            if (std::find(_constants_by_sort[qsort].begin(), _constants_by_sort[qsort].end(), c) 
                    == _constants_by_sort[qsort].end()) {
                sortsToRemove.push_back(qsort);
            }
        }
        for (int remSort : sortsToRemove) qConstSorts.erase(remSort);
    }
    // RESULT: The intersection of sorts of all eligible constants.
    // => If the q-constant has some sort, it means that ALL possible substitutions have that sort.

    /*
    log("  q-constant for arg %s @ pos %i of %s : %s\n   sorts ", 
            _name_back_table[arg].c_str(), argPos, 
            TOSTR(sig), TOSTR(qConstId));
    */
    _sorts_of_q_constants[qConstId];
    for (int sort : qConstSorts) {
        _sorts_of_q_constants[qConstId].insert(sort);
        //_constants_by_sort[sort].push_back(qConstId);
        //log("%s ", TOSTR(sort));
    } 
    //log("\n");

    return qConstId;
}

const std::vector<USignature> SIGVEC_EMPTY; 

const std::vector<USignature>& HtnInstance::getDecodedObjects(const USignature& qSig, bool checkQConstConds) {
    if (!hasQConstants(qSig) && _instantiator->isFullyGround(qSig)) return SIGVEC_EMPTY;

    Substitution s;
    for (int argPos = 0; argPos < qSig._args.size(); argPos++) {
        int arg = qSig._args[argPos];
        if (!checkQConstConds && _q_constants.count(arg) && !s.count(arg)) {
            s[arg] = getNameId("?" + std::to_string(argPos) + "_" + std::to_string(_primary_sort_of_q_constants[arg]));
        }
    }
    USignature normSig = qSig.substitute(s);
    auto& set = checkQConstConds ? _fact_sig_decodings : _fact_sig_decodings_unchecked;

    if (!set.count(normSig)) {
        // Calculate decoded objects
        //OBJ_CALC %s\n", TOSTR(normSig));

        std::vector<std::vector<int>> eligibleArgs(qSig._args.size());
        std::vector<int> qconsts, qconstIndices;
        for (int argPos = 0; argPos < qSig._args.size(); argPos++) {
            int arg = qSig._args[argPos];
            if (_q_constants.count(arg)) {
                // q constant
                qconsts.push_back(arg);
                qconstIndices.push_back(argPos);
                for (int c : getDomainOfQConstant(arg)) {
                    eligibleArgs[argPos].push_back(c);
                }
            } else if (_var_ids.count(arg)) {
                // Variable
                int sort = _signature_sorts_table[normSig._name_id][argPos];
                eligibleArgs[argPos] = std::vector<int>(_constants_by_sort[sort].begin(), _constants_by_sort[sort].end());
            } else {
                // normal constant
                eligibleArgs[argPos].push_back(arg);
            }
            assert(eligibleArgs[argPos].size() > 0);
        }

        if (checkQConstConds) {
            set[normSig];
            //log("DECOBJ %s\n", TOSTR(qSig));
            //for (const auto& e : eligibleArgs) log("DECOBJ -- %s\n", TOSTR(e));
            for (const USignature& sig : ArgIterator::instantiate(qSig, eligibleArgs)) {
                std::vector<int> vals;
                for (const int& i : qconstIndices) vals.push_back(sig._args[i]);
                if (_q_db.test(qconsts, vals)) set[normSig].push_back(sig);
            }
        } else {
            set[normSig] = ArgIterator::instantiate(qSig, eligibleArgs);
        }
    }

    return set[normSig]; 
}

const FlatHashSet<int>& HtnInstance::getSortsOfQConstant(int qconst) {
    return _sorts_of_q_constants[qconst];
}

const FlatHashSet<int>& HtnInstance::getConstantsOfSort(int sort) {
    return _constants_by_sort[sort]; 
}

const FlatHashSet<int>& HtnInstance::getDomainOfQConstant(int qconst) {
    return _constants_by_sort[_primary_sort_of_q_constants[qconst]];
}

void HtnInstance::addQFactDecoding(const USignature& qFact, const USignature& decFact) {
    _qfact_decodings[qFact];
    _qfact_decodings[qFact].insert(decFact);
}

void HtnInstance::removeQFactDecoding(const USignature& qFact, const USignature& decFact) {
    _qfact_decodings[qFact].erase(decFact);
}

const USigSet& HtnInstance::getQFactDecodings(const USignature& qFact) {
    return _qfact_decodings[qFact];
}

bool HtnInstance::hasQConstants(const USignature& sig) {
    for (const int& arg : sig._args) {
        if (_q_constants.count(arg)) return true;
    }
    return false;
}

bool HtnInstance::isAbstraction(const USignature& concrete, const USignature& abstraction) {
    
    // Different predicates?
    if (concrete._name_id != abstraction._name_id) return false;
    if (concrete._args.size() != abstraction._args.size()) return false;
    
    // Check syntactical fit
    std::vector<int> qArgs, decArgs;
    for (int i = 0; i < concrete._args.size(); i++) {
        const int& qarg = abstraction._args[i];
        const int& carg = concrete._args[i];
        
        // Same argument?
        if (qarg == carg) continue;
        // Different args, no q-constant arg?
        if (!_q_constants.count(qarg)) return false;
        
        if (_use_q_constant_mutexes) {
            qArgs.push_back(qarg);
            decArgs.push_back(carg);
        }

        // A q-constant that does not fit the concrete argument?
        if (!getDomainOfQConstant(qarg).count(carg)) return false;
    }

    // Check that q-constant assignment is valid
    if (_use_q_constant_mutexes && !_q_db.test(qArgs, decArgs)) return false;

    // A-OK
    return true;
}

void HtnInstance::addQConstantConditions(const HtnOp& op, const PositionedUSig& psig, const PositionedUSig& parentPSig, 
            int offset, const std::function<bool(const Signature&)>& state) {

    //log("QQ_ADD %s\n", TOSTR(op.getSignature()));

    if (!_use_q_constant_mutexes) return;
    if (!hasQConstants(psig.usig)) return;
    
    int oid = _q_db.addOp(op, psig.layer, psig.pos, parentPSig, offset);

    for (const auto& pre : op.getPreconditions()) {
        
        std::vector<int> ref;
        std::vector<int> qConstIndices;
        for (int i = 0; i < pre._usig._args.size(); i++) {
            const int& arg = pre._usig._args[i];
            if (_q_constants.count(arg)) {
                ref.push_back(arg);
                qConstIndices.push_back(i);
            }
        }
        if (ref.empty() || ref.size() > 2) continue;

        ValueSet good;
        ValueSet bad;
        //log("QQ %s\n", TOSTR(pre._usig));
        for (const auto& decPre : getDecodedObjects(pre._usig, true)) {
            bool holds = _instantiator->test(Signature(decPre, pre._negated), state);
            //log("QQ -- %s : %i\n", TOSTR(decPre), holds);
            auto& set = holds ? good : bad;
            std::vector<int> toAdd;
            for (const int& i : qConstIndices) toAdd.push_back(decPre._args[i]);
            set.insert(toAdd);
        }

        if (bad.empty() || std::min(good.size(), bad.size()) > _params.getIntParam("qcm")) continue;

        if (good.size() <= bad.size()) {
            _q_db.addCondition(oid, ref, QConstantCondition::CONJUNCTION_OR, good);
        } else {
            _q_db.addCondition(oid, ref, QConstantCondition::CONJUNCTION_NOR, bad);
        }
    }
}

bool HtnInstance::isRigidPredicate(int predId) {
    if (!_remove_rigid_predicates) return false;
    return !_fluent_predicates.count(predId);
}

void HtnInstance::removeRigidConditions(HtnOp& op) {

    SigSet newPres;
    for (const Signature& pre : op.getPreconditions()) {
        if (!isRigidPredicate(pre._usig._name_id)) {
            newPres.insert(pre); // only add fluent preconditions
        }
    }
    op.setPreconditions(newPres);
}

USignature HtnInstance::cutNonoriginalTaskArguments(const USignature& sig) {
    USignature sigCut(sig);
    sigCut._args.resize(_original_n_taskvars[sig._name_id]);
    return sigCut;
}

USignature HtnInstance::getNormalizedLifted(const USignature& opSig, std::vector<int>& placeholderArgs) {
    int nameId = opSig._name_id;
    
    // Get original signature of this operator (fully lifted)
    USignature origSig;
    if (_reductions.count(nameId)) {
        // Reduction
        origSig = _reductions[nameId].getSignature();
    } else {
        // Action
        origSig = _actions[nameId].getSignature();
    }

    // Substitution mapping
    for (int i = 0; i < opSig._args.size(); i++) {
        placeholderArgs.push_back(-i-1);
    }

    return origSig.substitute(Substitution(origSig._args, placeholderArgs)); 
}