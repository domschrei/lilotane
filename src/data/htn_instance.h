
#ifndef DOMPASCH_TREE_REXX_HTN_INSTANCE_H
#define DOMPASCH_TREE_REXX_HTN_INSTANCE_H

#include <assert.h> 
 
#include "libpanda.hpp"

#include "data/code_table.h"
#include "data/layer.h"
#include "data/action.h"
#include "data/reduction.h"
#include "data/signature.h"
#include "util/names.h"
#include "util/params.h"
#include "data/hashmap.h"

#include "data/instantiator.h"
#include "data/arg_iterator.h"
#include "data/q_constant_condition.h"

struct HtnInstance {

    Parameters& _params;

    // The raw parsed problem.
    ParsedProblem& _p;

    // Maps a string to its name ID within the problem.
    FlatHashMap<std::string, int> _name_table;
    NodeHashMap<int, std::string> _name_back_table;
    // Running ID to assign to new strings of the problem.
    int _name_table_running_id = 1;

    // Set of all name IDs that are variables (start with '?').
    FlatHashSet<int> _var_ids;

    // Maps a {predicate,task,method} name ID to a list of sorts IDs.
    NodeHashMap<int, std::vector<int>> _signature_sorts_table;

    FlatHashMap<int, int> _original_n_taskvars;

    // Maps a sort name ID to a list of constant name IDs of that sort.
    NodeHashMap<int, FlatHashSet<int>> _constants_by_sort;

    FlatHashSet<int> _q_constants;
    FlatHashMap<int, int> _primary_sort_of_q_constants;
    NodeHashMap<int, FlatHashSet<int>> _sorts_of_q_constants;
    QConstantDatabase _q_db;

    // Maps a normalized fact signature to a list of possible decodings
    // using the normalized arguments.
    NodeHashMap<USignature, std::vector<USignature>, USignatureHasher> _fact_sig_decodings;
    NodeHashMap<USignature, std::vector<USignature>, USignatureHasher> _fact_sig_decodings_unchecked;

    NodeHashMap<USignature, USigSet, USignatureHasher> _qfact_decodings;
    NodeHashSet<Substitution, Substitution::Hasher> _forbidden_substitutions;

    // Maps an action name ID to its action object.
    NodeHashMap<int, Action> _actions;

    // Maps a reduction name ID to its reduction object.
    NodeHashMap<int, Reduction> _reductions;

    NodeHashMap<USignature, Action, USignatureHasher> _actions_by_sig;
    NodeHashMap<USignature, Reduction, USignatureHasher> _reductions_by_sig;

    NodeHashMap<int, std::vector<int>> _task_id_to_reduction_ids;

    FlatHashSet<int> _equality_predicates;
    FlatHashSet<int> _fluent_predicates;

    Instantiator* _instantiator;

    Reduction _init_reduction;
    std::vector<Reduction> _init_reduction_choices;
    Action _action_blank;

    FlatHashMap<int, int> _split_action_from_first;

    const bool _remove_rigid_predicates;
    const bool _use_q_constant_mutexes;



    static void parse(std::string domainFile, std::string problemFile, ParsedProblem& pp);

    HtnInstance(Parameters& params, ParsedProblem& p);

    int getNameId(const std::string& name);

    std::vector<int> getArguments(int predNameId, const std::vector<std::pair<string, string>>& vars);
    std::vector<int> getArguments(int predNameId, const std::vector<std::string>& vars);
    USignature getSignature(const task& task);
    USignature getSignature(const method& method);
    Signature getSignature(int parentNameId, const literal& literal);
    USignature getInitTaskSignature(int pos);
    SigSet getInitState();
    SigSet getGoals();

    void extractPredSorts(const predicate_definition& p);
    void extractTaskSorts(const task& t);
    void extractMethodSorts(const method& m);
    void extractConstants();

    Reduction& createReduction(method& method);
    Action& createAction(const task& task);
    HtnOp& getOp(const USignature& opSig);

    Action replaceVariablesWithQConstants(const Action& a, int layerIdx, int pos, const std::function<bool(const Signature&)>& state);
    Reduction replaceVariablesWithQConstants(const Reduction& red, int layerIdx, int pos, const std::function<bool(const Signature&)>& state);    
    std::vector<int> replaceVariablesWithQConstants(const HtnOp& op, int layerIdx, int pos, const std::function<bool(const Signature&)>& state);
    int addQConstant(int layerIdx, int pos, const USignature& sig, int argPos, const FlatHashSet<int>& domain);

    void addQConstantConditions(const HtnOp& op, const PositionedUSig& psig, const PositionedUSig& parentPSig, 
            int offset, const std::function<bool(const Signature&)>& state);

    bool hasQConstants(const USignature& sig);
    bool isAbstraction(const USignature& concrete, const USignature& abstraction);
    const std::vector<USignature>& getDecodedObjects(const USignature& qFact, bool checkQConstConds);
    const FlatHashSet<int>& getSortsOfQConstant(int qconst);
    const FlatHashSet<int>& getDomainOfQConstant(int qconst);

    void addQFactDecoding(const USignature& qFact, const USignature& decFact);
    void removeQFactDecoding(const USignature& qFact, const USignature& decFact);
    const USigSet& getQFactDecodings(const USignature& qfact);

    const FlatHashSet<int>& getConstantsOfSort(int sort);

    bool isRigidPredicate(int predId);
    void removeRigidConditions(HtnOp& op);

    USignature getNormalizedLifted(const USignature& opSig, std::vector<int>& placeholderArgs);

    USignature cutNonoriginalTaskArguments(const USignature& sig);
};

#endif