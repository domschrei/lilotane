
#include <assert.h>

#include "data/layer.h"

Layer::Layer(int index, int size) : _index(index) {
    assert(size > 0);
    _content.resize(size);
}
int Layer::size() {return _content.size();}
int Layer::index() {return _index;}
Position& Layer::operator[](int pos) {assert(pos >= 0 && pos < size()); return _content[pos];}
void Layer::consolidate() {
    int succ = 0;
    for (int pos = 0; pos < size(); pos++) {
        _successor_positions.push_back(succ);
        succ += _content[pos].getMaxExpansionSize();
    }
}
int Layer::getNextLayerSize() {
    return _successor_positions.back()+1;
}
int Layer::getSuccessorPos(int oldPos) {
    assert(oldPos >= 0 && oldPos < _successor_positions.size());
    return _successor_positions[oldPos];
}