
#ifndef DOMPASCH_TREE_REXX_LAYER_H
#define DOMPASCH_TREE_REXX_LAYER_H

#include <vector>
#include <set>

#include "data/hashmap.h"
#include "data/signature.h"
#include "data/layer_state.h"
#include "util/names.h"

typedef std::pair<int, int> IntPair;

struct Position {

public:
    const static USignature NONE_SIG;
    const static SigSet EMPTY_SIG_SET;
    const static USigSet EMPTY_USIG_SET;

private:
    int _layer_idx;
    int _pos;

    USigSet _actions;
    USigSet _reductions;

    HashMap<USignature, USigSet, USignatureHasher> _expansions;
    HashMap<USignature, SigSet, USignatureHasher> _fact_changes;

    USigSet _axiomatic_ops;

    // All ACTUAL facts potentially occurring at this position.
    USigSet _facts;

    // All VIRTUAL facts potentially occurring at this position,
    // partitioned by their predicate name ID.
    HashMap<int, USigSet> _qfacts;

    // All facts that are definitely true at this position.
    USigSet _true_facts;
    // All facts that are definitely false at this position.
    USigSet _false_facts;

    HashMap<USignature, USigSet, USignatureHasher> _pos_fact_supports;
    HashMap<USignature, USigSet, USignatureHasher> _neg_fact_supports;

    HashMap<USignature, std::vector<TypeConstraint>, USignatureHasher> _q_constants_type_constraints;
    HashMap<USignature, HashSet<substitution_t, Substitution::Hasher>, USignatureHasher> _forbidden_substitutions_per_op;

    int _max_expansion_size = 1;

    // Prop. variable for each occurring signature, together with the position where it was originally encoded.
    HashMap<USignature, IntPair, USignatureHasher> _variables;

public:
    Position() {}
    void setPos(int layerIdx, int pos) {_layer_idx = layerIdx; _pos = pos;}

    void addFact(const USignature& fact) {_facts.insert(fact);}
    void addQFact(const USignature& qfact) {
        _qfacts[qfact._name_id];
        _qfacts[qfact._name_id].insert(qfact);
    }
    void addTrueFact(const USignature& fact) {_true_facts.insert(fact);}
    void addFalseFact(const USignature& fact) {_false_facts.insert(fact);}
    void addDefinitiveFact(const Signature& fact) {(fact._negated ? _false_facts : _true_facts).insert(fact._usig);}

    void addFactSupport(const Signature& fact, const USignature& operation) {
        auto& supports = (fact._negated ? _neg_fact_supports : _pos_fact_supports);
        supports[fact._usig];
        supports[fact._usig].insert(operation);
    }
    void touchFactSupport(const Signature& fact) {
        auto& supports = (fact._negated ? _neg_fact_supports : _pos_fact_supports);
        supports[fact._usig];
    }
    void addQConstantTypeConstraint(const USignature& op, const TypeConstraint& c) {
        _q_constants_type_constraints[op];
        _q_constants_type_constraints[op].push_back(c);
    }
    void addForbiddenSubstitution(const USignature& op, const substitution_t& s) {
        _forbidden_substitutions_per_op[op];
        _forbidden_substitutions_per_op[op].insert(s);
    }

    void addAction(const USignature& action) {
        _actions.insert(action);
    }
    void addReduction(const USignature& reduction) {
        _reductions.insert(reduction);
    }
    void addExpansion(const USignature& parent, const USignature& child) {
        _expansions[parent];
        _expansions[parent].insert(child);
    }
    void addAxiomaticOp(const USignature& op) {
        _axiomatic_ops.insert(op);
    }
    void addExpansionSize(int size) {_max_expansion_size = std::max(_max_expansion_size, size);}
    void setFactChanges(const USignature& op, const SigSet& factChanges) {
        _fact_changes[op] = factChanges;
    }
    const SigSet& getFactChanges(const USignature& op) const {
        return _fact_changes.count(op) ? _fact_changes.at(op) : EMPTY_SIG_SET;
    }
    void clearFactChanges() {
        _fact_changes.clear();
    }

    int encode(const USignature& sig);
    void setVariable(const USignature& sig, int v, int priorPos);
    bool hasVariable(const USignature& sig) const;
    int getVariable(const USignature& sig) const;
    int getPriorPosOfVariable(const USignature& sig) const;
    bool isVariableOriginallyEncoded(const USignature& sig) const;

    bool hasFact(const USignature& fact) const {return _facts.count(fact);}
    bool hasQFact(const USignature& fact) const {return _qfacts.count(fact._name_id) && _qfacts.at(fact._name_id).count(fact);}
    bool hasAction(const USignature& action) const {return _actions.count(action);}
    bool hasReduction(const USignature& red) const {return _reductions.count(red);}

    IntPair getPos() const {return IntPair(_layer_idx, _pos);}
    
    const USigSet& getFacts() const {return _facts;}
    const HashMap<int, USigSet>& getQFacts() const {return _qfacts;}
    const USigSet& getQFacts(int predId) const {return _qfacts.count(predId) ? _qfacts.at(predId) : EMPTY_USIG_SET;}
    int getNumQFacts() const {
        int sum = 0;
        for (const auto& entry : _qfacts) sum += entry.second.size();
        return sum;
    };
    const USigSet& getTrueFacts() const {return _true_facts;}
    const USigSet& getFalseFacts() const {return _false_facts;}
    const HashMap<USignature, USigSet, USignatureHasher>& getPosFactSupports() const {return _pos_fact_supports;}
    const HashMap<USignature, USigSet, USignatureHasher>& getNegFactSupports() const {return _neg_fact_supports;}
    const HashMap<USignature, std::vector<TypeConstraint>, USignatureHasher>& getQConstantsTypeConstraints() const {
        return _q_constants_type_constraints;
    }
    const HashMap<USignature, HashSet<substitution_t, Substitution::Hasher>, USignatureHasher>& 
    getForbiddenSubstitutions() const {
        return _forbidden_substitutions_per_op;
    }

    const USigSet& getActions() const {return _actions;}
    const USigSet& getReductions() const {return _reductions;}
    const HashMap<USignature, USigSet, USignatureHasher>& getExpansions() const {return _expansions;}
    const USigSet& getAxiomaticOps() const {return _axiomatic_ops;}
    int getMaxExpansionSize() const {return _max_expansion_size;}

    void clearUnneeded() {
        _expansions.clear();
        _axiomatic_ops.clear();
        _true_facts.clear();
        _pos_fact_supports.clear();
        _neg_fact_supports.clear();
        _q_constants_type_constraints.clear();
        for (const USignature& fact : _facts) {
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