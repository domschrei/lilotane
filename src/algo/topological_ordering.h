
#ifndef DOMPASCH_LILOTANE_TOPOLOGICAL_ORDERING_H
#define DOMPASCH_LILOTANE_TOPOLOGICAL_ORDERING_H

#include <vector>
#include <map>

namespace TopologicalOrdering {

/*
Input: A map mapping each node ID to a list of node IDs to which it has outgoing edges
Output: A flat list of node IDs in topological ordering
*/
std::vector<int> compute(const std::map<int, std::vector<int>>& orderingNodelist);

};

#endif
