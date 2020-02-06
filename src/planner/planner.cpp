
#include <iostream>
#include <functional>

#include "planner.h"
#include "parser/cwa.hpp"
#include "sat/encoding.h"

void Planner::findPlan() {
    
    Encoding enc(_htn);

    // Begin actual instantiation and solving loop
    int iteration = 0;
    printf("ITERATION %i\n", iteration++);

    Layer initLayer(iteration, _htn._init_reduction.getSubtasks().size()+1);
    
    // Initial state
    std::unordered_map<int, SigSet> initState;
    for (ground_literal lit : init) {
        int predId = _htn.getNameId(lit.predicate);
        Signature sig(predId, _htn.getArguments(lit.args));
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
        addToLayer(_htn._init_reduction.getSubtasks()[pos], initLayer, pos, state, newState);
        state = newState;
        enc.addOccurringFacts(state, initLayer, pos);
    }

    // Goal state (?)
    for (ground_literal lit : goal) {
        Signature sig(_htn.getNameId(lit.predicate), _htn.getArguments(lit.args));
        if (!lit.positive) sig.negate();
        initLayer[initLayer.size()-1].addFact(sig);
        state[sig._name_id].insert(sig);
        printf(" add fact %s @%i\n", Names::to_string(sig).c_str(), initLayer.size()-1);
    }

    enc.addOccurringFacts(state, initLayer, initLayer.size()-1);
    initLayer.consolidate();

    enc.addAssumptions(initLayer);
    bool solved = enc.solve();

    // Next layers

    Layer& oldLayer = initLayer;
    while (!solved) {
        
        printf("ITERATION %i\n", iteration++);
        
        Layer newLayer(iteration, oldLayer.getNextLayerSize());
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
                    Reduction parent = _htn._reductions_by_sig[parentSig];
                    printf("  propagating reduction %s, offset %i\n", Names::to_string(parentSig).c_str(), offset);
                    if (offset < parent.getSubtasks().size()) {
                        Signature taskSig = parent.getSubtasks()[offset];   
                        addToLayer(taskSig, newLayer, newPos+offset, state, newState);
                    } else {
                        // TODO Add blank action
                    }
                }

                state = newState;
                enc.addOccurringFacts(state, newLayer, newPos);
            }
        }

        newLayer.consolidate();

        enc.addAssumptions(newLayer);
        solved = enc.solve();

        oldLayer = newLayer;
    }

    // TODO Extract solution
}

void Planner::addToLayer(Signature& task, Layer& layer, int pos, 
        std::unordered_map<int, SigSet>& state, std::unordered_map<int, SigSet>& newState) {
    
    int layerIdx = layer.index();

    if (_htn._task_id_to_reduction_ids.count(task._name_id) == 0) {
        // Action
        Action& a = _htn._actions[task._name_id];
        printf("  task %s : action found\n", Names::to_string(task).c_str());
        std::vector<Action> actions = _instantiator.getApplicableInstantiations(a, state);
        
        for (Action action : actions) {
            action = _htn.replaceQConstants(action, layerIdx, pos);
            Signature sig = action.getSignature();
            _htn._actions_by_sig[sig] = action;
            printf("   add action %s @%i\n", Names::to_string(sig).c_str(), pos);

            // Add action to elements @pos
            layer[pos].addAction(sig);
            // Add effects to facts @pos+1
            for (Signature effect : _htn.getAllFactChanges(sig)) {
                layer[pos+1].addFact(effect);
                printf("   add fact %s @%i\n", Names::to_string(effect).c_str(), pos);
                newState[effect._name_id].insert(effect);
            }
        }
    } else {
        // Reduction
        std::vector<int>& redIds = _htn._task_id_to_reduction_ids[task._name_id];
        printf("  task %s : %i reductions found\n", Names::to_string(task).c_str(), redIds.size());

        // Filter and minimally instantiate methods
        // applicable in current (super)state
        for (int redId : redIds) {
            Reduction& r = _htn._reductions[redId];
            r = r.substituteRed(Substitution::get(r.getTaskArguments(), task._args));
            Signature origSig = r.getSignature();
            std::vector<Reduction> reductions = _instantiator.getMinimalApplicableInstantiations(r, state);
            printf("   reduction %s ~> %i instantiations\n", Names::to_string(origSig).c_str(), reductions.size());

            for (Reduction red : reductions) {
                // Rename any remaining constants in each action as unique q-constants 
                red = _htn.replaceQConstants(red, layerIdx, pos);
                Signature sig = red.getSignature();
                _htn._reductions_by_sig[sig] = red;
                printf("    add reduction %s @%i\n", Names::to_string(sig).c_str(), pos);
                for (Signature subtask : red.getSubtasks()) {
                    printf("    - %s\n", Names::to_string(subtask).c_str());
                }

                // Add reduction to elements @pos
                layer[pos].addReduction(sig);
                layer[pos].addExpansionSize(red.getSubtasks().size());
                // Add potential effects to facts @pos+1
                for (Signature effect : _htn.getAllFactChanges(sig)) {
                    printf("    add fact %s @%i\n", Names::to_string(effect).c_str(), pos);
                    layer[pos+1].addFact(effect);
                    newState[effect._name_id].insert(effect);
                }
            }
        }
    }
}