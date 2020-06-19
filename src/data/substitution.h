
#ifndef DOMPASCH_TREEREXX_SUBSTITUTION_H
#define DOMPASCH_TREEREXX_SUBSTITUTION_H

#include <vector>
#include <forward_list>

#include "data/hashmap.h"
#include "util/hash.h"

class Substitution {

public:
    struct Entry { 
        int first; 
        int second;

        Entry(int first, int second);
        Entry(const Entry& other);
        bool operator==(const Entry& other) const;
    };

private:
    std::forward_list<Entry> _entries;

public:
    Substitution();
    Substitution(const Substitution& other);
    Substitution(const std::vector<int>& src, const std::vector<int>& dest);

    int& operator[](const int& key);
    const int& operator[](const int& key) const;
    const int& at(const int& key) const;
    int count(const int& key) const;

    void clear();

    size_t size() const;
    bool empty() const;

    std::forward_list<Entry>::const_iterator begin() const;
    std::forward_list<Entry>::const_iterator end() const;

    bool operator==(const Substitution& other) const;
    bool operator!=(const Substitution& other) const;

    //static Substitution get(const std::vector<int>& src, const std::vector<int>& dest);
    static std::vector<Substitution> getAll(const std::vector<int>& src, const std::vector<int>& dest);

    struct Hasher {
        std::size_t operator()(const Substitution& s) const;
    };

private:
    void add(int key, int val);

};

#endif