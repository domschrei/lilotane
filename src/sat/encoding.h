
#ifndef DOMPASCH_TREE_REXX_ACTION_H
#define DOMPASCH_TREE_REXX_ACTION_H

#include <initializer_list>

#include "data/signature.h"
#include "data/htn_instance.h"

class Encoding {

private:
    HtnInstance& _htn;
    std::vector<std::vector<std::unordered_map<Signature, int>>> _variables;
    int _running_var_id = 1;

public:
    Encoding(HtnInstance& htn) : _htn(htn) {}
    void addInitialClauses(Layer& initLayer);
    void addUniversalClauses(Layer& layer);
    void addTransitionalClauses(Layer& oldLayer, Layer& newLayer);
    void addAssumptions(Layer& layer);
    
    bool solve();

private:
    // initial reductions/actions
    void addInitialTaskNetwork(Layer& layer);
    // initial state unit clauses at position 0
    void addInitialState(Layer& layer);

    // Preconditions, effects, at most one action
    void addActionConstraints(Layer& layer);
    // If (fact change and not substituted), then (one of the supporting actions/reductions)
    void addFrameAxioms(Layer& layer);
    // For each qconstant: ALO substitution variable
    // For each concerned fact: if substitution, then equivalence to original fact
    void addQConstantsDefinition(Layer& layer);
    // actions primitive, reductions non-primitive
    void addPrimitivenessDefinition(Layer& layer);
    // all positions at the layer must be primitive
    void assertAllPrimitive(Layer& layer);

    // sub-reductions/-actions of each reduction at previous layer
    void addExpansions(Layer& oldLayer, Layer& newLayer);
    // actions are propagated to next layer
    void addActionPropagations(Layer& oldLayer, Layer& newLayer);
    // facts are equivalent at each parent-child position pair
    void addFactEquivalencies(Layer& oldLayer, Layer& newLayer);

    void addClause(std::initializer_list<int> lits);

    void beginClause();
    void appendClause(std::initializer_list<int> lits);
    void endClause();

    int var(int layer, int pos, Signature& sig) {
        assert(layer < _variables.size());
        assert(pos < _variables[layer].size());

        std::unordered_map<Signature, int>& vars = _variables[layer][pos]; 
        if (!vars.count(sig)) {
            vars[sig] = _running_var_id++;
        }
        return vars[sig];
    }
    bool value(int layer, int pos, Signature& sig);

};

#endif