
#include <regex>

#include "data/htn_instance.h"

 
ParsedProblem& HtnInstance::parse(std::string domainFile, std::string problemFile) {

    const char* firstArg = "pandaPIparser";
    const char* domainStr = domainFile.c_str();
    const char* problemStr = problemFile.c_str();

    char* args[3];
    args[0] = (char*)firstArg;
    args[1] = (char*)domainStr;
    args[2] = (char*)problemStr;
    
    run_pandaPIparser(3, args);
    return get_parsed_problem();
}

HtnInstance::HtnInstance(Parameters& params, ParsedProblem& p) : _params(params), _p(p) {

    Names::init(_name_back_table);
    _instantiator = new Instantiator(params, *this);
    _effector_table = new EffectorTable(*this);

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
    for (auto sort_pair : _p.sorts) {
        log(" %s : ", sort_pair.first.c_str());
        for (auto c : sort_pair.second) {
            log("%s ", c.c_str());
        }
        log("\n");
    }

    // Create actions
    for (task t : primitive_tasks) {
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

    // If necessary, compile out actions which have some effect predicate
    // in positive AND negative form: create two new actions in these cases
    if (_params.isSet("q") || _params.isSet("qq")) {

        std::unordered_map<int, Action> newActions;

        for (const auto& pair : _actions) {
            int aId = pair.first;
            const Action& a = pair.second;
            const Signature& aSig = a.getSignature();

            // Find all negative effects to move
            std::unordered_set<int> posEffPreds;
            std::unordered_set<Signature, SignatureHasher> negEffsToMove;
            // Collect positive effect predicates
            for (const Signature& eff : a.getEffects()) {
                if (eff._negated) continue;
                posEffPreds.insert(eff._name_id);
            }
            // Match against negative effects
            for (const Signature& eff : a.getEffects()) {
                if (!eff._negated) continue;
                if (posEffPreds.count(eff._name_id)) {
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
            _signature_sorts_table[aFirst.getSignature()._name_id] = _signature_sorts_table[aId];
            newActions[idFirst] = aFirst;

            // Second action: no preconditions, all other effects
            int idSecond = getNameId(oldName + "_SECOND");
            Action aSecond = Action(idSecond, aSig._args);
            for (const Signature& eff : a.getEffects()) {
                if (!negEffsToMove.count(eff)) aSecond.addEffect(eff);
            }
            _signature_sorts_table[aSecond.getSignature()._name_id] = _signature_sorts_table[aId];
            newActions[idSecond] = aSecond;

            // Replace all occurrences of the action with BOTH new actions in correct order
            for (auto& rPair : _reductions) {
                Reduction& r = rPair.second;
                bool change = false;
                std::vector<Signature> newSubtasks;
                for (int i = 0; i < r.getSubtasks().size(); i++) {
                    const Signature& subtask = r.getSubtasks()[i];
                    if (subtask._name_id == aId) {
                        // Replace
                        change = true;
                        substitution_t s = Substitution::get(aSig._args, subtask._args);
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

            log("REPLACE_ACTION %s => \n  <\n    %s,\n    %s\n  >\n", Names::to_string(a).c_str(), 
                    Names::to_string(aFirst).c_str(), 
                    Names::to_string(aSecond).c_str());
            
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
            
            // Instantiate all possible init. reductions
            _init_reduction_choices = _instantiator->getApplicableInstantiations(
                    _init_reduction, std::unordered_map<int, SigSet>(), INSTANTIATE_FULL);
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

std::vector<int> HtnInstance::getArguments(const std::vector<std::pair<string, string>>& vars) {
    std::vector<int> args;
    for (auto var : vars) {
        args.push_back(getNameId(var.first));
    }
    return args;
}
std::vector<int> HtnInstance::getArguments(const std::vector<std::string>& vars) {
    std::vector<int> args;
    for (auto var : vars) {
        args.push_back(getNameId(var));
    }
    return args;
}

Signature HtnInstance::getSignature(const task& task) {
    return Signature(getNameId(task.name), getArguments(task.vars));
}
Signature HtnInstance::getSignature(const method& method) {
    return Signature(getNameId(method.name), getArguments(method.vars));
}
Signature HtnInstance::getSignature(const literal& literal) {
    Signature sig = Signature(getNameId(literal.predicate), getArguments(literal.arguments));
    if (!literal.positive) sig.negate();
    return sig;
}
Signature HtnInstance::getInitTaskSignature(int pos) {
    Signature subtask = _init_reduction.getSubtasks()[pos];
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
    subtask = subtask.substitute(Substitution::get(subtask._args, newArgs));
    return subtask;
}

SigSet HtnInstance::getInitState() {
    SigSet result;
    for (ground_literal lit : init) {
        Signature sig(getNameId(lit.predicate), getArguments(lit.args));
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
                log("EQUALITY %s\n", Names::to_string(sig).c_str());
            }
        }
    }

    return result;
}
SigSet HtnInstance::getGoals() {
    SigSet result;
    for (ground_literal lit : goal) {
        Signature sig(getNameId(lit.predicate), getArguments(lit.args));
        if (!lit.positive) sig.negate();
        result.insert(sig);
    }
    return result;
}

void HtnInstance::extractPredSorts(const predicate_definition& p) {
    int pId = getNameId(p.name);
    std::vector<int> sorts;
    for (auto var : p.argument_sorts) {
        sorts.push_back(getNameId(var));
    }
    assert(!_signature_sorts_table.count(pId));
    _signature_sorts_table[pId] = sorts;
}
void HtnInstance::extractTaskSorts(const task& t) {
    std::vector<int> sorts;
    for (auto var : t.vars) {
        int sortId = getNameId(var.second);
        sorts.push_back(sortId);
    }
    int tId = getNameId(t.name);
    assert(!_signature_sorts_table.count(tId));
    _signature_sorts_table[tId] = sorts;
}
void HtnInstance::extractMethodSorts(const method& m) {
    std::vector<int> sorts;
    for (auto var : m.vars) {
        int sortId = getNameId(var.second);
        sorts.push_back(sortId);
    }
    int mId = getNameId(m.name);
    assert(!_signature_sorts_table.count(mId));
    _signature_sorts_table[mId] = sorts;
}
void HtnInstance::extractConstants() {
    for (auto sortPair : _p.sorts) {
        int sortId = getNameId(sortPair.first);
        _constants_by_sort[sortId] = std::vector<int>();
        std::vector<int>& constants = _constants_by_sort[sortId];
        for (std::string c : sortPair.second) {
            constants.push_back(getNameId(c));
            //log("constant %s of sort %s\n", c.c_str(), sortPair.first.c_str());
        }
    }
}

Reduction& HtnInstance::createReduction(const method& method) {
    int id = getNameId(method.name);
    std::vector<int> args = getArguments(method.vars);
    
    int taskId = getNameId(method.at);
    std::vector<int> taskArgs = getArguments(method.atargs);
    _task_id_to_reduction_ids[taskId];
    _task_id_to_reduction_ids[taskId].push_back(id);

    assert(_reductions.count(id) == 0);
    _reductions[id] = Reduction(id, args, Signature(taskId, taskArgs));

    std::vector<literal> condLiterals;

    // Extract (in)equality constraints, put into preconditions to process later
    if (!method.constraints.empty()) {
        for (const literal& lit : method.constraints) {            
            //log("%s\n", Names::to_string(getSignature(lit)).c_str());
            assert(lit.predicate == "__equal" || fail("Unknown constraint predicate \"" + lit.predicate + "\"!\n"));
            condLiterals.push_back(lit);
        }
    }

    // Go through expansion of the method
    std::map<std::string, int> subtaskTagToIndex;   
    for (plan_step st : method.ps) {

        if (st.task.rfind("__method_precondition_", 0) == 0) {
            // This "subtask" is a method precondition which was compiled out
            
            // Find primitive task belonging to this method precondition
            task precTask;
            for (task t : primitive_tasks) {
                if (t.name == st.task) {
                    precTask = t;
                    break;
                }
            }
            // Add its preconditions to the method's preconditions
            for (literal lit : precTask.prec) {
                log("COND_LIT %s\n", lit.predicate.c_str());
                condLiterals.push_back(lit);
            }
            // (Do not add the task to the method's subtasks)

        } else {
            
            // Actual subtask
            Signature sig(getNameId(st.task), getArguments(st.args));
            _reductions[id].addSubtask(sig);
            subtaskTagToIndex[st.id] = subtaskTagToIndex.size();
        }
    }

    // Process preconditions and constraints of the method
    for (literal lit : condLiterals) {

        if (lit.predicate == "__equal") {
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
            std::vector<int> args; args.push_back(getNameId(arg1Str)); args.push_back(getNameId(arg2Str));
            _reductions[id].addPrecondition(Signature(newPredId, args, !lit.positive));

        } else {
            // Normal precondition
            Signature sig = getSignature(lit);
            _reductions[id].addPrecondition(sig);
        }
    }

    // Order subtasks
    if (!method.ordering.empty()) {
        std::map<int, std::vector<int>> orderingNodelist;
        for (auto order : method.ordering) {
            int indexLeft = subtaskTagToIndex[order.first];
            int indexRight = subtaskTagToIndex[order.second];
            assert(indexLeft >= 0 && indexLeft < _reductions[id].getSubtasks().size());
            assert(indexRight >= 0 && indexRight < _reductions[id].getSubtasks().size());
            orderingNodelist[indexLeft];
            orderingNodelist[indexLeft].push_back(indexRight);
        }
        _reductions[id].orderSubtasks(orderingNodelist);
    }

    log("ORDERING %s < ", Names::to_string(_reductions[id].getSignature()).c_str());
    for (Signature sg : _reductions[id].getSubtasks()) {
        log("%s ", Names::to_string(sg).c_str());
    }
    log(">\n");

    log(" %s : %i preconditions, %i subtasks\n", Names::to_string(_reductions[id].getSignature()).c_str(), 
                _reductions[id].getPreconditions().size(), 
                _reductions[id].getSubtasks().size());
    return _reductions[id];
}
Action& HtnInstance::createAction(const task& task) {
    int id = getNameId(task.name);
    std::vector<int> args = getArguments(task.vars);

    assert(_actions.count(id) == 0);
    _actions[id] = Action(id, args);
    for (auto p : task.prec) {
        Signature sig = getSignature(p);
        _actions[id].addPrecondition(sig);
    }
    for (auto p : task.eff) {
        Signature sig = getSignature(p);
        _actions[id].addEffect(sig);
        if (_params.isSet("rrp")) _fluent_predicates.insert(sig._name_id);
    }
    _actions[id].removeInconsistentEffects();
    return _actions[id];
}

SigSet HtnInstance::getAllFactChanges(const Signature& sig) {    
    SigSet result;
    if (sig == Position::NONE_SIG) return result;
    //log("FACT_CHANGES %s : ", Names::to_string(sig).c_str());
    for (Signature effect : _effector_table->getPossibleFactChanges(sig)) {
        std::vector<Signature> instantiation = ArgIterator::getFullInstantiation(effect, *this);
        for (Signature i : instantiation) {
            assert(_instantiator->isFullyGround(i));
            if (_params.isSet("rrp")) _fluent_predicates.insert(i._name_id);
            result.insert(i);
            //log("%s ", Names::to_string(i).c_str());
        }
    }
    //log("\n");
    return result;
}

Action HtnInstance::replaceQConstants(Action& a, int layerIdx, int pos) {
    Signature sig = a.getSignature();
    std::unordered_map<int, int> s = addQConstants(sig, layerIdx, pos);
    HtnOp op = a.substitute(s);
    return Action(op);
}
Reduction HtnInstance::replaceQConstants(Reduction& red, int layerIdx, int pos) {
    Signature sig = red.getSignature();
    std::unordered_map<int, int> s = addQConstants(sig, layerIdx, pos);
    return red.substituteRed(s);
}

std::unordered_map<int, int> HtnInstance::addQConstants(Signature& sig, int layerIdx, int pos) {
    std::unordered_map<int, int> s;
    std::vector<int> freeArgPositions = _instantiator->getFreeArgPositions(sig);
    for (int argPos : freeArgPositions) {
        addQConstant(layerIdx, pos, sig, argPos, s);
    }
    return s;
}

void HtnInstance::addQConstant(int layerIdx, int pos, Signature& sig, int argPos, std::unordered_map<int, int>& s) {

    int arg = sig._args[argPos];
    assert(_name_back_table[arg][0] == '?');
    assert(_signature_sorts_table.count(sig._name_id));
    assert(argPos < _signature_sorts_table[sig._name_id].size());
    int sort = _signature_sorts_table[sig._name_id][argPos];
    //log("%s\n", Names::to_string(sig).c_str());

    // Compute domain of the q constant
    std::unordered_set<int> domain;
    //log("DOMAIN %s : { ", Names::to_string(qConstId).c_str());
    for (int c : _constants_by_sort[sort]) {
        // A q constant may *not* be substituted by another q constant
        if (!_q_constants.count(c)) {
            domain.insert(c);
            //log("%s ", Names::to_string(c).c_str());
        } 
    }
    //log("}\n");  

    // If there is only a single option for the q constant: 
    // just insert that one option.
    if (domain.size() == 1) {
        int c; for (int x : domain) c = x;
        s[arg] = c;
        return;
    }   
     
    std::string qConstName = "Q_" + std::to_string(layerIdx) + "," 
        + std::to_string(pos) + "_" + std::to_string(SignatureHasher()(sig))
        + ":" + std::to_string(argPos) + "_" + _name_back_table[sort];
    int qConstId = getNameId(qConstName);

    // Add q const to substitution
    s[arg] = qConstId;
    
    // check if q const of that name already exists!
    if (_q_constants.count(qConstId)) return;
    _q_constants.insert(qConstId);

    // CALCULATE SORTS OF Q CONSTANT

    // 1. take single sort of the q-constant to start with: int sort.
    // 2. assume that the q-constant is of ALL (super) sorts
    std::set<int> qConstSorts;
    for (auto sortPair : _p.sorts) qConstSorts.insert(getNameId(sortPair.first));

    // 3. for each constant of the single sort:
    //      remove all q-constant sorts NOT containing that constant
    for (int c : _constants_by_sort[sort]) {
        std::vector<int> sortsToRemove;
        for (int qsort : qConstSorts) {
            if (std::find(_constants_by_sort[qsort].begin(), _constants_by_sort[qsort].end(), c) 
                    == _constants_by_sort[qsort].end()) {
                sortsToRemove.push_back(qsort);
            }
        }
        for (int remSort : sortsToRemove) qConstSorts.erase(remSort);
    }
    // RESULT: The intersection of all sorts of all eligible constants
/*
    // Collect all types of the q-constant
    std::vector<std::string> containedSorts;
    containedSorts.push_back(_name_back_table[sort]);
    for (int i = 0; i < containedSorts.size(); i++) {
        std::string containedSort = containedSorts[i];
        for (sort_definition sd : _p.sort_definitions) {
            if (sd.has_parent_sort && sd.parent_sort == containedSort) {
                for (std::string newSort : sd.declared_sorts) {
                    containedSorts.push_back(newSort);
                }
            }
        }
    }
    // Filter sorts which were simplified away
    std::unordered_set<int> sortsSet;   
    for (std::string sortStr : containedSorts) {
        if (_p.sorts.count(sortStr)) sortsSet.insert(getNameId(sortStr));
    }*/

    log("  q-constant for arg %s @ pos %i of %s : %s\n   sorts ", 
            _name_back_table[arg].c_str(), argPos, 
            Names::to_string(sig).c_str(), Names::to_string(qConstId).c_str());
    _sorts_of_q_constants[qConstId];
    for (int sort : qConstSorts) {
        _sorts_of_q_constants[qConstId].push_back(sort);
        //_constants_by_sort[sort].push_back(qConstId);
        log("%s ", Names::to_string(sort).c_str());
    } 
    log("\n");

    assert(!domain.empty());
    _domains_of_q_constants[qConstId] = domain;
}

std::vector<Signature> HtnInstance::getDecodedObjects(Signature qSig) {
    if (!hasQConstants(qSig)) return std::vector<Signature>();

    assert(_instantiator->isFullyGround(qSig));
    std::vector<std::vector<int>> eligibleArgs(qSig._args.size());
    for (int argPos = 0; argPos < qSig._args.size(); argPos++) {
        int arg = qSig._args[argPos];
        if (_q_constants.count(arg)) {
            // q constant
            assert(_domains_of_q_constants.count(arg) && _domains_of_q_constants[arg].size() > 0);
            for (int c : _domains_of_q_constants[arg]) {
                eligibleArgs[argPos].push_back(c);
            }
        } else {
            // normal constant
            eligibleArgs[argPos].push_back(arg);
        }
        assert(eligibleArgs[argPos].size() > 0);
    }

    std::vector<Signature> i = ArgIterator::instantiate(qSig, eligibleArgs);
    //log("DECODED_FACTS %s : { ", Names::to_string(qFact).c_str());
    for (Signature sig : i) {
        //log("%s ", Names::to_string(sig).c_str());
    }
    //log("}\n");

    return i; 
}

std::unordered_set<int> HtnInstance::getSortsOfQConstant(int qconst) {
    std::unordered_set<int> sorts;
    for (int sort : _sorts_of_q_constants[qconst]) sorts.insert(sort);
    return sorts;
}

std::unordered_set<int> HtnInstance::getConstantsOfSort(int sort) {
    std::unordered_set<int> cs;
    for (int c : _constants_by_sort[sort]) cs.insert(c);
    return cs; 
}

bool HtnInstance::hasQConstants(const Signature& sig) {
    for (int arg : sig._args) {
        if (_q_constants.count(arg)) return true;
    }
    return false;
}

bool HtnInstance::isRigidPredicate(int predId) {
    if (!_params.isSet("rrp")) return false;
    return !_fluent_predicates.count(predId);
}

void HtnInstance::removeRigidConditions(HtnOp& op) {

    SigSet newPres;
    for (const Signature& pre : op.getPreconditions()) {
        if (!isRigidPredicate(pre._name_id)) {
            newPres.insert(pre); // only add fluent preconditions
        }
    }
    op.setPreconditions(newPres);
}
