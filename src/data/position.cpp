
#include "position.h"

#include "sat/variable_domain.h"
#include "util/log.h"

Position::Position() : _layer_idx(-1), _pos(-1) {}
void Position::setPos(int layerIdx, int pos) {_layer_idx = layerIdx; _pos = pos;}

void Position::addQFact(const USignature& qfact) {
    auto& set = _qfacts[qfact._name_id];
    set.insert(qfact);
}
void Position::addTrueFact(const USignature& fact) {_true_facts.insert(fact);}
void Position::addFalseFact(const USignature& fact) {_false_facts.insert(fact);}
void Position::addDefinitiveFact(const Signature& fact) {(fact._negated ? _false_facts : _true_facts).insert(fact._usig);}

void Position::addFactSupport(const Signature& fact, const USignature& operation) {
    auto& set = (fact._negated ? _neg_fact_supports : _pos_fact_supports)[fact._usig];
    set.insert(operation);
}
void Position::touchFactSupport(const Signature& fact) {
    (fact._negated ? _neg_fact_supports : _pos_fact_supports)[fact._usig];
}
void Position::addQConstantTypeConstraint(const USignature& op, const TypeConstraint& c) {
    auto& vec = _q_constants_type_constraints[op];
    vec.push_back(c);
}
void Position::addForbiddenSubstitution(const USignature& op, const Substitution& s) {
    auto& set = _forbidden_substitutions_per_op[op];
    set.insert(s);
}
void Position::addValidSubstitutions(const USignature& op, const NodeHashSet<Substitution, Substitution::Hasher>& subs) {
    auto& set = _valid_substitutions_per_op[op];
    set.push_back(subs);
}

void Position::addAction(const USignature& action) {
    auto& entry = _actions[action];
    entry++;
    Log::d("+ACTION@(%i,%i) %s\n", _layer_idx, _pos, TOSTR(action));
}
void Position::addReduction(const USignature& reduction) {
    auto& entry = _reductions[reduction];
    entry++;
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
void Position::addExpansionSize(int size) {_max_expansion_size = std::max(_max_expansion_size, size);}
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
    _actions[action]--;
    if (_actions[action] == 0) {
        _actions.erase(action);
    }
}
void Position::removeReductionOccurrence(const USignature& reduction) {
    assert(_reductions.count(reduction));
    _reductions[reduction]--;
    if (_reductions[reduction] == 0) {
        _reductions.erase(reduction);
    }
}

int Position::encode(const USignature& sig) {
    if (!_variables.count(sig)) {
        // introduce a new variable
        assert(!VariableDomain::isLocked() || Log::e("Unknown variable %s queried!\n", VariableDomain::varName(_layer_idx, _pos, sig).c_str()));
        _variables[sig] = VariableDomain::nextVar();
        VariableDomain::printVar(_variables[sig], _layer_idx, _pos, sig);
    }
    return _variables[sig];
}

int Position::setVariable(const USignature& sig, int var) {
    assert(!_variables.count(sig));
    _variables[sig] = var;
    return var;
}

bool Position::hasVariable(const USignature& sig) const {
    return _variables.count(sig);
}

int Position::getVariable(const USignature& sig) const {
    return _variables.at(sig);
}

int Position::getVariableOrZero(const USignature& sig) const {
    const auto& it = _variables.find(sig);
    if (it == _variables.end()) return 0;
    return it->second;
}

const NodeHashMap<USignature, int, USignatureHasher>& Position::getVariableTable() const {
    return _variables;
}

bool Position::hasQFact(const USignature& fact) const {return _qfacts.count(fact._name_id) && _qfacts.at(fact._name_id).count(fact);}
bool Position::hasAction(const USignature& action) const {return _actions.count(action);}
bool Position::hasReduction(const USignature& red) const {return _reductions.count(red);}

int Position::getLayerIndex() const {return _layer_idx;}
int Position::getPositionIndex() const {return _pos;}

const NodeHashMap<int, USigSet>& Position::getQFacts() const {return _qfacts;}
const USigSet& Position::getQFacts(int predId) const {return _qfacts.count(predId) ? _qfacts.at(predId) : EMPTY_USIG_SET;}
int Position::getNumQFacts() const {
    int sum = 0;
    for (const auto& entry : _qfacts) sum += entry.second.size();
    return sum;
}
const USigSet& Position::getTrueFacts() const {return _true_facts;}
const USigSet& Position::getFalseFacts() const {return _false_facts;}
const NodeHashMap<USignature, USigSet, USignatureHasher>& Position::getPosFactSupports() const {return _pos_fact_supports;}
const NodeHashMap<USignature, USigSet, USignatureHasher>& Position::getNegFactSupports() const {return _neg_fact_supports;}
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

const NodeHashMap<USignature, int, USignatureHasher>& Position::getActions() const {return _actions;}
const NodeHashMap<USignature, int, USignatureHasher>& Position::getReductions() const {return _reductions;}
const NodeHashMap<USignature, USigSet, USignatureHasher>& Position::getExpansions() const {return _expansions;}
const NodeHashMap<USignature, USigSet, USignatureHasher>& Position::getPredecessors() const {return _predecessors;}
const USigSet& Position::getAxiomaticOps() const {return _axiomatic_ops;}
int Position::getMaxExpansionSize() const {return _max_expansion_size;}

void Position::clearAtPastLayer() {
    _fact_changes.clear();
    _fact_changes.reserve(0);
    _true_facts.clear();
    _true_facts.reserve(0);
    _false_facts.clear();
    _false_facts.reserve(0);

    /*
    NodeHashMap<USignature, int, USignatureHasher> cleanedVars;
    for (const auto& r : _reductions) cleanedVars[r.first] = _variables[r.first];
    for (const auto& a : _actions) cleanedVars[a.first] = _variables[a.first];
    _variables = std::move(cleanedVars);
    */
}

void Position::clearAtPastPosition() {
    _qfacts.clear();
    _qfacts.reserve(0);
    _expansions.clear();
    _expansions.reserve(0);
    _predecessors.clear();
    _predecessors.reserve(0);
    _axiomatic_ops.clear();
    _axiomatic_ops.reserve(0);
    _pos_fact_supports.clear();
    _pos_fact_supports.reserve(0);
    _neg_fact_supports.clear();
    _neg_fact_supports.reserve(0);
    _q_constants_type_constraints.clear();
    _q_constants_type_constraints.reserve(0);
    _forbidden_substitutions_per_op.clear();
    _forbidden_substitutions_per_op.reserve(0);
    _valid_substitutions_per_op.clear();
    _valid_substitutions_per_op.reserve(0);
}