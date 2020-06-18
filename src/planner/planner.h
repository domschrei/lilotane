
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

private:
    Parameters& _params;
    HtnInstance _htn;

    std::vector<Layer> _layers;
    Instantiator& _instantiator;
    Encoding _enc;

    int _layer_idx;
    int _pos;
    int _old_pos;

public:
    Planner(Parameters& params, ParsedProblem& problem) : _params(params), _htn(params, problem), 
            _instantiator(*(_htn._instantiator)), _enc(_params, _htn, _layers) {}
    int findPlan();

private:

    void outputPlan();

    void createFirstLayer();
    void createNextLayer();
    
    void createNextPosition();
    void createNextPositionFromAbove(const Position& above);
    void createNextPositionFromLeft(const Position& left);

    void addPrecondition(const USignature& op, const Signature& fact);
    void addEffect(const USignature& op, const Signature& fact);

    void propagateInitialState();
    void propagateActions(int offset);
    void propagateReductions(int offset);
    std::vector<USignature> getAllActionsOfTask(const USignature& task, std::function<bool(const Signature&)> state);
    std::vector<USignature> getAllReductionsOfTask(const USignature& task, std::function<bool(const Signature&)> state);
    void addNewFalseFacts();
    void introduceNewFalseFact(Position& newPos, const USignature& fact);
    void addQConstantTypeConstraints(const USignature& op);

    LayerState& getLayerState(int layer = -1);
    std::function<bool(const Signature&)> getStateEvaluator(int layer = -1, int pos = -1);
    // Introduces "fact" as FALSE at newPos.

};

#endif