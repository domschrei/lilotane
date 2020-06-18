
#ifndef DOMPASCH_TREEREXX_HASHMAP_H
#define DOMPASCH_TREEREXX_HASHMAP_H

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

#endif