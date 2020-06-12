
#ifndef DOMPASCH_TREEREXX_LAYER_STATE_H
#define DOMPASCH_TREEREXX_LAYER_STATE_H

#include <utility>
#include <limits>

#include "hashmap.h"
#include "signature.h"

class LayerState {

public:
    typedef HashMap<USignature, std::pair<int, int>, USignatureHasher> RangedSigMap;
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
            Signature operator*() {
                return _it->first;
            }
            void operator++() {
                ++_it;
                gotoNext();
            }
            bool operator!=(const Iterable& other) {
                return other._it != _it;
            }

        private:
            void gotoNext() {
                while (_it != _end && (_it->second.first > _pos || _it->second.second < _pos)) 
                    ++_it;
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
    HashMap<USignature, std::pair<int, int>, USignatureHasher> _pos_fact_occurrences;
    HashMap<USignature, std::pair<int, int>, USignatureHasher> _neg_fact_occurrences;

public:
    LayerState() {}
    LayerState(const LayerState& other) : _pos_fact_occurrences(other._pos_fact_occurrences), _neg_fact_occurrences(other._neg_fact_occurrences) {}
    LayerState(const LayerState& other, std::vector<int> offsets) {
        for (const auto& entry : other._pos_fact_occurrences) {
            const USignature& sig = entry.first;
            const auto& range = entry.second;
            _pos_fact_occurrences[sig] = std::pair<int, int>(offsets[range.first], offsets[range.second]);
        }
        for (const auto& entry : other._neg_fact_occurrences) {
            const USignature& sig = entry.first;
            const auto& range = entry.second;
            _neg_fact_occurrences[sig] = std::pair<int, int>(offsets[range.first], offsets[range.second]);
        }
    }

    void add(int pos, const Signature& fact) {
        auto& occ = fact._negated ? _neg_fact_occurrences : _pos_fact_occurrences;
        if (!occ.count(fact._usig)) {
            occ[fact._usig] = std::pair<int, int>(pos, INT32_MAX);
        }
        occ[fact._usig].first = std::min(occ[fact._usig].first, pos);
    }
    void withdraw(int pos, const Signature& fact) {
        auto& occ = fact._negated ? _neg_fact_occurrences : _pos_fact_occurrences;
        if (!occ.count(fact._usig)) return;
        occ[fact._usig].second = pos;
    }

    bool contains(int pos, const USignature& fact, bool negated) const {
        auto& occ = negated ? _neg_fact_occurrences : _pos_fact_occurrences;
        if (!occ.count(fact)) return false;
        const auto& pair = occ.at(fact);
        return pos >= pair.first && pos < pair.second;
    }
    bool contains(int pos, const Signature& fact) const {
        return contains(pos, fact._usig, fact._negated);  
    }

    const RangedSigMap& getPosFactOccurrences() const {
        return _pos_fact_occurrences;
    }
    const RangedSigMap& getNegFactOccurrences() const {
        return _neg_fact_occurrences;
    }

    Iterator at(int pos, bool negated) {
        return Iterator(negated ? _neg_fact_occurrences : _pos_fact_occurrences, pos);
    }

    LayerState& operator=(const LayerState& other) {
        _pos_fact_occurrences = other._pos_fact_occurrences;
        _neg_fact_occurrences = other._neg_fact_occurrences;
        return *this;
    }
};

#endif