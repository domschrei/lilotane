
#include <assert.h>

#include "data/layer.h"
#include "sat/variable_domain.h"
#include "util/log.h"

const USignature Position::NONE_SIG = USignature(-1, std::vector<int>());
const USigSet Position::EMPTY_USIG_SET;
const SigSet Position::EMPTY_SIG_SET;

Layer::Layer(int index, int size) : _index(index), _content(size) {
    assert(size > 0);
}
int Layer::size() const {return _content.size();}
int Layer::index() const {return _index;}
LayerState& Layer::getState() {return _state;}
Position& Layer::operator[](int pos) {assert(pos >= 0 && pos < size()); return _content[pos];}
Position& Layer::at(int pos) {return (*this)[pos];}
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
