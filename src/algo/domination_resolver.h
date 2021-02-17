
#ifndef DOMPASCH_LILOTANE_DOMINATION_RESOLVER_H
#define DOMPASCH_LILOTANE_DOMINATION_RESOLVER_H

#include "data/htn_instance.h"
#include "data/position.h"

class DominationResolver {

private:
    HtnInstance& _htn;

    size_t _num_dominated_ops = 0;

public:
    DominationResolver(HtnInstance& htn) : _htn(htn) {}

    enum DominationStatus {DOMINATING, DOMINATED, DIFFERENT, EQUIVALENT};
    struct DominationResult {
        DominationStatus status;
        Substitution qconstSubstitutions;
    };

    DominationResult getDominationStatus(const USignature& op, const USignature& other, Position& p);
    void eliminateDominatedOperations(Position& newPos);

    size_t getNumDominatedOps() const {
        return _num_dominated_ops;
    }
};

#endif