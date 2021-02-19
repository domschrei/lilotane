
#ifndef DOMPASCH_LILOTANE_OP_TABLE_H
#define DOMPASCH_LILOTANE_OP_TABLE_H

#include "data/signature.h"
#include "data/action.h"
#include "data/reduction.h"

class OpTable {

private:
    // Maps a signature of a ground or pseudo-ground action to the actual action object.
    NodeHashMap<USignature, Action, USignatureHasher> _actions_by_sig;
    // Maps a signature of a ground or pseudo-ground reduction to the actual reduction object.
    NodeHashMap<USignature, Reduction, USignatureHasher> _reductions_by_sig;

public:
    
    void addAction(const Action& a) {
        _actions_by_sig[a.getSignature()] = a;
    }
    
    void addReduction(const Reduction& r) {
        _reductions_by_sig[r.getSignature()] = r;
    }

    bool hasAction(const USignature& sig) const {
        return _actions_by_sig.count(sig);
    }

    bool hasReduction(const USignature& sig) const {
        return _reductions_by_sig.count(sig);
    }
    
    const Action& getAction(const USignature& sig) const {
        return _actions_by_sig.at(sig);
    }
    
    const Reduction& getReduction(const USignature& sig) const {
        return _reductions_by_sig.at(sig);
    }
};

#endif
