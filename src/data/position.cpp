
#include "position.h"

#include "sat/variable_domain.h"
#include "util/log.h"

Position::Position() : _layer_idx(-1), _pos(-1) {}
void Position::setPos(size_t layerIdx, size_t pos) {_layer_idx = layerIdx; _pos = pos;}

void Position::addQFact(const USignature& qfact) {
    _qfacts.insert(qfact);
}
void Position::addTrueFact(const USignature& fact) {_true_facts.insert(fact);}
void Position::addFalseFact(const USignature& fact) {_false_facts.insert(fact);}
void Position::addDefinitiveFact(const Signature& fact) {(fact._negated ? _false_facts : _true_facts).insert(fact._usig);}

void Position::addFactSupport(const Signature& fact, const USignature& operation) {
    auto& supp = fact._negated ? _neg_fact_supports : _pos_fact_supports;
    if (supp == nullptr) supp = new NodeHashMap<USignature, USigSet, USignatureHasher>();
    auto& set = (*supp)[fact._usig];
    set.insert(operation);
}
void Position::touchFactSupport(const Signature& fact) {
    auto& supp = fact._negated ? _neg_fact_supports : _pos_fact_supports;
    if (supp == nullptr) supp = new NodeHashMap<USignature, USigSet, USignatureHasher>();
    (*supp)[fact._usig];
}
void Position::touchFactSupport(const USignature& fact, bool negated) {
    auto& supp = negated ? _neg_fact_supports : _pos_fact_supports;
    if (supp == nullptr) supp = new NodeHashMap<USignature, USigSet, USignatureHasher>();
    (*supp)[fact];
}
void Position::addIndirectFactSupport(const Signature& fact, const USignature& op, Substitution&& sub) {
    addIndirectFactSupport(fact._usig, fact._negated, op, std::move(sub));
}
void Position::addIndirectFactSupport(const USignature& fact, bool negated, const USignature& op, Substitution&& sub) {
    auto& supp = negated ? _neg_indir_fact_supports : _pos_indir_fact_supports;
    if (supp == nullptr) supp = new IndirectFactSupportMap();
    auto& set = (*supp)[fact];
    set[op].push_back(std::move(sub));
}
void Position::setHasPrimitiveOps(bool has) {
    _has_primitive_ops = has;
}
void Position::setHasNonprimitiveOps(bool has) {
    _has_nonprimitive_ops = has;
}
bool Position::hasPrimitiveOps() {
    return _has_primitive_ops;
}
bool Position::hasNonprimitiveOps() {
    return _has_nonprimitive_ops;
}

void Position::addQConstantTypeConstraint(const USignature& op, const TypeConstraint& c) {
    auto& vec = _q_constants_type_constraints[op];
    vec.push_back(c);
}

void Position::addForbiddenSubstitution(const USignature& op, Substitution& s) {
    auto& set = _forbidden_substitutions_per_op[op];
    set.insert(std::move(s));
}
void Position::addValidSubstitutions(const USignature& op, NodeHashSet<Substitution, Substitution::Hasher>& subs) {
    auto& set = _valid_substitutions_per_op[op];
    set.push_back(std::move(subs));
}

void Position::addQFactDecoding(const USignature& qFact, const USignature& decFact) {
    _qfact_decodings[qFact];
    _qfact_decodings[qFact].insert(decFact);
}

void Position::removeQFactDecoding(const USignature& qFact, const USignature& decFact) {
    _qfact_decodings[qFact].erase(decFact);
}

bool Position::hasQFactDecodings(const USignature& qFact) {
    return _qfact_decodings.count(qFact);
}

const USigSet& Position::getQFactDecodings(const USignature& qFact) {
    assert(_qfact_decodings.count(qFact) || Log::e("No qfact decodings for %s!\n", TOSTR(qFact)));
    return _qfact_decodings.at(qFact);
}

void Position::addAction(const USignature& action) {
    _actions.insert(action);
    Log::d("+ACTION@(%i,%i) %s\n", _layer_idx, _pos, TOSTR(action));
}
void Position::addAction(USignature&& action) {
    Log::d("+ACTION@(%i,%i) %s\n", _layer_idx, _pos, TOSTR(action));
    _actions.insert(std::move(action));
}
void Position::addReduction(const USignature& reduction) {
    _reductions.insert(reduction);
    Log::d("+REDUCTION@(%i,%i) %s\n", _layer_idx, _pos, TOSTR(reduction));
}
void Position::addExpansion(const USignature& parent, const USignature& child) {
    auto& set = _expansions[parent];
    set.insert(child);
    auto& pred = _predecessors[child];
    pred.insert(parent);
}
void Position::addAxiomaticOp(const USignature& op) {
    _axiomatic_ops.insert(op);
}
void Position::addExpansionSize(size_t size) {_max_expansion_size = std::max(_max_expansion_size, size);}
void Position::setFactChanges(const USignature& op, const SigSet& factChanges) {
    _fact_changes[op] = factChanges;
}
const SigSet& Position::getFactChanges(const USignature& op) const {
    return _fact_changes.count(op) ? _fact_changes.at(op) : EMPTY_SIG_SET;
}
bool Position::hasFactChanges(const USignature& op) const {
    return _fact_changes.count(op);
}
void Position::moveFactChanges(Position& dest, const USignature& op) {
    dest._fact_changes[op] = std::move(_fact_changes[op]);
    _fact_changes.erase(op);
}

void Position::removeActionOccurrence(const USignature& action) {
    assert(_actions.count(action));
    _actions.erase(action);
    for (auto& [parent, children] : _expansions) {
        children.erase(action);
    }
    _predecessors.erase(action);
}
void Position::removeReductionOccurrence(const USignature& reduction) {
    assert(_reductions.count(reduction));
    _reductions.erase(reduction);
}

const NodeHashMap<USignature, int, USignatureHasher>& Position::getVariableTable(VarType type) const {
    return type == OP ? _op_variables : _fact_variables;
}
void Position::setVariableTable(VarType type, const NodeHashMap<USignature, int, USignatureHasher>& table) {
    if (type == OP) {
        _op_variables = table;
    } else {
        _fact_variables = table;
    }
}
void Position::moveVariableTable(VarType type, Position& destination) {
    auto& src = type == OP ? _op_variables : _fact_variables;
    auto& dest = type == OP ? destination._op_variables : destination._fact_variables;
    dest = std::move(src);
    src.clear();
    src.reserve(0);
}

bool Position::hasQFact(const USignature& fact) const {return _qfacts.count(fact);}
bool Position::hasAction(const USignature& action) const {return _actions.count(action);}
bool Position::hasReduction(const USignature& red) const {return _reductions.count(red);}

size_t Position::getLayerIndex() const {return _layer_idx;}
size_t Position::getPositionIndex() const {return _pos;}

const USigSet& Position::getQFacts() const {return _qfacts;}
const USigSet& Position::getTrueFacts() const {return _true_facts;}
const USigSet& Position::getFalseFacts() const {return _false_facts;}
NodeHashMap<USignature, USigSet, USignatureHasher>& Position::getPosFactSupports() {
    if (_pos_fact_supports == nullptr) return EMPTY_USIG_TO_USIG_SET_MAP;
    return *_pos_fact_supports;
}
NodeHashMap<USignature, USigSet, USignatureHasher>& Position::getNegFactSupports() {
    if (_neg_fact_supports == nullptr) return EMPTY_USIG_TO_USIG_SET_MAP;
    return *_neg_fact_supports;
}
IndirectFactSupportMap& Position::getPosIndirectFactSupports() {
    if (_pos_indir_fact_supports == nullptr) return EMPTY_INDIRECT_FACT_SUPPORT_MAP;
    return *_pos_indir_fact_supports;
}
IndirectFactSupportMap& Position::getNegIndirectFactSupports() {
    if (_neg_indir_fact_supports == nullptr) return EMPTY_INDIRECT_FACT_SUPPORT_MAP;
    return *_neg_indir_fact_supports;
}
const NodeHashMap<USignature, std::vector<TypeConstraint>, USignatureHasher>& Position::getQConstantsTypeConstraints() const {
    return _q_constants_type_constraints;
}
const NodeHashMap<USignature, NodeHashSet<Substitution, Substitution::Hasher>, USignatureHasher>& 
Position::getForbiddenSubstitutions() const {
    return _forbidden_substitutions_per_op;
}
const NodeHashMap<USignature, std::vector<NodeHashSet<Substitution, Substitution::Hasher>>, USignatureHasher>& 
Position::getValidSubstitutions() const {
    return _valid_substitutions_per_op;
}

USigSet& Position::getActions() {return _actions;}
const USigSet& Position::getReductions() const {return _reductions;}
const NodeHashMap<USignature, USigSet, USignatureHasher>& Position::getExpansions() const {return _expansions;}
const NodeHashMap<USignature, USigSet, USignatureHasher>& Position::getPredecessors() const {return _predecessors;}
const USigSet& Position::getAxiomaticOps() const {return _axiomatic_ops;}
size_t Position::getMaxExpansionSize() const {return _max_expansion_size;}

void Position::clearAtPastPosition() {
    _qfacts.clear();
    _qfacts.reserve(0);
    _expansions.clear();
    _expansions.reserve(0);
    _predecessors.clear();
    _predecessors.reserve(0);
    _axiomatic_ops.clear();
    _axiomatic_ops.reserve(0);
    _q_constants_type_constraints.clear();
    _q_constants_type_constraints.reserve(0);
    _forbidden_substitutions_per_op.clear();
    _forbidden_substitutions_per_op.reserve(0);
    _valid_substitutions_per_op.clear();
    _valid_substitutions_per_op.reserve(0);
    _qfact_decodings.clear();
    _qfact_decodings.reserve(0);

    for (const auto& r : _reductions) _fact_changes.erase(r);
    _fact_changes.reserve(0);
}

void Position::clearAtPastLayer() {
    if (_pos_fact_supports != nullptr) delete _pos_fact_supports;
    if (_neg_fact_supports != nullptr) delete _neg_fact_supports;
    if (_pos_indir_fact_supports != nullptr) delete _pos_indir_fact_supports;
    if (_neg_indir_fact_supports != nullptr) delete _neg_indir_fact_supports;
    _fact_changes.clear();
    _fact_changes.reserve(0);
    _true_facts.clear();
    _true_facts.reserve(0);
    _false_facts.clear();
    _false_facts.reserve(0);
    _fact_variables.clear();
    _fact_variables.reserve(0);
    _actions.clear();
    _actions.reserve(0);
    _reductions.clear();
    _reductions.reserve(0);
}