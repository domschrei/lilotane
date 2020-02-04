
#ifndef DOMPASCH_TREE_REXX_EFFECTOR_GRAPH_H
#define DOMPASCH_TREE_REXX_EFFECTOR_GRAPH_H


#include <unordered_map>
#include <functional>

#include "data/action.h"
#include "data/reduction.h"
#include "data/signature.h"
#include "parser/main.h"

class EffectorTable {

private:
    ParsedProblem& _problem;
    std::function<std::vector<Signature>(Signature&)> _child_generator;

    // Maps an action ID to its action object.
    std::unordered_map<int, Action> _actions;
    // Maps a reduction ID to its reduction object.
    std::unordered_map<int, Reduction> _reductions;

    // Maps an (action|reduction) name 
    // to the set of (partially lifted) fact signatures
    // that might be added to the state due to this operator. 
    std::unordered_map<int, std::vector<Signature>> _fact_changes; 

public:
    EffectorTable(ParsedProblem& p, std::function<std::vector<Signature>(Signature&)> generateChildren,
            std::unordered_map<int, Action> actions, std::unordered_map<int, Reduction> reductions) : 
            _problem(p), _child_generator(generateChildren), _actions(actions), _reductions(reductions) {}

    // Maps a (action|reduction) signature of any grounding state
    // to the corresponding set of (partially lifted) fact signatures
    // that might be added to the state due to this operator. 
    std::vector<Signature> getPossibleFactChanges(Signature sig);

};


#endif