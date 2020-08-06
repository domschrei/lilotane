
#ifndef DOMPASCH_TREEREXX_POSITION_H
#define DOMPASCH_TREEREXX_POSITION_H

#include <vector>
#include <set>

#include "data/hashmap.h"
#include "data/signature.h"
#include "util/names.h"
#include "sat/variable_domain.h"
#include "util/log.h"

typedef std::pair<int, int> IntPair;

struct Position {

public:
    const static USignature NONE_SIG;
    const static SigSet EMPTY_SIG_SET;
    const static USigSet EMPTY_USIG_SET;

private:
    size_t _layer_idx;
    size_t _pos;

    USigSet _actions;
    USigSet _reductions;

    NodeHashMap<USignature, USigSet, USignatureHasher> _expansions;
    NodeHashMap<USignature, USigSet, USignatureHasher> _predecessors;
    NodeHashMap<USignature, SigSet, USignatureHasher> _fact_changes;

    USigSet _axiomatic_ops;

    // All VIRTUAL facts potentially occurring at this position.
    USigSet _qfacts;

    // All facts that are definitely true at this position.
    USigSet _true_facts;
    // All facts that are definitely false at this position.
    USigSet _false_facts;

    NodeHashMap<USignature, USigSet, USignatureHasher> _pos_fact_supports;
    NodeHashMap<USignature, USigSet, USignatureHasher> _neg_fact_supports;

    NodeHashMap<USignature, std::vector<TypeConstraint>, USignatureHasher> _q_constants_type_constraints;

    NodeHashMap<USignature, NodeHashSet<Substitution, Substitution::Hasher>, USignatureHasher> _forbidden_substitutions_per_op;
    NodeHashMap<USignature, std::vector<NodeHashSet<Substitution, Substitution::Hasher>>, USignatureHasher> _valid_substitutions_per_op;

    size_t _max_expansion_size = 1;

    // Prop. variable for each occurring signature.
    NodeHashMap<USignature, int, USignatureHasher> _variables;

public:

    Position();
    void setPos(size_t layerIdx, size_t pos);

    void addQFact(const USignature& qfact);
    void addTrueFact(const USignature& fact);
    void addFalseFact(const USignature& fact);
    void addDefinitiveFact(const Signature& fact);

    void addFactSupport(const Signature& fact, const USignature& operation);
    void touchFactSupport(const Signature& fact);
    void touchFactSupport(const USignature& fact, bool negated);
    void addIndirectFactSupport(const Signature& fact, const USignature& op, const Substitution& s);
    void addQConstantTypeConstraint(const USignature& op, const TypeConstraint& c);

    void addForbiddenSubstitution(const USignature& op, const Substitution& s);
    void addValidSubstitutions(const USignature& op, const NodeHashSet<Substitution, Substitution::Hasher>& subs);
    void clearForbiddenSubstitutions(const USignature& op);
    void clearValidSubstitutions(const USignature& op);

    void addAction(const USignature& action);
    void addReduction(const USignature& reduction);
    void addExpansion(const USignature& parent, const USignature& child);
    void addAxiomaticOp(const USignature& op);
    void addExpansionSize(size_t size);
    void setFactChanges(const USignature& op, const SigSet& factChanges);
    const SigSet& getFactChanges(const USignature& op) const;
    bool hasFactChanges(const USignature& op) const;
    void moveFactChanges(Position& dest, const USignature& op);

    void removeActionOccurrence(const USignature& action);
    void removeReductionOccurrence(const USignature& reduction);

    const NodeHashMap<USignature, int, USignatureHasher>& getVariableTable() const;

    bool hasQFact(const USignature& fact) const;
    bool hasAction(const USignature& action) const;
    bool hasReduction(const USignature& red) const;

    const USigSet& getQFacts() const;
    int getNumQFacts() const;
    const USigSet& getTrueFacts() const;
    const USigSet& getFalseFacts() const;
    const NodeHashMap<USignature, USigSet, USignatureHasher>& getPosFactSupports() const;
    const NodeHashMap<USignature, USigSet, USignatureHasher>& getNegFactSupports() const;
    const NodeHashMap<USignature, std::vector<TypeConstraint>, USignatureHasher>& getQConstantsTypeConstraints() const;
    const NodeHashMap<USignature, NodeHashSet<Substitution, Substitution::Hasher>, USignatureHasher>& 
    getForbiddenSubstitutions() const;
    const NodeHashMap<USignature, std::vector<NodeHashSet<Substitution, Substitution::Hasher>>, USignatureHasher>& 
    getValidSubstitutions() const;

    const USigSet& getActions() const;
    const USigSet& getReductions() const;
    const NodeHashMap<USignature, USigSet, USignatureHasher>& getExpansions() const;
    const NodeHashMap<USignature, USigSet, USignatureHasher>& getPredecessors() const;
    const USigSet& getAxiomaticOps() const;
    size_t getMaxExpansionSize() const;

    size_t getLayerIndex() const;
    size_t getPositionIndex() const;
    
    void clearAtPastPosition();
    void clearAtPastLayer();

    inline int encode(const USignature& sig) {
        if (!_variables.count(sig)) {
            // introduce a new variable
            assert(!VariableDomain::isLocked() || Log::e("Unknown variable %s queried!\n", VariableDomain::varName(_layer_idx, _pos, sig).c_str()));
            _variables[sig] = VariableDomain::nextVar();
            VariableDomain::printVar(_variables[sig], _layer_idx, _pos, sig);
        }
        return _variables[sig];
    }

    inline int setVariable(const USignature& sig, int var) {
        assert(!_variables.count(sig));
        _variables[sig] = var;
        return var;
    }

    inline bool hasVariable(const USignature& sig) const {
        return _variables.count(sig);
    }

    inline int getVariable(const USignature& sig) const {
        assert(_variables.count(sig) || Log::e("Unknown variable %s queried!\n", VariableDomain::varName(_layer_idx, _pos, sig).c_str()));
        return _variables.at(sig);
    }

    inline int getVariableOrZero(const USignature& sig) const {
        const auto& it = _variables.find(sig);
        if (it == _variables.end()) return 0;
        return it->second;
    }
};


#endif