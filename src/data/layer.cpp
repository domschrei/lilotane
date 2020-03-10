
#include <assert.h>

#include "data/layer.h"
#include "sat/variable_domain.h"
#include "util/log.h"

const Signature Position::NONE_SIG = Signature(-1, std::vector<int>());

Layer::Layer(int index, int size) : _index(index) {
    assert(size > 0);
    _content.resize(size);
}
int Layer::size() const {return _content.size();}
int Layer::index() const {return _index;}
Position& Layer::operator[](int pos) {assert(pos >= 0 && pos < size()); return _content[pos];}
void Layer::consolidate() {
    int succ = 0;
    for (int pos = 0; pos < size(); pos++) {
        _successor_positions.push_back(succ);
        succ += _content[pos].getMaxExpansionSize();
    }
}
int Layer::getNextLayerSize() const {
    return _successor_positions.back()+1;
}
int Layer::getSuccessorPos(int oldPos) const {
    assert(oldPos >= 0 && oldPos < _successor_positions.size());
    return _successor_positions[oldPos];
}

int Position::encode(const Signature& sig) {
    bool neg = sig._negated;
    sig._negated = false;

    if (!_variables.count(sig)) {
        // introduce a new variable
        assert(!VariableDomain::isLocked() || fail("Unknown variable " + VariableDomain::varName(_layer_idx, _pos, sig) + " queried!\n"));
        IntPair& pair = (_variables[sig] = IntPair(VariableDomain::nextVar(), _pos));
        VariableDomain::printVar(pair.first, _layer_idx, _pos, sig);
    }

    //log("%i\n", vars[sig]);
    int val = (neg ? -1 : 1) * _variables[sig].first;
    sig._negated = neg;
    return val;
}

void Position::setVariable(const Signature& sig, int v, int priorPos) {
    bool neg = sig._negated;
    sig._negated = false;
    assert(!_variables.count(sig));
    assert(v > 0);
    _variables[sig] = IntPair(v, priorPos);
    sig._negated = neg;
}

int Position::getVariable(const Signature& sig) const {
    bool neg = sig._negated;
    sig._negated = false;
    assert(_variables.count(sig));
    int v = (neg ? -1 : 1) * _variables.at(sig).first;
    sig._negated = neg;
    return v;
}
int Position::getPriorPosOfVariable(const Signature& sig) const {
    bool neg = sig._negated;
    sig._negated = false;
    assert(_variables.count(sig));
    int priorPos = _variables.at(sig).second;
    sig._negated = neg;
    return priorPos;
}

bool Position::hasVariable(const Signature& sig) const {
    bool neg = sig._negated;
    sig._negated = false;
    bool has = _variables.count(sig);
    sig._negated = neg;
    return has;
}
bool Position::isVariableOriginallyEncoded(const Signature& sig) const {
    bool neg = sig._negated;
    sig._negated = false;
    assert(_variables.count(sig));
    bool is = _variables.at(sig).second == _pos;
    sig._negated = neg;
    return is;
}
