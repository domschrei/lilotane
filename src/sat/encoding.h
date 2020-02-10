
#ifndef DOMPASCH_TREE_REXX_ENCODING_H
#define DOMPASCH_TREE_REXX_ENCODING_H

extern "C" {
    #include "sat/ipasir.h"
}

#include <initializer_list>
#include <fstream>
#include <string>
#include <iostream>

#include "data/signature.h"
#include "data/htn_instance.h"
#include "data/action.h"

#define PRINT_TO_FILE true

typedef std::unordered_map<int, SigSet> State;

struct PlanItem {
    int id;
    Signature abstractTask;
    Signature reduction;
    std::vector<int> subtaskIds;
};

class Encoding {

private:
    HtnInstance& _htn;
    std::vector<std::vector<std::unordered_map<Signature, int, SignatureHasher>>> _variables;
    int _running_var_id = 1;

    void* _solver;
    std::ofstream _out;

    Signature _sig_primitive;
    int _substitute_name_id;

    // Maps a positional fact variable to the set of positional operator variables that may change it.
    std::unordered_map<int, std::vector<int>> _supports;
    // Maps a positional red. variable to its possible expansions: 
    // one vector of positional operator variables for each offset position.
    std::unordered_map<int, std::vector<std::vector<int>>> _expansions;
    // A set of predicate-indexed occurring fact signatures.
    State _prior_facts;
    State _posterior_facts;

    std::unordered_set<int> _q_constants;

    std::unordered_map<int, std::vector<int>> _q_constants_per_arg;

    std::unordered_set<int> _current_action_vars;
    std::unordered_set<int> _current_reduction_vars;

    bool _var_domain_locked = false;

public:
    Encoding(HtnInstance& htn);
    ~Encoding();
    /*
    void addInitialClauses(Layer& initLayer);
    void addUniversalClauses(Layer& layer);
    void addTransitionalClauses(Layer& oldLayer, Layer& newLayer);
    */
    void addInitialTasks(Layer& layer, int pos); 

    void addAction(Action& a, Layer& layer, int pos);
    void addAction(Action& a, Reduction& parent, Layer& oldLayer, int oldPos, Layer& newLayer, int newPos);
    void propagateAction(Action& a, Layer& oldLayer, int oldPos, Layer& newLayer, int newPos);

    void addReduction(Reduction& r, SigSet& allFactChanges, Layer& layer, int pos);
    void addReduction(Reduction& child, Reduction& parent, SigSet& allFactChanges, 
                        Layer& oldLayer, int oldPos, Layer& newLayer, int newPos);
    void consolidateReductionExpansion(Reduction& r, Layer& layer, int pos);
    
    void consolidateHtnOps(Layer& layer, int pos);

    void addTrueFacts(SigSet& facts, Layer& layer, int pos);
    void propagateFacts(const SigSet& facts, Layer& oldLayer, int oldPos, Layer& newLayer, int newPos);
    void consolidateFacts(Layer& layer, int pos);

    
    void addAssumptions(Layer& layer);

    bool solve();

    std::vector<PlanItem> extractClassicalPlan(Layer& finalLayer);
    std::vector<PlanItem> extractDecompositionPlan(std::vector<Layer>& allLayers);

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
        assert(!_htn._q_constants.count(trueConstId) || trueConstId < qConstId);
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
    /*
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
    */
    
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

    bool isEncoded(int layer, int pos, Signature& sig);
    int var(int layer, int pos, Signature& sig);
    std::string varName(int layer, int pos, Signature& sig);
    void printVar(int layer, int pos, Signature& sig);
    int varPrimitive(int layer, int pos);

    bool value(int layer, int pos, Signature& sig);

};

#endif