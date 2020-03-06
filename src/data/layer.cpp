
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
    Signature sigAbs = neg ? sig.abs() : sig;

    if (!_variables.count(sigAbs)) {
        // introduce a new variable
        assert(!VariableDomain::isLocked() || fail("Unknown variable " + VariableDomain::varName(_layer_idx, _pos, sigAbs) + " queried!\n"));
        _variables[sigAbs] = VariableDomain::nextVar();
        VariableDomain::printVar(_variables[sigAbs], _layer_idx, _pos, sigAbs);
    }

    //log("%i\n", vars[sig]);
    int val = (neg ? -1 : 1) * _variables[sigAbs];
    return val;
}

void Position::setVariable(const Signature& sig, int v) {
    assert(!_variables.count(sig.abs()));
    assert(v > 0);
    _variables[sig.abs()] = v;
}

int Position::getVariable(const Signature& sig) {
    bool neg = sig._negated;
    Signature sigAbs = neg ? sig.abs() : sig;
    assert(_variables.count(sigAbs));
    return (neg ? -1 : 1) * _variables[sigAbs];
}

bool Position::hasVariable(const Signature& sig) {
    return _variables.count(sig._negated ? sig.abs() : sig);
}