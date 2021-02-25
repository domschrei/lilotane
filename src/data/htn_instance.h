
#ifndef DOMPASCH_TREE_REXX_HTN_INSTANCE_H
#define DOMPASCH_TREE_REXX_HTN_INSTANCE_H

#include <assert.h>

#include "data/action.h"
#include "data/reduction.h"
#include "data/signature.h"
#include "util/names.h"
#include "util/params.h"
#include "util/hashmap.h"
#include "data/op_table.h"

#include "algo/arg_iterator.h"
#include "algo/sample_arg_iterator.h"

// Forward definitions
class ParsedProblem;
struct predicate_definition;
struct task;
struct method;
struct literal;

class HtnInstance {

private:
    Parameters& _params;

    // The raw parsed problem.
    ParsedProblem& _p;
    
    // Maps a string to its name ID within the problem.
    FlatHashMap<std::string, int> _name_table;
    // Maps a name ID to its string within the problem.
    NodeHashMap<int, std::string> _name_back_table;
    // Running number to assign new IDs to strings.
    int _name_table_running_id = 1;

    // Set of all name IDs that are variables (start with '?').
    FlatHashSet<int> _var_ids;
    // Set of all predicate name IDs.
    FlatHashSet<int> _predicate_ids;
    // Set of equality predicate name IDs.
    FlatHashSet<int> _equality_predicates;
    // Set of all q-constant IDs.
    FlatHashMap<int, IntPair> _q_constants_with_origin;

    NodeHashMap<int, NodeHashMap<USignature, std::vector<int>, USignatureHasher>> _q_const_to_op_domains;  

    // Maps a {predicate,task,method} name ID to a list of sorts IDs.
    NodeHashMap<int, std::vector<int>> _signature_sorts_table;

    // Maps a sort name ID to a set of constants of that sort.
    NodeHashMap<int, FlatHashSet<int>> _constants_by_sort;

    // Maps each q-constant to the sort it was created with.
    FlatHashMap<int, int> _primary_sort_of_q_constants;
    // Maps each q-constant to a list of sorts it is constrained with.
    NodeHashMap<int, FlatHashSet<int>> _sorts_of_q_constants;
    
    // Maps each {action,reduction} name ID to the number of task variables it originally had.
    FlatHashMap<int, int> _original_n_taskvars;

    // Lookup table for the possible decodings of a fact signature with normalized arguments.    
    NodeHashMap<USignature, std::vector<USignature>, USignatureHasher> _fact_sig_decodings;

    // Maps an action name ID to its action object.
    NodeHashMap<int, Action> _operators;
    // Maps a reduction name ID to its reduction object.
    NodeHashMap<int, Reduction> _methods;

    // Lookup for all actions and reductions instantiated so far.
    OpTable _op_table;

    // Maps a task name ID to the name IDs of possible reductions for the task.
    NodeHashMap<int, std::vector<int>> _task_id_to_reduction_ids;

    // Maps a reduction name ID to the primitivization (action name ID) that replaces it.
    FlatHashMap<int, int> _reduction_to_primitivization;
    // Maps a primitivization (action name ID) to its original reduction name ID
    // and the replaced child name ID.
    FlatHashMap<int, std::pair<int, int>> _primitivization_to_parent_and_child;

    FlatHashMap<int, int> _repeated_to_actual_action;

    // The initial reduction of the problem.
    Reduction _init_reduction;
    // Signature of the BLANK virtual action.
    USignature _blank_action_sig;
    
    const bool _share_q_constants;

public:

    // Special action representing a virtual "No-op".
    static Action BLANK_ACTION;

    HtnInstance(Parameters& params);
    ~HtnInstance();

    ParsedProblem* parse(std::string domainFile, std::string problemFile);

    USigSet getInitState();
    const Reduction& getInitReduction();
    const USignature& getBlankActionSig();
    Action getGoalAction();
    void printStatistics();
    size_t getNumFreeArguments(const Reduction& r);
    
    const NodeHashMap<int, Action>& getActionTemplates() const;
    NodeHashMap<int, Reduction>& getReductionTemplates();

    Action toAction(int actionName, const std::vector<int>& args) const;
    Reduction toReduction(int reductionName, const std::vector<int>& args) const;
    HtnOp& getOp(const USignature& opSig);
    const Action& getActionTemplate(int nameId) const;
    const Reduction& getReductionTemplate(int nameId) const;
    OpTable& getOpTable() {return _op_table;}

    bool hasReductions(int taskId) const;
    const std::vector<int>& getReductionIdsOfTaskId(int taskId) const;

    bool isReductionPrimitivizable(int reductionId) const;
    const Action& getReductionPrimitivization(int reductionId) const;

    bool isActionRepetition(int actionId) const;
    int getRepetitionNameOfAction(int actionId);
    USignature getRepetitionOfAction(const USignature& action);
    int getActionNameFromRepetition(int vChildId) const;
    const Action& getActionFromRepetition(int vChildId) const;

    const std::vector<int>& getSorts(int nameId) const;
    const FlatHashSet<int>& getConstantsOfSort(int sort) const;
    const FlatHashSet<int>& getSortsOfQConstant(int qconst);
    const IntPair& getOriginOfQConstant(int qconst) const;
    const FlatHashSet<int>& getDomainOfQConstant(int qconst) const;
    std::vector<int> popOperationDependentDomainOfQConstant(int qconst, const USignature& op);

    std::vector<int> getOpSortsForCondition(const USignature& sig, const USignature& op);

    std::vector<std::vector<int>> getEligibleArgs(const USignature& qFact, const std::vector<int>& restrictiveSorts = std::vector<int>());
    ArgIterator decodeObjects(const USignature& qSig, std::vector<std::vector<int>> eligibleArgs);
    SampleArgIterator decodeObjects(const USignature& qSig, std::vector<std::vector<int>> eligibleArgs, size_t numSamples);

    Action replaceVariablesWithQConstants(const Action& a, const std::vector<FlatHashSet<int>>& opArgDomains, int layerIdx, int pos);
    Reduction replaceVariablesWithQConstants(const Reduction& red, const std::vector<FlatHashSet<int>>& opArgDomains, int layerIdx, int pos);

    USignature getNormalizedLifted(const USignature& opSig, std::vector<int>& placeholderArgs);
    
    USignature cutNonoriginalTaskArguments(const USignature& sig);
    const std::pair<int, int>& getReductionAndActionFromPrimitivization(int primitivizationName);

    int nameId(const std::string& name, bool createQConstant = false, int layerIdx = -1, int pos = -1);
    std::string toString(int id) const;

    inline bool isVariable(int c) const {
        if (c < 0) return true;
        assert(_name_back_table.count(c) || Log::d("%i not in name_back_table !\n", c));
        return _var_ids.count(c);
    }

    inline bool isQConstant(int c) const {
        return c > _name_table_running_id;
    }

    inline bool hasQConstants(const USignature& sig) const {
        for (const int& arg : sig._args) if (isQConstant(arg)) return true;
        return false;
    }

    bool isUnifiable(const Signature& from, const Signature& to, FlatHashMap<int, int>* substitution = nullptr) {
        if (from._negated != to._negated) return false;
        return isUnifiable(from._usig, to._usig, substitution);
    }

    bool isUnifiable(const USignature& from, const USignature& to, FlatHashMap<int, int>* substitution = nullptr) {
        if (from._name_id != to._name_id) return false;
        if (from._args.size() != to._args.size()) return false;

        for (size_t i = 0; i < from._args.size(); i++) {

            if (!isVariable(from._args[i])) {
                // Constant parameter: must be equal
                if (from._args[i] != to._args[i]) return false;

            } else if (isVariable(to._args[i])) {
                // Both are variables: fine
                if (substitution != nullptr) {
                    (*substitution)[from._args[i]] = to._args[i];
                }
            
            } else {
                // Variable to constant: fine
                if (substitution != nullptr) {
                    (*substitution)[from._args[i]] = to._args[i];
                }
            }
        }
        return true;
    }

    std::vector<int> getFreeArgPositions(const std::vector<int>& sigArgs) {
        std::vector<int> argPositions;
        for (size_t i = 0; i < sigArgs.size(); i++) {
            int arg = sigArgs[i];
            if (isVariable(arg)) argPositions.push_back(i);
        }
        return argPositions;
    }

    inline bool isFullyGround(const USignature& sig) {
        for (int arg : sig._args) if (isVariable(arg)) return false;
        return true;
    }

    inline bool hasSomeInstantiation(const USignature& sig) {
        const std::vector<int>& types = getSorts(sig._name_id);
        //log("%s , %i\n", TOSTR(sig), types.size());
        assert(types.size() == sig._args.size());
        for (size_t argPos = 0; argPos < sig._args.size(); argPos++) {
            int sort = types[argPos];
            if (getConstantsOfSort(sort).empty()) {
                return false;
            }
        }
        return true;
    }

    bool hasConsistentlyTypedArgs(const USignature& sig) {
        const std::vector<int>& taskSorts = getSorts(sig._name_id);
        for (size_t argPos = 0; argPos < sig._args.size(); argPos++) {
            int sort = taskSorts[argPos];
            int arg = sig._args[argPos];
            if (isVariable(arg)) continue; // skip variable
            bool valid = false;
            if (isQConstant(arg)) {
                // q constant: TODO check if SOME SUBSTITUTEABLE CONSTANT has the correct sort
                for (int cnst : getDomainOfQConstant(arg)) {
                    if (getConstantsOfSort(sort).count(cnst)) {
                        valid = true;
                        break;
                    }
                }
            } else {
                // normal constant: check if it is contained in the correct sort
                valid = getConstantsOfSort(sort).count(arg);
            }
            if (!valid) {
                //log("arg %s not of sort %s => %s invalid\n", TOSTR(arg), TOSTR(sort), TOSTR(sig));
                return false;
            } 
        }
        return true;
    }

    inline bool isPredicate(int nameId) const {
        return _predicate_ids.count(nameId);
    }

    inline bool isAction(const USignature& sig) const {
        return _operators.count(sig._name_id);
    }

    inline bool isReduction(const USignature& sig) const {
        return _methods.count(sig._name_id);
    }

    inline size_t getNumberOfQConstants() const {
        return _q_constants_with_origin.size();
    }

    std::vector<TypeConstraint> getQConstantTypeConstraints(const USignature& sig) {

        std::vector<TypeConstraint> constraints;

        const std::vector<int>& taskSorts = getSorts(sig._name_id);
        for (size_t argPos = 0; argPos < sig._args.size(); argPos++) {
            int sigSort = taskSorts[argPos];
            int arg = sig._args[argPos];
            
            // Not a q-constant here
            if (!isQConstant(arg)) {
                // Must be of valid type
                assert(getConstantsOfSort(sigSort).count(arg));
                continue;
            }

            // Type is fine no matter which substitution is chosen
            if (getSortsOfQConstant(arg).count(sigSort)) continue;

            // Type is NOT fine, at least for some substitutions
            std::vector<int> good;
            std::vector<int> bad;
            const FlatHashSet<int>& validConstants = getConstantsOfSort(sigSort);
            // For each value the qconstant can assume:
            for (int c : getDomainOfQConstant(arg)) {
                // Is that constant of correct type?
                if (validConstants.count(c)) good.push_back(c);
                else bad.push_back(c);
            }

            if (good.size() >= bad.size()) {
                // arg must be EITHER of the GOOD ones
                constraints.emplace_back(arg, true, std::move(good));
            } else {
                // arg must be NEITHER of the BAD ones
                constraints.emplace_back(arg, false, std::move(bad));
            }
        }

        return constraints;
    }

private:

    void primitivizeSimpleReductions();
    
    std::vector<int> convertArguments(int predNameId, const std::vector<std::pair<std::string, std::string>>& vars);
    std::vector<int> convertArguments(int predNameId, const std::vector<std::string>& vars);
    USignature convertSignature(const task& task);
    USignature convertSignature(const method& method);
    Signature  convertSignature(int parentNameId, const literal& literal);

    void extractPredSorts(const predicate_definition& p);
    void extractTaskSorts(const task& t);
    void extractMethodSorts(const method& m);
    void extractConstants();
    SigSet extractEqualityConstraints(int opId, const std::vector<literal>& lits, const std::vector<std::pair<std::string, std::string>>& vars);
    SigSet extractGoals();

    Reduction& createReduction(method& method);
    Action& createAction(const task& task);

    std::vector<int> replaceVariablesWithQConstants(const HtnOp& op, const std::vector<FlatHashSet<int>>& opArgDomains, int layerIdx, int pos);
    void initQConstantSorts(int id, const FlatHashSet<int>& domain);

};

#endif