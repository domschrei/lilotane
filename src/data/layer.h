
#ifndef DOMPASCH_TREE_REXX_LAYER_H
#define DOMPASCH_TREE_REXX_LAYER_H

#include <vector>
#include <set>

#include "data/position.h"

class Layer {

private:
    size_t _index;
    std::vector<Position> _content;
    std::vector<size_t> _successor_positions;

public:
    Layer(size_t index, size_t size);

    size_t size() const;
    size_t index() const;
    size_t getNextLayerSize() const;
    size_t getSuccessorPos(size_t oldPos) const;
    std::pair<size_t, size_t> getPredecessorPosAndOffset(size_t thisPos) const;
    
    Position& at(size_t pos);
    Position& operator[](size_t pos);
    Position& last();
    
    void consolidate();
};

#endif