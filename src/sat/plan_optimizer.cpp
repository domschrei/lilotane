
#include "sat/plan_optimizer.h"

void PlanOptimizer::optimizePlan(int upperBound, Plan& plan, ConstraintAddition mode) {

    int layerIdx = _layers.size()-1;
    Layer& l = *_layers.at(layerIdx);
    int currentPlanLength = upperBound;
    Log::v("PLO BEGIN %i\n", currentPlanLength);

    // Add counting mechanism
    _stats.begin(STAGE_PLANLENGTHCOUNTING);
    int minPlanLength = 0;
    int maxPlanLength = 0;
    std::vector<int> planLengthVars(1, VariableDomain::nextVar());
    Log::d("VARNAME %i (plan_length_equals %i %i)\n", planLengthVars[0], 0, 0);
    // At position zero, the plan length is always equal to zero
    _sat.addClause(planLengthVars[0]);
    for (size_t pos = 0; pos+1 < l.size(); pos++) {

        // Collect sets of potential operations
        FlatHashSet<int> emptyActions, actualActions;
        for (const auto& aSig : l.at(pos).getActions()) {
            Log::d("PLO %i %s?\n", pos, TOSTR(aSig));
            int aVar = l.at(pos).getVariable(VarType::OP, aSig);
            if (isEmptyAction(aSig)) {
                emptyActions.insert(aVar);
            } else {
                actualActions.insert(aVar);
            }
        }
        for (const auto& rSig : l.at(pos).getReductions()) {
            Log::d("PLO %i %s?\n", pos, TOSTR(rSig));
            if (_htn.getOpTable().getReduction(rSig).getSubtasks().size() == 0) {
                // Empty reduction
                emptyActions.insert(l.at(pos).getVariable(VarType::OP, rSig));
            }
        }

        if (emptyActions.empty()) {
            // Only actual actions here: Increment lower and upper bound, keep all variables.
            minPlanLength++;
            bool increaseUpperBound = maxPlanLength < currentPlanLength;
            if (increaseUpperBound) maxPlanLength++;
            else {
                // Upper bound hit!
                // Cut counter variables by one, forbid topmost one
                _sat.addClause(-planLengthVars.back());
                planLengthVars.resize(planLengthVars.size()-1);
            }
            Log::d("[no empty ops]\n");
        } else if (actualActions.empty()) {
            // Only empty actions here: Keep current bounds, keep all variables.
            Log::d("[only empty ops]\n");
        } else {
            // Mix of actual and empty actions here: Increment upper bound, 
            bool increaseUpperBound = maxPlanLength < currentPlanLength;
            if (increaseUpperBound) maxPlanLength++;

            int emptySpotVar = 0;
            bool encodeDirectly = emptyActions.size() <= 3 || actualActions.size() <= 3;
            bool encodeEmptiesOnly = emptyActions.size() < actualActions.size();
            bool encodeActualsOnly = emptyActions.size() > actualActions.size();
            if (!encodeDirectly) {
                // Encode with a helper variable
                emptySpotVar = VariableDomain::nextVar();

                // Define for each action var whether it implies an empty spot or not
                for (int v : emptyActions) {
                    // IF the empty action occurs, THEN the spot is empty.
                    _sat.addClause(-v, emptySpotVar);
                }
                for (int v : actualActions) {
                    // IF the actual action occurs, THEN the spot is not empty.
                    _sat.addClause(-v, -emptySpotVar);
                }
            }

            // create new variables and constraints.
            std::vector<int> newPlanLengthVars(planLengthVars.size()+(increaseUpperBound?1:0));
            for (size_t i = 0; i < newPlanLengthVars.size(); i++) {
                newPlanLengthVars[i] = VariableDomain::nextVar();
            }

            // Propagate plan length from previous position to new position
            for (size_t i = 0; i < planLengthVars.size(); i++) {
                int prevVar = planLengthVars[i];
                int keptPlanLengthVar = newPlanLengthVars[i];

                if (i+1 < newPlanLengthVars.size()) {
                    // IF previous plan length is X AND spot is empty
                    // THEN the plan length stays X 
                    if (encodeDirectly) {
                        if (encodeEmptiesOnly) {
                            for (int v : emptyActions) {
                                _sat.addClause(-prevVar, -v, keptPlanLengthVar);
                            }
                        } else {
                            _sat.appendClause(-prevVar, keptPlanLengthVar);
                            for (int v : actualActions) _sat.appendClause(v);
                            _sat.endClause();
                        }
                    } else {
                        _sat.addClause(-prevVar, -emptySpotVar, keptPlanLengthVar);
                    }
                    
                    // IF previous plan length is X AND here is a non-empty spot 
                    // THEN the plan length becomes X+1
                    int incrPlanLengthVar = newPlanLengthVars[i+1];
                    if (encodeDirectly) {
                        if (encodeActualsOnly) {
                            for (int v : actualActions) {
                                _sat.addClause(-prevVar, -v, incrPlanLengthVar);
                            }
                        } else {
                            _sat.appendClause(-prevVar, incrPlanLengthVar);
                            for (int v : emptyActions) _sat.appendClause(v);
                            _sat.endClause();
                        }
                    } else {
                        _sat.addClause(-prevVar, emptySpotVar, incrPlanLengthVar);
                    }

                } else {
                    // Limit hit -- no more actions are admitted
                    // IF previous plan length is X THEN this spot must be empty
                    if (encodeDirectly) {
                        if (encodeActualsOnly) {
                            for (int v : actualActions) {
                                _sat.addClause(-prevVar, -v);
                            }
                        } else {
                            _sat.appendClause(-prevVar);
                            for (int v : emptyActions) _sat.appendClause(v);
                            _sat.endClause();
                        }
                    } else {
                        _sat.addClause(-prevVar, emptySpotVar);
                    }
                    // IF previous plan length is X THEN the plan length stays X
                    _sat.addClause(-prevVar, keptPlanLengthVar);
                }
            }
            planLengthVars = newPlanLengthVars;
        }

        Log::v("Position %i: Plan length bounds [%i,%i]\n", pos, minPlanLength, maxPlanLength);
    }

    Log::i("Tightened initial plan length bounds at layer %i: [0,%i] => [%i,%i]\n",
            layerIdx, l.size()-1, minPlanLength, maxPlanLength);
    assert((int)planLengthVars.size() == maxPlanLength-minPlanLength+1 || Log::e("%i != %i-%i+1\n", planLengthVars.size(), maxPlanLength, minPlanLength));
    
    // Add primitiveness of all positions at the final layer
    // as unit literals (instead of assumptions)
    _enc.addAssumptions(layerIdx, /*permanent=*/mode == ConstraintAddition::PERMANENT);
    _stats.end(STAGE_PLANLENGTHCOUNTING);

    int curr = currentPlanLength;
    currentPlanLength = findMinBySat(minPlanLength, std::min(maxPlanLength, currentPlanLength), 
        // Variable mapping
        [&](int currentMax) {
            return planLengthVars[currentMax-minPlanLength];
        }, 
        // Bound update on SAT 
        [&]() {
            // SAT: Shorter plan found!
            plan = _enc.extractPlan();
            int newPlanLength = getPlanLength(std::get<0>(plan));
            Log::i("Shorter plan (length %i) found\n", newPlanLength);
            assert(newPlanLength < curr);
            curr = newPlanLength;
            return newPlanLength;
        }, mode);

    float factor = (float)currentPlanLength / minPlanLength;
    if (factor <= 1) {
        Log::v("Plan is globally optimal (static lower bound: %i)\n", minPlanLength);
    } else if (minPlanLength == 0) {
        Log::v("Plan may be arbitrarily suboptimal (static lower bound: 0)\n");
    } else {
        Log::v("Plan may be suboptimal by a maximum factor of %.2f (static lower bound: %i)\n", factor, minPlanLength);
    }
}

int PlanOptimizer::findMinBySat(int lower, int upper, std::function<int(int)> varMap, 
            std::function<int(void)> boundUpdateOnSat, ConstraintAddition mode) {

    int originalUpper = upper;
    int current = upper;

    // Solving iterations
    while (true) {
        // Hit lower bound of possible plan lengths? 
        if (current == lower) {
            Log::v("PLO END %i\n", current);
            Log::i("Length of current plan is at lower bound (%i): finished\n", lower);
            break;
        }

        // Assume a shorter plan by one
        _stats.begin(STAGE_PLANLENGTHCOUNTING);

        // Permanently forbid any plan lengths greater than / equal to the last found plan
        if (mode == TRANSIENT) {
            upper = originalUpper;
        }
        while (upper > current) {
            Log::d("GUARANTEE PL!=%i\n", upper);
            int probedVar = varMap(upper);
            if (mode == TRANSIENT) _sat.assume(-probedVar);
            else _sat.addClause(-probedVar);
            upper--;
        }
        assert(upper == current);
        
        // Assume a plan length shorter than the last found plan
        Log::d("GUARANTEE PL!=%i\n", upper);
        int probedVar = varMap(upper);
        if (mode == TRANSIENT) _sat.assume(-probedVar);
        else _sat.addClause(-probedVar);

        _stats.end(STAGE_PLANLENGTHCOUNTING);

        Log::i("Searching for a plan of length < %i\n", upper);
        int result = _enc.solve();

        // Check result
        if (result == 10) {
            // SAT: Shorter plan found!
            current = boundUpdateOnSat();
            Log::v("PLO UPDATE %i\n", current);
        } else if (result == 20) {
            // UNSAT
            Log::v("PLO END %i\n", current);
            break;
        } else {
            // UNKNOWN
            Log::v("PLO END %i\n", current);
            break;
        }
    }

    return current;
}

bool PlanOptimizer::isEmptyAction(const USignature& aSig) {
    if (_htn.getBlankActionSig() == aSig)
        return true;
    if (_htn.getActionNameFromRepetition(aSig._name_id) == _htn.getBlankActionSig()._name_id)
        return true;
    return false;
}

int PlanOptimizer::getPlanLength(const std::vector<PlanItem>& classicalPlan) {
    int currentPlanLength = 0;
    for (size_t pos = 0; pos+1 < classicalPlan.size(); pos++) {
        const auto& aSig = classicalPlan[pos].abstractTask;
        Log::d("%s\n", TOSTR(aSig));
        // Reduction with an empty expansion?
        if (aSig._name_id < 0) continue;
        // No blank action, no second part of a split action?
        else if (!isEmptyAction(aSig)) {
            currentPlanLength++;
        }
    }
    return currentPlanLength;
}
