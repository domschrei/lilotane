
#ifndef DOMPASCH_TREE_REXX_ACTION_H
#define DOMPASCH_TREE_REXX_ACTION_H

#include <vector>
#include <unordered_set>

#include "data/htn_op.h"
#include "data/signature.h"
#include "data/bound_condition.h"

class Action : public HtnOp {
    
public:
    Action() : HtnOp() {}
    Action(HtnOp& op) : HtnOp(op) {}
    Action(Action& a) : HtnOp(a._id, a._args) {
        _preconditions = (a._preconditions);
        _effects = (a._effects); 
    }
    Action(int nameId, std::vector<int> args) : HtnOp(nameId, args) {}

    Action& operator=(const Action& op) {
        _id = op._id;
        _args = op._args;
        _preconditions = op._preconditions;
        _effects = op._effects;
        return *this;
    }
};

#endif