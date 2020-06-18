
#include "layer_state.h"


LayerState::Iterable::Iterable(RangedSigMap::iterator begin, RangedSigMap::iterator end, int pos) : 
        _begin(begin), _end(end), _pos(pos) {
    _it = _begin;
    gotoNext();
}
USignature LayerState::Iterable::operator*() {
    return _it->first;
}
void LayerState::Iterable::operator++() {
    ++_it;
    gotoNext();
}
bool LayerState::Iterable::operator!=(const Iterable& other) {
    return other._it != _it;
}
void LayerState::Iterable::gotoNext() {
    while (_it != _end && (_it->second.first > _pos || _it->second.second < _pos)) 
        ++_it;
}


LayerState::Iterator::Iterator(LayerState::RangedSigMap& map, int pos) : 
        _map(map), _pos(pos) {
    _it = _map.begin();
}
LayerState::Iterable LayerState::Iterator::begin() {
    return Iterable(_map.begin(), _map.end(), _pos);
}
LayerState::Iterable LayerState::Iterator::end() const {
    return Iterable(_map.end(), _map.end(), _pos);
}


LayerState::LayerState() {}
LayerState::LayerState(const LayerState& other) : _pos_fact_occurrences(other._pos_fact_occurrences), _neg_fact_occurrences(other._neg_fact_occurrences) {}
LayerState::LayerState(const LayerState& other, std::vector<int> offsets) {
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

void LayerState::add(int pos, const Signature& fact) {
    add(pos, fact._usig, fact._negated);
}
void LayerState::add(int pos, const USignature& fact, bool negated) {
    auto& occ = negated ? _neg_fact_occurrences : _pos_fact_occurrences;
    if (!occ.count(fact)) {
        occ[fact] = std::pair<int, int>(pos, INT32_MAX);
    }
    occ[fact].first = std::min(occ[fact].first, pos);
}
void LayerState::withdraw(int pos, const Signature& fact) {
    withdraw(pos, fact._usig, fact._negated);
}
void LayerState::withdraw(int pos, const USignature& fact, bool negated) {
    auto& occ = negated ? _neg_fact_occurrences : _pos_fact_occurrences;
    if (!occ.count(fact)) return;
    occ[fact].second = pos;
}

bool LayerState::contains(int pos, const USignature& fact, bool negated) const {
    auto& occ = negated ? _neg_fact_occurrences : _pos_fact_occurrences;
    if (!occ.count(fact)) return false;
    const auto& pair = occ.at(fact);
    return pos >= pair.first && pos < pair.second;
}
bool LayerState::contains(int pos, const Signature& fact) const {
    return contains(pos, fact._usig, fact._negated);  
}

const LayerState::RangedSigMap& LayerState::getPosFactOccurrences() const {
    return _pos_fact_occurrences;
}
const LayerState::RangedSigMap& LayerState::getNegFactOccurrences() const {
    return _neg_fact_occurrences;
}

LayerState::Iterator LayerState::at(int pos, bool negated) {
    return Iterator(negated ? _neg_fact_occurrences : _pos_fact_occurrences, pos);
}

LayerState& LayerState::operator=(const LayerState& other) {
    _pos_fact_occurrences = other._pos_fact_occurrences;
    _neg_fact_occurrences = other._neg_fact_occurrences;
    return *this;
}