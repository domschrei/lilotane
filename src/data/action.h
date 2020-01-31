
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
    Action(int nameId, std::vector<Argument> args) : HtnOp(nameId, args) {}
};

#endif