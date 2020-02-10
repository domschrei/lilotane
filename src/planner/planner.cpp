
#include <iostream>
#include <functional>
#include <regex>

#include "planner.h"
//#include "parser/cwa.hpp"

void Planner::findPlan() {
    
    // Begin actual instantiation and solving loop
    int iteration = 0;
    printf("ITERATION %i\n", iteration);

    int initSize = _htn._init_reduction.getSubtasks().size()+1;
    printf("Creating initial layer of size %i\n", initSize);
    Layer initLayer(iteration, initSize);
    
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
    for (auto pair : initState) _enc.addTrueFacts(pair.second, initLayer, 0);
    std::unordered_map<int, SigSet> state = initState;

    // For each position where there is an initial task
    for (int pos = 0; pos < initLayer.size()-1; pos++) {

        std::unordered_map<int, SigSet> newState(state);
        Signature subtask = _htn._init_reduction.getSubtasks()[pos];
        std::vector<int> newArgs;
        for (int arg : subtask._args) {
            std::string name = _htn._name_back_table[arg];
            std::smatch matches;
            if (std::regex_match(name, matches, std::regex("\\?var_for_(.*)_[0-9]+"))) {
                name = matches.str(1);
                newArgs.push_back(_htn.getNameId(name));
            } else {
                printf("%s was not matched by initial task argname substitution!\n", name.c_str());
                exit(1);
            }
        }
        assert(newArgs.size() == subtask._args.size());
        subtask = subtask.substitute(Substitution::get(subtask._args, newArgs));

        std::vector<Signature> added = addToLayer(NULL, subtask, initLayer, pos, state, newState);

        handleAddedHtnOps(added, initLayer, pos, initLayer, pos, state, newState);

        state = newState;

        _enc.consolidateHtnOps(initLayer, pos);
        _enc.consolidateFacts(initLayer, pos);
        _enc.addInitialTasks(initLayer, pos);
    }

    // Goal state (?)
    SigSet goalSet;
    for (ground_literal lit : goal) {
        Signature sig(_htn.getNameId(lit.predicate), _htn.getArguments(lit.args));
        if (!lit.positive) sig.negate();
        initLayer[initLayer.size()-1].addFact(sig);
        state[sig._name_id].insert(sig);
        goalSet.insert(sig);
        printf(" add fact %s @%i\n", Names::to_string(sig).c_str(), initLayer.size()-1);
    }
    _enc.addTrueFacts(goalSet, initLayer, initLayer.size()-1);
    _enc.consolidateFacts(initLayer, initLayer.size()-1);

    initLayer.consolidate();

    _enc.addAssumptions(initLayer);
    bool solved = _enc.solve();

    
    // Next layers

    int maxIterations = 20;
    std::vector<Layer> allLayers;
    allLayers.push_back(initLayer);

    while (!solved && iteration < maxIterations) {
        Layer& oldLayer = allLayers.back();
        printf("Unsolvable at layer %i\n", oldLayer.index());  
        iteration++;      
        printf("ITERATION %i\n", iteration);
        
        Layer newLayer(iteration, oldLayer.getNextLayerSize());
        printf(" NEW_LAYER_SIZE %i\n", newLayer.size());

        state = initState;
        for (int oldPos = 0; oldPos < oldLayer.size(); oldPos++) {
            int newPos = oldLayer.getSuccessorPos(oldPos);
            int maxOffset = oldLayer[oldPos].getMaxExpansionSize();

            for (int offset = 0; offset < maxOffset; offset++) {
                //printf("%i,%i,%i,%i\n", oldPos, newPos, offset, newLayer.size());
                assert(newPos+offset < newLayer.size());
                std::unordered_map<int, SigSet> newState(state);
                
                std::vector<Signature> added;

                if (offset == 0) {
                    // Propagate facts
                    newLayer[newPos].setFacts(oldLayer[oldPos].getFacts());
                    _enc.propagateFacts(oldLayer[oldPos].getFacts(), oldLayer, oldPos, newLayer, newPos);
                    
                    // Propagate actions (maintain variable values)
                    for (Signature aSig : oldLayer[oldPos].getActions()) {
                        newLayer[newPos].addAction(aSig);
                        added.push_back(aSig);
                        added.push_back(aSig); // action is its own parent
                    }
                }

                // Propagate reductions
                for (Signature parentSig : oldLayer[oldPos].getReductions()) {
                    Reduction parent = _htn._reductions_by_sig[parentSig];
                    printf("  propagating reduction %s, offset %i\n", Names::to_string(parentSig).c_str(), offset);
                    Signature sig;
                    if (offset < parent.getSubtasks().size()) {
                        sig = parent.getSubtasks()[offset];   
                    } else {
                        // Add blank action
                        sig = _htn._action_blank.getSignature();
                    }
                    std::vector<Signature> addedNow = addToLayer(&parent, sig, newLayer, newPos+offset, state, newState);
                    added.insert(added.end(), addedNow.begin(), addedNow.end());
                }

                // Handle computed htn ops and all possible fact changes
                // (Must be done AFTER all possible reductions and actions were instantiated;
                // q-constants may have been added which need to be considered for fact changes, too) 
                handleAddedHtnOps(added, oldLayer, oldPos, newLayer, newPos+offset, state, newState);

                // Finalize set of facts at this position
                state = newState;
                _enc.consolidateHtnOps(newLayer, newPos+offset);
                _enc.consolidateFacts(newLayer, newPos+offset);
            }

            // Finalize reductions at the old layer, old position
            for (Signature parentSig : oldLayer[oldPos].getReductions()) {
                Reduction parent = _htn._reductions_by_sig[parentSig];
                _enc.consolidateReductionExpansion(parent, oldLayer, oldPos, newLayer, newPos);
            }
        }

        newLayer.consolidate();

        _enc.addAssumptions(newLayer);
        solved = _enc.solve();

        allLayers.push_back(newLayer);
    }

    if (!solved) {
        printf("Limit exceeded. Terminating.\n");
        exit(0);
    }

    printf("Found a solution after %i layers\n", allLayers.size());

    // Extract solution
    std::vector<PlanItem> actionPlan = _enc.extractClassicalPlan(allLayers.back());
    std::vector<PlanItem> decompPlan = _enc.extractDecompositionPlan(allLayers);

    printf("==>\n");
    std::unordered_set<int> actionIds;
    for (PlanItem item : actionPlan) {
        actionIds.insert(item.id);
        printf("%i %s\n", item.id, Names::to_string_nobrackets(item.abstractTask).c_str());
    }

    bool root = true;
    for (PlanItem item : decompPlan) {
        std::string subtaskIdStr;
        for (int subtaskId : item.subtaskIds) subtaskIdStr += " " + std::to_string(subtaskId);
        
        if (root) {
            printf("root%s\n", subtaskIdStr.c_str());
            root = false;
            continue;
        } else if (item.id == 0 || actionIds.count(item.id)) continue;
        
        printf("%i %s -> %s%s\n", item.id, Names::to_string_nobrackets(item.abstractTask).c_str(),
                Names::to_string_nobrackets(item.reduction).c_str(), subtaskIdStr.c_str());
    }
    printf("<==\n");

    printf("End of solution plan.\n");
}

void Planner::handleAddedHtnOps(std::vector<Signature>& added, 
        Layer& oldLayer, int oldPos, Layer& newLayer, int newPos, 
        std::unordered_map<int, SigSet>& state, std::unordered_map<int, SigSet>& newState) {

    bool propagation = newLayer.index() > 0;

    for (int sigIdx = 0; sigIdx < added.size(); ) {
        
        Signature sig, sigParent;
        sig = added[sigIdx++];
        if (propagation) sigParent = added[sigIdx++];

        SigSet factChanges = _htn.getAllFactChanges(sig);

        // Add htn op to the encoding
        if (_htn._reductions_by_sig.count(sig)) {
            // Reduction
            Reduction& r = _htn._reductions_by_sig[sig];
            if (propagation) {
                _enc.addReduction(r, _htn._reductions_by_sig[sigParent], factChanges, oldLayer, oldPos, newLayer, newPos);
            } else {
                _enc.addReduction(r, factChanges, oldLayer, oldPos);
            }
        } else {
            // Action
            assert(_htn._actions_by_sig.count(sig));
            Action& a = _htn._actions_by_sig[sig];
            if (propagation) {
                if (_htn._actions_by_sig.count(sigParent)) {
                    // Parent is an action -> propagate action
                    _enc.propagateAction(a, oldLayer, oldPos, newLayer, newPos);
                } else {
                    // Parent is a reduction -> add action as part of an expansion
                    _enc.addAction(a, _htn._reductions_by_sig[sigParent], oldLayer, oldPos, newLayer, newPos);
                }
            } else {
                _enc.addAction(a, oldLayer, oldPos);
            }
        }

        // Apply possible fact changes
        for (Signature effect : factChanges) {
            newLayer[newPos+1].addFact(effect);
            //printf("   add fact %s @%i\n", Names::to_string(effect).c_str(), newPos+offset);
            newState[effect._name_id].insert(effect);
        }
    }
}

std::vector<Signature> Planner::addToLayer(Reduction* parent, Signature& task, Layer& layer, int pos, 
        std::unordered_map<int, SigSet>& state, std::unordered_map<int, SigSet>& newState) {
    
    int layerIdx = layer.index();

    std::vector<Signature> result;

    if (_htn._task_id_to_reduction_ids.count(task._name_id) == 0) {
        // Action
        assert(_htn._actions.count(task._name_id));
        Action& a = _htn._actions[task._name_id];
        HtnOp op = a.substitute(Substitution::get(a.getArguments(), task._args));
        Action act = (Action) op;
        printf("  task %s : action found: %s\n", Names::to_string(task).c_str(), Names::to_string(act).c_str());

        std::vector<Action> actions = _instantiator.getApplicableInstantiations(act, state);
        
        for (Action action : actions) {
            action = _htn.replaceQConstants(action, layerIdx, pos);
            Signature sig = action.getSignature();
            _htn._actions_by_sig[sig] = action;
            printf("   add action %s @%i\n", Names::to_string(sig).c_str(), pos);

            // Add action to elements @pos
            layer[pos].addAction(sig);

            result.push_back(sig);
            if (parent != NULL) result.push_back(parent->getSignature());

            /*
            // Add effects to facts @pos+1
            for (Signature effect : _htn.getAllFactChanges(sig)) {
                layer[pos+1].addFact(effect);
                printf("   add fact %s @%i\n", Names::to_string(effect).c_str(), pos);
                newState[effect._name_id].insert(effect);
            }*/
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
                result.push_back(sig);
                if (parent != NULL) result.push_back(parent->getSignature());

                // Add reduction to elements @pos
                layer[pos].addReduction(sig);
                layer[pos].addExpansionSize(red.getSubtasks().size());

                /*
                // Add potential effects to facts @pos+1
                for (Signature effect : _htn.getAllFactChanges(sig)) {
                    printf("    add fact %s @%i\n", Names::to_string(effect).c_str(), pos);
                    layer[pos+1].addFact(effect);
                    newState[effect._name_id].insert(effect);
                }*/
            }
        }
    }

    return result;
}