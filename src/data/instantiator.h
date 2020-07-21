
#ifndef DOMPASCH_TREE_REXX_INSTANTIATOR_H
#define DOMPASCH_TREE_REXX_INSTANTIATOR_H

#include <functional>

#include "data/hashmap.h"
#include "data/reduction.h"
#include "data/action.h"
#include "data/code_table.h"
#include "data/signature.h"
#include "data/network_traversal.h"
#include "util/params.h"

class HtnInstance; // incomplete forward def

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
    Parameters& _params;
    HtnInstance* _htn;
    NetworkTraversal _traversal;
    
    int _inst_mode;
    float _q_const_rating_factor;
    int _q_const_instantiation_limit;

    NodeHashMap<int, FlatHashMap<int, float>> _precond_ratings;

    // Maps an (action|reduction) name 
    // to the set of (partially lifted) fact signatures
    // that might be added to the state due to this operator. 
    NodeHashMap<int, SigSet> _fact_changes;

public:
    Instantiator(Parameters& params, HtnInstance& htn) : _params(params), _htn(&htn), _traversal(htn) {
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

    std::vector<Reduction> getApplicableInstantiations(const Reduction& r,
            const std::function<bool(const Signature&)>& state, int mode = -1);
    std::vector<Action> getApplicableInstantiations(const Action& a,
            const std::function<bool(const Signature&)>& state, int mode = -1);

    USigSet instantiate(const HtnOp& op, const std::function<bool(const Signature&)>& state);
    USigSet instantiateLimited(const HtnOp& op, const std::function<bool(const Signature&)>& state, 
            const std::vector<int>& argsByPriority, int limit, bool returnUnfinished);

    const FlatHashMap<int, float>& getPreconditionRatings(const USignature& opSig);

    NodeHashSet<Substitution, Substitution::Hasher> getOperationSubstitutionsCausingEffect(
            const SigSet& factChanges, const USignature& fact, bool negated);

    SigSet getAllFactChanges(const USignature& sig);

    // Maps a (action|reduction) signature of any grounding state
    // to a corresponding list of (partially lifted) fact signatures
    // that might be added to the state due to this operator. 
    SigSet getPossibleFactChanges(const USignature& sig);


    bool isFullyGround(const USignature& sig);
    std::vector<int> getFreeArgPositions(const std::vector<int>& sigArgs);
    bool fits(USignature& sig, USignature& groundSig, FlatHashMap<int, int>* substitution);
    bool hasSomeInstantiation(const USignature& sig);

    bool hasConsistentlyTypedArgs(const USignature& sig);
    std::vector<TypeConstraint> getQConstantTypeConstraints(const USignature& sig);

    bool test(const Signature& sig, const std::function<bool(const Signature&)>& state);
    bool hasValidPreconditions(const SigSet& preconds, const std::function<bool(const Signature&)>& state);
};


#endif