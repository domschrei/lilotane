
#ifndef DOMPASCH_TREEREXX_NETWORK_TRAVERSAL_H
#define DOMPASCH_TREEREXX_NETWORK_TRAVERSAL_H

#include <functional>

#include "data/signature.h"

class HtnInstance;

class NetworkTraversal {

private:
    HtnInstance* _htn;

public:
    NetworkTraversal(HtnInstance& htn) : _htn(&htn) {}
    void traverse(const USignature& opSig, std::function<void(const USignature&, int)> onVisit);

private:
    std::vector<USignature> getPossibleChildren(const USignature& opSig);

};


#endif