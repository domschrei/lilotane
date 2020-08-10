
#include "literal_tree.h"


LiteralTree::Node::~Node() {
    for (const auto& [lit, child] : children) {
        delete child;
    }
}

void LiteralTree::Node::insert(const std::vector<int>& lits, size_t idx) {
    if (idx == lits.size()) {
        validLeaf = true;
        return;
    }
    auto it = children.find(lits[idx]);
    Node* child;
    if (it == children.end()) {
        children[lits[idx]] = new Node(); // insert child
        child = children[lits[idx]];
    } else child = it->second;
    // recursion
    child->insert(lits, idx+1);
}

bool LiteralTree::Node::contains(const std::vector<int>& lits, size_t idx) const {
    if (idx == lits.size()) return validLeaf;
    if (!children.count(lits[idx])) return false;
    return children.at(lits[idx])->contains(lits, idx+1);
}

void LiteralTree::Node::encode(std::vector<int>& cls, std::vector<int>& path, size_t pathSize) const {
    
    int insertionIdx = 0;
    if (pathSize >= path.size()) path.resize(2*pathSize+1);
    
    if (!validLeaf) {
        // Insert new clause: IF the current path, THEN either of the children.
        cls.reserve(cls.size()+pathSize+children.size()+1);
        cls.insert(cls.end(), path.begin(), path.begin()+pathSize);
        insertionIdx = cls.size(); // upcoming zeros will be overwritten inside the loop
        cls.insert(cls.end(), children.size()+1, 0);

        // Set correct literals for children and recursively encode children
        for (const auto& [lit, child] : children) {
            cls[insertionIdx++] = lit;
            path[pathSize] = -lit;
            child->encode(cls, path, pathSize+1);
        }

    } // else: No need to encode children as this node is already valid.
}

void LiteralTree::insert(const std::vector<int>& lits) {
    /*
    Log::d("TREE INSERT ");
    for (int lit : lits) Log::log_notime(Log::V4_DEBUG, "%i ", lit);
    Log::log_notime(Log::V4_DEBUG, "\n");
    */
    _root.insert(lits, 0);
}

bool LiteralTree::contains(const std::vector<int>& lits) const {
    return _root.contains(lits, 0);
}

bool LiteralTree::containsEmpty() const {
    return _root.validLeaf;
}

std::vector<int> LiteralTree::encode(std::vector<int> headLits) const {
    std::vector<int> cls;
    size_t initPathSize = headLits.size();
    _root.encode(cls, headLits, initPathSize);
    
    Log::d("TREE ENCODE ");
    for (const auto& lit : cls) {
        Log::log_notime(Log::V4_DEBUG, "%i ", lit);
    }
    Log::log_notime(Log::V4_DEBUG, "\n");
    
    return cls;
}
