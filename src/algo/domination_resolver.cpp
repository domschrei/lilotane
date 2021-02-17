
#include "algo/domination_resolver.h"

DominationResolver::DominationResult DominationResolver::getDominationStatus(const USignature& op, const USignature& other, Position& p) {
    DominationResult res;
    res.status = DIFFERENT;

    if (op._name_id != other._name_id) return res;
    assert(op._args.size() == other._args.size());
    
    DominationStatus status = EQUIVALENT;
    FlatHashSet<int> dummyDomain;
    std::vector<int> sameDomainQConstArgIndices;

    for (size_t argIdx = 0; argIdx < op._args.size(); argIdx++) {
        int arg = op._args[argIdx];
        int otherArg = other._args[argIdx];

        // TODO Every q-constant must have a globally invariant domain for this to work.
        if (arg == otherArg) continue; 
        
        bool isQ = _htn.isQConstant(arg);
        bool isOtherQ = _htn.isQConstant(otherArg);
        if (!isQ && !isOtherQ) return res; // Different ground constants

        // Check whether both q-constants originate from the same position
        IntPair position(p.getLayerIndex(), p.getPositionIndex());
        if (isQ && isOtherQ && _htn.getOriginOfQConstant(arg) != _htn.getOriginOfQConstant(otherArg)) {
            return res;
        }
        
        // Compare domains of pseudo-constants
        
        const auto& domain = isQ ? _htn.getDomainOfQConstant(arg) : dummyDomain;
        if (!isQ) dummyDomain.insert(arg);
        const auto& otherDomain = isOtherQ ? _htn.getDomainOfQConstant(otherArg) : dummyDomain;
        if (!isOtherQ) dummyDomain.insert(otherArg);
        assert(dummyDomain.size() <= 1);

        if (domain.size() > otherDomain.size()) {
            // This op may dominate the other op

            // Contradicts previous argument indices -> ops are different
            if (status == DOMINATED) return res;
            // Check if this domain actually contains the other domain
            for (int c : otherDomain) if (!domain.count(c)) return res;
            // Yes: Dominating w.r.t. this position
            status = DOMINATING;
            res.qconstSubstitutions[otherArg] = arg;

        } else if (domain.size() < otherDomain.size()) {
            // This op may be dominated by the other op

            // Contradicts previous argument indices -> ops are different
            if (status == DOMINATED) return res;
            // Check if the other domain actually contains this domain
            for (int c : domain) if (!otherDomain.count(c)) return res;
            // Yes: Dominated w.r.t. this position
            status = DOMINATED;
            res.qconstSubstitutions[arg] = otherArg;

        } else if (domain != otherDomain) {
            // Different domains
            return res;

        } else {
            // The two pseudo-constants are not equal, but share the same domain
            sameDomainQConstArgIndices.push_back(argIdx);
        }
        // Domains are equal

        if (!isQ || !isOtherQ) dummyDomain.clear();
    }

    if (status == EQUIVALENT) {
        // The operations are equal. Both dominate another.
        // Tie break using lexicographic ordering of arguments
        for (size_t argIdx = 0; argIdx < op._args.size(); argIdx++) {
            if (op._args[argIdx] != other._args[argIdx]) {
                status = op._args[argIdx] < other._args[argIdx] ? DOMINATED : DOMINATING;
                break;
            }    
        }
        for (size_t argIdx = 0; argIdx < op._args.size(); argIdx++) {
            if (_htn.isQConstant(status == DOMINATED ? other._args[argIdx] : op._args[argIdx])) {
                if (status == DOMINATED) res.qconstSubstitutions[op._args[argIdx]] = other._args[argIdx];
                else res.qconstSubstitutions[other._args[argIdx]] = op._args[argIdx];
            } 
        }
        res.status = status;
        return res;
    } else {
        // Add to substitution all q-constant pairs with identical domain
        for (int argIdx : sameDomainQConstArgIndices) {
            if (status == DOMINATED) res.qconstSubstitutions[op._args[argIdx]] = other._args[argIdx];
            else res.qconstSubstitutions[other._args[argIdx]] = op._args[argIdx];
        }
    }
    res.status = status;
    return res;
}

void DominationResolver::eliminateDominatedOperations(Position& newPos) {

    // Map of an op name id to (map of a dominating op to a set of dominated ops)
    NodeHashMap<int, NodeHashMap<USignature, USigSubstitutionMap, USignatureHasher>> dominatingActionsByName;
    NodeHashMap<int, NodeHashMap<USignature, USigSubstitutionMap, USignatureHasher>> dominatingReductionsByName;

    // For each operation
    const USigSet* ops[2] = {&newPos.getActions(), &newPos.getReductions()};
    NodeHashMap<int, NodeHashMap<USignature, USigSubstitutionMap, USignatureHasher>>* dMaps[2] = {
        &dominatingActionsByName, &dominatingReductionsByName
    };
    for (size_t i = 0; i < 2; i++) {

        for (const auto& op : *ops[i]) {
            auto& dominatingOps = (*dMaps[i])[op._name_id];

            // Compare operation with each currently dominating op of the same name
            USigSubstitutionMap dominated;
            
            if (dominatingOps.empty()) {
                dominatingOps[op];
                continue;
            }

            for (auto& [other, dominatedByOther] : dominatingOps) {
                auto result = getDominationStatus(op, other, newPos);
                if (result.status == DOMINATED) {
                    // This op is being dominated; mark for deletion
                    //Log::d("DOM %s << %s\n", TOSTR(op), TOSTR(other));
                    dominatedByOther[op] = std::move(result.qconstSubstitutions);
                    dominated.clear();
                    break;
                }
                if (result.status == DOMINATING) {
                    // This op dominates the other op
                    //Log::d("DOM %s >> %s\n", TOSTR(op), TOSTR(other));
                    dominated[other] = std::move(result.qconstSubstitutions);
                }
            }
            // Delete all ops transitively dominated by this op
            assert(!dominatingOps.count(op));
            for (const auto& [other, s] : dominated) {
                std::vector<USignature> dominatedVec(1, other);
                std::vector<Substitution> subVec(1, s);
                for (size_t j = 0; j < dominatedVec.size(); j++) {
                    auto dominatedOp = dominatedVec[j];
                    auto domS = subVec[j];
                    //Log::d("DOM %s, j=%i : %s\n", TOSTR(op), j, TOSTR(dominatedOp));
                    //Log::d("DOM sub: %s\n", TOSTR(domS));
                    if (!dominatingOps[op].count(dominatedOp) || domS.size() < dominatingOps[op][dominatedOp].size()) 
                        dominatingOps[op][dominatedOp] = domS;
                    //assert(dominatedOp.substitute(domS) == op || Log::e("%s -> %s != %s\n", TOSTR(dominatedOp), TOSTR(dominatedOp.substitute(domS)), TOSTR(op)));
                    if (dominatingOps.count(dominatedOp)) {
                        for (const auto& [domDomOp, domDomS] : dominatingOps[dominatedOp]) {
                            dominatedVec.push_back(domDomOp);
                            Substitution cat = domDomS.concatenate(domS);
                            //Log::d("DOM concat (%s , %s) ~> %s\n", TOSTR(domDomS), TOSTR(domS), TOSTR(cat));
                            subVec.push_back(cat); 
                        }
                        dominatingOps.erase(dominatedOp);
                    }
                }
            }
        }

        // Remove all dominated ops
        for (auto& [nameId, dMap] : *dMaps[i]) for (auto& [op, dominated] : dMap) {
            for (auto& [other, s] : dominated) {
                Log::v("%s dominates %s (%s)\n", TOSTR(op), TOSTR(other), TOSTR(s));
                //assert(other.substitute(s) == op);
                newPos.replaceOperation(other, op, std::move(s));
                _num_dominated_ops++;
            }
        }
    }
}
