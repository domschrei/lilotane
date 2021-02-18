
#ifndef DOMPASCH_LILOTANE_PLAN_OPTIMIZER_H
#define DOMPASCH_LILOTANE_PLAN_OPTIMIZER_H

#include "data/layer.h"
#include "data/htn_instance.h"
#include "data/plan.h"
#include "sat/sat_interface.h"
#include "sat/variable_provider.h"
#include "sat/encoding.h"

class PlanOptimizer {

private:
    HtnInstance& _htn;
    std::vector<Layer*>& _layers;
    Encoding& _enc;
    SatInterface& _sat;
    EncodingStatistics& _stats;

public:
    PlanOptimizer(HtnInstance& htn, std::vector<Layer*>& layers, Encoding& enc) : 
            _htn(htn), _layers(layers), _enc(enc), 
            _sat(_enc.getSatInterface()), _stats(_enc.getEncodingStatistics()) {}

    enum ConstraintAddition { TRANSIENT, PERMANENT };

    void optimizePlan(int upperBound, Plan& plan, ConstraintAddition mode);

    int findMinBySat(int lower, int upper, std::function<int(int)> varMap, 
                std::function<int(void)> boundUpdateOnSat, ConstraintAddition mode);

    bool isEmptyAction(const USignature& aSig);
    int getPlanLength(const std::vector<PlanItem>& classicalPlan);
};

#endif