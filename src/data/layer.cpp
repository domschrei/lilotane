
#include <assert.h>

#include "data/layer.h"
#include "sat/variable_domain.h"
#include "util/log.h"

const USignature Position::NONE_SIG = USignature(-1, std::vector<int>());

Layer::Layer(int index, int size) : _index(index) {
    assert(size > 0);
    _content.resize(size);
}
int Layer::size() const {return _content.size();}
int Layer::index() const {return _index;}
LayerState& Layer::getState() {return _state;}
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

int Position::encode(const USignature& sig) {
    
    if (!_variables.count(sig)) {
        // introduce a new variable
        assert(!VariableDomain::isLocked() || fail("Unknown variable " + VariableDomain::varName(_layer_idx, _pos, sig) + " queried!\n"));
        _variables[sig] = IntPair(VariableDomain::nextVar(), _pos);
        IntPair& pair = _variables[sig];
        VariableDomain::printVar(pair.first, _layer_idx, _pos, sig);
    }

    //log("%i\n", vars[sig]);
    int val = _variables[sig].first;
    return val;
}

void Position::setVariable(const USignature& sig, int v, int priorPos) {
    assert(!_variables.count(sig));
    assert(v > 0);
    _variables[sig] = IntPair(v, priorPos);
}

int Position::getVariable(const USignature& sig) const {
    assert(_variables.count(sig));
    int v = _variables.at(sig).first;
    return v;
}
int Position::getPriorPosOfVariable(const USignature& sig) const {
    assert(_variables.count(sig));
    int priorPos = _variables.at(sig).second;
    return priorPos;
}

bool Position::hasVariable(const USignature& sig) const {
    return _variables.count(sig);
}
bool Position::isVariableOriginallyEncoded(const USignature& sig) const {
    assert(_variables.count(sig));
    return _variables.at(sig).second == _pos;
}
