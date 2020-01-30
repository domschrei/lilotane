
#ifndef DOMPASCH_TREE_REXX_ACTION_H
#define DOMPASCH_TREE_REXX_ACTION_H

#include <vector>
#include <unordered_set>

#include "data/signature.h"

class Action {

private:
    int _name_id;
    std::vector<int> _args;

    std::unordered_set<Signature> _preconditions;
    std::unordered_set<Signature> _effects;
    
public:
    Action(int nameId, std::vector<int> args) : _name_id(nameId), _args(args) {}
    bool isFullyGround() {
        for (int arg : _args) {
            if (arg < 0) return false;
        }
        return true;
    }
    Signature getSignature() {
        return Signature(_name_id, _args);
    }
};

#endif