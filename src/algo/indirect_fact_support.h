
#ifndef DOMPASCH_LILOTANE_INDIRECT_FACT_SUPPORT_H
#define DOMPASCH_LILOTANE_INDIRECT_FACT_SUPPORT_H

#include "data/htn_instance.h"
#include "data/position.h"
#include "sat/variable_provider.h"

typedef NodeHashMap<USignature, NodeHashMap<int, LiteralTree<int>>, USignatureHasher> IndirectSupportMap;

class IndirectFactSupport {

private:
    HtnInstance& _htn;
    VariableProvider& _vars;

public:
    IndirectFactSupport(HtnInstance& htn, VariableProvider& vars) : _htn(htn), _vars(vars) {}

    std::pair<IndirectSupportMap, IndirectSupportMap> computeFactSupports(Position& newPos, Position& left) {
        static Position NULL_POS;

        // Retrieve supports
        NodeHashMap<USignature, USigSet, USignatureHasher>* dirSupports[2] 
                = {&newPos.getNegFactSupports(), &newPos.getPosFactSupports()};
        IndirectFactSupportMap* indirSupports[2] 
                = {&newPos.getNegIndirectFactSupports(), &newPos.getPosIndirectFactSupports()};
        
        // Structures for indirect supports:
        // Maps each fact to a map of operation variables to a tree of substitutions 
        // which make the operation (possibly) change the variable.
        std::pair<IndirectSupportMap, IndirectSupportMap> result;
        auto& [negResult, posResult] = result;

        // For both fact polarities
        for (size_t i = 0; i < 2; i++) {
            
            // For each fact with some indirect support, for each action in the support
            auto& output = i == 0 ? negResult : posResult;
            for (const auto& [fact, entry] : *indirSupports[i]) for (const auto& [op, subs] : entry) {
                assert(!_htn.isActionRepetition(op._name_id));

                // Skip if the operation is already a DIRECT support for the fact
                auto it = dirSupports[i]->find(fact);
                if (it != dirSupports[i]->end() && it->second.count(op)) continue;

                int opVar = left.getVariableOrZero(VarType::OP, op);
                USignature virtOp(_htn.getRepetitionNameOfAction(op._name_id), op._args);
                int virtOpVar = left.getVariableOrZero(VarType::OP, virtOp);
                
                // Not an encoded action? (May have been pruned away)
                if (opVar == 0 && virtOpVar == 0) continue;

                // For each substitution leading to the desired ground effect:
                for (const auto& sub : subs) {
                    std::vector<int> path(sub.size());
                    size_t i = 0;
                    for (const auto& [src, dest] : sub) {
                        path[i++] = _vars.varSubstitution(src, dest);
                    }
                    if (opVar != 0) output[fact][opVar].insert(path);
                    if (virtOpVar != 0) output[fact][virtOpVar].insert(path);
                }
            }
        }

        return result;
    }

};

#endif
