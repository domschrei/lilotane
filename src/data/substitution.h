
#ifndef DOMPASCH_TREEREXX_SUBSTITUTION_H
#define DOMPASCH_TREEREXX_SUBSTITUTION_H

#include <vector>

#include "data/hashmap.h"
#include "util/hash.h"

typedef FlatHashMap<int, int> substitution_t;

namespace Substitution {
    substitution_t get(const std::vector<int>& src, const std::vector<int>& dest);
    std::vector<substitution_t> getAll(const std::vector<int>& src, const std::vector<int>& dest);
    bool implies(const substitution_t& super, const substitution_t& sub);

    struct Hasher {
        std::size_t operator()(const substitution_t& s) const;
    };
}

#endif