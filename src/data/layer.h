
#ifndef DOMPASCH_TREE_REXX_LAYER_H
#define DOMPASCH_TREE_REXX_LAYER_H

#include <vector>
#include <unordered_set>
#include <set>

#include "data/signature.h"
#include "data/layer_state.h"
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

    HashMap<Signature, SigSet, SignatureHasher> _expansions;

    SigSet _axiomatic_ops;

    SigSet _facts;
    SigSet _true_facts;
    HashMap<Signature, SigSet, SignatureHasher> _fact_supports;

    HashMap<Signature, SigSet, SignatureHasher> _qfact_decodings;
    HashMap<Signature, SigSet, SignatureHasher> _qfact_abstractions;

    HashMap<Signature, std::vector<TypeConstraint>, SignatureHasher> _q_constants_type_constraints;
    HashMap<Signature, std::unordered_set<substitution_t, Substitution::Hasher>, SignatureHasher> _forbidden_substitutions_per_op;

    int _max_expansion_size = 1;

    // Prop. variable for each occurring signature, together with the position where it was originally encoded.
    HashMap<Signature, IntPair, SignatureHasher> _variables;

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
    void touchFactSupport(const Signature& fact) {
        _fact_supports[fact];
    }
    void addQConstantTypeConstraint(const Signature& op, const TypeConstraint& c) {
        _q_constants_type_constraints[op];
        _q_constants_type_constraints[op].push_back(c);
    }
    void addForbiddenSubstitution(const Signature& op, const substitution_t& s) {
        _forbidden_substitutions_per_op[op];
        _forbidden_substitutions_per_op[op].insert(s);
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

    int encode(const Signature& sig);
    void setVariable(const Signature& sig, int v, int priorPos);
    bool hasVariable(const Signature& sig) const;
    int getVariable(const Signature& sig) const;
    int getPriorPosOfVariable(const Signature& sig) const;
    bool isVariableOriginallyEncoded(const Signature& sig) const;

    bool hasFact(const Signature& fact) const {return _facts.count(fact);}
    bool hasAction(const Signature& action) const {return _actions.count(action);}
    bool hasReduction(const Signature& red) const {return _reductions.count(red);}

    IntPair getPos() const {return IntPair(_layer_idx, _pos);}
    
    const SigSet& getFacts() const {return _facts;}
    const SigSet& getTrueFacts() const {return _true_facts;}
    const HashMap<Signature, SigSet, SignatureHasher>& getQFactDecodings() const {return _qfact_decodings;}
    const HashMap<Signature, SigSet, SignatureHasher>& getQFactAbstractions() const {return _qfact_abstractions;}
    const HashMap<Signature, SigSet, SignatureHasher>& getFactSupports() const {return _fact_supports;}
    const HashMap<Signature, std::vector<TypeConstraint>, SignatureHasher>& getQConstantsTypeConstraints() const {
        return _q_constants_type_constraints;
    }
    const HashMap<Signature, std::unordered_set<substitution_t, Substitution::Hasher>, SignatureHasher>& 
    getForbiddenSubstitutions() const {
        return _forbidden_substitutions_per_op;
    }

    const SigSet& getActions() const {return _actions;}
    const SigSet& getReductions() const {return _reductions;}
    const HashMap<Signature, SigSet, SignatureHasher>& getExpansions() const {return _expansions;}
    const SigSet& getAxiomaticOps() const {return _axiomatic_ops;}
    int getMaxExpansionSize() const {return _max_expansion_size;}

    void clearUnneeded() {
        _expansions.clear();
        _axiomatic_ops.clear();
        _true_facts.clear();
        _fact_supports.clear();
        _qfact_decodings.clear();
        _qfact_abstractions.clear();
        _q_constants_type_constraints.clear();
        for (const Signature& fact : _facts) {
            _variables.erase(fact);
        }
        _facts.clear();
    }
};

class Layer {

private:
    int _index;
    std::vector<Position> _content;
    LayerState _state;
    std::vector<int> _successor_positions;

public:
    Layer(int index, int size);

    int size() const;
    int index() const;
    int getNextLayerSize() const;
    int getSuccessorPos(int oldPos) const;
    LayerState& getState();
    
    Position& operator[](int pos);
    
    void consolidate();
};

#endif