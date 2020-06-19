
#include <assert.h>

#include "substitution.h"

Substitution::Entry::Entry(int first, int second) : first(first), second(second) {}
Substitution::Entry::Entry(const Entry& other) : first(other.first), second(other.second) {}

bool Substitution::Entry::operator==(const Entry& other) const {
    return first == other.first && second == other.second;
}




Substitution::Substitution() {}

Substitution::Substitution(const Substitution& other) {
    _entries = other._entries;
}

Substitution::Substitution(const std::vector<int>& src, const std::vector<int>& dest) {
    assert(src.size() == dest.size());
    for (int i = 0; i < src.size(); i++) {
        if (src[i] != dest[i]) {
            assert(!count(src[i]) || (*this)[src[i]] == dest[i]);
            add(src[i], dest[i]);
        }
    }
}

int& Substitution::operator[](const int& key) {

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

const int& Substitution::operator[](const int& key) const {
    return at(key);
}

const int& Substitution::at(const int& key) const {
    auto it = _entries.begin();
    while (it != _entries.end() && it->first != key) ++it;
    return it->second;
}

int Substitution::count(const int& key) const {
    auto it = _entries.begin();
    while (it != _entries.end() && it->first != key) ++it;
    return it != _entries.end();
}

void Substitution::clear() {
    _entries.clear();
}

bool Substitution::empty() const {
    return _entries.empty();
}

size_t Substitution::size() const {
    size_t size = 0;
    auto it = _entries.begin();
    while (it != _entries.end()) {++it; size++;}
    return size;
}

std::vector<Substitution> Substitution::getAll(const std::vector<int>& src, const std::vector<int>& dest) {
    std::vector<Substitution> ss;
    ss.emplace_back(); // start with empty substitution
    assert(src.size() == dest.size());
    for (int i = 0; i < src.size(); i++) {
        assert(src[i] != 0 && dest[i] != 0);
        if (src[i] != dest[i]) {

            // Iterate over existing substitutions
            int priorSize = ss.size();
            for (int j = 0; j < priorSize; j++) {
                Substitution& s = ss[j];
                
                // Does the substitution already have such a key but with a different value?
                if (s.count(src[i]) && s[src[i]] != dest[i]) {
                    // yes -- branch: keep original substitution, add alternative

                    Substitution s1(s);
                    s1[src[i]] = dest[i];
                    ss.push_back(s1); // overwritten substitution

                } else {
                    // Just add to substitution
                    s[src[i]] = dest[i];
                }
            }
        }
    }
    return ss;
}

void Substitution::add(int key, int val) {
    (*this)[key] = val;
}

bool Substitution::operator==(const Substitution& other) const {
    return _entries == other._entries;
}
bool Substitution::operator!=(const Substitution& other) const {
    return !(*this == other);
}

std::size_t Substitution::Hasher::operator()(const Substitution& s) const {
    size_t hash = 1337;
    for (const auto& pair : s) {
        hash_combine(hash, pair.first);
        hash_combine(hash, pair.second);
    }
    hash_combine(hash, s.size());
    return hash;
}

std::forward_list<Substitution::Entry>::const_iterator Substitution::begin() const {
    return _entries.begin();
}
std::forward_list<Substitution::Entry>::const_iterator Substitution::end() const {
    return _entries.end();
}

/*
Substitution::Iterable::Iterable(const std::vector<int>& keys, const::std::vector<int>& vals, int pos) :
            _keys(keys), _vals(vals), _pos(pos) {}

std::pair<int, int> Substitution::Iterable::operator*() {
    return std::pair<int, int>(_keys[_pos], _vals[_pos]);
}

void Substitution::Iterable::operator++() {
    _pos++;
}

bool Substitution::Iterable::operator!=(const Iterable& other) {
    return other._pos != _pos;
}*/