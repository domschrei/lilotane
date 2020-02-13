
#include <iostream>
#include <functional>
#include <regex>

#include "planner.h"
#include "util/log.h"
//#include "parser/cwa.hpp"

void Planner::findPlan() {
    
    // Begin actual instantiation and solving loop
    int iteration = 0;
    printf("ITERATION %i\n", iteration);

    int initSize = _htn._init_reduction.getSubtasks().size()+1;
    printf("Creating initial layer of size %i\n", initSize);
    _layer_idx = 0;
    _pos = 0;
    _layers.push_back(Layer(iteration, initSize));
    Layer& initLayer = _layers[0];
    initLayer[_pos].setPos(_layer_idx, _pos);
    
    // Initial state
    SigSet initState = _htn.getInitState();
    for (Signature fact : initState) {
        initLayer[_pos].addFact(fact.abs());
        initLayer[_pos].addTrueFact(fact);
        initLayer[_pos].extendState(fact);
    }

    // For each position where there is an initial task
    while (_pos+1 < initLayer.size()) {
        if (_pos > 0) createNext(initLayer[_pos-1]);

        Signature subtask = _htn.getInitTaskSignature(_pos);
        for (Signature rSig : getAllReductionsOfTask(subtask, initLayer[_pos].getState())) {
            initLayer[_pos].addReduction(rSig);
            initLayer[_pos].addExpansionSize(_htn._reductions_by_sig[rSig].getSubtasks().size());
            // Add preconditions
            for (Signature fact : _htn._reductions_by_sig[rSig].getPreconditions()) {
                initLayer[_pos].addFact(fact, Reason(_layer_idx, _pos, rSig));
                initLayer[_pos].extendState(fact);
                for (Signature decFact : _htn.getDecodedFacts(fact)) {
                    initLayer[_pos].addFact(decFact, Reason(_layer_idx, _pos, fact.abs()));
                    initLayer[_pos].extendState(decFact);
                }
            }
        }
        for (Signature aSig : getAllActionsOfTask(subtask, initLayer[_pos].getState())) {
            initLayer[_pos].addAction(aSig);
            // Add preconditions
            for (Signature fact : _htn._actions_by_sig[aSig].getPreconditions()) {
                initLayer[_pos].addFact(fact, Reason(_layer_idx, _pos, aSig));
                initLayer[_pos].extendState(fact);
                for (Signature decFact : _htn.getDecodedFacts(fact)) {
                    initLayer[_pos].addFact(decFact, Reason(_layer_idx, _pos, fact.abs()));
                    initLayer[_pos].extendState(decFact);
                }
            }
        }

        _enc.encode(_layer_idx, _pos++);
    }

    // Final position with goal state
    createNext(initLayer[_pos-1]);
    SigSet goalSet = _htn.getGoals();
    for (Signature fact : goalSet) {
        initLayer[_pos].addFact(fact.abs());
        initLayer[_pos].addTrueFact(fact);
    } 
    _enc.encode(_layer_idx, _pos++);

    initLayer.consolidate();

    bool solved = _enc.solve();
    
    // Next layers

    int maxIterations = 2;

    while (!solved && iteration < maxIterations) {
        _enc.printFailedVars(_layers.back());
        
        printf("Unsolvable at layer %i\n", _layer_idx);  
        iteration++;      
        printf("ITERATION %i\n", iteration);
        
        _layers.push_back(Layer(iteration, _layers.back().getNextLayerSize()));
        Layer& newLayer = _layers.back();
        printf(" NEW_LAYER_SIZE %i\n", newLayer.size());
        Layer& oldLayer = _layers[_layer_idx];
        _layer_idx++;
        _pos = 0;

        for (int oldPos = 0; oldPos < oldLayer.size(); oldPos++) {
            int newPos = oldLayer.getSuccessorPos(oldPos);
            int maxOffset = oldLayer[oldPos].getMaxExpansionSize();

            for (int offset = 0; offset < maxOffset; offset++) {
                assert(_pos == newPos + offset);

                //printf("%i,%i,%i,%i\n", oldPos, newPos, offset, newLayer.size());
                assert(newPos+offset < newLayer.size());

                if (newPos+offset == 0)
                    createNext(oldLayer[oldPos], oldPos);
                else
                    createNext(newLayer[newPos+offset-1], oldLayer[oldPos], oldPos);
                
                _enc.encode(_layer_idx, _pos++);
             }
        }

        newLayer.consolidate();
        solved = _enc.solve();
    }

    if (!solved) {
        _enc.printFailedVars(_layers.back());
        //printf("Limit exceeded. Solving without assumptions ...\n");
        //solved = _enc.solve();
        if (!solved) {
            printf("No success. Exiting.\n");
            return;
        }
    }

    printf("Found a solution at layer %i.\n", _layers.size()-1);

    // Extract solution
    std::vector<PlanItem> actionPlan = _enc.extractClassicalPlan();
    std::vector<PlanItem> decompPlan = _enc.extractDecompositionPlan();

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

void Planner::createNext(const Position& left) {
    Position& newPos = _layers[_layer_idx][_pos];
    newPos.setPos(_layer_idx, _pos);
    assert(left.getPos() == IntPair(_layer_idx, _pos-1));

    // Propagate occurring facts
    for (auto entry : left.getFacts()) {
        newPos.addFact(entry.first, Reason(_layer_idx, _pos-1, entry.first));
    }

    // Propagate state
    newPos.extendState(left.getState());

    // Propagate fact changes
    for (auto entry : left.getActions()) {
        const Signature& aSig = entry.first; 
        for (Signature fact : _htn.getAllFactChanges(aSig)) {
            
            newPos.addFact(fact.abs(), Reason(_layer_idx, _pos-1, (fact._negated ? aSig.opposite() : aSig)));
            newPos.extendState(fact);

            for (Signature decFact : _htn.getDecodedFacts(fact)) {
                newPos.addFact(decFact.abs(), Reason(_layer_idx, _pos, fact.abs()));
                newPos.extendState(decFact);
            }
        }
    }
    for (auto entry : left.getReductions()) {
        const Signature& rSig = entry.first; 
        if (rSig == Position::NONE_SIG) continue;
        for (Signature fact : _htn.getAllFactChanges(rSig)) {

            newPos.addFact(fact.abs(), Reason(_layer_idx, _pos-1, (fact._negated ? rSig.opposite() : rSig)));
            newPos.extendState(fact);

            for (Signature decFact : _htn.getDecodedFacts(fact)) {
                newPos.addFact(decFact.abs(), Reason(_layer_idx, _pos, fact.abs()));
                newPos.extendState(decFact);
            }
        }
    }
}

void Planner::createNext(const Position& above, int oldPos) {
    Position& newPos = _layers[_layer_idx][_pos];
    newPos.setPos(_layer_idx, _pos);

    int offset = _pos - _layers[_layer_idx-1].getSuccessorPos(oldPos);
    if (offset == 0) {

        // Propagate occurring facts
        for (auto entry : above.getFacts()) {
            newPos.addFact(entry.first, Reason(_layer_idx-1, oldPos, entry.first));
        }
    }

    // Propagate state
    newPos.extendState(above.getState());

    // Propagate actions
    for (auto entry : above.getActions()) {
        const Signature& aSig = entry.first;
        Reason why = Reason(_layer_idx-1, oldPos, aSig);

        if (offset < 1) {
            // proper action propagation
            newPos.addAction(aSig, why);
            // Add preconditions of action
            const Action& a = _htn._actions_by_sig[aSig];
            for (Signature fact : a.getPreconditions()) {

                newPos.addFact(fact.abs(), Reason(_layer_idx, _pos, (fact._negated ? aSig.opposite() : aSig)));
                for (Signature decFact : _htn.getDecodedFacts(fact)) {
                    newPos.addFact(decFact.abs(), Reason(_layer_idx, _pos, fact.abs()));
                }
            }
        } else {
            // action expands to "blank" at non-zero offsets
            newPos.addAction(_htn._action_blank.getSignature(), why);
        }
    }

    // Expand reductions
    for (auto entry : above.getReductions()) {
        const Signature& rSig = entry.first;
        if (rSig == Position::NONE_SIG) continue;
        const Reduction& r = _htn._reductions_by_sig[rSig];
        Reason why = Reason(_layer_idx-1, oldPos, rSig);

        int numAdded = 0;
        if (offset < r.getSubtasks().size()) {
            // Proper expansion
            const Signature& subtask = r.getSubtasks()[offset];
            // reduction(s)?
            for (Signature subRSig : getAllReductionsOfTask(subtask, newPos.getState())) {
                numAdded++;
                const Reduction& subR = _htn._reductions_by_sig[subRSig];
                newPos.addReduction(subRSig, why);
                newPos.addExpansionSize(subR.getSubtasks().size());
                // Add preconditions of reduction
                for (Signature fact : subR.getPreconditions()) {
                    newPos.addFact(fact.abs(), Reason(_layer_idx, _pos, fact._negated ? subRSig.opposite() : subRSig));
                    for (Signature decFact : _htn.getDecodedFacts(fact)) {
                        newPos.addFact(decFact.abs(), Reason(_layer_idx, _pos, fact.abs()));
                    }
                }
            }
            // action(s)?
            for (Signature aSig : getAllActionsOfTask(subtask, newPos.getState())) {
                numAdded++;
                newPos.addAction(aSig, why);
                // Add preconditions of action
                const Action& a = _htn._actions_by_sig[aSig];
                for (Signature fact : a.getPreconditions()) {
                    newPos.addFact(fact.abs(), Reason(_layer_idx, _pos, fact._negated ? aSig.opposite() : aSig));
                    for (Signature decFact : _htn.getDecodedFacts(fact)) {
                        newPos.addFact(decFact.abs(), Reason(_layer_idx, _pos, fact.abs()));
                    }
                }
            }
        } else {
            // Blank
            numAdded++;
            newPos.addAction(_htn._action_blank.getSignature(), why);
        }

        if (numAdded == 0) {
            // Explicitly forbid the parent!
            printf("FORBIDDING reduction %s@(%i,%i)\n", Names::to_string(rSig).c_str(), _layer_idx-1, oldPos);
            newPos.addReduction(Position::NONE_SIG, why);
        }
    }
}

void Planner::createNext(const Position& left, const Position& above, int oldPos) {
    createNext(left); // applies effects from left position
    createNext(above, oldPos); // instantiates new children using the "new" state
}

std::vector<Signature> Planner::getAllReductionsOfTask(const Signature& task, const State& state) {
    std::vector<Signature> result;

    if (!_htn._task_id_to_reduction_ids.count(task._name_id)) return result;

    std::vector<int>& redIds = _htn._task_id_to_reduction_ids[task._name_id];
    //printf("  task %s : %i reductions found\n", Names::to_string(task).c_str(), redIds.size());

    // Filter and minimally instantiate methods
    // applicable in current (super)state
    for (int redId : redIds) {
        Reduction& r = _htn._reductions[redId];
        r = r.substituteRed(Substitution::get(r.getTaskArguments(), task._args));
        Signature origSig = r.getSignature();
        std::vector<Reduction> reductions = _instantiator.getMinimalApplicableInstantiations(r, state);
        //printf("   reduction %s ~> %i instantiations\n", Names::to_string(origSig).c_str(), reductions.size());

        for (Reduction red : reductions) {
            // Rename any remaining constants in each action as unique q-constants 
            red = _htn.replaceQConstants(red, _layer_idx, _pos);
            Signature sig = red.getSignature();
            _htn._reductions_by_sig[sig] = red;
            result.push_back(sig);
        }
    }
    return result;
}

std::vector<Signature> Planner::getAllActionsOfTask(const Signature& task, const State& state) {
    std::vector<Signature> result;

    if (!_htn._actions.count(task._name_id)) return result;

    Action& a = _htn._actions[task._name_id];
    HtnOp op = a.substitute(Substitution::get(a.getArguments(), task._args));
    Action act = (Action) op;
    printf("  task %s : action found: %s\n", Names::to_string(task).c_str(), Names::to_string(act).c_str());

    std::vector<Action> actions = _instantiator.getApplicableInstantiations(act, _layers[_layer_idx][_pos].getState());
    for (Action action : actions) {
        action = _htn.replaceQConstants(action, _layer_idx, _pos);
        Signature sig = action.getSignature();
        _htn._actions_by_sig[sig] = action;
        result.push_back(sig);
    }
    return result;
}

