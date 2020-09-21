
#include <assert.h>

#include "data/layer.h"
#include "sat/variable_domain.h"
#include "util/log.h"

const USignature Position::NONE_SIG = USignature(-1, std::vector<int>());
const USigSet Position::EMPTY_USIG_SET;
const SigSet Position::EMPTY_SIG_SET;
const NodeHashMap<USignature, USigSet, USignatureHasher> Position::EMPTY_USIG_TO_USIG_SET_MAP;

Layer::Layer(size_t index, size_t size) : _index(index), _content(size) {
    assert(size > 0);
}
size_t Layer::size() const {return _content.size();}
size_t Layer::index() const {return _index;}
Position& Layer::operator[](size_t pos) {assert(pos < size()); return _content[pos];}
Position& Layer::at(size_t pos) {return (*this)[pos];}
Position& Layer::last() {return (*this)[size()-1];}
void Layer::consolidate() {
    int succ = 0;
    for (size_t pos = 0; pos < size(); pos++) {
        _successor_positions.push_back(succ);
        succ += _content[pos].getMaxExpansionSize();
    }
}
size_t Layer::getNextLayerSize() const {
    return _successor_positions.back()+1;
}
size_t Layer::getSuccessorPos(size_t oldPos) const {
    assert(oldPos < _successor_positions.size());
    return _successor_positions[oldPos];
}
