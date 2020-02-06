
#ifndef DOMPASCH_TREE_REXX_LAYER_H
#define DOMPASCH_TREE_REXX_LAYER_H

#include <vector>
#include <unordered_set>

#include "data/signature.h"

struct Position {

    SigSet _facts;
    SigSet _actions;
    SigSet _reductions;
    int _max_expansion_size = 1;

    void addFact(Signature& fact) {_facts.insert(fact);}
    void addAction(Signature& action) {_actions.insert(action);}
    void addReduction(Signature& reduction) {_reductions.insert(reduction);}
    void addExpansionSize(int size) {_max_expansion_size = std::max(_max_expansion_size, size);}

    void setFacts(const SigSet& facts) {_facts = facts;}
    void setActions(const SigSet& actions) {_actions = actions;}

    bool containsFact(Signature& fact) const {return _facts.count(fact) > 0;}
    bool containsAction(Signature& action) const {return _actions.count(action) > 0;}
    bool containsReduction(Signature& red) const {return _reductions.count(red) > 0;}

    const SigSet& getFacts() const {return _facts;}
    const SigSet& getActions() const {return _actions;}
    const SigSet& getReductions() const {return _reductions;}
    int getMaxExpansionSize() const {return _max_expansion_size;}
};

class Layer {

private:
    int _index;
    std::vector<Position> _content;
    std::vector<int> _successor_positions;

public:
    Layer(int index, int size) : _index(index) {_content.resize(size);}
    int size() {return _content.size();}
    int index() {return _index;}
    Position& operator[](int pos) {return _content[pos];}
    void consolidate() {
        int succ = 0;
        for (int pos = 0; pos < size(); pos++) {
            _successor_positions.push_back(succ);
            succ += _content[pos].getMaxExpansionSize();
        }
    }
    int getNextLayerSize() {
        return _successor_positions.back();
    }
    int getSuccessorPos(int oldPos) {return _successor_positions[oldPos];}
};

#endif