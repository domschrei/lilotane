
#ifndef DOMPASCH_TREE_REXX_HTN_INSTANCE_H
#define DOMPASCH_TREE_REXX_HTN_INSTANCE_H

#include <assert.h>

#include "data/code_table.h"
#include "data/action.h"
#include "data/reduction.h"
#include "data/signature.h"
#include "util/names.h"
#include "util/params.h"
#include "data/hashmap.h"

#include "data/arg_iterator.h"
#include "data/q_constant_condition.h"

// Forward definitions
class Instantiator;
class ParsedProblem;
struct predicate_definition;
struct task;
struct method;
struct literal;

class HtnInstance {

public:
    typedef std::function<bool(const USignature&, bool)> StateEvaluator;

private:

    Parameters& _params;

    // The raw parsed problem.
    ParsedProblem& _p;

    QConstantDatabase _q_db;
    Instantiator* _instantiator;
    
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

    // Collection of a set of q-constant substitutions which are invalid. 
    // Periodically cleared after being encoded.
    NodeHashSet<Substitution, Substitution::Hasher> _forbidden_substitutions;

    // Maps an action name ID to its action object.
    NodeHashMap<int, Action> _actions;
    // Maps a reduction name ID to its reduction object.
    NodeHashMap<int, Reduction> _reductions;

    // Maps a signature of a ground or pseudo-ground action to the actual action object.
    NodeHashMap<USignature, Action, USignatureHasher> _actions_by_sig;
    // Maps a signature of a ground or pseudo-ground reduction to the actual reduction object.
    NodeHashMap<USignature, Reduction, USignatureHasher> _reductions_by_sig;

    // Maps a task name ID to the name IDs of possible reductions for the task.
    NodeHashMap<int, std::vector<int>> _task_id_to_reduction_ids;

    // Maps a virtual "_FIRST" action ID to the original action that was split into parts.  
    FlatHashMap<int, int> _split_action_from_first;
    // Maps a reduction name ID to the surrogate action that replaces it.
    FlatHashMap<int, int> _reduction_to_surrogate;
    // Maps a surrogate action name ID to its original reduction name ID
    // and the replaced child name ID.
    FlatHashMap<int, std::pair<int, int>> _surrogate_to_orig_parent_and_child;

    FlatHashMap<int, int> _virtualized_to_actual_action;

    // The initial reduction of the problem.
    Reduction _init_reduction;
    // Signature of the BLANK virtual action.
    USignature _blank_action_sig;
    
    // Whether q constant mutexes are created and used.
    const bool _use_q_constant_mutexes;

    const bool _share_q_constants;

    USigSet _relevant_facts;

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
    
    const NodeHashMap<int, Action> getActionTemplates() const;
    const NodeHashMap<int, Reduction> getReductionTemplates() const;
    int getMinRES(int nameId);

    Action toAction(int actionName, const std::vector<int>& args) const;
    Reduction toReduction(int reductionName, const std::vector<int>& args) const;
    HtnOp& getOp(const USignature& opSig);
    const Action& getActionTemplate(int nameId) const;
    const Reduction& getReductionTemplate(int nameId) const;
    const Action& getAction(const USignature& sig) const;
    const Reduction& getReduction(const USignature& sig) const;
    void addAction(const Action& a);
    void addReduction(const Reduction& r);

    bool hasReductions(int taskId) const;
    const std::vector<int>& getReductionIdsOfTaskId(int taskId) const;

    bool hasSurrogate(int reductionId) const;
    const Action& getSurrogate(int reductionId) const;

    bool isVirtualizedChildOfAction(int actionId) const;
    int getVirtualizedChildNameOfAction(int actionId);
    USignature getVirtualizedChildOfAction(const USignature& action);
    const Action& getActionOfVirtualizedChild(int vChildId) const;
    int getActionNameOfVirtualizedChild(int vChildId) const;

    const std::vector<int>& getSorts(int nameId) const;
    const FlatHashSet<int>& getConstantsOfSort(int sort) const;
    const FlatHashSet<int>& getSortsOfQConstant(int qconst);
    const IntPair& getOriginOfQConstant(int qconst) const;
    const FlatHashSet<int>& getDomainOfQConstant(int qconst) const;
    std::vector<int> popOperationDependentDomainOfQConstant(int qconst, const USignature& op);

    std::vector<int> getOpSortsForCondition(const USignature& sig, const USignature& op);
    std::vector<FlatHashSet<int>> getDomainsOfOpArgs(const HtnOp& op, const StateEvaluator& state);

    ArgIterator decodeObjects(const USignature& qFact, const std::vector<int>& restrictiveSorts = std::vector<int>());

    void addForbiddenSubstitution(const std::vector<int>& qArgs, const std::vector<int>& decArgs);
    const NodeHashSet<Substitution, Substitution::Hasher>& getForbiddenSubstitutions();
    void clearForbiddenSubstitutions();

    Action replaceVariablesWithQConstants(const Action& a, int layerIdx, int pos, const StateEvaluator& state);
    Reduction replaceVariablesWithQConstants(const Reduction& red, int layerIdx, int pos, const StateEvaluator& state);    
    
    void addQConstantConditions(const HtnOp& op, const PositionedUSig& psig, const PositionedUSig& parentPSig, 
            int offset, const StateEvaluator& state);

    USignature getNormalizedLifted(const USignature& opSig, std::vector<int>& placeholderArgs);
    
    USignature cutNonoriginalTaskArguments(const USignature& sig);
    int getSplitAction(int firstActionName);
    const std::pair<int, int>& getParentAndChildFromSurrogate(int surrogateActionName);

    inline const USigSet& getRelevantFacts() const {
        return _relevant_facts;
    }

    inline void addRelevantFact(const USignature& sig) {
        _relevant_facts.insert(sig);
    }

    inline bool isRelevant(const USignature& sig) {
        return _relevant_facts.count(sig);
    }

    Instantiator& getInstantiator();
    QConstantDatabase& getQConstantDatabase();

    int nameId(const std::string& name, bool createQConstant = false, int layerIdx = -1, int pos = -1);
    std::string toString(int id) const;

    inline bool isVariable(int c) const {
        return _var_ids.count(c);
    }

    inline bool isQConstant(int c) const {
        return c > _name_table_running_id;
    }

    inline bool hasQConstants(const USignature& sig) const {
        for (const int& arg : sig._args) if (isQConstant(arg)) return true;
        return false;
    }

    inline bool isAbstraction(const USignature& concrete, const USignature& abstraction) {
        
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
            if (!isQConstant(qarg)) return false;
            
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

    inline bool isPredicate(int nameId) const {
        return _predicate_ids.count(nameId);
    }

    inline bool isAction(const USignature& sig) const {
        return _actions.count(sig._name_id);
    }

    inline bool isReduction(const USignature& sig) const {
        return _reductions.count(sig._name_id);
    }

    inline bool isSecondPartOfSplitAction(const USignature& sig) const {
        return toString(sig._name_id).rfind("__LLT_SECOND") != std::string::npos;
    }

    inline size_t getNumberOfQConstants() const {
        return _q_constants_with_origin.size();
    }


private:

    void replaceSurrogateReductionsWithAction();
    void splitActionsWithConflictingEffects();
    enum MinePrecMode { NO_MINING, USE_FOR_INSTANTIATION, USE_EVERYWHERE };
    void minePreconditions(MinePrecMode mode);

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

    std::vector<int> replaceVariablesWithQConstants(const HtnOp& op, int layerIdx, int pos, const StateEvaluator& state);
    void addQConstant(int id, const FlatHashSet<int>& domain);

};

#endif