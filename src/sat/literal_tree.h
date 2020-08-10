
#ifndef DOMPASCH_LILOTANE_LITERAL_TREE_H
#define DOMPASCH_LILOTANE_LITERAL_TREE_H

#include <vector>

#include <data/hashmap.h>
#include <util/log.h>

class LiteralTree {

struct Node {

    FlatHashMap<int, Node*> children;
    bool validLeaf = false;

    ~Node();
    void insert(const std::vector<int>& lits, size_t idx);
    bool contains(const std::vector<int>& lits, size_t idx) const;
    void encode(std::vector<std::vector<int>>& cls, std::vector<int>& path, size_t pathSize) const;
};

private:
    Node _root;

public:
    void insert(const std::vector<int>& lits);
    bool contains(const std::vector<int>& lits) const;
    bool containsEmpty() const;
    std::vector<std::vector<int>> encode(std::vector<int> headLits = std::vector<int>()) const;
};


#endif