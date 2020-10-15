
#ifndef DOMPASCH_TREE_REXX_PLANNER_H
#define DOMPASCH_TREE_REXX_PLANNER_H

#include <assert.h> 
 
#include "util/names.h"
#include "util/params.h"
#include "data/hashmap.h"
#include "data/layer.h"
#include "data/htn_instance.h"
#include "data/instantiator.h"
#include "data/arg_iterator.h"
#include "sat/encoding.h"

typedef std::pair<std::vector<PlanItem>, std::vector<PlanItem>> Plan;

class Planner {

public:
    typedef std::function<bool(const USignature&, bool)> StateEvaluator;

private:
    Parameters& _params;
    HtnInstance& _htn;

    std::vector<Layer*> _layers;
    Instantiator& _instantiator;
    Encoding _enc;

    USigSet _init_state;
    USigSet _pos_layer_facts;
    USigSet _neg_layer_facts;
    USigSet _necessary_facts;
    USigSet _defined_facts;

    NodeHashMap<USignature, SigSet, USignatureHasher> _fact_changes_cache;

    size_t _layer_idx;
    size_t _pos;
    size_t _old_pos;

    float _sat_time_limit = 0;
    float _init_plan_time_limit = 0;
    bool _nonprimitive_support;
    float _optimization_factor;
    float _time_at_first_plan = 0;

    bool _has_plan;
    Plan _plan;

public:
    Planner(Parameters& params, HtnInstance& htn) : _params(params), _htn(htn),
            _instantiator(_htn.getInstantiator()), _enc(_params, _htn, _layers, [this](){checkTermination();}), 
            _init_plan_time_limit(_params.getFloatParam("T")), _nonprimitive_support(_params.isNonzero("nps")), 
            _optimization_factor(_params.getFloatParam("of")), _has_plan(false) {}
    int findPlan();

    friend int terminateSatCall(void* state);
    void checkTermination();
    bool cancelOptimization();

private:

    void outputPlan();

    void createFirstLayer();
    void createNextLayer();
    
    void createNextPosition();
    void createNextPositionFromAbove();
    void createNextPositionFromLeft(Position& left);

    void addPrecondition(const USignature& op, const Signature& fact, 
            std::vector<IntPairTree>& goodSubs, 
            IntPairTree& badSubs, 
            bool addQFact = true);
    void addSubstitutionConstraints(const USignature& op, 
            std::vector<IntPairTree>& goodSubs, 
            IntPairTree& badSubs);
    enum EffectMode { INDIRECT, DIRECT, DIRECT_NO_QFACT };
    bool addEffect(const USignature& op, const Signature& fact, EffectMode mode);
    bool addAction(Action& a);
    bool addReduction(Reduction& r, const USignature& task);

    void propagateInitialState();
    void propagateActions(size_t offset);
    void propagateReductions(size_t offset);
    std::vector<USignature> getAllActionsOfTask(const USignature& task, const StateEvaluator& state);
    std::vector<USignature> getAllReductionsOfTask(const USignature& task, const StateEvaluator& state);
    void introduceNewFacts();
    void introduceNewFact(Position& newPos, const USignature& fact);
    void addQConstantTypeConstraints(const USignature& op);

    enum DominationStatus {DOMINATING, DOMINATED, DIFFERENT, EQUIVALENT};
    DominationStatus getDominationStatus(const USignature& op, const USignature& other, Substitution& qconstSubstitutions);
    void eliminateDominatedOperations();

    USigSet& getCurrentState(bool negated);
    StateEvaluator getStateEvaluator(int layer = -1, int pos = -1);
    // Introduces "fact" as FALSE at newPos.

    std::vector<int> getSortedSubstitutedArgIndices(const std::vector<int>& qargs, const std::vector<int>& sorts) const;
    std::vector<IntPair> decodingToPath(const std::vector<int>& qargs, const std::vector<int>& decArgs, const std::vector<int>& sortedIndices) const;

};

#endif