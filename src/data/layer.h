
#ifndef DOMPASCH_TREE_REXX_LAYER_H
#define DOMPASCH_TREE_REXX_LAYER_H

#include <vector>
#include <unordered_set>

struct Position {

    std::unordered_set<int> _pos_facts;
    std::unordered_set<int> _neg_facts;
    std::unordered_set<int> _actions;
    std::unordered_set<int> _reductions;
    int _max_expansion_size = 1;

    void addFact(int fact) {(fact > 0 ? _pos_facts : _neg_facts).insert(std::abs(fact));}
    void addAction(int action) {_actions.insert(action);}
    void addReduction(int reduction) {_reductions.insert(reduction);}
    void addExpansionSize(int size) {_max_expansion_size = std::max(_max_expansion_size, size);}

    bool containsFactPos(int fact) const {return _pos_facts.count(fact) > 0;}
    bool containsFactNeg(int fact) const {return _neg_facts.count(fact) > 0;}
    bool containsAction(int action) const {return _actions.count(action) > 0;}
    bool containsReduction(int red) const {return _reductions.count(red) > 0;}

    const std::unordered_set<int>& getPosFacts() const {return _pos_facts;}
    const std::unordered_set<int>& getNegFacts() const {return _neg_facts;}
    const std::unordered_set<int>& getActions() const {return _actions;}
    const std::unordered_set<int>& getReductions() const {return _reductions;}
    int getMaxExpansionSize() const {return _max_expansion_size;}
};

class Layer {

private:
    std::vector<Position> _content;
    std::vector<int> _successor_positions;

public:
    int size() {return _content.size();}
    Position& operator[](int pos) {return _content[pos];}
    void consolidate() {
        int succ = 0;
        for (int pos = 0; pos < size(); pos++) {
            _successor_positions.push_back(succ);
            succ += _content[pos].getMaxExpansionSize();
        }
    }
};

#endif