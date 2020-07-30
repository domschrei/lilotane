
#ifndef DOMPASCH_TREEREXX_LITERAL_TREE_H
#define DOMPASCH_TREEREXX_LITERAL_TREE_H

#include <data/hashmap.h>
#include <util/log.h>

class LiteralTree {

struct Node {

    FlatHashMap<int, Node*> children;
    bool validLeaf = false;

    ~Node();
    void insert(const std::vector<int>& lits, int idx);
    bool contains(const std::vector<int>& lits, int idx);
    void encode(std::vector<std::vector<int>>& cls, std::vector<int>& path);
};

private:
    Node _root;

public:
    void insert(const std::vector<int>& lits);
    bool contains(const std::vector<int>& lits, int idx);
    std::vector<std::vector<int>> encode(const std::vector<int>& headLits = std::vector<int>());
};


#endif