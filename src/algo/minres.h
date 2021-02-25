
#ifndef DOMPASCH_LILOTANE_MINRES_H
#define DOMPASCH_LILOTANE_MINRES_H

#include "data/htn_instance.h"
#include "algo/network_traversal.h"
#include "algo/arg_iterator.h"

// Minimum Recursive Expansion Size
class MinRES {

private:
    HtnInstance& _htn;
    FlatHashMap<int, int> _min_recursive_expansion_sizes;

public:
    MinRES(HtnInstance& htn) : _htn(htn) {
        //computeMinNumPrimitiveChildren();
    }

    int getMinNumPrimitiveChildren(int sigName) {
        
        if (_min_recursive_expansion_sizes.count(sigName))
            return _min_recursive_expansion_sizes[sigName];

        assert(_htn.isAction(USignature(sigName, std::vector<int>())) || Log::e("Invalid query for MinRES: %s\n", TOSTR(sigName))); 

        int blankId = _htn.getBlankActionSig()._name_id;
        int& minNumChildren = _min_recursive_expansion_sizes[sigName];
        minNumChildren = sigName == blankId
                        ||  _htn.getActionNameFromRepetition(sigName) == blankId
                    ? 0 : 1;
        return minNumChildren;
    }

    void computeMinNumPrimitiveChildren() {

        for (const auto& [nameId, action] : _htn.getActionTemplates()) {
            Log::d("%s : MinRES = %i\n", TOSTR(action.getSignature()), 
                getMinNumPrimitiveChildren(nameId));
        }

        NetworkTraversal nt(_htn);
        
        bool change = true;
        size_t numPasses = 0;
        while (change) {
            change = false;
            for (const auto& [nameId, reduction] : _htn.getReductionTemplates()) {

                if (_min_recursive_expansion_sizes.count(nameId)) continue;

                bool canComputeMinRes = true;
                for (const auto& sig : nt.getPossibleChildren(reduction.getSignature())) {
                    if (sig._name_id != nameId && !_min_recursive_expansion_sizes.count(sig._name_id)) {
                        canComputeMinRes = false;
                        break;
                    }
                }
                if (!canComputeMinRes) continue;

                std::vector<int> args(_htn.getSorts(nameId).size(), -1);
                USignature normSig(nameId, args);
                int& minNumChildren = _min_recursive_expansion_sizes[nameId];
                const auto& r = _htn.getReductionTemplate(nameId);
                for (size_t o = 0; o < r.getSubtasks().size(); o++) {
                    int minNumChildrenAtO = 999999;

                    std::vector<USignature> children;
                    nt.getPossibleChildren(r.getSubtasks(), o, children);
                    FlatHashSet<int> childrenIds;
                    for (const auto& child : children) 
                        if (child._name_id != nameId) childrenIds.insert(child._name_id);
                    for (const auto& child : childrenIds) {
                        minNumChildrenAtO = std::min(minNumChildrenAtO, getMinNumPrimitiveChildren(child));
                    }

                    assert(minNumChildrenAtO < 999999);
                    minNumChildren += minNumChildrenAtO;
                }

                Log::d("%s : MinRES = %i\n", TOSTR(reduction.getSignature()), 
                    getMinNumPrimitiveChildren(nameId));
                
                change = true;
            }
            numPasses++;
        }
    }
};

#endif
