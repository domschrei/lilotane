
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
            _q_db([this](int arg) {return _q_constants.count(arg);}) {

    Names::init(_name_back_table);
    _instantiator = new Instantiator(params, *this);

    _remove_rigid_predicates = _params.isSet("rrp");

    for (const predicate_definition& p : predicate_definitions)
        extractPredSorts(p);
    for (const task& t : primitive_tasks)
        extractTaskSorts(t);
    for (const task& t : abstract_tasks)
        extractTaskSorts(t);
    for (const method& m : methods)
        extractMethodSorts(m);
    
    extractConstants();

    log("Sorts extracted.\n");
    for (const auto& sort_pair : _p.sorts) {
        log(" %s : ", sort_pair.first.c_str());
        for (const std::string& c : sort_pair.second) {
            log("%s ", c.c_str());
        }
        log("\n");
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
    for (const method& method : methods) {
        createReduction(method);
    }

    // If necessary, compile out actions which have some effect predicate
    // in positive AND negative form: create two new actions in these cases
    if (_params.isSet("q") || _params.isSet("qq")) {

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
    
    log("%i operators and %i methods created.\n", _actions.size(), _reductions.size());
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
            log("%s was not matched by initial task argname substitution!\n", name.c_str());
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
                log("EQUALITY %s\n", TOSTR(sig));
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

Reduction& HtnInstance::createReduction(const method& method) {
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
    if (!method.constraints.empty()) {
        for (const literal& lit : method.constraints) {            
            //log("%s\n", TOSTR(getSignature(lit)));
            assert(lit.predicate == "__equal" || fail("Unknown constraint predicate \"" + lit.predicate + "\"!\n"));
            condLiterals.push_back(lit);
        }
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
        log("%s\n", subtaskName.c_str());

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

                //log(" %s\n", taskName.c_str());
                if (subtaskName.rfind(taskName) != std::string::npos) {

                    int size = t.name.size();
                    if (size < maxSize) continue;
                    maxSize = size;

                    numFound++;
                    precTask = t;
                }
            }
            assert(numFound >= 1);
            log("Using %i preconds of prim. task %s as preconds of method %s\n", 
                    precTask.prec.size(), precTask.name.c_str(), st.task.c_str());

            // Add its preconditions to the method's preconditions
            for (const literal& lit : precTask.prec) {
                //log("COND_LIT %s\n", lit.predicate.c_str());
                condLiterals.push_back(lit);
            }

            // If necessary, (re-)add parameters of the method precondition task
            for (const auto& varPair : precTask.vars) {
                
                if (varPair.first[0] != '?') continue; // not a variable

                int varId = getNameId(varPair.first + "_" + std::to_string(id));
                if (std::find(args.begin(), args.end(), varId) == args.end()) {
                    // Arg is not contained, must be added
                    r.addArgument(varId);
                    args = r.getArguments();
                    _signature_sorts_table[id].push_back(getNameId(varPair.second));
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

        if (lit.predicate == dummy_equal_literal) {
            // Equality precondition

            // Find out "type" of this equality predicate
            std::string arg1Str = lit.arguments[0];
            std::string arg2Str = lit.arguments[1];
            //log("%s,%s :: ", arg1Str.c_str(), arg2Str.c_str());
            int sort1 = -1, sort2 = -1;
            for (int argPos = 0; argPos < method.vars.size(); argPos++) {
                //log("(%s,%s) ", method.vars[argPos].first.c_str(), method.vars[argPos].second.c_str());
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

    log(" %s : %i preconditions, %i subtasks\n", TOSTR(_reductions[id].getSignature()), 
                _reductions[id].getPreconditions().size(), 
                _reductions[id].getSubtasks().size());
    log("  PRE ");
    for (const Signature& sig : r.getPreconditions()) {
        log("%s ", TOSTR(sig));
    }
    log("\n");

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

Action HtnInstance::replaceQConstants(const Action& a, int layerIdx, int pos, const std::function<bool(const Signature&)>& state) {
    USignature sig = a.getSignature();
    Substitution s = addQConstants(sig, layerIdx, pos, a.getPreconditions(), state);
    HtnOp op = a.substitute(s);
    return Action(op);
}
Reduction HtnInstance::replaceQConstants(const Reduction& red, int layerIdx, int pos, const std::function<bool(const Signature&)>& state) {
    USignature sig = red.getSignature();
    Substitution s = addQConstants(sig, layerIdx, pos, red.getPreconditions(), state);
    return red.substituteRed(s);
}

Substitution HtnInstance::addQConstants(const USignature& sig, int layerIdx, int pos, 
            const SigSet& conditions, const std::function<bool(const Signature&)>& state) {
    
    std::vector<int> freeArgPositions = _instantiator->getFreeArgPositions(sig._args);
    // Sort freeArgPositions ascendingly by their arg's importance / impact 
    std::sort(freeArgPositions.begin(), freeArgPositions.end(), [&](int left, int right) {
        int sortLeft = _signature_sorts_table[sig._name_id][left];
        int sortRight = _signature_sorts_table[sig._name_id][right];
        return getConstantsOfSort(sortLeft).size() < getConstantsOfSort(sortRight).size();
    });

    Substitution s;
    for (int argPos : freeArgPositions) {
        size_t domainHash = 0;
        FlatHashSet<int> domain = computeDomainOfArgument(sig, argPos, conditions, state, s, domainHash);
        if (domain.empty()) {
            // No valid value for this argument at this position: return failure
            //log("%s : %s has no valid domain!\n", TOSTR(sig), TOSTR(sig._args[argPos]));
            s.clear();
            break;
        } else {
            addQConstant(layerIdx, pos, sig, argPos, domain, domainHash, s);
        }
    }
    return s;
}

FlatHashSet<int> HtnInstance::computeDomainOfArgument(const USignature& sig, int argPos, 
            const SigSet& conditions, const std::function<bool(const Signature&)>& state, Substitution& substitution, size_t& domainHash) {
    
    int arg = sig._args[argPos];

    assert(_name_back_table[arg][0] == '?');
    assert(_signature_sorts_table.count(sig._name_id));
    assert(argPos < _signature_sorts_table[sig._name_id].size());
    
    //log("%s\n", TOSTR(sig));

    // Get domain of the q constant
    int sort = _signature_sorts_table[sig._name_id][argPos];
    const FlatHashSet<int>& domain = getConstantsOfSort(sort);
    assert(!domain.empty());
    assert(!substitution.count(arg));

    // Reduce the q-constant's domain according to the associated facts and the current state.
    FlatHashSet<int> actualDomain;
    size_t sum = 0;
    for (const int& c : domain) {
        //substitution[arg] = c;
        //SigSet substConditions;
        //for (const auto& cond : conditions) substConditions.insert(cond.substitute(substitution));
        //if (_instantiator->hasValidPreconditions(substConditions, state)) {
            actualDomain.insert(c);
            sum += 7*c;
        //} 
    }
    domainHash = std::hash<int>{}(sum);
    //log("%.2f%% of q-const domain eliminated\n", 100 - 100.f * actualDomain.size()/domain.size());
    //substitution.erase(arg);
    return actualDomain;
}

void HtnInstance::addQConstant(int layerIdx, int pos, const USignature& sig, int argPos, 
                const FlatHashSet<int>& domain, size_t domainHash, Substitution& s) {

    int arg = sig._args[argPos];
    int sort = _signature_sorts_table[sig._name_id][argPos];

    // If there is only a single option for the q constant: 
    // just insert that one option, do not create/retrieve a q constant.
    if (domain.size() == 1) {
        int c = -1; for (int x : domain) c = x;
        s[arg] = c;
        return;
    }
    
    // Create / retrieve an ID for the q constant
    std::string qConstName = "Q_" + std::to_string(layerIdx) + "," 
        + std::to_string(pos) + "_" + std::to_string(USignatureHasher()(sig))
        + ":" + std::to_string(argPos) + "_" + _name_back_table[sort];
    int qConstId = getNameId(qConstName);

    // Add q const to substitution
    s[arg] = qConstId;

    // Check if q const of that name already exists
    if (_q_constants.count(qConstId)) return;

    // -- q constant is NEW
    
    // Insert q constant into set of q constants
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
}

const std::vector<USignature> SIGVEC_EMPTY; 

const std::vector<USignature>& HtnInstance::getDecodedObjects(const USignature& qSig, bool checkQConstConds) {
    if (!hasQConstants(qSig)) return SIGVEC_EMPTY;

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

        assert(_instantiator->isFullyGround(qSig));
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
    
    for (int i = 0; i < concrete._args.size(); i++) {
        const int& qarg = abstraction._args[i];
        const int& carg = concrete._args[i];
        // Same argument?
        if (qarg == carg) continue;
        // Different args, no q-constant arg?
        if (!_q_constants.count(qarg)) return false;
        // A q-constant that does not fit the concrete argument?
        if (!getDomainOfQConstant(qarg).count(carg)) return false;
    }
    // A-OK
    return true;
}

void HtnInstance::addQConstantConditions(const HtnOp& op, const PositionedUSig& psig, const PositionedUSig& parentPSig, 
            int offset, const std::function<bool(const Signature&)>& state) {

    //log("QQ_ADD %s\n", TOSTR(op.getSignature()));

    if (!_params.isSet("cqm")) return;
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

        if (bad.empty() || std::min(good.size(), bad.size()) > 16) continue;

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