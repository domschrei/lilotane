
#include "q_constant_condition.h"
#include "util/names.h"

const PositionedUSig QConstantDatabase::PSIG_ROOT(0, 0, USignature());

QConstantDatabase::QConstantDatabase(const std::function<bool(int)>& isQConstant) : _is_q_constant(isQConstant) {
    _op_ids[PSIG_ROOT] = 0;
    _op_sigs.push_back(PSIG_ROOT);
    _op_possible_parents.emplace_back();
    _op_children_at_offset.emplace_back();
    _conditions_per_op.emplace_back();
}

bool QConstantDatabase::isUniversal(QConstantCondition* cond) {
    for (int qconst : cond->reference) {
        if (getRootOp(qconst) == cond->originOp) return true;
    }
    return false;
}

bool QConstantDatabase::isParentAndChild(int parent, int child) {
    if (parent == child) return false;
    std::vector<int> stack; stack.push_back(child);
    while (!stack.empty()) {
        int node = stack.back(); stack.pop_back();
        if (node != 0) {
            if (node == parent) return true;
            for (int parent : _op_possible_parents[node]) stack.push_back(parent);
        }
    }
    return false;
}

int QConstantDatabase::getRootOp(int qconst) {
    return std::get<0>(_q_const_map[qconst]);
}
const NodeHashSet<QConstantCondition*, QConstCondHasher, QConstCondEquals>& QConstantDatabase::getConditions(int qconst) {
    return std::get<1>(_q_const_map[qconst]);
}

int QConstantDatabase::addOp(const HtnOp& op, int layer, int pos, const PositionedUSig& parent, int offset) {
    
    USignature opSig = op.getSignature();
    const PositionedUSig opPSig{layer, pos, opSig};

    int parentId = _op_ids.count(parent) ? _op_ids[parent] : _op_ids[PSIG_ROOT];
    
    //log("%s\n", TOSTR(opSig));
    if (_op_ids.count(opPSig)) {
        int id = _op_ids[opPSig];
        if (parentId != _op_ids[PSIG_ROOT]) {
            _op_possible_parents[id].push_back(parentId);
            while (offset >= _op_children_at_offset[parentId].size()) {
                _op_children_at_offset[parentId].emplace_back();
            } 
            _op_children_at_offset[parentId][offset].push_back(id);
        }
        return id;
    }
    
    int opId = _op_possible_parents.size();
    _op_ids[opPSig] = opId;

    _op_sigs.push_back(opPSig);
    _op_possible_parents.emplace_back();
    _op_possible_parents.back().push_back(parentId);
    _op_children_at_offset.emplace_back();
    _conditions_per_op.emplace_back();
    assert(_op_sigs.size()               == opId+1);
    assert(_op_possible_parents.size()   == opId+1);
    assert(_op_children_at_offset.size() == opId+1);
    assert(_conditions_per_op.size()     == opId+1);

    while (offset >= _op_children_at_offset[parentId].size()) {
        _op_children_at_offset[parentId].emplace_back();
    } 
    _op_children_at_offset[parentId][offset].push_back(opId);

    // Add op's q constants to map
    for (int arg : op.getArguments()) {
        if (_is_q_constant(arg) && !_q_const_map.count(arg)) {
            _q_const_map[arg];
            std::get<0>(_q_const_map[arg]) = opId;
        }
    }
    
    return opId;
}

QConstantCondition* QConstantDatabase::addCondition(int op, const std::vector<int>& reference, char conjunction, const ValueSet& values) {
    
    QConstantCondition* cond = new QConstantCondition(op, reference, conjunction, values);
    
    // Simplify condition w.r.t. present conditions
    forEachFitCondition(reference, [&](QConstantCondition* subcond) {
        
        // The "sub condition"'s scope must fully contain the cond's scope
        // and must reference a subset of the cond's references.
        if ((isParentAndChild(subcond->originOp, cond->originOp) || cond->originOp == subcond->originOp) 
                && subcond->referencesSubsetOf(cond->reference)) {

            // Find and remove all tuples which evaluate to FALSE in that subset condition
            std::vector<std::vector<int>> tuplesToRemove;
            for (const auto& tuple : cond->values) {
                //log("QCONSTCOND_SIMP is %s:%s simplified by %s ?\n", TOSTR(cond->reference), TOSTR(tuple), TOSTR(subcond->reference));
                if (!subcond->test(cond->reference, tuple)) {
                    //log("QCONSTCOND_SIMP remove %s from %s\n", TOSTR(tuple), TOSTR(cond->reference));
                    tuplesToRemove.push_back(tuple);
                }
            }
            for (const auto& tuple : tuplesToRemove) cond->values.erase(tuple);
        }
        return true;
    });

    // Check if already present
    for (const auto& q : reference) {
        if (_q_const_map.count(q) && getConditions(q).count(cond)) {
            Log::d("QCONSTCOND_DUPLICATE %s@%i,%i %s\n", TOSTR(_op_sigs[cond->originOp].usig), 
                    _op_sigs[cond->originOp].layer, _op_sigs[cond->originOp].pos, cond->toStr().c_str());
            delete cond;
            return nullptr;
        } 
    }

    // Add condition to flat pointer list and to map of q-constants
    _conditions_per_op[op].push_back(cond);
    for (const auto& q : reference) {
        if (!_q_const_map.count(q)) {
            _q_const_map[q];
            std::get<0>(_q_const_map[q]) = op;
        }
        std::get<1>(_q_const_map[q]).insert(cond);
    }

    Log::d("QCONSTCOND_CREATE %s@%i,%i %s (universal: %s)\n", TOSTR(_op_sigs[op].usig), _op_sigs[op].layer, _op_sigs[op].pos, cond->toStr().c_str(), isUniversal(cond) ? "true" : "false");

    return cond;
}

bool QConstantDatabase::test(const std::vector<int>& refQConsts, const std::vector<int>& vals) {
    
    bool holds = true;
    forEachFitCondition(refQConsts, [&](QConstantCondition* cond) {

        // Continue to next fit condition if the condition is not universally relevant
        if (!isUniversal(cond)) return true;

        // Check if the reference is fitting
        bool validReference = true;
        std::vector<int> v; v.reserve(cond->reference.size());
        for (int ref : cond->reference) {
            int i = 0; for (; i < refQConsts.size(); i++) if (refQConsts[i] == ref) break;
            if (i == refQConsts.size()) {
                validReference = false;
                break;
            } else {
                v.push_back(vals[i]);
            }
        }
        
        // Check the condition, if fitting
        if (validReference && !cond->test(v)) {
            //log("QCONSTCOND %s / %s not valid\n", TOSTR(cond->reference), TOSTR(v));
            holds = false;
            return false; // break
        } 
        return true; // continue
    });

    /*
    std::string out = "QCONSTCOND [ ";
    for (int arg : refQConsts) out += Names::to_string(arg) + ",";
    out += " / ";
    for (int arg : vals) out += Names::to_string(arg) + ",";
    out += " ] => %s\n";
    log(out.c_str(), holds ? "TRUE" : "FALSE");
    */

    return holds;
}

NodeHashSet<PositionedUSig, PositionedUSigHasher> QConstantDatabase::backpropagateConditions(int layer, int pos, const NodeHashMap<USignature, int, USignatureHasher>& leafOps) {

    FlatHashSet<int> parentOps;
    for (const auto& leafOp : leafOps) {
        PositionedUSig psig(layer, pos, leafOp.first);
        if (!_op_ids.count(psig)) continue;
        for (int id : _op_possible_parents[_op_ids[psig]]) parentOps.insert(id);
    }

    NodeHashSet<PositionedUSig, PositionedUSigHasher> updatedOps;

    std::vector<int> parentOpStack(parentOps.begin(), parentOps.end());
    while (!parentOpStack.empty()) {
        int parentOp = parentOpStack.back(); parentOpStack.pop_back();

        if (parentOp == 0) continue;
        Log::d("??%s\n", TOSTR(_op_sigs[parentOp].usig));
        const auto& children = _op_children_at_offset[parentOp];
        for (int offset = 0; offset < children.size(); offset++) {

            NodeHashSet<QConstantCondition*, QConstCondHasher, QConstCondEqualsNoOpCheck> isect;
            bool first = true;

            // Calculate intersection of these child ops' conds
            for (const int& childOp : children[offset]) {
                Log::d(" -- ??%s\n", TOSTR(_op_sigs[childOp].usig));
                if (first) {
                    // Initial set of locally relevant conditions
                    for (const auto& cond : _conditions_per_op[childOp]) {
                        if (isUniversal(cond)) continue;
                        isect.insert(cond);
                    }
                    first = false;
                } else {
                    // Reduce intersection by all conditions NOT associated to this op
                    std::vector<QConstantCondition*> toDelete;
                    for (const auto& cond : isect) {
                        auto& opConds = _conditions_per_op[childOp];
                        bool found = false;
                        for (const auto& opCond : opConds) {
                            if (!isUniversal(opCond) && QConstCondEqualsNoOpCheck()(cond, opCond))
                                found = true;
                        }
                        if (!found) toDelete.push_back(cond);
                    }
                    for (const auto& cond : toDelete) isect.erase(cond);
                }
            }

            if (!isect.empty()) {
                // Propagate found conditions up to parent
                for (const auto& cond : isect) {
                    auto newCond = addCondition(parentOp, cond->reference, cond->conjunction, cond->values);
                    if (newCond == nullptr) continue;
                    Log::d("QQ PROPAGATE %s to %s (now universal: %s)\n", cond->toStr().c_str(), TOSTR(_op_sigs[parentOp].usig), isUniversal(newCond) ? "true" : "false");
                    
                    if (!updatedOps.count(_op_sigs[parentOp])) {
                        updatedOps.insert(_op_sigs[parentOp]);
                        for (int grandpaOp : _op_possible_parents[parentOp]) {
                            parentOpStack.push_back(grandpaOp);
                        }
                    }
                }
            }
        }
    }

    return updatedOps;
}

void QConstantDatabase::forEachFitCondition(const std::vector<int>& qConsts, std::function<bool(QConstantCondition*)> onVisit) {

    // Remember checked conditions
    NodeHashSet<QConstantCondition*, QConstCondHasher, QConstCondEquals> checked;

    // For each condition referencing one of the q constants:
    for (const auto& qconst : qConsts) {
        if (_q_const_map.count(qconst)) {
            for (const auto& cond : getConditions(qconst)) {
                if (checked.count(cond)) continue;
                else checked.insert(cond);

                bool proceed = onVisit(cond);
                if (!proceed) return;
            }
        }
    }
}