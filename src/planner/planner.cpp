
#include <iostream>
#include <functional>

#include "planner.h"
#include "parser/cwa.hpp"


void Planner::findPlan() {
    Names::init(_name_back_table);

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


    _instantiator = new Instantiator(_p, _var_ids, _constants_by_sort, _predicate_sorts_table);
    _effector_table = new EffectorTable(_p, _actions, _reductions, _task_id_to_reduction_ids);


    // Begin actual instantiation and solving loop
    int iteration = 0;
    printf("ITERATION %i\n", iteration++);

    Layer initLayer(initReduction.getSubtasks().size()+1);
    
    // Initial state
    std::unordered_map<int, SigSet> initState;
    for (ground_literal lit : init) {
        int predId = getNameId(lit.predicate);
        Signature sig(predId, getArguments(lit.args));
        if (!lit.positive) sig.negate();
        initLayer[0].addFact(sig);
        printf(" add fact %s @%i\n", Names::to_string(sig).c_str(), 0);
        initState[predId];
        initState[predId].insert(sig);
    }
    std::unordered_map<int, SigSet> state = initState;

    // For each position where there is an initial task
    for (int pos = 0; pos < initLayer.size()-1; pos++) {

        std::unordered_map<int, SigSet> newState(state);
        addToLayer(initReduction.getSubtasks()[pos], initLayer, pos, state, newState);
        state = newState;
    }

    // Goal state (?)
    for (ground_literal lit : goal) {
        Signature sig(getNameId(lit.predicate), getArguments(lit.args));
        if (!lit.positive) sig.negate();
        initLayer[initLayer.size()-1].addFact(sig);
        printf(" add fact %s @%i\n", Names::to_string(sig).c_str(), initLayer.size()-1);
    }

    initLayer.consolidate();

    // TODO sat encoding
    // TODO sat call
    bool solved = false;



    // Next layers

    Layer& oldLayer = initLayer;
    while (!solved) {
        
        printf("ITERATION %i\n", iteration++);
        
        Layer newLayer(oldLayer.getNextLayerSize());
        printf(" NEW_LAYER_SIZE %i\n", newLayer.size());

        state = initState;
        for (int oldPos = 0; oldPos < oldLayer.size(); oldPos++) {
            int newPos = oldLayer.getSuccessorPos(oldPos);

            for (int offset = 0; newPos+offset < oldLayer.getSuccessorPos(oldPos+1); offset++) {
                std::unordered_map<int, SigSet> newState(state);
                
                if (offset == 0) {
                    // Propagate facts
                    newLayer[newPos].setFacts(oldLayer[oldPos].getFacts());
                    
                    // Propagate actions (maintain variable values)
                    newLayer[newPos].setActions(oldLayer[oldPos].getFacts());
                }

                // Propagate reductions
                for (Signature parentSig : oldLayer[oldPos].getReductions()) {
                    Reduction parent = _reductions_by_sig[parentSig];
                    Signature parentTask = parent.getSubtasks()[offset];
                    addToLayer(parentTask, newLayer, newPos+offset, state, newState);
                }

                state = newState;
            }
        }

        newLayer.consolidate();

        // TODO sat encoding
        // TODO sat call
        solved = false;
        oldLayer = newLayer;
    }

    // TODO Extract solution
}

void Planner::addToLayer(Signature& task, Layer& layer, int pos, 
        std::unordered_map<int, SigSet>& state, std::unordered_map<int, SigSet>& newState) {
    
    if (_task_id_to_reduction_ids.count(task._name_id) == 0) {
        // Action
        Action& a = _actions[task._name_id];
        printf("init task #%i : action #%i found\n", task._name_id, task._name_id);
        std::vector<Action> actions = _instantiator->getApplicableInstantiations(a, state);
        
        // TODO Add any remaining constants in each action as q-constants

        for (Action action : actions) {
            Signature sig = action.getSignature();
            _actions_by_sig[sig] = action;
            printf(" add action %s @%i\n", Names::to_string(sig).c_str(), pos);

            // Add action to elements @pos
            layer[pos].addAction(sig);
            // Add preconditions to facts @pos
            for (Signature pre : action.getPreconditions()) {
                layer[pos].addFact(pre);
            }
            // Add effects to facts @pos+1
            for (Signature effect : _effector_table->getPossibleFactChanges(sig)) {
                layer[pos+1].addFact(effect);
                newState[effect._name_id].insert(effect);
            }
        }
    } else {
        // Reduction
        std::vector<int>& redIds = _task_id_to_reduction_ids[task._name_id];
        printf("init task #%i : %i reductions found\n", task._name_id, redIds.size());

        // Filter and minimally instantiate methods
        // applicable in current (super)state
        for (int redId : redIds) {
            Reduction& r = _reductions[redId];
            Signature origSig = r.getSignature();
            std::vector<Reduction> reductions = _instantiator->getMinimalApplicableInstantiations(r, state);
            printf("  reduction %s ~> %i instantiations\n", Names::to_string(origSig).c_str(), reductions.size());

            // TODO Add any remaining constants in each action as q-constants 

            for (Reduction red : reductions) {
                Signature sig = red.getSignature();
                _reductions_by_sig[sig] = red;
                printf("   add reduction %s @%i\n", Names::to_string(sig).c_str(), pos);

                // Add reduction to elements @pos
                layer[pos].addReduction(sig);
                layer[pos].addExpansionSize(red.getSubtasks().size());
                // Add preconditions to facts @pos
                for (Signature pre : red.getPreconditions()) {
                    layer[pos].addFact(pre);
                }
                // Add potential effects to facts @pos+1
                for (Signature effect : _effector_table->getPossibleFactChanges(sig)) {
                    layer[pos+1].addFact(effect);
                    newState[effect._name_id].insert(effect);
                }
            }
        }
    }
}