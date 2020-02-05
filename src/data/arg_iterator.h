
#ifndef DOMPASCH_TREE_REXX_ARG_ITERATOR_H
#define DOMPASCH_TREE_REXX_ARG_ITERATOR_H

#include <unordered_map>
#include <vector>

#include "data/signature.h"

class ArgIterator {

public:
    static std::vector<Signature> getFullInstantiation(Signature sig, 
        std::unordered_map<int, std::vector<int>>& _constants_by_sort, 
        std::unordered_map<int, std::vector<int>>& _predicate_sorts_table, 
        std::unordered_set<int>& _var_ids) {

        std::vector<Signature> instantiation;

        // enumerate all arg combinations for variable args
        // Get all constants of the respective type(s)
        assert(_predicate_sorts_table.count(sig._name_id));
        std::vector<int> sorts = _predicate_sorts_table[sig._name_id];
        std::vector<std::vector<int>> constantsPerArg;
        int sortIdx = 0;
        for (int pos = 0; pos < sorts.size(); pos++) {
            int arg = sig._args[pos];
            if (_var_ids.count(arg)) {
                // free argument
                int sort = sorts[sortIdx++];
                assert(_constants_by_sort.count(sort));
                constantsPerArg.push_back(_constants_by_sort[sort]);
            } else {
                // constant
                constantsPerArg.push_back(std::vector<int>(1, arg));
            }
        }

        // Iterate over all possible assignments
        std::vector<int> counter(constantsPerArg.size(), 0);
        while (true) {
            // Assemble the assignment
            std::vector<int> newArgs(counter.size());
            for (int argPos = 0; argPos < counter.size(); argPos++) {
                
                assert(argPos < constantsPerArg.size());
                assert(counter[argPos] < constantsPerArg[argPos].size());

                newArgs[argPos] = constantsPerArg[argPos][counter[argPos]];
            }
            instantiation.push_back(sig.substitute(Substitution::get(sig._args, newArgs)));            

            // Increment exponential counter
            int x = 0;
            while (x < counter.size()) {
                if (counter[x]+1 == constantsPerArg[x].size()) {
                    // max value reached
                    counter[x] = 0;
                    if (x+1 == counter.size()) break;
                } else {
                    // increment
                    counter[x]++;
                    break;
                }
                x++;
            }
            // Counter finished?
            if (counter[x] == 0 && x+1 == counter.size()) break;
        }

        return instantiation;
    }

};

#endif