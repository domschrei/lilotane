
#ifndef DOMPASCH_LILOTANE_SUBSTITUTION_H
#define DOMPASCH_LILOTANE_SUBSTITUTION_H

#include <vector>
#include <forward_list>

#include "util/hashmap.h"
#include "util/hash.h"

class Substitution {

public:
    struct Entry { 
        int first; 
        int second;

        Entry(int first, int second);
        Entry(const Entry& other);
        inline bool operator==(const Entry& other) const {
            return first == other.first && second == other.second;
        }
    };

private:
    std::forward_list<Entry> _entries;

public:
    Substitution();
    Substitution(const Substitution& other);
    Substitution(Substitution&& old);
    Substitution(const std::vector<int>& src, const std::vector<int>& dest);

    void clear();

    size_t size() const;
    bool empty() const;

    Substitution concatenate(const Substitution& second) const;

    std::forward_list<Entry>::const_iterator begin() const;
    std::forward_list<Entry>::const_iterator end() const;

    //static Substitution get(const std::vector<int>& src, const std::vector<int>& dest);
    static std::vector<Substitution> getAll(const std::vector<int>& src, const std::vector<int>& dest);

    struct Hasher {
        inline std::size_t operator()(const Substitution& s) const {
            size_t hash = 1337;
            for (const auto& pair : s) {
                hash_combine(hash, pair.first);
                hash_combine(hash, pair.second);
            }
            hash_combine(hash, s.size());
            return hash;
        }
    };

    inline int& operator[](const int& key) {

        auto it = _entries.begin();

        // Empty list?
        if (it == _entries.end()) {
            _entries.emplace_front(key, 0);
            return _entries.begin()->second;
        }

        // Scan entries
        auto nextIt = _entries.begin();
        while (it != _entries.end()) {
            
            // Key found: return associated value
            if (it->first == key) return it->second;
            
            // Peek next position
            ++nextIt;

            // Break if this is the position to insert
            if (nextIt == _entries.end() || nextIt->first > key) break;

            // Proceed to next position
            ++it;
        }
        
        // Insert, make iterator point to inserted entry
        _entries.emplace_after(it, key, 0);
        ++it;

        return it->second;
    }

    inline int operator[](const int& key) const {
        return at(key);
    }

    inline int at(const int& key) const {
        auto it = _entries.begin();
        while (it != _entries.end() && it->first != key) ++it;
        return it->second;
    }

    inline std::forward_list<Substitution::Entry>::const_iterator find(int key) const {
        auto it = _entries.begin();
        while (it != _entries.end() && it->first != key) ++it;
        return it;
    }

    inline std::forward_list<Substitution::Entry>::iterator find(int key) {
        std::forward_list<Substitution::Entry>::iterator it = _entries.begin();
        while (it != _entries.end() && it->first != key) ++it;
        return it;
    }

    inline int count(const int& key) const {
        auto it = _entries.begin();
        while (it != _entries.end() && it->first != key) ++it;
        return it != _entries.end();
    }


    inline bool operator==(const Substitution& other) const {
        return _entries == other._entries;
    }

    inline bool operator!=(const Substitution& other) const {
        return !(*this == other);
    }

    inline void operator=(const Substitution& other) {
        _entries = other._entries;
    }

private:
    inline void add(int key, int val) {
        (*this)[key] = val;
    }

};

#endif