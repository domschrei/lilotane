
#include "topological_ordering.h"

/*
Input: A map mapping each node ID to a list of node IDs to which it has outgoing edges
Output: A flat list of node IDs in topological ordering
*/
std::vector<int> TopologicalOrdering::compute(const std::map<int, std::vector<int>>& orderingNodelist) {
    
    // Initialize "visited" state for each node
    std::map<int, int> visitedStates;
    for (auto pair : orderingNodelist) {
        visitedStates[pair.first] = 0;
    }

    // Topological ordering via multiple DFS
    std::vector<int> sortedNodes;
    while (true) {

        // Pick an unvisited node
        int node = -1;
        for (auto pair : orderingNodelist) {
            if (visitedStates[pair.first] == 0) {
                node = pair.first;
                break;
            }
        }
        if (node == -1) break; // no node left: done

        // Traverse ordering graph
        std::vector<int> nodeStack;
        nodeStack.push_back(node);
        while (!nodeStack.empty()) {

            int n = nodeStack.back();

            if (visitedStates[n] == 2) {
                // Closed node: pop
                nodeStack.pop_back();

            } else if (visitedStates[n] == 1) {
                // Open node: close, pop
                nodeStack.pop_back();
                visitedStates[n] = 2;
                sortedNodes.insert(sortedNodes.begin(), n);

            } else {
                // Unvisited node: open, visit children
                visitedStates[n] = 1;
                if (!orderingNodelist.count(n)) continue;
                for (int m : orderingNodelist.at(n)) {
                    if (visitedStates[m] < 2)
                        nodeStack.push_back(m);
                }
            }
        }
    }

    return sortedNodes;
}
