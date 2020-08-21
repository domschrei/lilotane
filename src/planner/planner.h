
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

class Planner {

public:
    typedef std::function<bool(const USignature&, bool)> StateEvaluator;

private:
    Parameters& _params;
    HtnInstance _htn;

    std::vector<Layer*> _layers;
    Instantiator& _instantiator;
    Encoding _enc;

    size_t _layer_idx;
    size_t _pos;
    size_t _old_pos;

    float _sat_time_limit = 0;
    bool _nonprimitive_support;

public:
    Planner(Parameters& params, ParsedProblem& problem) : _params(params), _htn(params, problem), 
            _instantiator(_htn.getInstantiator()), _enc(_params, _htn, _layers), 
            _nonprimitive_support(_params.isNonzero("nps")) {}
    int findPlan();
    friend int terminateSatCall(void* state);

private:

    void outputPlan();

    void createFirstLayer();
    void createNextLayer();
    
    void createNextPosition();
    void createNextPositionFromAbove(const Position& above);
    void createNextPositionFromLeft(Position& left);

    void addPrecondition(const USignature& op, const Signature& fact, 
            std::vector<NodeHashSet<Substitution, Substitution::Hasher>>& goodSubs, 
            NodeHashSet<Substitution, Substitution::Hasher>& badSubs);
    void addSubstitutionConstraints(const USignature& op, 
            std::vector<NodeHashSet<Substitution, Substitution::Hasher>>& goodSubs, 
            NodeHashSet<Substitution, Substitution::Hasher>& badSubs);
    bool addEffect(const USignature& op, const Signature& fact);
    bool addAction(Action& a);
    bool addReduction(Reduction& r, const USignature& task);

    void propagateInitialState();
    void propagateActions(size_t offset);
    void propagateReductions(size_t offset);
    std::vector<USignature> getAllActionsOfTask(const USignature& task, const StateEvaluator& state);
    std::vector<USignature> getAllReductionsOfTask(const USignature& task, const StateEvaluator& state);
    void addNewFalseFacts();
    void introduceNewFalseFact(Position& newPos, const USignature& fact);
    void addQConstantTypeConstraints(const USignature& op);

    void pruneRetroactively(const NodeHashSet<PositionedUSig, PositionedUSigHasher>& updatedOps);

    LayerState& getLayerState(int layer = -1);
    StateEvaluator getStateEvaluator(int layer = -1, int pos = -1);
    // Introduces "fact" as FALSE at newPos.

};

#endif