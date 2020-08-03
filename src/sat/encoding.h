
#ifndef DOMPASCH_TREE_REXX_ENCODING_H
#define DOMPASCH_TREE_REXX_ENCODING_H

extern "C" {
    #include "sat/ipasir.h"
}

#include <initializer_list>
#include <fstream>
#include <string>
#include <iostream>

#include "util/params.h"
#include "data/layer.h"
#include "data/signature.h"
#include "data/htn_instance.h"
#include "data/action.h"
#include "sat/variable_domain.h"

#define PRINT_TO_FILE true

typedef NodeHashMap<int, SigSet> State;

struct PlanItem {
    PlanItem() {
        id = -1;
        abstractTask = Position::NONE_SIG;
        reduction = Position::NONE_SIG;
        subtaskIds = std::vector<int>(0);
    }
    PlanItem(int id, const USignature& abstractTask, const USignature& reduction, const std::vector<int> subtaskIds) :
        id(id), abstractTask(abstractTask), reduction(reduction), subtaskIds(subtaskIds) {}
    int id = -1;
    USignature abstractTask;
    USignature reduction;
    std::vector<int> subtaskIds;
};

class Encoding {

private:
    Parameters& _params;
    HtnInstance& _htn;
    std::vector<Layer*>& _layers;
    
    int _layer_idx;
    int _pos;
    int _old_pos;
    int _offset;

    NodeHashMap<USignature, int, USignatureHasher> _substitution_variables;
    NodeHashSet<Substitution, Substitution::Hasher> _forbidden_substitutions;
    FlatHashSet<int> _new_fact_vars;

    void* _solver;
    std::ofstream _out;

    USignature _sig_primitive;
    USignature _sig_substitution;
    int _substitute_name_id;

    FlatHashSet<int> _q_constants;
    FlatHashMap<std::pair<int, int>, int, IntPairHasher> _q_equality_variables;
    std::vector<int> _primitive_ops;
    std::vector<int> _nonprimitive_ops;

    std::vector<int> _last_assumptions;
    std::vector<int> _no_decision_variables;

    const bool _print_formula;
    const bool _use_q_constant_mutexes;
    const bool _implicit_primitiveness;

    int _num_cls;
    int _num_lits;
    int _num_asmpts;

    int _prior_num_cls;
    int _prior_num_lits;

    std::map<std::string, int> _num_cls_per_stage;
    std::map<std::string, int> _total_num_cls_per_stage;

    float _sat_call_start_time;

public:
    Encoding(Parameters& params, HtnInstance& htn, std::vector<Layer*>& layers);
    ~Encoding();

    void encode(int layerIdx, int pos);
    void addAssumptions(int layerIdx);
    void setTerminateCallback(void * state, int (*terminate)(void * state));
    int solve();

    float getTimeSinceSatCallStart();

    void stage(std::string name);
    void printStages();

    std::pair<std::vector<PlanItem>, std::vector<PlanItem>> extractPlan();
    std::vector<PlanItem> extractClassicalPlan();
    std::vector<PlanItem> extractDecompositionPlan();

    void printFailedVars(Layer& layer);
    void printSatisfyingAssignment();

private:

    void clearDonePositions();

    void encodeFactVariables(Position& pos, const Position& left, Position& above);
    void encodeFrameAxioms(Position& pos, const Position& left);
    void initSubstitutionVars(int opVar, int qconst, Position& pos);

    void setVariablePhases(const std::vector<int>& vars);
    
    std::set<std::set<int>> getCnf(const std::vector<int>& dnf);

    void addClause(int lit);
    void addClause(int lit1, int lit2);
    void addClause(int lit1, int lit2, int lit3);
    void addClause(const std::initializer_list<int>& lits);
    void addClause(const std::vector<int>& cls);

    void appendClause(int lit);
    void appendClause(int lit1, int lit2);
    void appendClause(const std::initializer_list<int>& lits);
    
    void endClause();
    void assume(int lit);

    int varPrimitive(int layer, int pos);
    int varSubstitution(const USignature& sigSubst);
    int varQConstEquality(int q1, int q2);
    const USignature& sigSubstitute(int qConstId, int trueConstId);

    bool isEncoded(int layer, int pos, const USignature& sig);
    bool isEncodedSubstitution(const USignature& sig);
    
    int getVariable(const Position& pos, const USignature& sig);
    int getVariable(int layer, int pos, const USignature& sig);
    int encodeVariable(Position& pos, const USignature& sig, bool decisionVar = true);

    std::string varName(int layer, int pos, const USignature& sig);
    void printVar(int layer, int pos, const USignature& sig);

    bool value(int layer, int pos, const USignature& sig);
    USignature getDecodedQOp(int layer, int pos, const USignature& sig);
    void checkAndApply(const Action& a, State& state, State& newState, int layer, int pos);

};

#endif