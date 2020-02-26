
#ifndef DOMPASCH_TREE_REXX_INSTANTIATOR_H
#define DOMPASCH_TREE_REXX_INSTANTIATOR_H


#include <unordered_set>

#include "parser/cwa.hpp"
#include "data/reduction.h"
#include "data/action.h"
#include "data/code_table.h"
#include "data/signature.h"
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
            for (int preArg : pre._args) {
                if (preArg == arg) r++;
            } 
        }
        for (const Signature& eff : op.getEffects()) {
            for (int effArg : eff._args) {
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
    int _inst_mode;

public:
    Instantiator(Parameters& params, HtnInstance& htn) : _params(params), _htn(&htn) {
        if (_params.isSet("qq")) {
            _inst_mode = INSTANTIATE_NOTHING;
        } else if (_params.isSet("q")) {
            _inst_mode = INSTANTIATE_PRECONDITIONS;
        } else {
            _inst_mode = INSTANTIATE_FULL;
        }
    }

    std::vector<Reduction> getApplicableInstantiations(Reduction& r,
            std::unordered_map<int, SigSet> facts, int mode = -1);
    std::vector<Action> getApplicableInstantiations(Action& a,
            std::unordered_map<int, SigSet> facts, int mode = -1);

    SigSet instantiate(const HtnOp& op, const std::unordered_map<int, SigSet>& facts);
            
    std::pair<std::unordered_map<int, SigSet>, std::unordered_map<int, SigSet>> getPossibleFactChanges(Reduction& r);
    std::pair<std::unordered_map<int, SigSet>, std::unordered_map<int, SigSet>> getFactChanges(Action& a);

    std::unordered_map<Signature, 
                       std::unordered_set<substitution_t, Substitution::Hasher>, 
                       SignatureHasher> 
        getOperationSubstitutionsCausingEffect(
            const std::unordered_set<Signature, SignatureHasher>& operations, 
            const Signature& fact);

    bool isFullyGround(const Signature& sig);
    std::vector<int> getFreeArgPositions(const Signature& sig);
    bool fits(Signature& sig, Signature& groundSig, std::unordered_map<int, int>* substitution);
    bool hasSomeInstantiation(const Signature& sig);
    bool hasConsistentlyTypedArgs(const Signature& sig);
    bool test(const Signature& sig, std::unordered_map<int, SigSet> facts);
    bool hasValidPreconditions(const HtnOp& op, std::unordered_map<int, SigSet> facts);

    std::unordered_map<int, int> substitution(std::vector<int> origArgs, std::vector<int> newArgs) {
        std::unordered_map<int, int> s;
        assert(origArgs.size() == newArgs.size());
        for (int i = 0; i < origArgs.size(); i++) {
            s[origArgs[i]] = newArgs[i];
        }
        return s;
    }

};


#endif