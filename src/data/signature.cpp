
#include "data/signature.h"

namespace Substitution {

    substitution_t get(std::vector<int> src, std::vector<int> dest) {
        substitution_t s;
        assert(src.size() == dest.size());
        for (int i = 0; i < src.size(); i++) {
            if (src[i] != dest[i]) {
                assert(!s.count(src[i]) || s[src[i]] == dest[i]);
                s[src[i]] = dest[i];
            }
        }
        return s;
    }

    std::vector<substitution_t> getAll(std::vector<int> src, std::vector<int> dest) {
        std::vector<substitution_t> ss;
        ss.push_back(substitution_t()); // start with empty substitution
        assert(src.size() == dest.size());
        for (int i = 0; i < src.size(); i++) {
            if (src[i] != dest[i]) {

                // Iterate over existing substitutions
                for (substitution_t& s : ss) {
                    
                    // Does the substitution already have this key? 
                    if (s.count(src[i]) && s[src[i]] != dest[i]) {
                        // yes -- branch: keep original substitution, add alternative

                        substitution_t s1 = s;
                        s1[src[i]] = dest[i];
                        ss.push_back(s1); // overwritten substitution

                    } else {
                        // Just add to substitution
                        s[src[i]] = dest[i];
                    }
                }
            }
        }
        return ss;
    }
}