
#ifndef DOMPASCH_TREE_REXX_PLANNER_H
#define DOMPASCH_TREE_REXX_PLANNER_H
 
#include "util/names.h"
#include "util/params.h"
#include "util/hashmap.h"
#include "data/layer.h"
#include "data/htn_instance.h"
#include "algo/instantiator.h"
#include "algo/arg_iterator.h"
#include "algo/precondition_inference.h"
#include "algo/minres.h"
#include "algo/fact_analysis.h"
#include "algo/retroactive_pruning.h"
#include "algo/domination_resolver.h"
#include "algo/plan_writer.h"
#include "sat/encoding.h"

typedef std::pair<std::vector<PlanItem>, std::vector<PlanItem>> Plan;

class Planner {

public:
    typedef std::function<bool(const USignature&, bool)> StateEvaluator;

private:
    Parameters& _params;
    HtnInstance& _htn;

    FactAnalysis _analysis;
    Instantiator _instantiator;
    Encoding _enc;
    MinRES _minres;
    RetroactivePruning _pruning;
    DominationResolver _domination_resolver;
    PlanWriter _plan_writer;

    std::vector<Layer*> _layers;

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

    // statistics
    size_t _num_instantiated_positions = 0;
    size_t _num_instantiated_actions = 0;
    size_t _num_instantiated_reductions = 0;

public:
    Planner(Parameters& params, HtnInstance& htn) : _params(params), _htn(htn),
            _analysis(_htn), 
            _instantiator(params, htn, _analysis), 
            _enc(_params, _htn, _analysis, _layers, [this](){checkTermination();}), 
            _minres(_htn), 
            _pruning(_layers, _enc),
            _domination_resolver(_htn),
            _plan_writer(_htn, _params),
            _init_plan_time_limit(_params.getFloatParam("T")), _nonprimitive_support(_params.isNonzero("nps")), 
            _optimization_factor(_params.getFloatParam("of")), _has_plan(false) {

        // Mine additional preconditions for reductions from their subtasks
        PreconditionInference::infer(_htn, _analysis, PreconditionInference::MinePrecMode(_params.getIntParam("mp")));
    }
    int findPlan();
    void improvePlan(int& iteration);

    friend int terminateSatCall(void* state);
    void checkTermination();
    bool cancelOptimization();

private:

    void createFirstLayer();
    void createNextLayer();
    
    void createNextPosition();
    void createNextPositionFromAbove();
    void createNextPositionFromLeft(Position& left);

    void incrementPosition();

    void addPreconditionConstraints();
    void addPreconditionsAndConstraints(const USignature& op, const SigSet& preconditions, bool isActionRepetition);
    std::optional<SubstitutionConstraint> addPrecondition(const USignature& op, const Signature& fact, bool addQFact = true);
    
    enum EffectMode { INDIRECT, DIRECT, DIRECT_NO_QFACT };
    bool addEffect(const USignature& op, const Signature& fact, EffectMode mode);

    std::optional<Reduction> createValidReduction(const USignature& rSig, const USignature& task);

    void propagateInitialState();
    void propagateActions(size_t offset);
    void propagateReductions(size_t offset);
    std::vector<USignature> instantiateAllActionsOfTask(const USignature& task);
    std::vector<USignature> instantiateAllReductionsOfTask(const USignature& task);
    void initializeNextEffects();
    void initializeFact(Position& newPos, const USignature& fact);
    void addQConstantTypeConstraints(const USignature& op);

    int getTerminateSatCall();
    void clearDonePositions(int offset);
    void printStatistics();

};

#endif