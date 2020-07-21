
#ifndef DOMPASCH_TREEREXX_POSITION_H
#define DOMPASCH_TREEREXX_POSITION_H

#include <vector>
#include <set>

#include "data/hashmap.h"
#include "data/signature.h"
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

    NodeHashMap<USignature, int, USignatureHasher> _actions;
    NodeHashMap<USignature, int, USignatureHasher> _reductions;

    NodeHashMap<USignature, USigSet, USignatureHasher> _expansions;
    NodeHashMap<USignature, USigSet, USignatureHasher> _predecessors;
    NodeHashMap<USignature, SigSet, USignatureHasher> _fact_changes;

    USigSet _axiomatic_ops;

    // All ACTUAL facts potentially occurring at this position.
    USigSet _facts;
    USigSet* _mirrored_facts = nullptr;

    // All VIRTUAL facts potentially occurring at this position,
    // partitioned by their predicate name ID.
    NodeHashMap<int, USigSet> _qfacts;

    // All facts that are definitely true at this position.
    USigSet _true_facts;
    // All facts that are definitely false at this position.
    USigSet _false_facts;

    NodeHashMap<USignature, USigSet, USignatureHasher> _pos_fact_supports;
    NodeHashMap<USignature, USigSet, USignatureHasher> _neg_fact_supports;

    NodeHashMap<USignature, std::vector<TypeConstraint>, USignatureHasher> _q_constants_type_constraints;
    NodeHashMap<USignature, NodeHashSet<Substitution, Substitution::Hasher>, USignatureHasher> _forbidden_substitutions_per_op;

    int _max_expansion_size = 1;

    // Prop. variable for each occurring signature, together with the position where it was originally encoded.
    NodeHashMap<USignature, int, USignatureHasher> _variables;

public:

    Position();
    void setPos(int layerIdx, int pos);

    void addFact(const USignature& fact);
    void addQFact(const USignature& qfact);
    void addTrueFact(const USignature& fact);
    void addFalseFact(const USignature& fact);
    void addDefinitiveFact(const Signature& fact);

    void addFactSupport(const Signature& fact, const USignature& operation);
    void touchFactSupport(const Signature& fact);
    void addQConstantTypeConstraint(const USignature& op, const TypeConstraint& c);
    void addForbiddenSubstitution(const USignature& op, const std::vector<int>& src, const std::vector<int>& dest);

    void addAction(const USignature& action);
    void addReduction(const USignature& reduction);
    void addExpansion(const USignature& parent, const USignature& child);
    void addAxiomaticOp(const USignature& op);
    void addExpansionSize(int size);
    void setFactChanges(const USignature& op, const SigSet& factChanges);
    const SigSet& getFactChanges(const USignature& op) const;
    bool hasFactChanges(const USignature& op) const;
    void moveFactChanges(Position& dest, const USignature& op);

    void setMirroredFacts(Position& left);

    void removeActionOccurrence(const USignature& action);
    void removeReductionOccurrence(const USignature& reduction);

    int encode(const USignature& sig);
    void setVariableReference(const USignature& sig, int priorPos);
    bool hasVariable(const USignature& sig) const;
    int getVariableOrReference(const USignature& sig) const;
    bool isVariableOriginallyEncoded(const USignature& sig) const;

    bool hasFact(const USignature& fact) const;
    bool hasQFact(const USignature& fact) const;
    bool hasAction(const USignature& action) const;
    bool hasReduction(const USignature& red) const;

    int getLayerIndex() const;
    int getPositionIndex() const;

    const USigSet& getFacts() const;
    const NodeHashMap<int, USigSet>& getQFacts() const;
    const USigSet& getQFacts(int predId) const;
    int getNumQFacts() const;
    const USigSet& getTrueFacts() const;
    const USigSet& getFalseFacts() const;
    const NodeHashMap<USignature, USigSet, USignatureHasher>& getPosFactSupports() const;
    const NodeHashMap<USignature, USigSet, USignatureHasher>& getNegFactSupports() const;
    const NodeHashMap<USignature, std::vector<TypeConstraint>, USignatureHasher>& getQConstantsTypeConstraints() const;
    const NodeHashMap<USignature, NodeHashSet<Substitution, Substitution::Hasher>, USignatureHasher>& 
    getForbiddenSubstitutions() const;

    const NodeHashMap<USignature, int, USignatureHasher>& getActions() const;
    const NodeHashMap<USignature, int, USignatureHasher>& getReductions() const;
    const NodeHashMap<USignature, USigSet, USignatureHasher>& getExpansions() const;
    const NodeHashMap<USignature, USigSet, USignatureHasher>& getPredecessors() const;
    const USigSet& getAxiomaticOps() const;
    int getMaxExpansionSize() const;

    void clearAtPastPosition();
    void clearAtPastLayer();
};


#endif