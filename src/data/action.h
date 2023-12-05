
#ifndef DOMPASCH_TREE_REXX_ACTION_H
#define DOMPASCH_TREE_REXX_ACTION_H

#include <string>
#include <vector>

#include "util/hashmap.h"
#include "data/htn_op.h"
#include "data/signature.h"

class Action : public HtnOp
{

public:
    Action();
    Action(const HtnOp &op);
    Action(const Action &a);
    Action(int nameId, const std::basic_string<int> &args);
    Action(int nameId, std::basic_string<int> &&args);

    Action &operator=(const Action &op);
};

#endif