
#ifndef DOMPASCH_TREE_REXX_HTN_INSTANCE_H
#define DOMPASCH_TREE_REXX_HTN_INSTANCE_H

#include <unordered_map>
#include <assert.h> 
 
#include "parser/main.hpp"
#include "data/code_table.h"
#include "data/layer.h"
#include "data/action.h"
#include "data/reduction.h"
#include "data/signature.h"
#include "util/names.h"
#include "util/params.h"

#include "data/instantiator.h"
#include "data/effector_table.h"
#include "data/arg_iterator.h"

// External functions
int run_pandaPIparser(int argc, char** argv);
ParsedProblem& get_parsed_problem();

struct HtnInstance {

    Parameters& _params;

    // The raw parsed problem.
    ParsedProblem& _p;

    // Maps a string to its name ID within the problem.
    std::unordered_map<std::string, int> _name_table;
    std::unordered_map<int, std::string> _name_back_table;
    // Running ID to assign to new strings of the problem.
    int _name_table_running_id = 1;

    // Set of all name IDs that are variables (start with '?').
    std::unordered_set<int> _var_ids;

    // Maps a {predicate,task,method} name ID to a list of sorts IDs.
    std::unordered_map<int, std::vector<int>> _signature_sorts_table;

    // Maps a sort name ID to a list of constant name IDs of that sort.
    std::unordered_map<int, std::unordered_set<int>> _constants_by_sort;

    std::unordered_set<int> _q_constants;
    std::unordered_map<int, int> _primary_sort_of_q_constants;
    std::unordered_map<int, std::unordered_set<int>> _sorts_of_q_constants;

    // Maps an action name ID to its action object.
    std::unordered_map<int, Action> _actions;

    // Maps a reduction name ID to its reduction object.
    std::unordered_map<int, Reduction> _reductions;

    std::unordered_map<Signature, Action, SignatureHasher> _actions_by_sig;
    std::unordered_map<Signature, Reduction, SignatureHasher> _reductions_by_sig;

    std::unordered_map<int, std::vector<int>> _task_id_to_reduction_ids;

    std::unordered_set<int> _equality_predicates;
    std::unordered_set<int> _fluent_predicates;

    Instantiator* _instantiator;
    EffectorTable* _effector_table;

    Reduction _init_reduction;
    std::vector<Reduction> _init_reduction_choices;
    Action _action_blank;

    std::unordered_map<int, int> _split_action_from_first;

    static ParsedProblem& parse(std::string domainFile, std::string problemFile);

    HtnInstance(Parameters& params, ParsedProblem& p);

    int getNameId(const std::string& name);

    std::vector<int> getArguments(int predNameId, const std::vector<std::pair<string, string>>& vars);
    std::vector<int> getArguments(int predNameId, const std::vector<std::string>& vars);
    Signature getSignature(const task& task);
    Signature getSignature(const method& method);
    Signature getSignature(int parentNameId, const literal& literal);
    Signature getInitTaskSignature(int pos);
    SigSet getInitState();
    SigSet getGoals();

    void extractPredSorts(const predicate_definition& p);
    void extractTaskSorts(const task& t);
    void extractMethodSorts(const method& m);
    void extractConstants();

    Reduction& createReduction(const method& method);
    Action& createAction(const task& task);

    SigSet getAllFactChanges(const Signature& sig);

    Action replaceQConstants(const Action& a, int layerIdx, int pos);
    Reduction replaceQConstants(const Reduction& red, int layerIdx, int pos);
    std::unordered_map<int, int> addQConstants(const Signature& sig, int layerIdx, int pos);
    void addQConstant(int layerIdx, int pos, const Signature& sig, int argPos, std::unordered_map<int, int>& s);

    bool hasQConstants(const Signature& sig);
    std::vector<Signature> getDecodedObjects(const Signature& qFact);
    const std::unordered_set<int>& getSortsOfQConstant(int qconst);
    const std::unordered_set<int>& getDomainOfQConstant(int qconst);

    const std::unordered_set<int>& getConstantsOfSort(int sort);

    bool isRigidPredicate(int predId);
    void removeRigidConditions(HtnOp& op);
};

#endif