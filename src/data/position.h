
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
    const static NodeHashSet<Substitution, Substitution::Hasher> EMPTY_SUBST_SET;
    const static std::vector<NodeHashSet<Substitution, Substitution::Hasher>> EMPTY_SUBST_SET_VEC;

private:
    int _layer_idx;
    int _pos;

    NodeHashMap<USignature, int, USignatureHasher> _fact_ids;
    NodeHashMap<USignature, int, USignatureHasher> _op_ids;

    std::vector<bool> _primitive_ops;

    std::vector<std::vector<USigSet>> _expansions;
    std::vector<USigSet> _predecessors;
    std::vector<SigSet> _fact_changes;

    // All VIRTUAL facts potentially occurring at this position,
    // partitioned by their predicate name ID.
    NodeHashMap<int, USigSet> _qfacts;

    // All facts that are definitely true at this position.
    USigSet _true_facts;
    // All facts that are definitely false at this position.
    USigSet _false_facts;

    std::vector<USigSet> _pos_fact_supports;
    std::vector<USigSet> _neg_fact_supports;

    NodeHashMap<USignature, std::vector<TypeConstraint>, USignatureHasher> _q_constants_type_constraints;

    std::vector<NodeHashSet<Substitution, Substitution::Hasher>> _forbidden_substitutions_per_op;
    std::vector<std::vector<NodeHashSet<Substitution, Substitution::Hasher>>> _valid_substitutions_per_op;

    int _max_expansion_size = 1;

    std::vector<int> _fact_variables;
    std::vector<int> _op_variables;
    int _primitive_variable = 0;

public:

    Position();
    void setPos(int layerIdx, int pos);

    void addQFact(const USignature& qfact);
    void addTrueFact(const USignature& fact);
    void addFalseFact(const USignature& fact);
    void addDefinitiveFact(const Signature& fact);

    void addFactSupport(const Signature& fact, const USignature& operation);
    void touchFactSupport(const Signature& fact);
    void addQConstantTypeConstraint(const USignature& op, const TypeConstraint& c);

    void addForbiddenSubstitution(const USignature& op, const Substitution& s);
    void addValidSubstitutions(const USignature& op, const NodeHashSet<Substitution, Substitution::Hasher>& subs);

    void addAction(const USignature& action);
    void addReduction(const USignature& reduction);
    void addExpansion(const USignature& parent, int offset, const USignature& child);
    void addPredecessor(const USignature& parent, const USignature& child);
    void addExpansionSize(int size);
    void setFactChanges(const USignature& op, const SigSet& factChanges);
    const SigSet& getFactChanges(const USignature& op) const;
    void moveFactChanges(Position& dest, const USignature& op);

    int encodeFact(const USignature& sig);
    int encodeOp(const USignature& sig);
    int encodeFact(int factId);
    int encodeOp(int opId);
    int encodePrimitiveness();
    
    int setFactVariable(const USignature& sig, int var);
    int setFactVariable(int factId, int var);
    bool hasFactVariable(const USignature& sig) const;
    bool hasFactVariable(int factId) const;
    int getFactVariable(const USignature& sig) const;
    int getFactVariableOrZero(const USignature& sig) const;
    int getFactVariable(int factId) const;
    int getFactVariableOrZero(int factId) const;
    
    int setOpVariable(const USignature& sig, int var);
    bool hasOpVariable(const USignature& sig) const;
    int getOpVariable(const USignature& sig) const;
    int getOpVariableOrZero(const USignature& sig) const;
    int getOpVariable(int opId) const;
    int getOpVariableOrZero(int opId) const;
    
    bool hasQFact(const USignature& fact) const;
    bool hasOp(const USignature& op) const;
    bool isPrimitive(int opId) const;

    int getLayerIndex() const;
    int getPositionIndex() const;

    const NodeHashMap<int, USigSet>& getQFacts() const;
    const USigSet& getQFacts(int predId) const;
    int getNumQFacts() const;
    const USigSet& getTrueFacts() const;
    const USigSet& getFalseFacts() const;
    const USigSet& getPosFactSupport(const USignature& fact) const;
    const USigSet& getNegFactSupport(const USignature& fact) const;
    const USigSet& getFactSupport(const Signature& fact) const;
    const USigSet& getFactSupport(const USignature& fact, bool negated) const;
    const NodeHashMap<USignature, std::vector<TypeConstraint>, USignatureHasher>& getQConstantsTypeConstraints() const;
    const NodeHashSet<Substitution, Substitution::Hasher>& 
    getForbiddenSubstitutions(const USignature& op) const;
    const std::vector<NodeHashSet<Substitution, Substitution::Hasher>>& 
    getValidSubstitutions(const USignature& op) const;

    const NodeHashMap<USignature, int, USignatureHasher>& getFacts() const;
    const NodeHashMap<USignature, int, USignatureHasher>& getOps() const;

    const USigSet& getExpansions(const USignature& sig, int offset) const;
    const USigSet& getPredecessors(const USignature& sig) const;
    int getMaxExpansionSize() const;

    void clearAtPastPosition();
    void clearAtPastLayer();

    int getFactIdOrNegative(const USignature& sig) const;
    int factId(const USignature& sig);

private:
    int getFactId(const USignature& sig) const;
    int getOpId(const USignature& sig) const;
    int opId(const USignature& sig);
    int anyId(const USignature& sig, NodeHashMap<USignature, int, USignatureHasher>& map);
    int anyIdOrNegative(const USignature& sig, const NodeHashMap<USignature, int, USignatureHasher>& map) const;

    int encode(const USignature& sig, int id, std::vector<int>& vars);
    int setVariable(int id, int var, std::vector<int>& vars);
    int getVariableOrZero(int id, const std::vector<int>& vars) const;

    template<typename T>
    static void resizeIfNeeded(std::vector<T>& vec, int id) {
        if (id >= vec.size()) {
            vec.resize(2*id+1);
        }
    }

};


#endif