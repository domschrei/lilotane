
#ifndef DOMPASCH_TREE_REXX_INSTANTIATOR_H
#define DOMPASCH_TREE_REXX_INSTANTIATOR_H

#include <functional>

#include "data/hashmap.h"
#include "data/reduction.h"
#include "data/action.h"
#include "data/code_table.h"
#include "data/signature.h"
#include "data/network_traversal.h"
#include "data/fact_frame.h"
#include "util/params.h"
#include "data/htn_instance.h"

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
    typedef std::function<bool(const USignature&, bool)> StateEvaluator;
    static USigSet EMPTY_USIG_SET;

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
    NodeHashMap<int, SigSet> _lifted_fact_changes;

    NodeHashMap<int, FactFrame> _fact_frames;

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
            const StateEvaluator& state, int mode = -1);
    std::vector<Action> getApplicableInstantiations(const Action& a,
            const StateEvaluator& state, int mode = -1);

    USigSet instantiate(const HtnOp& op, const StateEvaluator& state);
    USigSet instantiateLimited(const HtnOp& op, const StateEvaluator& state, 
            const std::vector<int>& argsByPriority, size_t limit, bool returnUnfinished);

    const FlatHashMap<int, float>& getPreconditionRatings(const USignature& opSig);

    // Maps a (action|reduction) signature of any grounding state
    // to a corresponding list of (partially lifted) fact signatures
    // that might be added to the state due to this operator. 
    SigSet getPossibleFactChanges(const USignature& sig, bool fullyInstantiate = true);

    FactFrame getFactFrame(const USignature& sig, bool simpleMode = false, USigSet& currentOps = EMPTY_USIG_SET);

    std::vector<int> getFreeArgPositions(const std::vector<int>& sigArgs);
    bool fits(const USignature& from, const USignature& to, FlatHashMap<int, int>* substitution = nullptr);
    bool fits(const Signature& from, const Signature& to, FlatHashMap<int, int>* substitution = nullptr);
    bool hasSomeInstantiation(const USignature& sig);

    bool hasConsistentlyTypedArgs(const USignature& sig);
    std::vector<TypeConstraint> getQConstantTypeConstraints(const USignature& sig);

    inline bool isFullyGround(const USignature& sig) {
        for (int arg : sig._args) if (_htn->isVariable(arg)) return false;
        return true;
    }

    inline bool testWithNoVarsNoQConstants(const USignature& sig, bool negated, const StateEvaluator& state) {
        // fact positive : true iff contained in facts
        if (!negated) return state(sig, negated);
        // fact negative.
        // if contained in facts : return true
        //   (fact occurred negative)
        if (state(sig, negated)) return true;
        // else: return true iff fact does NOT occur in positive form
        return !state(sig, !negated);
    }

    inline bool test(const USignature& sig, bool negated, const StateEvaluator& state) {
        if (!isFullyGround(sig)) return true;
        
        // Q-Fact:
        if (_htn->hasQConstants(sig)) {
            for (const auto& decSig : _htn->decodeObjects(sig, true)) {
                if (testWithNoVarsNoQConstants(decSig, negated, state)) return true;
            }
            return false;
        }

        return testWithNoVarsNoQConstants(sig, negated, state);
    }

    inline bool test(const Signature& sig, const StateEvaluator& state) {
        return test(sig._usig, sig._negated, state);
    }

    inline bool hasValidPreconditions(const SigSet& preconds, const StateEvaluator& state) {
        for (const Signature& pre : preconds) if (!test(pre, state)) {
            return false;
        } 
        return true;
    }
};


#endif