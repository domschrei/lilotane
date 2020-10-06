
#ifndef DOMPASCH_LILOTANE_POSITION_H
#define DOMPASCH_LILOTANE_POSITION_H

#include <vector>
#include <set>

#include "data/hashmap.h"
#include "data/signature.h"
#include "util/names.h"
#include "sat/variable_domain.h"
#include "util/log.h"

typedef std::pair<int, int> IntPair;
typedef NodeHashMap<USignature, std::vector<Substitution>, USignatureHasher> IndirectFactSupportMapEntry;
typedef NodeHashMap<USignature, IndirectFactSupportMapEntry, USignatureHasher> IndirectFactSupportMap;

enum VarType { FACT, OP };

struct Position {

public:
    const static USignature NONE_SIG;
    const static SigSet EMPTY_SIG_SET;
    const static USigSet EMPTY_USIG_SET;
    static NodeHashMap<USignature, USigSet, USignatureHasher> EMPTY_USIG_TO_USIG_SET_MAP;
    static IndirectFactSupportMap EMPTY_INDIRECT_FACT_SUPPORT_MAP;

private:
    size_t _layer_idx;
    size_t _pos;

    USigSet _actions;
    USigSet _reductions;

    NodeHashMap<USignature, USigSet, USignatureHasher> _expansions;
    NodeHashMap<USignature, USigSet, USignatureHasher> _predecessors;

    USigSet _axiomatic_ops;

    // All VIRTUAL facts potentially occurring at this position.
    USigSet _qfacts;
    // Maps a q-fact to the set of possibly valid decoded facts.
    NodeHashMap<USignature, USigSet, USignatureHasher> _qfact_decodings;

    // All facts that are definitely true at this position.
    USigSet _true_facts;
    // All facts that are definitely false at this position.
    USigSet _false_facts;

    NodeHashMap<USignature, USigSet, USignatureHasher>* _pos_fact_supports = nullptr;
    NodeHashMap<USignature, USigSet, USignatureHasher>* _neg_fact_supports = nullptr;
    IndirectFactSupportMap* _pos_indir_fact_supports = nullptr;
    IndirectFactSupportMap* _neg_indir_fact_supports = nullptr;

    NodeHashMap<USignature, std::vector<TypeConstraint>, USignatureHasher> _q_constants_type_constraints;

    NodeHashMap<USignature, NodeHashSet<Substitution, Substitution::Hasher>, USignatureHasher> _forbidden_substitutions_per_op;
    NodeHashMap<USignature, std::vector<NodeHashSet<Substitution, Substitution::Hasher>>, USignatureHasher> _valid_substitutions_per_op;

    size_t _max_expansion_size = 1;

    // Prop. variable for each occurring signature.
    NodeHashMap<USignature, int, USignatureHasher> _op_variables;
    NodeHashMap<USignature, int, USignatureHasher> _fact_variables;

    bool _has_primitive_ops = false;
    bool _has_nonprimitive_ops = false;

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
    void addIndirectFactSupport(const Signature& fact, const USignature& op, Substitution&& sub);
    void addIndirectFactSupport(const USignature& fact, bool negated, const USignature& op, Substitution&& sub);
    void setHasPrimitiveOps(bool has);
    void setHasNonprimitiveOps(bool has);
    bool hasPrimitiveOps();
    bool hasNonprimitiveOps();

    void addQConstantTypeConstraint(const USignature& op, const TypeConstraint& c);

    void addForbiddenSubstitution(const USignature& op, Substitution& s);
    void addValidSubstitutions(const USignature& op, NodeHashSet<Substitution, Substitution::Hasher>& subs);
    void clearForbiddenSubstitutions(const USignature& op);
    void clearValidSubstitutions(const USignature& op);

    bool hasQFactDecodings(const USignature& qFact);
    void addQFactDecoding(const USignature& qFact, const USignature& decFact);
    void removeQFactDecoding(const USignature& qFact, const USignature& decFact);
    const USigSet& getQFactDecodings(const USignature& qfact);

    void addAction(const USignature& action);
    void addAction(USignature&& action);
    void addReduction(const USignature& reduction);
    void addExpansion(const USignature& parent, const USignature& child);
    void addAxiomaticOp(const USignature& op);
    void addExpansionSize(size_t size);
    
    void removeActionOccurrence(const USignature& action);
    void removeReductionOccurrence(const USignature& reduction);

    const NodeHashMap<USignature, int, USignatureHasher>& getVariableTable(VarType type) const;
    void setVariableTable(VarType type, const NodeHashMap<USignature, int, USignatureHasher>& table);
    void moveVariableTable(VarType type, Position& destination);

    bool hasQFact(const USignature& fact) const;
    bool hasAction(const USignature& action) const;
    bool hasReduction(const USignature& red) const;

    const USigSet& getQFacts() const;
    int getNumQFacts() const;
    const USigSet& getTrueFacts() const;
    const USigSet& getFalseFacts() const;
    NodeHashMap<USignature, USigSet, USignatureHasher>& getPosFactSupports();
    NodeHashMap<USignature, USigSet, USignatureHasher>& getNegFactSupports();
    IndirectFactSupportMap& getPosIndirectFactSupports();
    IndirectFactSupportMap& getNegIndirectFactSupports();
    const NodeHashMap<USignature, std::vector<TypeConstraint>, USignatureHasher>& getQConstantsTypeConstraints() const;
    const NodeHashMap<USignature, NodeHashSet<Substitution, Substitution::Hasher>, USignatureHasher>& 
    getForbiddenSubstitutions() const;
    const NodeHashMap<USignature, std::vector<NodeHashSet<Substitution, Substitution::Hasher>>, USignatureHasher>& 
    getValidSubstitutions() const;

    USigSet& getActions();
    const USigSet& getReductions() const;
    const NodeHashMap<USignature, USigSet, USignatureHasher>& getExpansions() const;
    const NodeHashMap<USignature, USigSet, USignatureHasher>& getPredecessors() const;
    const USigSet& getAxiomaticOps() const;
    size_t getMaxExpansionSize() const;

    size_t getLayerIndex() const;
    size_t getPositionIndex() const;
    
    void clearAfterInstantiation();
    void clearAtPastPosition();
    void clearAtPastLayer();
    inline void clearSubstitutions() {
        _forbidden_substitutions_per_op.clear();
        _forbidden_substitutions_per_op.reserve(0);
        _valid_substitutions_per_op.clear();
        _valid_substitutions_per_op.reserve(0);
    }

    inline int encode(VarType type, const USignature& sig) {
        auto& vars = type == OP ? _op_variables : _fact_variables;
        auto it = vars.find(sig);
        if (it == vars.end()) {
            // introduce a new variable
            assert(!VariableDomain::isLocked() || Log::e("Unknown variable %s queried!\n", VariableDomain::varName(_layer_idx, _pos, sig).c_str()));
            int var = VariableDomain::nextVar();
            vars[sig] = var;
            VariableDomain::printVar(var, _layer_idx, _pos, sig);
            return var;
        } else return it->second;
    }

    inline int setVariable(VarType type, const USignature& sig, int var) {
        auto& vars = type == OP ? _op_variables : _fact_variables;
        assert(!vars.count(sig));
        vars[sig] = var;
        return var;
    }

    inline bool hasVariable(VarType type, const USignature& sig) const {
        return (type == OP ? _op_variables : _fact_variables).count(sig);
    }

    inline int getVariable(VarType type, const USignature& sig) const {
        auto& vars = type == OP ? _op_variables : _fact_variables;
        assert(vars.count(sig) || Log::e("Unknown variable %s queried!\n", VariableDomain::varName(_layer_idx, _pos, sig).c_str()));
        return vars.at(sig);
    }

    inline int getVariableOrZero(VarType type, const USignature& sig) const {
        auto& vars = type == OP ? _op_variables : _fact_variables;
        const auto& it = vars.find(sig);
        if (it == vars.end()) return 0;
        return it->second;
    }

    inline void removeVariable(VarType type, const USignature& sig) {
        auto& vars = type == OP ? _op_variables : _fact_variables;
        vars.erase(sig);
    }
};


#endif