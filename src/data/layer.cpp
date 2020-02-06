
#include "data/layer.h"

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
    return _successor_positions.back();
}
int Layer::getSuccessorPos(int oldPos) {return _successor_positions[oldPos];}