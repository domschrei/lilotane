
#ifndef DOMPASCH_TREE_REXX_ACTION_H
#define DOMPASCH_TREE_REXX_ACTION_H

#include <vector>

#include "util/hashmap.h"
#include "data/htn_op.h"
#include "data/signature.h"

class Action : public HtnOp {
    
public:
    Action();
    Action(const HtnOp& op);
    Action(const Action& a);
    Action(int nameId, const std::vector<int>& args);
    Action(int nameId, std::vector<int>&& args);

    Action& operator=(const Action& op);
};

#endif