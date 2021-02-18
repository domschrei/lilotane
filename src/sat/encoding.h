
#ifndef DOMPASCH_TREE_REXX_ENCODING_H
#define DOMPASCH_TREE_REXX_ENCODING_H

#include "util/params.h"
#include "data/layer.h"
#include "data/signature.h"
#include "data/htn_instance.h"
#include "data/action.h"
#include "data/plan.h"
#include "sat/variable_domain.h"
#include "sat/literal_tree.h"
#include "sat/sat_interface.h"
#include "algo/fact_analysis.h"
#include "algo/indirect_fact_support.h"
#include "sat/variable_provider.h"

typedef NodeHashMap<int, SigSet> State;

class Encoding {

private:
    Parameters& _params;
    HtnInstance& _htn;
    FactAnalysis& _analysis;
    std::vector<Layer*>& _layers;
    EncodingStatistics _stats;
    SatInterface _sat;
    VariableProvider _vars;
    IndirectFactSupport _indir_support;

    std::function<void()> _termination_callback;
    
    size_t _layer_idx;
    size_t _pos;
    size_t _old_pos;
    size_t _offset;

    NodeHashSet<Substitution, Substitution::Hasher> _forbidden_substitutions;
    FlatHashMap<std::pair<int, int>, int, IntPairHasher> _q_equality_variables;
    FlatHashSet<int> _new_fact_vars;

    FlatHashSet<int> _q_constants;
    FlatHashSet<int> _new_q_constants;
    //FlatHashMap<int, int> _coarsened_q_constants;

    std::vector<int> _primitive_ops;
    std::vector<int> _nonprimitive_ops;

    const bool _use_q_constant_mutexes;
    const bool _implicit_primitiveness;

    float _sat_call_start_time;

public:
    Encoding(Parameters& params, HtnInstance& htn, FactAnalysis& analysis, std::vector<Layer*>& layers, std::function<void()> terminationCallback);
    ~Encoding() {
        // Append assumptions to written formula, close stream
        if (!_params.isNonzero("cs") && !_sat.hasLastAssumptions()) {
            addAssumptions(_layers.size()-1);
        }
    }

    void encode(size_t layerIdx, size_t pos);
    void addAssumptions(int layerIdx, bool permanent = false);
    void setTerminateCallback(void * state, int (*terminate)(void * state));
    int solve();

    void addUnitConstraint(int lit);
    void setNecessaryFacts(USigSet& set);

    float getTimeSinceSatCallStart();

    Plan extractPlan();
    enum PlanExtraction {ALL, PRIMITIVE_ONLY};
    std::vector<PlanItem> extractClassicalPlan(PlanExtraction mode = PRIMITIVE_ONLY);
    std::vector<PlanItem> extractDecompositionPlan();

    enum ConstraintAddition { TRANSIENT, PERMANENT };
    void optimizePlan(int upperBound, Plan& plan, ConstraintAddition mode);
    int findMinBySat(int lower, int upper, std::function<int(int)> varMap, std::function<int(void)> boundUpdateOnSat, ConstraintAddition mode);
    int getPlanLength(const std::vector<PlanItem>& classicalPlan);
    bool isEmptyAction(const USignature& aSig);

    void printFailedVars(Layer& layer);
    void printSatisfyingAssignment();

private:
    void encodeOperationVariables(Position& pos);
    void encodeFactVariables(Position& pos, Position& left, Position& above);
    void encodeFrameAxioms(Position& pos, Position& left);
    void encodeOperationConstraints(Position& pos);
    void encodeSubstitutionVars(const USignature& opSig, int opVar, int qconst, Position& pos);
    void encodeQFactSemantics(Position& pos);
    void encodeActionEffects(Position& pos, Position& left);
    void encodeQConstraints(Position& pos);
    void encodeSubtaskRelationships(Position& pos, Position& above);

    void setVariablePhases(const std::vector<int>& vars);
    void clearDonePositions();
    
    std::set<std::set<int>> getCnf(const std::vector<int>& dnf);

    int varQConstEquality(int q1, int q2);
    
    bool value(VarType type, int layer, int pos, const USignature& sig);
    
    std::string varName(int layer, int pos, const USignature& sig);
    void printVar(int layer, int pos, const USignature& sig);

    USignature getDecodedQOp(int layer, int pos, const USignature& sig);
};

#endif