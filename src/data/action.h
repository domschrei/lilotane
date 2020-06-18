
#ifndef DOMPASCH_TREE_REXX_ACTION_H
#define DOMPASCH_TREE_REXX_ACTION_H

#include <vector>

#include "data/hashmap.h"
#include "data/htn_op.h"
#include "data/signature.h"

class Action : public HtnOp {
    
public:
    Action();
    Action(HtnOp& op);
    Action(const Action& a);
    Action(int nameId, std::vector<int> args);

    Action& operator=(const Action& op);
};

#endif