
#ifndef DOMPASCH_LILOTANE_LITERAL_TREE_H
#define DOMPASCH_LILOTANE_LITERAL_TREE_H

#include <vector>
#include <functional>

#include <data/hashmap.h>
#include <util/log.h>

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
            if (!children.count(lits[idx])) return false;
            return children.at(lits[idx])->contains(lits, idx+1);
        }

        size_t getSizeOfEncoding() const {
            if (validLeaf) return 0;
            size_t paths = 1;
            for (const auto& [lit, child] : children) {
                paths += child->getSizeOfEncoding();
            }
            return paths;
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

        size_t getSizeOfNegationEncoding() const {
            if (validLeaf) return 0;
            size_t paths = 1;
            for (const auto& [lit, child] : children) {
                if (child->validLeaf) paths++;
                else paths += child->getSizeOfNegationEncoding();
            }
            return paths;
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
        typename LiteralTree<U, UHash>::Node* convert(std::function<U(T)> map) const {
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

    void operator=(LiteralTree<T, THash>&& other) {
        _root.children = std::move(other._root.children);
        _root.validLeaf = other._root.validLeaf;
    }

    void operator=(const LiteralTree<T>& other) {
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

    bool empty() const {
        return _root.children.empty() && !_root.validLeaf;
    }

    size_t getSizeOfEncoding() const {
        return _root.getSizeOfEncoding();
    }
    size_t getSizeOfNegationEncoding() const {
        return _root.getSizeOfNegationEncoding();
    }

    bool contains(const std::vector<T>& lits) const {
        return _root.contains(lits, 0);
    }

    bool containsEmpty() const {
        return _root.validLeaf;
    }

    std::vector<std::vector<T>> encode(std::vector<T> headLits = std::vector<T>()) const {
        std::vector<std::vector<T>> cls;
        _root.encode(cls, headLits);
        
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
        _root.encodeNegation(cls, headLits);
        
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

    //template <typename T, typename Hash = robin_hood::hash<T>>
    template <typename U, typename UHash = robin_hood::hash<U>>
    LiteralTree<U, UHash> convert(std::function<U(T)> map) const {
        LiteralTree<U, UHash> tree;
        tree._root = *_root.convert(map);
        return tree;
    }
};


#endif