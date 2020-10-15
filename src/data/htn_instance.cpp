
#include <algorithm>
#include <getopt.h>
#include <sys/stat.h>
#include <iomanip>

#include "data/htn_instance.h"
#include "data/instantiator.h"
#include "util/regex.h"

#include "libpanda.hpp"

Action HtnInstance::BLANK_ACTION;

HtnInstance::HtnInstance(Parameters& params) :
             _params(params), _p(*parse(params.getDomainFilename(), params.getProblemFilename())), 
            _q_db([this](int arg) {return isQConstant(arg);}),
            _use_q_constant_mutexes(_params.getIntParam("qcm") > 0),
            _share_q_constants(_params.isNonzero("sqq")) {

    Names::init(_name_back_table);
    _instantiator = new Instantiator(params, *this);
    
    // Create blank action without any preconditions or effects
    int blankId = nameId("__BLANK___");
    BLANK_ACTION = Action(blankId, std::vector<int>());
    _actions[blankId] = BLANK_ACTION;
    addAction(BLANK_ACTION);
    _blank_action_sig = BLANK_ACTION.getSignature();
    _signature_sorts_table[blankId];

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

    // Create reductions
    for (method& method : methods) {
        createReduction(method);
    }

    // Instantiate possible "root" / "top" methods
    for (const auto& rPair : _reductions) {
        const Reduction& r = rPair.second;
        if (_name_back_table[r.getNameId()].rfind("__top_method") == 0) {
            // Initial "top" method
            _init_reduction = r;
        }
    }

    if (_params.isNonzero("stats")) {
        printStatistics();
        exit(0);
    }

    // Create replacements for surrogate methods with only one subtask
    if (_params.isNonzero("surr")) replaceSurrogateReductionsWithAction();

    // If necessary, compile out actions which have some effect predicate
    // in positive AND negative form: create two new actions in these cases
    if (_params.isNonzero("sace")) splitActionsWithConflictingEffects();

    // Mine additional preconditions for reductions from their subtasks
    minePreconditions(MinePrecMode(_params.getIntParam("mp")));

    Log::i("%i operators and %i methods created.\n", _actions.size(), _reductions.size());

    _instantiator->computeMinNumPrimitiveChildren();
}

ParsedProblem* HtnInstance::parse(std::string domainFile, std::string problemFile) {

    const char* firstArg = "pandaPIparser";
    const char* domainStr = domainFile.c_str();
    const char* problemStr = problemFile.c_str();

    struct stat sb;
    if (stat(domainStr, &sb) != 0 || !S_ISREG(sb.st_mode)) {
        Log::e("Domain file \"%s\" is not a regular file. Exiting.\n", domainStr);
        exit(1);
    }
    if (stat(problemStr, &sb) != 0 || !S_ISREG(sb.st_mode)) {
        Log::e("Problem file \"%s\" is not a regular file. Exiting.\n", problemStr);
        exit(1);
    }

    char* args[3];
    args[0] = (char*)firstArg;
    args[1] = (char*)domainStr;
    args[2] = (char*)problemStr;

    ParsedProblem* p = new ParsedProblem();
    optind = 1;
    run_pandaPIparser(3, args, *p);
    return p;
}

void HtnInstance::printStatistics() {
    static size_t BIG = 999999UL;

    size_t maxPredicateArity = 0;

    size_t minExpansionSize = BIG;
    size_t maxExpansionSize = 0;
    size_t maxLiftedBranchingFactor = 0;
    
    size_t maxReductionPreconditions = 0;
    size_t maxReductionArity = 0;
    size_t maxReductionFreeArgs = 0;
    
    size_t maxActionPreconditions = 0;
    size_t maxActionEffects = 0;
    size_t maxActionArity = 0;

    for (auto predId : _predicate_ids) {
        maxPredicateArity = std::max(maxPredicateArity, _signature_sorts_table[predId].size());
    }

    FlatHashMap<int, size_t> numMethodsMatchingTaskNameId;
    for (const auto& [nameId, r] : _reductions) {
        if (_name_back_table[r.getNameId()].rfind("__top_method") == 0)
            continue;
        minExpansionSize = std::min(minExpansionSize, r.getSubtasks().size());
        maxExpansionSize = std::max(maxExpansionSize, r.getSubtasks().size());
        maxReductionPreconditions = std::max(maxReductionPreconditions, r.getPreconditions().size());
        maxReductionArity = std::max(maxReductionArity, r.getArguments().size());
        maxReductionFreeArgs = std::max(maxReductionFreeArgs, getNumFreeArguments(r));
        
        int taskNameId = r.getTaskSignature()._name_id;
        if (!numMethodsMatchingTaskNameId.count(taskNameId)) 
            numMethodsMatchingTaskNameId[taskNameId] = 1;
        else numMethodsMatchingTaskNameId[taskNameId]++;
    }
    for (const auto& [nameId, a] : _actions) {
        maxActionPreconditions = std::max(maxActionPreconditions, a.getPreconditions().size());
        maxActionEffects = std::max(maxActionEffects, a.getEffects().size());
        maxActionArity = std::max(maxActionArity, a.getArguments().size());
    }

    for (const auto& [taskNameId, numMethods] : numMethodsMatchingTaskNameId) {
        maxLiftedBranchingFactor = std::max(maxLiftedBranchingFactor, numMethods);
    }

    FlatHashSet<int> allConstants;
    for (const auto& [sort, constants] : _constants_by_sort) {
        allConstants.insert(constants.begin(), constants.end());
    }
    int initStateSize = getInitState().size();
    int numInitTasks = getInitReduction().getSubtasks().size();

    Log::e("Domain stats:\n");

    Log::e("STATS numoperators %lu\n", _actions.size()-1); // minus blank action
    Log::e("STATS nummethods %lu\n", _reductions.size()-1); // minus __top_method
    
    Log::e("STATS maxexpansionsize %lu\n", maxExpansionSize);
    Log::e("STATS maxliftedbranchingfactor %lu\n", maxLiftedBranchingFactor);
    Log::e("STATS maxreductionfreeargs %lu\n", maxReductionFreeArgs);
    
    Log::e("STATS maxactionpreconditions %lu\n", maxActionPreconditions);
    Log::e("STATS maxreductionpreconditions %lu\n", maxReductionPreconditions);
    Log::e("STATS maxactioneffects %lu\n", maxActionEffects);
    
    Log::e("STATS maxpredicatearity %lu\n", maxPredicateArity);    
    Log::e("STATS maxactionarity %lu\n", maxActionArity);
    Log::e("STATS maxreductionarity %lu\n", maxReductionArity);    

    Log::e("Problem stats:\n");

    Log::e("STATS numconstants %lu\n", allConstants.size());
    Log::e("STATS numinitfacts %lu\n", initStateSize);
    Log::e("STATS numinittasks %lu\n", numInitTasks);
}

size_t HtnInstance::getNumFreeArguments(const Reduction& r) {
    size_t freeArgs = 0;
    for (int arg : r.getArguments()) {
        if (std::find(r.getTaskArguments().begin(), r.getTaskArguments().end(), arg) == r.getTaskArguments().end()) {
            // Argument is not contained in task arguments: Free variable
            freeArgs++;
        }
    }
    return freeArgs;
}

void HtnInstance::replaceSurrogateReductionsWithAction() {

    for (const auto& entry : _reductions) {
        const Reduction& red = entry.second;
        if (red.getSubtasks().size() != 1) continue;
        
        // Surrogate method
        USignature childSig = red.getSubtasks().at(0);
        int childId = childSig._name_id;
        if (!_actions.count(childId)) continue;
        
        // Primitive subtask
        Substitution s(_actions.at(childId).getArguments(), childSig._args);
        Action childAct = _actions.at(childId).substitute(s);
        std::string name = "__SURROGATE*" + std::string(TOSTR(entry.first)) + "*" + std::string(TOSTR(childId)) + "*";
        int id = nameId(name);
        Log::d("SURROGATE %s %i\n", name.c_str(), entry.first);
        _actions[id] = Action(id, red.getArguments());
        for (const auto& pre : red.getPreconditions()) _actions[id].addPrecondition(pre);
        for (const auto& pre : red.getExtraPreconditions()) _actions[id].addExtraPrecondition(pre);
        for (const auto& pre : childAct.getPreconditions()) _actions[id].addPrecondition(pre);
        for (const auto& pre : childAct.getExtraPreconditions()) _actions[id].addExtraPrecondition(pre);
        for (const auto& eff : childAct.getEffects()) _actions[id].addEffect(eff);
        _reduction_to_surrogate[entry.first] = id;
        _signature_sorts_table[id] = _signature_sorts_table[entry.first];
        //Log::d("SURROGATE par: %s\n", TOSTR(red));
        //Log::d("SURROGATE src: %s\n", TOSTR(_actions[childId]));
        //Log::d("SURROGATE des: %s\n", TOSTR(_actions[id]));
        _surrogate_to_orig_parent_and_child[id] = std::pair<int, int>(entry.first, childId);
    }
}

void HtnInstance::splitActionsWithConflictingEffects() {

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
        int idFirst = nameId(oldName + "_FIRST");
        Action aFirst = Action(idFirst, aSig._args);
        aFirst.setPreconditions(a.getPreconditions());
        for (const Signature& eff : negEffsToMove) aFirst.addEffect(eff);
        _signature_sorts_table[aFirst.getNameId()] = aSorts;
        newActions[idFirst] = aFirst;

        // Second action: no preconditions, all other effects
        int idSecond = nameId(oldName + "__LLT_SECOND");
        Action aSecond = Action(idSecond, aSig._args);
        for (const Signature& eff : a.getEffects()) {
            if (!negEffsToMove.count(eff)) aSecond.addEffect(eff);
        }
        _signature_sorts_table[aSecond.getNameId()] = aSorts;
        newActions[idSecond] = aSecond;

        // Replace all occurrences of the action with BOTH new actions in correct order
        for (auto& rPair : _reductions) {
            Reduction& r = rPair.second;
            bool change = false;
            std::vector<USignature> newSubtasks;
            for (size_t i = 0; i < r.getSubtasks().size(); i++) {
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
                r.setSubtasks(std::move(newSubtasks));
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

void HtnInstance::minePreconditions(MinePrecMode mode) {
    
    if (mode == NO_MINING) return;

    int precondsBefore = 0;
    int minedPreconds = 0;
    for (auto& [rId, r] : _reductions) {
        precondsBefore += r.getPreconditions().size();
        // Mine additional preconditions, if possible
        auto factFrame = _instantiator->getFactFrame(r.getSignature(), /*simpleMode=*/true);
        for (auto& pre : factFrame.preconditions) {
            if (!r.getPreconditions().count(pre)) {
                
                bool hasFreeArgs = false;
                for (int arg : pre._usig._args) hasFreeArgs |= arg == nameId("??_");
                if (hasFreeArgs) continue;

                Log::d("%s : MINED_PRE %s\n", TOSTR(r.getSignature()), TOSTR(pre));
                if (mode == USE_FOR_INSTANTIATION) {
                    r.addExtraPrecondition(std::move(pre));
                }
                if (mode == USE_EVERYWHERE) {
                    r.addPrecondition(std::move(pre));
                }
                minedPreconds++;
            }
        }
    }

    float newRatio = precondsBefore == 0 ? std::numeric_limits<float>::infinity() : 
            100.f * (((float)precondsBefore+minedPreconds) / precondsBefore - 1);

    Log::i("Mined %i new reduction preconditions (+%.1f%%).\n", minedPreconds, newRatio);
}

int HtnInstance::nameId(const std::string& name, bool createQConstant, int layerIdx, int pos) {
    int id = -1;
    if (!_name_table.count(name)) {
        if (createQConstant) {
            id = std::numeric_limits<int>::max() - _q_constants_with_origin.size();
            assert(layerIdx >= 0 && pos >= 0);
            _q_constants_with_origin[id] = IntPair(layerIdx, pos);
        } else {
            id = _name_table_running_id++;
            if (name[0] == '?') {
                // variable
                _var_ids.insert(id);
            }
        }
        _name_table[name] = id;
        _name_back_table[id] = name;
    }
    return id == -1 ? _name_table[name] : id;
}

std::string HtnInstance::toString(int id) const {
    return _name_back_table.at(id);
}

std::vector<int> HtnInstance::convertArguments(int predNameId, const std::vector<std::pair<string, string>>& vars) {
    std::vector<int> args;
    for (const auto& var : vars) {
        int id = var.first[0] == '?' ? nameId(var.first + "_" + std::to_string(predNameId)) : nameId(var.first);
        args.push_back(id);
    }
    return args;
}
std::vector<int> HtnInstance::convertArguments(int predNameId, const std::vector<std::string>& vars) {
    std::vector<int> args;
    for (const auto& var : vars) {
        int id = var[0] == '?' ? nameId(var + "_" + std::to_string(predNameId)) : nameId(var);
        args.push_back(id);
    }
    return args;
}

USignature HtnInstance::convertSignature(const task& task) {
    return USignature(nameId(task.name), convertArguments(nameId(task.name), task.vars));
}
USignature HtnInstance::convertSignature(const method& method) {
    return USignature(nameId(method.name), convertArguments(nameId(method.name), method.vars));
}
Signature HtnInstance::convertSignature(int parentNameId, const literal& literal) {
    Signature sig = Signature(nameId(literal.predicate), convertArguments(parentNameId, literal.arguments));
    if (!literal.positive) sig.negate();
    return sig;
}

USigSet HtnInstance::getInitState() {
    USigSet result;
    for (const ground_literal& lit : _p.init) if (lit.positive) {
        result.emplace(nameId(lit.predicate), convertArguments(nameId(lit.predicate), lit.args));
    }

    // Insert all necessary equality predicates

    // For each equality predicate:
    for (int eqPredId : _equality_predicates) {

        // For each pair of constants of correct sorts: TODO something more efficient
        const std::vector<int>& sorts = getSorts(eqPredId);
        assert(sorts[0] == sorts[1]);
        for (int c1 : _constants_by_sort[sorts[0]]) {
            for (int c2 : _constants_by_sort[sorts[1]]) {

                // Add equality lit to state if the two are equal
                if (c1 != c2) continue;
                std::vector<int> args;
                args.push_back(c1); args.push_back(c2);
                Log::d("EQUALITY %s\n", TOSTR(args));
                result.emplace(eqPredId, std::move(args));
            }
        }
    }

    return result;
}

SigSet HtnInstance::extractGoals() {
    SigSet result;
    for (const ground_literal& lit : _p.goal) {
        Signature sig(nameId(lit.predicate), convertArguments(nameId(lit.predicate), lit.args));
        if (!lit.positive) sig.negate();
        result.insert(sig);
    }
    return result;
}

Action HtnInstance::getGoalAction() {

    // Create virtual goal action
    Action goalAction(nameId("_GOAL_ACTION_"), std::vector<int>());
    USignature goalSig = goalAction.getSignature();
    
    // Extract primitive goals, add to preconds of goal action
    for (Signature& fact : extractGoals()) {
        goalAction.addPrecondition(std::move(fact));
    }
    _actions_by_sig[goalSig] = goalAction;
    _actions[goalSig._name_id] = goalAction;

    return goalAction;
}

const Reduction& HtnInstance::getInitReduction() {
    return _init_reduction;
}

const USignature& HtnInstance::getBlankActionSig() {
    return _blank_action_sig;
}

void HtnInstance::extractPredSorts(const predicate_definition& p) {
    int pId = nameId(p.name);
    _predicate_ids.insert(pId);
    std::vector<int> sorts;
    for (const std::string& var : p.argument_sorts) {
        sorts.push_back(nameId(var));
    }
    assert(!_signature_sorts_table.count(pId));
    _signature_sorts_table[pId] = std::move(sorts);
}

void HtnInstance::extractTaskSorts(const task& t) {
    std::vector<int> sorts;
    for (const auto& var : t.vars) {
        int sortId = nameId(var.second);
        sorts.push_back(sortId);
    }
    int tId = nameId(t.name);
    assert(!_signature_sorts_table.count(tId));
    _signature_sorts_table[tId] = std::move(sorts);
    _original_n_taskvars[tId] = t.number_of_original_vars;
}

void HtnInstance::extractMethodSorts(const method& m) {
    std::vector<int> sorts;
    for (const auto& var : m.vars) {
        int sortId = nameId(var.second);
        sorts.push_back(sortId);
    }
    int mId = nameId(m.name);
    assert(!_signature_sorts_table.count(mId));
    _signature_sorts_table[mId] = std::move(sorts);
}

void HtnInstance::extractConstants() {
    for (const auto& sortPair : _p.sorts) {
        int sortId = nameId(sortPair.first);
        _constants_by_sort[sortId];
        FlatHashSet<int>& constants = _constants_by_sort[sortId];
        for (const std::string& c : sortPair.second) {
            constants.insert(nameId(c));
            //log("constant %s of sort %s\n", c.c_str(), sortPair.first.c_str());
        }
    }
}

Reduction& HtnInstance::createReduction(method& method) {
    int id = nameId(method.name);
    std::vector<int> args = convertArguments(id, method.vars);
    
    int taskId = nameId(method.at);
    _task_id_to_reduction_ids[taskId];
    _task_id_to_reduction_ids[taskId].push_back(id);
    {
        std::vector<int> taskArgs = convertArguments(id, method.atargs);
        assert(_reductions.count(id) == 0);
        _reductions[id] = Reduction(id, args, USignature(taskId, std::move(taskArgs)));
    }
    Reduction& r = _reductions[id];

    std::vector<literal> condLiterals;

    // Extract (in)equality constraints, put into preconditions to process later   
    for (const literal& lit : method.constraints) {            
        assert(lit.predicate == "__equal" || Log::e("Unknown constraint predicate \"\"!\n", lit.predicate.c_str()));
        condLiterals.push_back(lit);
    }

    // Go through expansion of the method
    std::map<std::string, size_t> subtaskTagToIndex;   
    for (const plan_step& st : method.ps) {
        
        // Normalize task name
        std::string subtaskName = st.task;
        Regex::extractCoreNameOfSplittingMethod(subtaskName);
        Log::d("%s\n", subtaskName.c_str());

        if (subtaskName.rfind(method_precondition_action_name) != std::string::npos) {
            // This "subtask" is a method precondition which was compiled out
            
            // Find primitive task belonging to this method precondition
            task precTask;
            size_t maxSize = 0;
            int numFound = 0;
            for (const task& t : primitive_tasks) {
                
                // Normalize task name
                std::string taskName = t.name;
                Regex::extractCoreNameOfSplittingMethod(taskName);

                //Log::d(" ~~~ %s\n", taskName.c_str());
                if (subtaskName.rfind(taskName) != std::string::npos) {

                    size_t size = t.name.size();
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

                int varId = nameId(varPair.first + "_" + std::to_string(id));
                if (std::find(args.begin(), args.end(), varId) == args.end()) {
                    // Arg is not contained, must be added
                    r.addArgument(varId);
                    args = r.getArguments();
                    _signature_sorts_table[id].push_back(nameId(varPair.second));
                    method.vars.push_back(varPair);
                }
            }

            // (Do not add the task to the method's subtasks)

        } else {
            // Actual subtask
            _reductions[id].addSubtask(USignature(nameId(st.task), convertArguments(id, st.args)));
            subtaskTagToIndex[st.id] = subtaskTagToIndex.size();
        }
    }

    // Process constraints of the method
    for (auto& pre : extractEqualityConstraints(id, condLiterals, method.vars))
        _reductions[id].addPrecondition(std::move(pre));

    // Process preconditions of the method
    for (const literal& lit : condLiterals) {
        if (lit.predicate == dummy_equal_literal) continue;

        // Normal precondition
        _reductions[id].addPrecondition(convertSignature(id, lit));
    }

    // Order subtasks
    if (!method.ordering.empty()) {
        std::map<int, std::vector<int>> orderingNodelist;
        for (const auto& order : method.ordering) {
            size_t indexLeft = subtaskTagToIndex[order.first];
            size_t indexRight = subtaskTagToIndex[order.second];
            assert(indexLeft < _reductions[id].getSubtasks().size());
            assert(indexRight < _reductions[id].getSubtasks().size());
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
        Log::log_notime(Log::V4_DEBUG, "%s ", TOSTR(sig));
    }
    Log::log_notime(Log::V4_DEBUG, "\n");

    return _reductions[id];
}

Action& HtnInstance::createAction(const task& task) {
    int id = nameId(task.name);
    std::vector<int> args = convertArguments(id, task.vars);

    assert(_actions.count(id) == 0);
    _actions[id] = Action(id, std::move(args));

    // Process (equality) constraints
    for (auto& pre : extractEqualityConstraints(id, task.constraints, task.vars))
        _actions[id].addPrecondition(std::move(pre));
    for (auto& pre : extractEqualityConstraints(id, task.prec, task.vars))
        _actions[id].addPrecondition(std::move(pre));

    // Process preconditions
    for (const auto& p : task.prec) {
        _actions[id].addPrecondition(convertSignature(id, p));
    }
    for (const auto& p : task.eff) {
        _actions[id].addEffect(convertSignature(id, p));
    }
    _actions[id].removeInconsistentEffects();
    return _actions[id];
}

SigSet HtnInstance::extractEqualityConstraints(int opId, const std::vector<literal>& lits, const std::vector<std::pair<std::string, std::string>>& vars) { 
    SigSet result;

    for (const literal& lit : lits) {

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
            for (size_t argPos = 0; argPos < vars.size(); argPos++) {
                //Log::d("(%s,%s) ", method.vars[argPos].first.c_str(), method.vars[argPos].second.c_str());
                if (arg1Str == vars[argPos].first)
                    sort1 = nameId(vars[argPos].second);
                if (arg2Str == vars[argPos].first)
                    sort2 = nameId(vars[argPos].second);
            }
            //log("\n");
            assert(sort1 >= 0 && sort2 >= 0);
            // Use the "larger" sort as the sort for both argument positions
            int eqSort = (_constants_by_sort[sort1].size() > _constants_by_sort[sort2].size() ? sort1 : sort2);

            // Create equality predicate
            std::string newPredicate = "__equal_" + _name_back_table[eqSort] + "_" + _name_back_table[eqSort];
            int newPredId = nameId(newPredicate);
            if (!_signature_sorts_table.count(newPredId)) {
                // Predicate is new: remember sorts
                _signature_sorts_table[newPredId] = std::vector<int>(2, eqSort);
                _equality_predicates.insert(newPredId);
                _predicate_ids.insert(newPredId);
            }

            // Add as a precondition
            std::vector<int> args(2); 
            args[0] = nameId(arg1Str + "_" + std::to_string(opId)); 
            args[1] = nameId(arg2Str + "_" + std::to_string(opId));
            result.emplace(newPredId, std::move(args), !lit.positive);
        }
    }

    return result;
}

HtnOp& HtnInstance::getOp(const USignature& opSig) {
    auto it = _actions.find(opSig._name_id);
    if (it != _actions.end()) return (HtnOp&)it->second;
    else return (HtnOp&)_reductions.at(opSig._name_id);
}

const Action& HtnInstance::getActionTemplate(int nameId) const {
    return _actions.at(nameId);
}

const Reduction& HtnInstance::getReductionTemplate(int nameId) const {
    return _reductions.at(nameId);
}

const Action& HtnInstance::getAction(const USignature& sig) const {
    assert(_actions_by_sig.count(sig) || Log::e("No action created for %s yet!\n", TOSTR(sig)));
    return _actions_by_sig.at(sig);
}

const Reduction& HtnInstance::getReduction(const USignature& sig) const {
    return _reductions_by_sig.at(sig);
}

void HtnInstance::addAction(const Action& a) {
    _actions_by_sig[a.getSignature()] = a;
}

void HtnInstance::addReduction(const Reduction& r) {
    _reductions_by_sig[r.getSignature()] = r;
}

bool HtnInstance::hasReductions(int taskId) const {
    return _task_id_to_reduction_ids.count(taskId);
}

const std::vector<int>& HtnInstance::getReductionIdsOfTaskId(int taskId) const {
    return _task_id_to_reduction_ids.at(taskId);
}

bool HtnInstance::hasSurrogate(int reductionId) const {
    return _reduction_to_surrogate.count(reductionId);
}

const Action& HtnInstance::getSurrogate(int reductionId) const {
    return _actions.at(_reduction_to_surrogate.at(reductionId));
}

bool HtnInstance::isVirtualizedChildOfAction(int actionId) const {
    return _virtualized_to_actual_action.count(actionId);
}

int HtnInstance::getVirtualizedChildNameOfAction(int actionId) {
    return nameId("__REPEATED_" + _name_back_table[actionId]);
}

USignature HtnInstance::getVirtualizedChildOfAction(const USignature& action) {

    int repOpNameId = getVirtualizedChildNameOfAction(action._name_id);
    USignature sig(repOpNameId, action._args);

    if (!_actions_by_sig.count(sig)) {

        // Define the operator
        if (!_actions.count(repOpNameId)) {
            const Action& op = _actions[action._name_id];
            Action a(repOpNameId, op.getArguments());
            a.setPreconditions(op.getPreconditions());
            a.setExtraPreconditions(op.getExtraPreconditions());
            a.setEffects(op.getEffects());
            _actions[repOpNameId] = std::move(a);
            _virtualized_to_actual_action[repOpNameId] = action._name_id;
            const auto& sorts = _signature_sorts_table[action._name_id];
            _signature_sorts_table[repOpNameId] = sorts;
        }

        // Define the action
        Action a = _actions[repOpNameId];
        _actions_by_sig[sig] = a.substitute(Substitution(a.getArguments(), sig._args));
    }

    return sig;
}

const Action& HtnInstance::getActionOfVirtualizedChild(int vChildId) const {
    return _actions.at(_virtualized_to_actual_action.at(vChildId));
}

int HtnInstance::getActionNameOfVirtualizedChild(int vChildId) const {
    auto it = _virtualized_to_actual_action.find(vChildId);
    return it == _virtualized_to_actual_action.end() ? -1 : it->second;
}

Action HtnInstance::replaceVariablesWithQConstants(const Action& a, int layerIdx, int pos, const StateEvaluator& state) {
    std::vector<int> newArgs = replaceVariablesWithQConstants((const HtnOp&)a, layerIdx, pos, state);
    if (newArgs.size() == 1 && newArgs[0] == -1) {
        // No valid substitution.
        return a;
    }
    return toAction(a.getNameId(), newArgs);
}
Reduction HtnInstance::replaceVariablesWithQConstants(const Reduction& red, int layerIdx, int pos, const StateEvaluator& state) {
    std::vector<int> newArgs = replaceVariablesWithQConstants((const HtnOp&)red, layerIdx, pos, state);
    if (newArgs.size() == 1 && newArgs[0] == -1) {
        // No valid substitution.
        return red;
    }
    return red.substituteRed(Substitution(red.getArguments(), newArgs));
}

std::vector<int> HtnInstance::replaceVariablesWithQConstants(const HtnOp& op, int layerIdx, int pos, const StateEvaluator& state) {
    
    if (op.getArguments().size() == 0) return std::vector<int>();
    std::vector<int> vecFailure(1, -1);

    std::vector<int> args = op.getArguments();
    const std::vector<int>& sorts = _signature_sorts_table[op.getNameId()];
    std::vector<int> varargIndices;
    for (size_t i = 0; i < op.getArguments().size(); i++) {
        const int& arg = op.getArguments()[i];
        if (isVariable(arg)) varargIndices.push_back(i);
    }
    std::vector<bool> occursInPreconditions(op.getArguments().size(), false);
    std::vector<FlatHashSet<int>> domainPerVariable(op.getArguments().size());

    // Check each precondition regarding its valid decodings w.r.t. current state
    //const SigSet* preSets[2] = {&op.getPreconditions(), &op.getExtraPreconditions()};
    const SigSet* preSets[1] = {&op.getPreconditions()};
    for (const auto& preSet : preSets) for (const auto& preSig : *preSet) {

        // Check base condition; if unsatisfied, discard op 
        if (!_instantiator->test(preSig, state)) return vecFailure;

        // Find mapping from precond args to op args
        std::vector<int> opArgIndices(preSig._usig._args.size(), -1);
        for (size_t preIdx = 0; preIdx < preSig._usig._args.size(); preIdx++) {
            const int& arg = preSig._usig._args[preIdx];
            for (size_t i = 0; i < args.size(); i++) {
                if (args[i] == arg) {
                    opArgIndices[preIdx] = i;
                    occursInPreconditions[i] = true;
                    break;
                }
            }
        }

        // Compute sorts of the condition's args w.r.t. op signature
        std::vector<int> preSorts(preSig._usig._args.size());
        for (size_t i = 0; i < preSorts.size(); i++) {
            preSorts[i] = sorts[opArgIndices[i]];
        }

        // Check possible decodings of precondition
        bool any = false;
        bool anyValid = false;
        for (const auto& decUSig : decodeObjects(preSig._usig, /*restrictiveSorts=*/preSorts)) {
            any = true;
            assert(_instantiator->isFullyGround(decUSig));

            // Valid?
            if (!_instantiator->testWithNoVarsNoQConstants(decUSig, preSig._negated, state)) continue;
            
            // Valid precondition decoding found: Increase domain of concerned variables
            anyValid = true;
            for (size_t i = 0; i < opArgIndices.size(); i++) {
                int opArgIdx = opArgIndices[i];
                const int& arg = decUSig._args[i];
                if (opArgIdx >= 0) {
                    domainPerVariable[opArgIdx].insert(arg);
                }
            }
        }
        if (any && !anyValid) return vecFailure;
    }

    // Assemble new operator arguments
    FlatHashMap<int, int> numIntroducedQConstsPerType;
    NodeHashMap<int, std::vector<int>> domainsPerQConst;
    for (int i : varargIndices) {
        int vararg = args[i];
        auto& domain = domainPerVariable[i];
        if (!occursInPreconditions[i]) {
            domain = _constants_by_sort[sorts[i]];
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

            // Assemble name
            int sortCounter = 0;
            int primarySort = _signature_sorts_table[op.getSignature()._name_id][i];
            auto it = numIntroducedQConstsPerType.find(primarySort);
            if (it == numIntroducedQConstsPerType.end()) {
                numIntroducedQConstsPerType[primarySort] = 1;
            } else {
                sortCounter = it->second;
                it->second++;
            }
            std::vector<int> domainVec(domain.begin(), domain.end());
            std::stringstream domainHash;
            domainHash << std::hex << USignatureHasher()(USignature(primarySort, domainVec));
            std::string qConstName = "Q_" + std::to_string(layerIdx) + "," 
                + std::to_string(pos) + "_" + _name_back_table[primarySort]
                + ":" + std::to_string(sortCounter) 
                + "_" + domainHash.str()
                + (_share_q_constants ? std::string() : "_#"+std::to_string(_q_constants_with_origin.size()));
            
            // Initialize q-constant
            args[i] = nameId(qConstName, /*createQConstant=*/true, layerIdx, pos);
            addQConstant(args[i], domain);
            domainsPerQConst[args[i]] = std::move(domainVec);
            assert(getOriginOfQConstant(args[i]) == IntPair(layerIdx, pos));
            /*
            Log::d("QC %s : %s ~> %s ( ", TOSTR(op.getSignature()), TOSTR(vararg), TOSTR(args[i]), domain.size());
            for (int c : domain) {
                Log::log_notime(Log::V4_DEBUG, "%s ", TOSTR(c));
            }
            Log::log_notime(Log::V4_DEBUG, ")\n");
            */
        }
    }

    // Remember exact domain of each q constant for this operation
    USignature newSig(op.getSignature()._name_id, args);
    for (auto& [qconst, domain] : domainsPerQConst) {
        _q_const_to_op_domains[qconst][newSig] = std::move(domain);
    }

    return args;
}

void HtnInstance::addQConstant(int id, const FlatHashSet<int>& domain) {

    // Create or retrieve the exact sort (= domain of constants) for this q-constant
    std::string qSortName = "qsort_" + _name_back_table[id];
    int newSortId = nameId(qSortName);
    _constants_by_sort[newSortId].insert(domain.begin(), domain.end());
    _primary_sort_of_q_constants[id] = newSortId;

    // CALCULATE ADDITIONAL SORTS OF Q CONSTANT

    // 1. assume that the q-constant is of ALL (super) sorts
    FlatHashSet<int> qConstSorts;
    for (const auto& sortPair : _p.sorts) qConstSorts.insert(nameId(sortPair.first));

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
    _sorts_of_q_constants[id] = qConstSorts;
}

const std::vector<USignature> SIGVEC_EMPTY; 

ArgIterator HtnInstance::decodeObjects(const USignature& qSig,
        const std::vector<int>& restrictiveSorts) {

    std::vector<std::vector<int>> eligibleArgs;

    if (!hasQConstants(qSig) && _instantiator->isFullyGround(qSig)) 
        return ArgIterator(qSig._name_id, std::move(eligibleArgs));

    eligibleArgs.resize(qSig._args.size());
    size_t numChoices = 1;
    for (size_t argPos = 0; argPos < qSig._args.size(); argPos++) {
        int arg = qSig._args[argPos];
        if (isVariable(arg) || isQConstant(arg)) {
            // Q-constant sort or variable
            const auto& domain = _constants_by_sort.at(isQConstant(arg) ? _primary_sort_of_q_constants[arg] 
                        : getSorts(qSig._name_id).at(argPos));
            if (restrictiveSorts.empty()) {
                eligibleArgs[argPos].insert(eligibleArgs[argPos].end(), domain.begin(), domain.end());
            } else {
                const auto& restrictiveDomain = _constants_by_sort.at(restrictiveSorts.at(argPos));
                for (int c : domain) {
                    if (restrictiveDomain.count(c)) eligibleArgs[argPos].push_back(c);
                }
            }
        } else {
            // normal constant
            eligibleArgs[argPos].push_back(arg);
        }
        assert(eligibleArgs[argPos].size() > 0);
        numChoices *= eligibleArgs[argPos].size();
    }

    return ArgIterator(qSig._name_id, std::move(eligibleArgs));
}

const std::vector<int>& HtnInstance::getSorts(int nameId) const {
    return _signature_sorts_table.at(nameId);
}

const FlatHashSet<int>& HtnInstance::getConstantsOfSort(int sort) const {
    return _constants_by_sort.at(sort);
}

const FlatHashSet<int>& HtnInstance::getSortsOfQConstant(int qconst) {
    return _sorts_of_q_constants[qconst];
}

std::vector<int> HtnInstance::getOpSortsForCondition(const USignature& sig, const USignature& op) {
    std::vector<int> sigSorts(sig._args.size());
    const auto& opSorts = _signature_sorts_table[op._name_id];
    for (size_t sigIdx = 0; sigIdx < sigSorts.size(); sigIdx++) {
        for (size_t opIdx = 0; opIdx < op._args.size(); opIdx++) {
            if (sig._args[sigIdx] == op._args[opIdx]) {
                // Found
                sigSorts[sigIdx] = opSorts[opIdx];
                break;
            }
        }
    }
    return sigSorts;
}

const FlatHashSet<int>& HtnInstance::getDomainOfQConstant(int qconst) const {
    return _constants_by_sort.at(_primary_sort_of_q_constants.at(qconst));
}

const IntPair& HtnInstance::getOriginOfQConstant(int qconst) const {
    return _q_constants_with_origin.at(qconst);
}

std::vector<int> HtnInstance::popOperationDependentDomainOfQConstant(int qconst, const USignature& op) {
    auto it1 = _q_const_to_op_domains.find(qconst);
    assert(it1 != _q_const_to_op_domains.end());
    auto& opDomains = it1->second;
    auto it2 = opDomains.find(op);
    assert(it2 != opDomains.end());
    std::vector<int> domain = it2->second;
    if (opDomains.size() == 1) {
        _q_const_to_op_domains.erase(it1);
    } else {
        opDomains.erase(it2);
    }
    return domain;
}

void HtnInstance::addForbiddenSubstitution(const std::vector<int>& qArgs, const std::vector<int>& decArgs) {
    _forbidden_substitutions.emplace(qArgs, decArgs);
}

const NodeHashSet<Substitution, Substitution::Hasher>& HtnInstance::getForbiddenSubstitutions() {
    return _forbidden_substitutions;
}

void HtnInstance::clearForbiddenSubstitutions() {
    _forbidden_substitutions.clear();
}

const NodeHashMap<int, Action> HtnInstance::getActionTemplates() const {
    return _actions;
}
const NodeHashMap<int, Reduction> HtnInstance::getReductionTemplates() const {
    return _reductions;
}
int HtnInstance::getMinRES(int nameId) {
    return _instantiator->getMinNumPrimitiveChildren(nameId);
}

Action HtnInstance::toAction(int actionName, const std::vector<int>& args) const {
    const auto& op = _actions.at(actionName);
    return op.substitute(Substitution(op.getArguments(), args));
}

Reduction HtnInstance::toReduction(int reductionName, const std::vector<int>& args) const {
    const auto& op = _reductions.at(reductionName);
    return op.substituteRed(Substitution(op.getArguments(), args));
}

USignature HtnInstance::cutNonoriginalTaskArguments(const USignature& sig) {
    USignature sigCut(sig);
    sigCut._args.resize(_original_n_taskvars[sig._name_id]);
    return sigCut;
}

int HtnInstance::getSplitAction(int firstActionName) {
    return _split_action_from_first[firstActionName];
}

const std::pair<int, int>& HtnInstance::getParentAndChildFromSurrogate(int surrogateActionName) {
    return _surrogate_to_orig_parent_and_child[surrogateActionName];
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
    for (size_t i = 0; i < opSig._args.size(); i++) {
        placeholderArgs.push_back(-i-1);
    }

    return origSig.substitute(Substitution(origSig._args, placeholderArgs)); 
}

Instantiator& HtnInstance::getInstantiator() {
    return *_instantiator;
}

QConstantDatabase& HtnInstance::getQConstantDatabase() {
    return _q_db;
}

HtnInstance::~HtnInstance() {
    delete _instantiator;
    delete &_p;
}