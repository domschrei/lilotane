
#ifndef DOMPASCH_LILOTANE_NETWORK_TRAVERSAL_H
#define DOMPASCH_LILOTANE_NETWORK_TRAVERSAL_H

#include <functional>

#include "data/signature.h"

class HtnInstance;

class NetworkTraversal {

public:
    enum TraverseOrder {TRAVERSE_PREORDER, TRAVERSE_POSTORDER};


private:
    HtnInstance* _htn;

public:
    NetworkTraversal(HtnInstance& htn) : _htn(&htn) {}
    void traverse(const USignature& opSig, TraverseOrder order, std::function<void(const USignature&, int)> onVisit);
    std::vector<USignature> getPossibleChildren(const USignature& opSig);
    void getPossibleChildren(const std::vector<USignature>& subtasks, int offset, std::vector<USignature>& result);

};


#endif