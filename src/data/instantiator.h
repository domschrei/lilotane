
#ifndef DOMPASCH_TREE_REXX_INSTANTIATOR_H
#define DOMPASCH_TREE_REXX_INSTANTIATOR_H


#include <unordered_set>

#include "parser/cwa.hpp"
#include "data/reduction.h"
#include "data/action.h"
#include "data/code_table.h"
#include "data/signature.h"

class HtnInstance; // incomplete forward def

class Instantiator {

private:
    HtnInstance* _htn;

public:
    Instantiator(HtnInstance& htn) : _htn(&htn) {}

    std::vector<Reduction> getMinimalApplicableInstantiations(Reduction& r, 
            std::unordered_map<int, SigSet> facts);

    template<class T>
    std::vector<T> instantiatePreconditions(T& r,
            std::unordered_map<int, SigSet> facts);

    std::vector<Action> getApplicableInstantiations(Action& a,
            std::unordered_map<int, SigSet> facts);
            
    std::pair<std::unordered_map<int, SigSet>, std::unordered_map<int, SigSet>> getPossibleFactChanges(Reduction& r);
    std::pair<std::unordered_map<int, SigSet>, std::unordered_map<int, SigSet>> getFactChanges(Action& a);

    std::unordered_map<Signature, 
                       std::unordered_set<substitution_t, Substitution::Hasher>, 
                       SignatureHasher> 
        getOperationSubstitutionsCausingEffect(
            const std::unordered_set<Signature, SignatureHasher>& operations, 
            const Signature& fact);

    bool isFullyGround(Signature& sig);
    std::vector<int> getFreeArgPositions(Signature& sig);
    bool fits(Signature& sig, Signature& groundSig, std::unordered_map<int, int>* substitution);

    bool test(Signature& sig, std::unordered_map<int, SigSet> facts);

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