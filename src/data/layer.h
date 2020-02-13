
#ifndef DOMPASCH_TREE_REXX_LAYER_H
#define DOMPASCH_TREE_REXX_LAYER_H

#include <vector>
#include <unordered_set>

#include "data/signature.h"

typedef std::pair<int, int> IntPair;

struct Reason {
    bool axiomatic = false;
    int layer = -1;
    int pos = -1;
    Signature sig;
    Reason(int layer, int pos, Signature sig) : layer(layer), pos(pos), sig(sig) {}
    Reason() : axiomatic(true) {}
    IntPair getOriginPos() const {return IntPair(layer, pos);}
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

private:
    int _layer_idx;
    int _pos;

    CausalSigSet _facts;
    CausalSigSet _true_facts;
    CausalSigSet _actions;
    CausalSigSet _reductions;
    int _max_expansion_size = 1;
    std::unordered_map<int, SigSet> _state;
    std::unordered_map<Signature, int, SignatureHasher> _variables;

public:
    Position() {}
    void setPos(int layerIdx, int pos) {_layer_idx = layerIdx; _pos = pos;}

    void addFact(const Signature& fact, Reason rs = Reason()) {_facts[fact]; _facts[fact].insert(rs);}
    void addTrueFact(const Signature& fact, Reason rs = Reason()) {_true_facts[fact]; _true_facts[fact].insert(rs);}
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
    const CausalSigSet& getActions() const {return _actions;}
    const CausalSigSet& getReductions() const {return _reductions;}
    const std::unordered_map<int, SigSet>& getState() const {return _state;}
    int getMaxExpansionSize() const {return _max_expansion_size;}

    std::string varName(const Signature& sig) const {
        return Names::to_string(sig)+ "@(" + std::to_string(_layer_idx) + "," + std::to_string(_pos) + ")";
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