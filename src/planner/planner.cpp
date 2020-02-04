
#include <iostream>

#include "planner.h"
#include "parser/cwa.hpp"

void Planner::findPlan() {

    for (predicate_definition p : predicate_definitions)
        extractPredSorts(p);
    for (task t : primitive_tasks)
        extractTaskSorts(t);
    for (task t : abstract_tasks)
        extractTaskSorts(t);
    for (method m : methods)
        extractMethodSorts(m);
    
    extractConstants();

    printf("Sorts extracted.\n");
    for (auto sort_pair : _p.sorts) {
        printf("%s : ", sort_pair.first.c_str());
        for (auto c : sort_pair.second) {
            printf("%s ", c.c_str());
        }
        printf("\n");
    }

    for (task t : primitive_tasks)
        getAction(t);
    Reduction& initReduction = getReduction(methods[0]);
    for (int mIdx = 1; mIdx < methods.size(); mIdx++)
        getReduction(methods[mIdx]);
    
    printf("%i operators and %i methods created.\n", _actions.size(), _reductions.size());

    Layer initLayer(initReduction.getSubtasks().size()+1);
    
    // Initial state
    for (ground_literal lit : init) {
        Signature sig(getNameId(lit.predicate), getArguments(lit.args));
        if (!lit.positive) sig.negate();
        initLayer[0].addFact(getFact(sig));
    }

    // For each position where there is an initial task
    for (int pos = 0; pos < initLayer.size()-1; pos++) {
        Signature& initTask = initReduction.getSubtasks()[pos];
        std::vector<int>& redIds = _task_id_to_reduction_ids[initTask._name_id];
        printf("init task #%i : %i reductions found\n", initTask._name_id, redIds.size());

        // TODO filter and minimally instantiate methods
        // applicable in current (super)state

        // TODO filter and fully instantiate actions
        // applicable in current (super)state

        // TODO get all facts potentially changing by methods,
        // apply to (super)state (need reduction/action effector data structure)
        
        // TODO get all action effects, apply to (super)state
    }

    // Goal state (?)
    for (ground_literal lit : goal) {
        Signature sig(getNameId(lit.predicate), getArguments(lit.args));
        if (!lit.positive) sig.negate();
        initLayer[initLayer.size()-1].addFact(getFact(sig));
    }


    // SAT ENCODING
    // SAT CALL


    // Next layers:

    // Propagate reductions: do like for initial tasks from init reduction, 
    // but now for last layer's reductions

    // Propagate actions (maintain variable values)

    // Propagate facts
}