
#ifndef DOMPASCH_LILOTANE_LAYER_STATE_H
#define DOMPASCH_LILOTANE_LAYER_STATE_H

#include <utility>
#include <limits>

#include "hashmap.h"
#include "signature.h"

class LayerState {

public:
    typedef NodeHashMap<USignature, std::pair<int, int>, USignatureHasher> RangedSigMap;
    class Iterable {
        private:
            RangedSigMap::iterator _begin;
            RangedSigMap::iterator _end;

            RangedSigMap::iterator _it;
            int _pos;

        public:
            Iterable(RangedSigMap::iterator begin, RangedSigMap::iterator end, int pos);
            USignature operator*();
            void operator++();
            bool operator!=(const Iterable& other);

        private:
            void gotoNext();
    };
    class Iterator {
        private:
            RangedSigMap& _map;
            int _pos;
            RangedSigMap::iterator _it;

        public:
            Iterator(RangedSigMap& map, int pos);
            Iterable begin();
            Iterable end() const;
    };

private:
    NodeHashMap<USignature, std::pair<int, int>, USignatureHasher> _pos_fact_occurrences;
    NodeHashMap<USignature, std::pair<int, int>, USignatureHasher> _neg_fact_occurrences;

public:
    LayerState();
    LayerState(const LayerState& other);
    LayerState(const LayerState& other, std::vector<int> offsets);

    const RangedSigMap& getPosFactOccurrences() const;
    const RangedSigMap& getNegFactOccurrences() const;

    LayerState& operator=(const LayerState& other);

    inline void add(int pos, const Signature& fact) {
        add(pos, fact._usig, fact._negated);
    }

    inline void add(int pos, const USignature& fact, bool negated) {
        auto& occ = negated ? _neg_fact_occurrences : _pos_fact_occurrences;
        if (!occ.count(fact)) {
            occ[fact] = std::pair<int, int>(pos, INT32_MAX);
        }
        occ[fact].first = std::min(occ[fact].first, pos);
    }

    inline void withdraw(int pos, const Signature& fact) {
        withdraw(pos, fact._usig, fact._negated);
    }

    inline void withdraw(int pos, const USignature& fact, bool negated) {
        auto& occ = negated ? _neg_fact_occurrences : _pos_fact_occurrences;
        auto it = occ.find(fact);
        if (it == occ.end()) return;
        it->second.second = pos;
    }

    inline bool contains(int pos, const USignature& fact, bool negated) const {
        auto& occ = negated ? _neg_fact_occurrences : _pos_fact_occurrences;
        auto it = occ.find(fact);
        if (it == occ.end()) return false;
        auto [first, last] = it->second;
        return pos >= first && pos < last;
    }

    inline bool contains(int pos, const Signature& fact) const {
        return contains(pos, fact._usig, fact._negated);  
    }
    
    inline LayerState::Iterator at(int pos, bool negated) {
        return Iterator(negated ? _neg_fact_occurrences : _pos_fact_occurrences, pos);
    }
};

#endif