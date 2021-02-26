
#ifndef DOMPASCH_LILOTANE_LITERAL_TREE_H
#define DOMPASCH_LILOTANE_LITERAL_TREE_H

#include <vector>
#include <functional>

#include "util/hashmap.h"
#include "util/log.h"

/*
On an abstract level, this class template represents a set of sequences whereas some global order
is imposed on the elements that may occur in a sequence, and all sequences are sorted accordingly.
*/
template <typename T, typename THash = robin_hood::hash<T>>
class LiteralTree {

    template <typename, typename>
    friend class LiteralTree;

    struct Node {

        FlatHashMap<T, Node*, THash> children;
        bool validLeaf = false;

        Node() = default;
        Node(const Node& other) : validLeaf(other.validLeaf) {
            for (const auto& [key, child] : other.children) {
                children[key] = new Node(*child);
            }
        }
        Node(Node&& other) : children(std::move(other.children)), validLeaf(other.validLeaf) {
            other.children.clear();
        }

        void operator=(const Node& other) {
            validLeaf = other.validLeaf;
            for (const auto& [key, child] : other.children) {
                children[key] = new Node(*child);
            }
        }

        ~Node() {
            for (const auto& [lit, child] : children) {
                delete child;
            }
        }
        
        void insert(const std::vector<T>& lits, size_t idx) {
            if (idx == lits.size()) {
                validLeaf = true;
                return;
            }
            auto it = children.find(lits[idx]);
            Node* child;
            if (it == children.end()) {
                child = new Node(); // insert child
                children[lits[idx]] = child;
            } else child = it->second;
            // recursion
            child->insert(lits, idx+1);
        }

        bool contains(const std::vector<T>& lits, size_t idx) const {
            if (idx == lits.size()) return validLeaf;
            auto it = children.find(lits[idx]);
            if (it == children.end()) return false;
            return it->second->contains(lits, idx+1);
        }

        /*
        Returns true if the tree has a path of which <lits> is a subpath.
        */
        bool subsumes(const std::vector<T>& lits, size_t idx) const {

            // No literals left in the given path?
            if (idx == lits.size()) {
                if (validLeaf) return true;
                // If any (transitive) child is a valid leaf, return true
                for (auto& [key, child] : children) {
                    //Log::d("(1) i=%i n=%i Does child node (%s,%s) subsume %s?\n", 
                    //    idx, lits.size(), TOSTR(key.first), TOSTR(key.second), TOSTR(lits));    
                    if (child->subsumes(lits, idx)) return true;
                }
                //Log::d("(1) i=%i n=%i Node does not subsume %s\n", idx, lits.size(), TOSTR(lits));
                return false;
            }

            // Valid child node according to next literal present?
            auto it = children.find(lits[idx]);
            if (it != children.end()) {
                // Yes: check if it subsumes the remaining path
                //Log::d("(2) i=%i n=%i Does child node (%s,%s) subsume %s?\n", 
                //        idx, lits.size(), TOSTR(it->first.first), TOSTR(it->first.second), TOSTR(lits));
                if (it->second->subsumes(lits, idx+1)) return true;
            }

            // No valid child node:
            // Any (transitive) child must subsume the same path
            for (auto& [key, child] : children) {
                //Log::d("(3) i=%i n=%i Does child node (%s,%s) subsume %s?\n", 
                //        idx, lits.size(), TOSTR(key.first), TOSTR(key.second), TOSTR(lits));    
                if (child->subsumes(lits, idx)) return true;
            }
            //Log::d("(2) i=%i n=%i Node does not subsume %s\n", idx, lits.size(), TOSTR(lits));
            return false;
        }

        /*
        Returns true if the tree has a path which is a sub-path of <lits>.
        */
        bool hasPathSubsumedBy(const std::vector<T>& lits, size_t idx) const {
                
            // No literals left in the given path? -> Path completed.
            if (idx == lits.size()) return validLeaf;

            // Direct valid child?
            auto it = children.find(lits[idx]);
            if (it != children.end() && it->second->hasPathSubsumedBy(lits, idx+1))
                return true;

            // No valid child: try a later position
            for (size_t i = idx+1; i < lits.size(); i++) {
                if (hasPathSubsumedBy(lits, i)) return true;
            }
            return false;
        }

        std::pair<size_t, size_t> getSizeOfEncoding() const {
            std::pair<size_t, size_t> result;
            if (validLeaf) return result;
            auto& [cls, lits] = result;
            cls = 1;
            lits = children.size();
            for (const auto& [lit, child] : children) {
                auto [cCls, cLits] = child->getSizeOfEncoding();
                cls += cCls;
                lits += cLits + cCls;
            }
            return result;
        }
        void encode(std::vector<std::vector<T>>& cls, std::vector<T>& path) const {
            if (validLeaf) return;

            // orClause: IF the current path, THEN either of the children.
            int pathSize = path.size();
            std::vector<T> orClause(pathSize + children.size());
            size_t i = 0;
            for (; i < path.size(); i++) {
                if constexpr (std::is_arithmetic<T>()) orClause[i] = -path[i];
                else if constexpr (std::is_same<T, std::pair<int, int>>::value) {
                    orClause[i] = std::pair<int, int>{-path[i].first, path[i].second};
                } else orClause[i] = path[i];
            }
            for (const auto& [lit, child] : children) {
                orClause[i++] = lit;
                path.resize(pathSize+1);
                path.back() = lit;
                child->encode(cls, path);
            }
            cls.push_back(std::move(orClause));
        }

        std::pair<size_t, size_t> getSizeOfNegationEncoding() const {
            std::pair<size_t, size_t> result;
            if (validLeaf) return result;
            auto& [cls, lits] = result;
            cls = 0;
            lits = 0;
            for (const auto& [lit, child] : children) {
                if (child->validLeaf) { 
                    cls++;
                    lits++;
                } else {
                    auto [cCls, cLits] = child->getSizeOfNegationEncoding();
                    cls += cCls;
                    lits += cLits + cCls;
                }
            }
            return result;
        }
        void encodeNegation(std::vector<std::vector<T>>& cls, std::vector<T>& path) const {
            if (validLeaf) return;

            size_t pathSize = path.size();
            std::vector<T> clause(pathSize + 1);
            for (size_t i = 0; i < pathSize; i++) {
                if constexpr (std::is_arithmetic<T>()) clause[i] = -path[i];
                else clause[i] = path[i];
            }
            // For each child that is a valid leaf, encode the negated path to it
            for (const auto& [lit, child] : children) if (child->validLeaf) {
                if constexpr (std::is_arithmetic<T>()) clause[pathSize] = -lit;
                else clause[pathSize] = lit;
                cls.push_back(clause);
            }

            // For all other children, encode recursively
            for (const auto& [lit, child] : children) if (!child->validLeaf) {
                path.resize(pathSize+1);
                path.back() = lit;
                child->encodeNegation(cls, path);
            }
        }

        template <typename U, typename UHash = robin_hood::hash<U>>
        typename LiteralTree<U, UHash>::Node* convert(std::function<U(const T&)> map) const {
            using NNode = typename LiteralTree<U, UHash>::Node;
            NNode *newNode = new NNode();
            newNode->validLeaf = validLeaf;
            for (const auto& [lit, child] : children) {
                newNode->children[map(lit)] = child->convert(map);
            }
            return newNode;
        }
    };

    Node _root;

public:

    LiteralTree() = default;
    LiteralTree(const LiteralTree& other) : _root(other._root) {}
    LiteralTree(LiteralTree&& other) : _root(std::move(other._root)) {}

    void operator=(LiteralTree<T, THash>&& other) {
        _root.children = std::move(other._root.children);
        _root.validLeaf = other._root.validLeaf;
    }

    void operator=(const LiteralTree<T, THash>& other) {
        _root = other._root;
    }

    void insert(const std::vector<T>& lits) {
        /*
        Log::d("TREE INSERT ");
        for (int lit : lits) Log::log_notime(Log::V4_DEBUG, "%i ", lit);
        Log::log_notime(Log::V4_DEBUG, "\n");
        */
        _root.insert(lits, 0);
    }

    void merge(LiteralTree<T, THash>&& other) {
        std::vector<std::pair<Node*, Node*>> nodeStack;
        nodeStack.emplace_back(&_root, &other._root);
        while (!nodeStack.empty()) {
            auto [node, otherNode] = nodeStack.back();
            nodeStack.pop_back();
            if (otherNode->validLeaf) node->validLeaf = true;
            for (auto& [key, val] : otherNode->children) {
                if (node->children.count(key)) {
                    // Already contained: recurse
                    nodeStack.emplace_back(node->children.at(key), val);
                } else {
                    // Key is not contained yet: just insert
                    node->children[key] = val;
                }
            }
            otherNode->children.clear();
            if (node != &_root) delete otherNode;
        }
    }

    void intersect(LiteralTree<T, THash>&& other) {
        std::vector<std::pair<Node*, Node*>> nodeStack;
        nodeStack.emplace_back(&_root, &other._root);
        while (!nodeStack.empty()) {
            auto [node, otherNode] = nodeStack.back();
            nodeStack.pop_back();
            node->validLeaf = node->validLeaf && otherNode->validLeaf;
            std::vector<T> keysToRemove;
            for (auto& [key, val] : node->children) {
                if (!otherNode->children.count(key)) {
                    // Not contained in both: remove!
                    delete val;
                    delete otherNode->children[key];
                    keysToRemove.push_back(key);
                } else {
                    // Contained in both: Check children
                    nodeStack.emplace_back(val, otherNode->children.at(key));
                }
            }
            for (auto& key : keysToRemove) node->children.erase(key);
            otherNode->children.clear();
            if (node != &_root) delete otherNode;
        }
    }

    bool empty() const {
        return _root.children.empty() && !_root.validLeaf;
    }

    size_t getSizeOfEncoding() const {
        return _root.getSizeOfEncoding().second;
    }
    size_t getSizeOfNegationEncoding() const {
        return _root.getSizeOfNegationEncoding().second;
    }

    bool contains(const std::vector<T>& lits) const {
        return _root.contains(lits, 0);
    }

    bool subsumes(const std::vector<T>& lits) const {
        return _root.subsumes(lits, 0);
    }

    bool hasPathSubsumedBy(const std::vector<T>& lits) const {
        return _root.hasPathSubsumedBy(lits, 0);
    }

    bool containsEmpty() const {
        return _root.validLeaf;
    }

    std::vector<std::vector<T>> encode(std::vector<T> headLits = std::vector<T>()) const {
        std::vector<std::vector<T>> cls;

        //size_t headSize = headLits.size();

        _root.encode(cls, headLits);

        /*
        auto [predCls, predLits] = _root.getSizeOfEncoding();
        predLits += headSize * predCls;
        assert(cls.size() == predCls || Log::e("%i != %i\n", cls.size(), predCls));
        size_t lits = 0;
        for (const auto& c : cls) lits += c.size();
        assert(lits == predLits || Log::e("%i != %i\n", lits, predLits));
        */

        /*
        Log::d("TREE ENCODE ");
        for (const auto& c : cls) {
            if constexpr (std::is_arithmetic<T>())
                for (const auto& lit : c) Log::log_notime(Log::V4_DEBUG, "%i ", lit);
            else
                for (const auto& lit : c) Log::log_notime(Log::V4_DEBUG, "(%s , %s) ", TOSTR(lit.first), TOSTR(lit.second));
            Log::log_notime(Log::V4_DEBUG, "0 ");
        }
        Log::log_notime(Log::V4_DEBUG, "\n");
        */

        return cls;
    }

    std::vector<std::vector<T>> encodeNegation(std::vector<T> headLits = std::vector<T>()) const {
        std::vector<std::vector<T>> cls;

        //size_t headSize = headLits.size();

        _root.encodeNegation(cls, headLits);
        
        /*
        auto [predCls, predLits] = _root.getSizeOfNegationEncoding();
        predLits += headSize * predCls;
        assert(cls.size() == predCls || Log::e("%i != %i\n", cls.size(), predCls));
        size_t lits = 0;
        for (const auto& c : cls) lits += c.size();
        assert(lits == predLits || Log::e("%i != %i\n", lits, predLits));
        */

        /*
        Log::d("TREE ENCODE_NEG ");
        for (const auto& c : cls) {
            if constexpr (std::is_arithmetic<T>())
                for (const auto& lit : c) Log::log_notime(Log::V4_DEBUG, "%i ", lit);
            else
                for (const auto& lit : c) Log::log_notime(Log::V4_DEBUG, "(%s , %s) ", TOSTR(lit.first), TOSTR(lit.second));
            Log::log_notime(Log::V4_DEBUG, "0 ");
        }
        Log::log_notime(Log::V4_DEBUG, "\n");
        */

        return cls;
    }

    template <typename U, typename UHash = robin_hood::hash<U>>
    void convert(std::function<U(const T&)> map, LiteralTree<U, UHash>& result) const {
        result._root = *_root.convert(map);
    }
};


#endif