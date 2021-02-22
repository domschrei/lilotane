
#ifndef DOMPASCH_LILOTANE_HASHMAP_H
#define DOMPASCH_LILOTANE_HASHMAP_H

//#include "../../lib/DySECT/include/cuckoo_dysect.h"
//#define FlatHashMap dysect::cuckoo_dysect_inplace

#ifndef FlatHashMap
#include "util/robin_hood.h"
#define FlatHashMap robin_hood::unordered_flat_map
#define NodeHashMap robin_hood::unordered_node_map
#define FlatHashSet robin_hood::unordered_flat_set
#define NodeHashSet robin_hood::unordered_node_set
#endif

#ifndef FlatHashMap
#include <unordered_map>
#include <unordered_set>
#define FlatHashMap std::unordered_map
#define NodeHashMap std::unordered_map
#define FlatHashSet std::unordered_set
#define NodeHashSet std::unordered_set
#endif

#include "util/hash.h"

typedef std::pair<int, int> IntPair;
struct IntPairHasher {
size_t operator()(const std::pair<int, int>& pair) const {
    size_t h = 17;
    hash_combine(h, pair.first);
    hash_combine(h, pair.second);
    return h;
}
};

struct IntVecHasher {
    inline std::size_t operator()(const std::vector<int>& s) const {
        size_t hash = s.size();
        for (const int& arg : s) {
            hash_combine(hash, arg);
        }
        return hash;
    }
};

#endif