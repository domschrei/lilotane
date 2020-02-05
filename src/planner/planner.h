
#ifndef DOMPASCH_TREE_REXX_PLANNER_H
#define DOMPASCH_TREE_REXX_PLANNER_H

#include <unordered_map>
#include <assert.h> 
 
#include "data/layer.h"
#include "util/names.h"

#include "data/htn_instance.h"
#include "data/instantiator.h"
#include "data/effector_table.h"
#include "data/arg_iterator.h"


class Planner {

private:
    HtnInstance _htn;

    std::vector<Layer> _layers;
    Instantiator& _instantiator;
    EffectorTable& _effector_table;

public:
    Planner(ParsedProblem& problem) : _htn(problem), _instantiator(*(_htn._instantiator)), _effector_table(*(_htn._effector_table)) {}
    void findPlan();

private:

    void addToLayer(Signature& task, Layer& layer, int layerIdx, int pos, std::unordered_map<int, SigSet>& state, std::unordered_map<int, SigSet>& newState);

    /*
    int getFact(Signature& sig) {
        return _atom_table(sig);
    }
    int getAction(Signature& sig) {
        return _action_table(sig);
    }
    int getReduction(Signature& sig) {
        return _reduction_table(sig);
    }
    */

};

#endif