
#ifndef DOMPASCH_TREEREXX_LAYER_STATE_H
#define DOMPASCH_TREEREXX_LAYER_STATE_H

#include <utility>
#include <limits>

#include "hashmap.h"
#include "signature.h"

class LayerState {

public:
    typedef HashMap<Signature, std::pair<int, int>, SignatureHasher> RangedSigMap;
    class Iterable {
        private:
            RangedSigMap::iterator _begin;
            RangedSigMap::iterator _end;

            RangedSigMap::iterator _it;
            int _pos;

        public:
            Iterable(RangedSigMap::iterator begin, RangedSigMap::iterator end, int pos) : 
                    _begin(begin), _end(end), _pos(pos) {
                _it = _begin;
                gotoNext();
            }
            const Signature& operator*() {
                return _it->first;
            }
            void operator++() {
                _it++;
                gotoNext();
            }
            bool operator!=(const Iterable& other) {
                return other._it != _it;
            }

        private:
            void gotoNext() {
                while (_it != _end && (_it->second.first > _pos || _it->second.second < _pos)) 
                    _it++;
            }
    };
    class Iterator {
        private:
            RangedSigMap& _map;
            int _pos;
            RangedSigMap::iterator _it;

        public:
            Iterator(RangedSigMap& map, int pos) : 
                    _map(map), _pos(pos) {
                _it = _map.begin();
            }

            Iterable begin() {
                return Iterable(_map.begin(), _map.end(), _pos);
            }
            Iterable end() const {
                return Iterable(_map.end(), _map.end(), _pos);
            }
    };

private:
    HashMap<Signature, std::pair<int, int>, SignatureHasher> _fact_occurrences;

public:
    LayerState() {}
    LayerState(const LayerState& other) {
        _fact_occurrences = other._fact_occurrences;
    }
    LayerState(const LayerState& other, std::vector<int> offsets) {
        for (const auto& entry : other._fact_occurrences) {
            const Signature& sig = entry.first;
            const auto& range = entry.second;
            _fact_occurrences[sig] = std::pair<int, int>(offsets[range.first], offsets[range.second]);
        }
    }

    void add(int pos, const Signature& fact) {
        if (!_fact_occurrences.count(fact)) {
            _fact_occurrences[fact] = std::pair<int, int>(pos, INT32_MAX);
        }
        _fact_occurrences[fact].first = std::min(_fact_occurrences[fact].first, pos);
    }
    void withdraw(int pos, const Signature& fact) {
        if (!_fact_occurrences.count(fact)) return;
        _fact_occurrences[fact].second = pos;
    }

    bool contains(int pos, const Signature& fact) const {
        if (!_fact_occurrences.count(fact)) return false;
        const auto& pair = _fact_occurrences.at(fact);
        return pos >= pair.first && pos < pair.second;    
    }

    const HashMap<Signature, std::pair<int, int>, SignatureHasher>& getFactOccurrences() const {
        return _fact_occurrences;
    }

    Iterator at(int pos) {
        return Iterator(_fact_occurrences, pos);
    }

    LayerState& operator=(const LayerState& other) {
        _fact_occurrences = other._fact_occurrences;
        return *this;
    }
};

#endif