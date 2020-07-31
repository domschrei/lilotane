
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
    auto& suppVec = (fact._negated ? _neg_fact_supports : _pos_fact_supports);
    int id = factId(fact._usig);
    resizeIfNeeded(suppVec, id);
    suppVec[id].insert(operation);
}
void Position::touchFactSupport(const Signature& fact) {
    auto& suppVec = (fact._negated ? _neg_fact_supports : _pos_fact_supports);
    int id = factId(fact._usig);
    resizeIfNeeded(suppVec, id);
    if (suppVec[id].empty()) suppVec[id].insert(NONE_SIG); 
}
void Position::addQConstantTypeConstraint(const USignature& op, const TypeConstraint& c) {
    auto& vec = _q_constants_type_constraints[op];
    vec.push_back(c);
}
void Position::addForbiddenSubstitution(const USignature& op, const Substitution& s) {
    auto& vec = _forbidden_substitutions_per_op;
    int id = opId(op);
    resizeIfNeeded(vec, id);
    vec[id].insert(s);
}
void Position::addValidSubstitutions(const USignature& op, const NodeHashSet<Substitution, Substitution::Hasher>& subs) {
    auto& vec = _valid_substitutions_per_op;
    int id = opId(op);
    resizeIfNeeded(vec, id);
    vec[id].push_back(subs);
}

void Position::addAction(const USignature& action) {
    int id = opId(action);
    resizeIfNeeded(_primitive_ops, id);
    _primitive_ops[id] = true;
    Log::d("+ACTION@(%i,%i) %s\n", _layer_idx, _pos, TOSTR(action));
}
void Position::addReduction(const USignature& reduction) {
    int id = opId(reduction);
    resizeIfNeeded(_primitive_ops, id);
    _primitive_ops[id] = false;
    Log::d("+REDUCTION@(%i,%i) %s\n", _layer_idx, _pos, TOSTR(reduction));
}
void Position::addExpansion(const USignature& parent, int offset, const USignature& child) {
    int parentId = opId(parent);
    resizeIfNeeded(_expansions, parentId);
    resizeIfNeeded(_expansions[parentId], offset);
    _expansions[parentId][offset].insert(child);
}
void Position::addPredecessor(const USignature& parent, const USignature& child) {
    int childId = opId(child);
    resizeIfNeeded(_predecessors, childId);
    _predecessors[childId].insert(parent);
}
void Position::addExpansionSize(int size) {_max_expansion_size = std::max(_max_expansion_size, size);}
void Position::setFactChanges(const USignature& op, const SigSet& factChanges) {
    int id = opId(op);
    resizeIfNeeded(_fact_changes, id);
    _fact_changes[id] = factChanges;
}
const SigSet& Position::getFactChanges(const USignature& op) const {
    int id = getOpId(op);
    return id < _fact_changes.size() ? _fact_changes.at(id) : EMPTY_SIG_SET;
}
void Position::moveFactChanges(Position& dest, const USignature& op) {
    int id = getOpId(op);
    int destId = dest.opId(op);
    if (id < _fact_changes.size()) {
        resizeIfNeeded(dest._fact_changes, destId);
        dest._fact_changes[destId] = std::move(_fact_changes[id]);
        _fact_changes[id].clear();
        _fact_changes[id].reserve(0);
    }
}

int Position::encodeFact(const USignature& sig) {
    return encode(sig, factId(sig), _fact_variables);
}
int Position::encodeOp(const USignature& sig) {
    return encode(sig, opId(sig), _op_variables);
}
int Position::encodeFact(int factId) {
    return encode(NONE_SIG, factId, _fact_variables);
}
int Position::encodeOp(int opId) {
    return encode(NONE_SIG, opId, _op_variables);
}

int Position::encodePrimitiveness() {
    if (_primitive_variable == 0) {
        _primitive_variable = VariableDomain::nextVar();
    }
    return _primitive_variable;
}

int Position::encode(const USignature& sig, int id, std::vector<int>& vars) {
    resizeIfNeeded(vars, id);
    if (vars[id] == 0) {
        // introduce a new variable
        assert(!VariableDomain::isLocked() || Log::e("Unknown variable %s queried!\n", VariableDomain::varName(_layer_idx, _pos, sig).c_str()));
        vars[id] = VariableDomain::nextVar();
        VariableDomain::printVar(vars[id], _layer_idx, _pos, sig);
    }
    return vars[id];
}

int Position::setFactVariable(const USignature& sig, int var) {
    return setVariable(factId(sig), var, _fact_variables);
}
int Position::setFactVariable(int factId, int var) {
    return setVariable(factId, var, _fact_variables);
}
int Position::setOpVariable(const USignature& sig, int var) {
    return setVariable(opId(sig), var, _op_variables);
}

int Position::setVariable(int id, int var, std::vector<int>& vars) {
    resizeIfNeeded(vars, id);
    assert(vars[id] == 0);
    vars[id] = var;
    return var;
}

bool Position::hasFactVariable(const USignature& sig) const {
    int id = anyIdOrNegative(sig, _fact_ids);
    return id >= 0 && id < _fact_variables.size() && _fact_variables[id] != 0;
}
bool Position::hasFactVariable(int factId) const {
    return factId >= 0 && factId < _fact_variables.size() && _fact_variables[factId] != 0;
}
bool Position::hasOpVariable(const USignature& sig) const {
    int id = anyIdOrNegative(sig, _op_ids);
    return id >= 0 && id < _op_variables.size() && _op_variables[id] != 0;
}

int Position::getVariableOrZero(int id, const std::vector<int>& vars) const {
    if (id >= vars.size()) return 0;
    return vars[id];
}

int Position::getFactVariable(const USignature& sig) const {
    int var = getVariableOrZero(getFactId(sig), _fact_variables);
    assert(var != 0);
    return var;
}
int Position::getFactVariable(int factId) const {
    int var = getVariableOrZero(factId, _fact_variables);
    assert(var != 0);
    return var;
}
int Position::getOpVariable(const USignature& sig) const {
    int var = getVariableOrZero(getOpId(sig), _op_variables);
    assert(var != 0);
    return var;
}
int Position::getOpVariable(int opId) const {
    int var = getVariableOrZero(opId, _op_variables);
    assert(var != 0);
    return var;
}

int Position::getFactVariableOrZero(const USignature& sig) const {
    int id = anyIdOrNegative(sig, _fact_ids);
    if (id < 0) return 0;
    return getVariableOrZero(id, _fact_variables);
}
int Position::getFactVariableOrZero(int factId) const {
    return getVariableOrZero(factId, _fact_variables);
}
int Position::getOpVariableOrZero(const USignature& sig) const {
    int id = anyIdOrNegative(sig, _op_ids);
    if (id < 0) return 0;
    return getVariableOrZero(id, _op_variables);
}
int Position::getOpVariableOrZero(int opId) const {
    return getVariableOrZero(opId, _op_variables);
}

bool Position::hasQFact(const USignature& fact) const {return _qfacts.count(fact._name_id) && _qfacts.at(fact._name_id).count(fact);}
bool Position::hasOp(const USignature& op) const {
    return anyIdOrNegative(op, _op_ids) >= 0;
}
bool Position::isPrimitive(int opId) const {
    return _primitive_ops[opId];
}

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
const USigSet& Position::getPosFactSupport(const USignature& fact) const {
    int id = anyIdOrNegative(fact, _fact_ids);
    if (id < 0 || id >= _pos_fact_supports.size()) return EMPTY_USIG_SET;
    return _pos_fact_supports[id];
}
const USigSet& Position::getNegFactSupport(const USignature& fact) const {
    int id = anyIdOrNegative(fact, _fact_ids);
    if (id < 0 || id >= _neg_fact_supports.size()) return EMPTY_USIG_SET;
    return _neg_fact_supports[id];
}
const USigSet& Position::getFactSupport(const Signature& fact) const {
    return fact._negated ? getNegFactSupport(fact._usig) : getPosFactSupport(fact._usig);
}

const USigSet& Position::getFactSupport(const USignature& fact, bool negated) const {
    return negated ? getNegFactSupport(fact) : getPosFactSupport(fact);
}

const NodeHashMap<USignature, std::vector<TypeConstraint>, USignatureHasher>& Position::getQConstantsTypeConstraints() const {
    return _q_constants_type_constraints;
}
const NodeHashSet<Substitution, Substitution::Hasher>& 
Position::getForbiddenSubstitutions(const USignature& op) const {
    int id = anyIdOrNegative(op, _op_ids);
    if (id < 0 || id >= _forbidden_substitutions_per_op.size()) return EMPTY_SUBST_SET; 
    return _forbidden_substitutions_per_op[id];
}
const std::vector<NodeHashSet<Substitution, Substitution::Hasher>>& 
Position::getValidSubstitutions(const USignature& op) const {
    int id = anyIdOrNegative(op, _op_ids);
    if (id < 0 || id >= _valid_substitutions_per_op.size()) return EMPTY_SUBST_SET_VEC; 
    return _valid_substitutions_per_op[id];
}

const NodeHashMap<USignature, int, USignatureHasher>& Position::getFacts() const {
    return _fact_ids;
}
const NodeHashMap<USignature, int, USignatureHasher>& Position::getOps() const {
    return _op_ids;
}

const USigSet& Position::getExpansions(const USignature& sig, int offset) const {
    int id = anyIdOrNegative(sig, _op_ids);
    if (id < 0 || id >= _expansions.size() || offset >= _expansions[id].size()) return EMPTY_USIG_SET; 
    return _expansions[id][offset];
}
const USigSet& Position::getPredecessors(const USignature& sig) const {
    int id = anyIdOrNegative(sig, _op_ids);
    if (id < 0 || id >= _predecessors.size()) return EMPTY_USIG_SET; 
    return _predecessors[id];
}
int Position::getMaxExpansionSize() const {return _max_expansion_size;}

void Position::clearAtPastLayer() {
    _fact_changes.resize(0);
    _true_facts.clear();
    _true_facts.reserve(0);
    _false_facts.clear();
    _false_facts.reserve(0);
    _expansions.resize(0);
    _fact_variables.resize(0);

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
    _predecessors.resize(0);
    _pos_fact_supports.resize(0);
    _neg_fact_supports.resize(0);
    _q_constants_type_constraints.clear();
    _q_constants_type_constraints.reserve(0);
    _forbidden_substitutions_per_op.resize(0);
    _valid_substitutions_per_op.resize(0);
}

int Position::anyId(const USignature& sig, NodeHashMap<USignature, int, USignatureHasher>& map) {
    auto it = map.find(sig);
    if (it == map.end()) {
        int id = map.size();
        map[sig] = id;
        return id;
    } else return it->second;
}

int Position::anyIdOrNegative(const USignature& sig, const NodeHashMap<USignature, int, USignatureHasher>& map) const {
    auto it = map.find(sig);
    if (it == map.end()) return -1;
    return it->second;
}

int Position::factId(const USignature& sig) {
    return anyId(sig, _fact_ids);
}
int Position::opId(const USignature& sig) {
    return anyId(sig, _op_ids);
}

int Position::getFactId(const USignature& sig) const {
    return _fact_ids.at(sig);
}
int Position::getFactIdOrNegative(const USignature& sig) const {
    return anyIdOrNegative(sig, _fact_ids);
}
int Position::getOpId(const USignature& sig) const {
    return _op_ids.at(sig);
}
