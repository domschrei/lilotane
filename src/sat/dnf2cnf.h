
#ifndef DOMPASCH_LILOTANE_DNF_2_CNF_H
#define DOMPASCH_LILOTANE_DNF_2_CNF_H

#include <set>
#include <vector>
#include <assert.h>

#include "util/log.h"

class Dnf2Cnf {

public:
    static std::set<std::set<int>> getCnf(const std::vector<int>& dnf) {
        std::set<std::set<int>> cnf;

        if (dnf.empty()) return cnf;

        int size = 1;
        int clsSize = 0;
        std::vector<int> clsStarts;
        for (size_t i = 0; i < dnf.size(); i++) {
            if (dnf[i] == 0) {
                size *= clsSize;
                clsSize = 0;
            } else {
                if (clsSize == 0) clsStarts.push_back(i);
                clsSize++;
            }
        }
        assert(size > 0);

        // Iterate over all possible combinations
        std::vector<int> counter(clsStarts.size(), 0);
        while (true) {
            // Assemble the combination
            std::set<int> newCls;
            for (size_t pos = 0; pos < counter.size(); pos++) {
                const int& lit = dnf[clsStarts[pos]+counter[pos]];
                assert(lit != 0);
                newCls.insert(lit);
            }
            cnf.insert(newCls);            

            // Increment exponential counter
            size_t x = 0;
            while (x < counter.size()) {
                if (dnf[clsStarts[x]+counter[x]+1] == 0) {
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

        if (cnf.size() > 1000) Log::w("CNF of size %i generated\n", cnf.size());
        //else Log::d("CNF of size %i generated\n", cnf.size());

        return cnf;
    }
};

#endif
