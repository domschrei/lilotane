
#ifndef DOMPASCH_TREE_REXX_INSTANTIATOR_H
#define DOMPASCH_TREE_REXX_INSTANTIATOR_H

#include <functional>

#include "data/htn_instance.h"
#include "util/hashmap.h"
#include "data/reduction.h"
#include "data/action.h"
#include "data/signature.h"
#include "algo/network_traversal.h"
#include "algo/fact_analysis.h"
#include "util/params.h"

struct ArgComparator {
    HtnOp& op;

    ArgComparator(HtnOp& op) : op(op) {}
    bool operator()(const int& a, const int& b) const {
        return rating(a) > rating(b);
    }
    int rating(int arg) const {
        int r = 0;
        for (const Signature& pre : op.getPreconditions()) {
            for (int preArg : pre._usig._args) {
                if (preArg == arg) r++;
            } 
        }
        for (const Signature& eff : op.getEffects()) {
            for (int effArg : eff._usig._args) {
                if (effArg == arg) r++;
            } 
        }
        return r;
    }
};

const int INSTANTIATE_NOTHING = 0;
const int INSTANTIATE_PRECONDITIONS = 1;
const int INSTANTIATE_FULL = 2;

class Instantiator {

private:
    static USigSet EMPTY_USIG_SET;

    Parameters& _params;
    HtnInstance& _htn;
    FactAnalysis& _analysis;
    NetworkTraversal _traversal;
    
    int _inst_mode;
    float _q_const_rating_factor;
    int _q_const_instantiation_limit;

    NodeHashMap<int, FlatHashMap<int, float>> _precond_ratings;

public:
    Instantiator(Parameters& params, HtnInstance& htn, FactAnalysis& analysis) : 
            _params(params), _htn(htn), _analysis(analysis), _traversal(htn) {
        
        if (_params.isNonzero("qq")) {
            _inst_mode = INSTANTIATE_NOTHING;
        } else if (_params.isNonzero("q")) {
            _inst_mode = INSTANTIATE_PRECONDITIONS;
        } else {
            _inst_mode = INSTANTIATE_FULL;
        }
        _q_const_rating_factor = _params.getFloatParam("qrf");
        _q_const_instantiation_limit = _params.getIntParam("qit");
    }

    std::vector<USignature> getApplicableInstantiations(const Reduction& r, int mode = -1);
    std::vector<USignature> getApplicableInstantiations(const Action& a, int mode = -1);

private:
    std::vector<USignature> instantiate(const HtnOp& op);
    std::vector<USignature> instantiateLimited(const HtnOp& op, const std::vector<int>& argIndicesByPriority, 
            size_t limit, bool returnUnfinished);
    
    const FlatHashMap<int, float>& getPreconditionRatings(const USignature& opSig);
};


#endif