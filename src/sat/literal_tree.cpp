
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

void LiteralTree::Node::encode(std::vector<std::vector<int>>& cls, std::vector<int>& path) const {
    // orClause: IF the current path, THEN either of the children.
    std::vector<int> orClause(path.size() + children.size());
    // newPath: current path plus one of the children of this node.
    std::vector<int> newPath(path.size()+1);
    size_t i = 0;
    for (; i < path.size(); i++) {
        orClause[i] = -path[i];
        newPath[i] = path[i];
    }
    for (const auto& [lit, child] : children) {
        orClause[i++] = lit;
        newPath.back() = lit;
        child->encode(cls, newPath);
    }
    if (!validLeaf) cls.push_back(orClause);
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

std::vector<std::vector<int>> LiteralTree::encode(std::vector<int> headLits) const {
    std::vector<std::vector<int>> cls;
    _root.encode(cls, headLits);
    /*
    Log::d("TREE ENCODE ");
    for (const auto& c : cls) {
        for (int lit : c) Log::log_notime(Log::V4_DEBUG, "%i ", lit);
        Log::log_notime(Log::V4_DEBUG, "0 ");
    }
    Log::log_notime(Log::V4_DEBUG, "\n");
    */
    return cls;
}
