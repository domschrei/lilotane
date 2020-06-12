
#ifndef DOMPASCH_TREEREXX_HASHMAP_H
#define DOMPASCH_TREEREXX_HASHMAP_H

//#include "../../lib/DySECT/include/cuckoo_dysect.h"
//#define HashMap dysect::cuckoo_dysect_inplace

#ifndef HashMap
#include "util/robin_hood.h"
#define HashMap robin_hood::unordered_node_map
#define HashSet robin_hood::unordered_node_set
#endif

#ifndef HashMap
#include <unordered_map>
#include <unordered_set>
#define HashMap std::unordered_map
#define HashSet std::unordered_set
#endif

#endif