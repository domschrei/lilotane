
#ifndef DOMPASCH_TREE_REXX_INSTANTIATOR_H
#define DOMPASCH_TREE_REXX_INSTANTIATOR_H


#include <unordered_set>

#include "parser/cwa.hpp"
#include "parser/main.h"
#include "data/reduction.h"
#include "data/action.h"
#include "data/code_table.h"
#include "data/signature.h"

class Instantiator {

private:
    ParsedProblem& _problem;
    std::unordered_set<int>& _var_ids;
    std::unordered_map<int, std::vector<int>>& _constants_by_sort;
    std::unordered_map<int, std::vector<int>>& _predicate_sorts_table;

public:
    Instantiator(ParsedProblem& p, std::unordered_set<int>& varIds,
                std::unordered_map<int, std::vector<int>>& constants, 
                std::unordered_map<int, std::vector<int>>& predicateSortsTable) : 
            _problem(p), _var_ids(varIds), _constants_by_sort(constants), 
            _predicate_sorts_table(predicateSortsTable) {}

    std::vector<Reduction> getMinimalApplicableInstantiations(Reduction& r, 
            std::unordered_map<int, SigSet> facts);

    template<class T>
    std::vector<T> instantiatePreconditions(T& r,
            std::unordered_map<int, SigSet> facts);

    std::vector<Action> getApplicableInstantiations(Action& a,
            std::unordered_map<int, SigSet> facts);
            
    std::pair<std::unordered_map<int, SigSet>, std::unordered_map<int, SigSet>> getPossibleFactChanges(Reduction& r);
    std::pair<std::unordered_map<int, SigSet>, std::unordered_map<int, SigSet>> getFactChanges(Action& a);

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
    }

};


#endif