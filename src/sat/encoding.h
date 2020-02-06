
#ifndef DOMPASCH_TREE_REXX_ENCODING_H
#define DOMPASCH_TREE_REXX_ENCODING_H

#include <initializer_list>

#include "sat/ipasir.h"

#include "data/signature.h"
#include "data/htn_instance.h"
#include "data/action.h"

typedef std::unordered_map<int, SigSet> State;

class Encoding {

private:
    HtnInstance& _htn;
    std::vector<std::vector<std::unordered_map<Signature, int, SignatureHasher>>> _variables;
    int _running_var_id = 1;

    void* _solver;

    Signature _sig_primitive;
    Action _action_blank;
    int _substitute_name_id;

    // Maps a positional fact variable to the set of positional operator variables that may change it.
    std::unordered_map<int, std::vector<int>> _supports;
    // Maps a positional red. variable to its possible expansions: 
    // one vector of positional operator variables for each offset position.
    std::unordered_map<int, std::vector<std::vector<int>>> _expansions;
    // Maps a position in the current layer to a set of predicate-indexed fact signatures occurring there.
    // The signatures carry a relevant "negated" bit, so both positive and negative facts are contained.
    std::vector<State> _occurring_facts;

public:
    Encoding(HtnInstance& htn);
    /*
    void addInitialClauses(Layer& initLayer);
    void addUniversalClauses(Layer& layer);
    void addTransitionalClauses(Layer& oldLayer, Layer& newLayer);
    */

    void addAction(Action& a, Layer& layer, int pos);
    void addAction(Action& a, Reduction& parent, Layer& oldLayer, int oldPos, Layer& newLayer, int newPos);
    void addReduction(Reduction& r, SigSet& allFactChanges, Layer& layer, int pos);
    void addReduction(Reduction& child, Reduction& parent, SigSet& allFactChanges, 
                        Layer& oldLayer, int oldPos, Layer& newLayer, int newPos);
    void endReduction(Reduction& r, Layer& layer, int pos);
    void addOccurringFacts(State& state, Layer& layer, int pos);

    void addFrameAxioms();
    
    void addAssumptions(Layer& layer);

    bool solve();

private:
    /*
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
    */

    Signature sigSubstitute(int qConstId, int trueConstId) {
        std::vector<int> args(2);
        args[0] = (qConstId);
        args[1] = (trueConstId);
        return Signature(_substitute_name_id, args);
    }

    std::vector<int>& getSupport(int factVar) {
        assert(_supports.count(factVar));
        return _supports[factVar];
    }
    void addToSupport(int factVar, int opVar) {
        if (!_supports.count(factVar)) _supports[factVar];
        _supports[factVar].push_back(opVar);
    }
    int getNearestPriorOccurrence(Signature factSig, int pos) {
        for (int posBefore = pos-1; posBefore >= 0; posBefore--) {
            if (_occurring_facts[posBefore].count(factSig._name_id)) {
                if (_occurring_facts[posBefore][factSig._name_id].count(factSig)) {
                    return posBefore;
                }
                factSig.negate();
                if (_occurring_facts[posBefore][factSig._name_id].count(factSig)) {
                    return posBefore;
                }
                factSig.negate();
            }
        }
        return -1;
    }

    std::vector<int>& getExpansion(int redVar, int offset) {
        assert(_expansions.count(redVar));
        assert(offset < _expansions[redVar].size());
        return _expansions[redVar][offset];
    }
    void addToExpansion(int redVar, int opVar, int offset) {
        if (!_expansions.count(redVar)) _expansions[redVar];
        while (offset >= _expansions[redVar].size()) _expansions[redVar].push_back(std::vector<int>());
        _expansions[redVar][offset].push_back(opVar);
    }

    void addClause(std::initializer_list<int> lits);
    void appendClause(std::initializer_list<int> lits);
    void endClause();
    void assume(int lit);

    int var(int layer, int pos, Signature& sig);
    int varPrimitive(int layer, int pos);

    bool value(int layer, int pos, Signature& sig);

};

#endif