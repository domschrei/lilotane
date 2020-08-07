
#ifndef DOMPASCH_LILOTANE_LAYER_STATE_H
#define DOMPASCH_LILOTANE_LAYER_STATE_H

#include <utility>
#include <limits>

#include "hashmap.h"
#include "signature.h"

class LayerState {

public:
    typedef FlatHashMap<USignature, std::pair<int, int>, USignatureHasher> RangedSigMap;
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
    FlatHashMap<USignature, std::pair<int, int>, USignatureHasher> _pos_fact_occurrences;
    FlatHashMap<USignature, std::pair<int, int>, USignatureHasher> _neg_fact_occurrences;

public:
    LayerState();
    LayerState(const LayerState& other);
    LayerState(const LayerState& other, std::vector<int> offsets);

    void add(int pos, const Signature& fact);
    void add(int pos, const USignature& fact, bool negated);
    void withdraw(int pos, const Signature& fact);
    void withdraw(int pos, const USignature& fact, bool negated);

    bool contains(int pos, const USignature& fact, bool negated) const;
    bool contains(int pos, const Signature& fact) const;

    const RangedSigMap& getPosFactOccurrences() const;
    const RangedSigMap& getNegFactOccurrences() const;

    Iterator at(int pos, bool negated);
    LayerState& operator=(const LayerState& other);
};

#endif