
#ifndef DOMPASCH_TREE_REXX_LAYER_H
#define DOMPASCH_TREE_REXX_LAYER_H

#include <vector>
#include <set>

#include "data/hashmap.h"
#include "data/signature.h"
#include "data/layer_state.h"
#include "util/names.h"
#include "data/position.h"

class Layer {

private:
    int _index;
    std::vector<Position> _content;
    LayerState _state;
    std::vector<int> _successor_positions;

public:
    Layer(int index, int size);

    int size() const;
    int index() const;
    int getNextLayerSize() const;
    int getSuccessorPos(int oldPos) const;
    LayerState& getState();
    
    Position& at(int pos);
    Position& operator[](int pos);
    Position& last();
    
    void consolidate();
};

#endif