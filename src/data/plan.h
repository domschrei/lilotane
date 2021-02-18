
#ifndef DOMPASCH_LILOTANE_PLAN_H
#define DOMPASCH_LILOTANE_PLAN_H

#include "data/signature.h"

struct PlanItem {
    int id = -1;
    USignature abstractTask;
    USignature reduction;
    std::vector<int> subtaskIds;
    
    PlanItem() {
        id = -1;
        abstractTask = Sig::NONE_SIG;
        reduction = Sig::NONE_SIG;
        subtaskIds = std::vector<int>(0);
    }
    PlanItem(int id, const USignature& abstractTask, const USignature& reduction, const std::vector<int> subtaskIds) :
        id(id), abstractTask(abstractTask), reduction(reduction), subtaskIds(subtaskIds) {}
};

typedef std::pair<std::vector<PlanItem>, std::vector<PlanItem>> Plan;

#endif
