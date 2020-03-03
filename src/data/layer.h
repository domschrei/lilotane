
#ifndef DOMPASCH_TREE_REXX_LAYER_H
#define DOMPASCH_TREE_REXX_LAYER_H

#include <vector>
#include <unordered_set>

#include "data/signature.h"
#include "util/names.h"

typedef std::pair<int, int> IntPair;

struct Position {

public:
    const static Signature NONE_SIG;

private:
    int _layer_idx;
    int _pos;

    SigSet _actions;
    SigSet _reductions;

    std::unordered_map<Signature, SigSet, SignatureHasher> _expansions;

    SigSet _axiomatic_ops;

    SigSet _facts;
    SigSet _true_facts;
    std::unordered_map<Signature, SigSet, SignatureHasher> _fact_supports;

    std::unordered_map<Signature, SigSet, SignatureHasher> _qfact_decodings;
    std::unordered_map<Signature, SigSet, SignatureHasher> _qfact_abstractions;

    std::unordered_map<Signature, std::vector<TypeConstraint>, SignatureHasher> _q_constants_type_constraints;

    int _max_expansion_size = 1;

    // Not being encoded; used for reducing instantiation of the next position.
    std::unordered_map<int, SigSet> _state;

    // Prop. variable for each occurring signature.
    std::unordered_map<Signature, int, SignatureHasher> _variables;

public:
    Position() {}
    void setPos(int layerIdx, int pos) {_layer_idx = layerIdx; _pos = pos;}

    void addFact(const Signature& fact) {_facts.insert(fact);}
    void addTrueFact(const Signature& fact) {_true_facts.insert(fact);}
    void addQFactDecoding(const Signature& qfact, const Signature& decoding) {
        _qfact_decodings[qfact]; _qfact_decodings[qfact].insert(decoding);
        _qfact_abstractions[decoding]; _qfact_abstractions[decoding].insert(qfact);
    }
    void addFactSupport(const Signature& fact, const Signature& operation) {
        _fact_supports[fact];
        _fact_supports[fact].insert(operation);
    }
    void addQConstantTypeConstraint(const Signature& op, const TypeConstraint& c) {
        _q_constants_type_constraints[op];
        _q_constants_type_constraints[op].push_back(c);
    }

    void addAction(const Signature& action) {
        _actions.insert(action);
    }
    void addReduction(const Signature& reduction) {
        _reductions.insert(reduction);
    }
    void addExpansion(const Signature& parent, const Signature& child) {
        _expansions[parent];
        _expansions[parent].insert(child);
    }
    void addAxiomaticOp(const Signature& op) {
        _axiomatic_ops.insert(op);
    }
    void addExpansionSize(int size) {_max_expansion_size = std::max(_max_expansion_size, size);}
    
    void setFacts(const SigSet& facts) {
        _facts = facts;
    }
    void extendState(const Signature& fact) {_state[fact._name_id].insert(fact);}
    void extendState(const SigSet& set) {for (Signature sig : set) extendState(sig);}
    void extendState(const std::unordered_map<int, SigSet>& state) {
        for (auto entry : state)
            extendState(entry.second);
    }

    int encode(const Signature& sig);

    bool hasFact(const Signature& fact) const {return _facts.count(fact);}
    bool hasAction(const Signature& action) const {return _actions.count(action);}
    bool hasReduction(const Signature& red) const {return _reductions.count(red);}
    bool containsInState(const Signature& fact) const {return _state.count(fact._name_id) && _state.at(fact._name_id).count(fact);}
    bool hasVariable(const Signature& sig);
    int getVariable(const Signature& sig);

    IntPair getPos() const {return IntPair(_layer_idx, _pos);}
    
    const SigSet& getFacts() const {return _facts;}
    const SigSet& getTrueFacts() const {return _true_facts;}
    const std::unordered_map<Signature, SigSet, SignatureHasher>& getQFactDecodings() const {return _qfact_decodings;}
    const std::unordered_map<Signature, SigSet, SignatureHasher>& getQFactAbstractions() const {return _qfact_abstractions;}
    const std::unordered_map<Signature, SigSet, SignatureHasher>& getFactSupports() const {return _fact_supports;}
    const std::unordered_map<Signature, std::vector<TypeConstraint>, SignatureHasher>& getQConstantsTypeConstraints() const {
        return _q_constants_type_constraints;
    }

    const SigSet& getActions() const {return _actions;}
    const SigSet& getReductions() const {return _reductions;}
    const std::unordered_map<int, SigSet>& getState() const {return _state;}
    int getMaxExpansionSize() const {return _max_expansion_size;}
};

class Layer {

private:
    int _index;
    std::vector<Position> _content;
    std::vector<int> _successor_positions;

public:
    Layer(int index, int size);
    int size() const;
    int index() const;
    Position& operator[](int pos);
    void consolidate();
    int getNextLayerSize() const;
    int getSuccessorPos(int oldPos) const;
};

#endif