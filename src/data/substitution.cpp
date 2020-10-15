
#include <assert.h>

#include "substitution.h"

Substitution::Entry::Entry(int first, int second) : first(first), second(second) {}
Substitution::Entry::Entry(const Entry& other) : first(other.first), second(other.second) {}

Substitution::Substitution() {}

Substitution::Substitution(const Substitution& other) : _entries(other._entries) {}
Substitution::Substitution(Substitution&& old) : _entries(std::move(old._entries)) {}

Substitution::Substitution(const std::vector<int>& src, const std::vector<int>& dest) {
    assert(src.size() == dest.size());
    for (size_t i = 0; i < src.size(); i++) {
        if (src[i] != dest[i]) {
            assert(!count(src[i]) || (*this)[src[i]] == dest[i]);
            add(src[i], dest[i]);
        }
    }
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

Substitution Substitution::concatenate(const Substitution& second) const {
    Substitution s;
    for (const auto& [src, dest] : *this) {
        if (second.count(dest)) {
            s[src] = second[dest];
        } else {
            s[src] = dest;
        }
    }
    for (const auto& [src, dest] : second) {
        if (!s.count(src)) s[src] = dest;
    }
    return s;
}

std::vector<Substitution> Substitution::getAll(const std::vector<int>& src, const std::vector<int>& dest) {
    std::vector<Substitution> ss;
    ss.emplace_back(); // start with empty substitution
    assert(src.size() == dest.size());
    for (size_t i = 0; i < src.size(); i++) {
        assert(src[i] != 0 && dest[i] != 0);
        if (src[i] != dest[i]) {

            // Iterate over existing substitutions
            int priorSize = ss.size();
            for (int j = 0; j < priorSize; j++) {
                Substitution& s = ss[j];
                
                // Does the substitution already have such a key but with a different value?
                auto it = s.find(src[i]);
                if (it != s.end() && it->second != dest[i]) {
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

std::forward_list<Substitution::Entry>::const_iterator Substitution::begin() const {
    return _entries.begin();
}
std::forward_list<Substitution::Entry>::const_iterator Substitution::end() const {
    return _entries.end();
}
