
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

#include "sat/encoding.h"


class Planner {

private:
    HtnInstance _htn;

    std::vector<Layer> _layers;
    Instantiator& _instantiator;
    EffectorTable& _effector_table;
    Encoding _enc;

    int _layer_idx;
    int _pos;

public:
    Planner(ParsedProblem& problem) : _htn(problem), _instantiator(*(_htn._instantiator)), 
            _effector_table(*(_htn._effector_table)), _enc(_htn, _layers) {}
    void findPlan();

private:

    void createNext(const Position& left);
    void createNext(const Position& above, int oldPos);
    void createNext(const Position& left, const Position& above, int oldPos);

    std::vector<Signature> getAllReductionsOfTask(const Signature& task, const State& state);
    std::vector<Signature> getAllActionsOfTask(const Signature& task, const State& state);

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