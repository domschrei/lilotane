
#ifndef DOMPASCH_TREE_REXX_REDUCTION_H
#define DOMPASCH_TREE_REXX_REDUCTION_H

#include <vector>
#include <unordered_set>

#include "data/signature.h"

class Reduction {

private:
    int _name_id;
    std::vector<int> _args;

    std::unordered_set<Signature> _preconditions;
    std::vector<Signature> _subtasks;

public:
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