
#ifndef DOMPASCH_TREE_REXX_LAYER_H
#define DOMPASCH_TREE_REXX_LAYER_H

#include <vector>
#include <unordered_set>

#include "data/signature.h"
#include "util/names.h"

typedef std::pair<int, int> IntPair;

struct Reason {
    bool axiomatic = false;
    int layer = -1;
    int pos = -1;
    Signature sig;
    Reason(int layer, int pos, const Signature& sig) : layer(layer), pos(pos), sig(sig) {}
    Reason() : axiomatic(true) {
        sig = Signature();
    }
    IntPair getOriginPos() const {return IntPair(layer, pos);}

    bool operator==(const Reason& b) const {
        if (axiomatic != b.axiomatic) return false;
        if (layer != b.layer) return false;
        if (pos != b.pos) return false;
        if (sig != b.sig) return false;
        return true;
    }
    bool operator!=(const Reason& b) const {
        return !(*this == b);
    }
};

struct ReasonHasher {
    std::size_t operator()(const Reason& s) const {
        size_t hash = 0;
        hash_combine(hash, s.axiomatic);
        hash_combine(hash, s.layer);
        hash_combine(hash, s.pos);
        SignatureHasher h;
        hash_combine(hash, h(s.sig));
        return hash;
    }
};

typedef std::unordered_map<Signature, std::unordered_set<Reason, ReasonHasher>, SignatureHasher> CausalSigSet;

struct Position {

public:
    const static Signature NONE_SIG;

private:
    int _layer_idx;
    int _pos;

    CausalSigSet _facts;
    CausalSigSet _true_facts;
    std::unordered_map<Signature, SigSet, SignatureHasher> _fact_supports;

    std::unordered_map<Signature, SigSet, SignatureHasher> _qfact_decodings;
    std::unordered_map<Signature, SigSet, SignatureHasher> _qfact_abstractions;

    std::unordered_map<Signature, std::vector<TypeConstraint>, SignatureHasher> _q_constants_type_constraints;

    CausalSigSet _actions;
    CausalSigSet _reductions;
    int _max_expansion_size = 1;

    // Not being encoded; used for reducing instantiation of the next position.
    std::unordered_map<int, SigSet> _state;

    // Prop. variable for each occurring signature.
    std::unordered_map<Signature, int, SignatureHasher> _variables;

public:
    Position() {}
    void setPos(int layerIdx, int pos) {_layer_idx = layerIdx; _pos = pos;}

    void addFact(const Signature& fact, Reason rs = Reason()) {_facts[fact]; _facts[fact].insert(rs);}
    void addTrueFact(const Signature& fact, Reason rs = Reason()) {_true_facts[fact]; _true_facts[fact].insert(rs);}
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

    void addAction(const Signature& action, Reason rs = Reason()) {
        _actions[action]; 
        _actions[action].insert(rs);
    }
    void addReduction(const Signature& reduction, Reason rs = Reason()) {
        _reductions[reduction]; 
        _reductions[reduction].insert(rs);
    }
    void addExpansionSize(int size) {_max_expansion_size = std::max(_max_expansion_size, size);}
    void setFacts(CausalSigSet facts) {
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
    
    const CausalSigSet& getFacts() const {return _facts;}
    const CausalSigSet& getTrueFacts() const {return _true_facts;}
    const std::unordered_map<Signature, SigSet, SignatureHasher>& getQFactDecodings() const {return _qfact_decodings;}
    const std::unordered_map<Signature, SigSet, SignatureHasher>& getQFactAbstractions() const {return _qfact_abstractions;}
    const std::unordered_map<Signature, SigSet, SignatureHasher>& getFactSupports() const {return _fact_supports;}
    const std::unordered_map<Signature, std::vector<TypeConstraint>, SignatureHasher>& getQConstantsTypeConstraints() const {
        return _q_constants_type_constraints;
    }

    const CausalSigSet& getActions() const {return _actions;}
    const CausalSigSet& getReductions() const {return _reductions;}
    const std::unordered_map<int, SigSet>& getState() const {return _state;}
    int getMaxExpansionSize() const {return _max_expansion_size;}

    std::string varName(const Signature& sig) const {
        std::string out = Names::to_string(sig) + "@(" + std::to_string(_layer_idx) + "," + std::to_string(_pos) + ")";
        return out;
    }
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