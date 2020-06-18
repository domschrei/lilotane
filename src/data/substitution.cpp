
#include <assert.h>

#include "substitution.h"

namespace Substitution {

    substitution_t get(const std::vector<int>& src, const std::vector<int>& dest) {
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

    std::vector<substitution_t> getAll(const std::vector<int>& src, const std::vector<int>& dest) {
        std::vector<substitution_t> ss;
        ss.emplace_back(); // start with empty substitution
        assert(src.size() == dest.size());
        for (int i = 0; i < src.size(); i++) {
            assert(src[i] != 0 && dest[i] != 0);
            if (src[i] != dest[i]) {

                // Iterate over existing substitutions
                int priorSize = ss.size();
                for (int j = 0; j < priorSize; j++) {
                    substitution_t& s = ss[j];
                    
                    // Does the substitution already have such a key but with a different value? 
                    if (s.count(src[i]) && s[src[i]] != dest[i]) {
                        // yes -- branch: keep original substitution, add alternative

                        substitution_t s1(s);
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

    bool implies(const substitution_t& super, const substitution_t& sub) {
        for (const auto& e : sub) {
            if (!super.count(e.first) || super.at(e.first) != e.second) return false;
        }
        return true;
    }

    std::size_t Hasher::operator()(const substitution_t& s) const {
        size_t hash = 1337;
        for (const auto& pair : s) {
            hash_combine(hash, pair.first);
            hash_combine(hash, pair.second);
        }
        hash_combine(hash, s.size());
        return hash;
    }
}