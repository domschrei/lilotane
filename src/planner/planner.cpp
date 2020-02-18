
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
                addPrecondition(rSig, fact);
            }
        }
        for (Signature aSig : getAllActionsOfTask(subtask, initLayer[_pos].getState())) {
            initLayer[_pos].addAction(aSig);
            // Add preconditions
            for (Signature fact : _htn._actions_by_sig[aSig].getPreconditions()) {
                addPrecondition(aSig, fact);
            }
        }
        addNewFalseFacts();
        _enc.encode(_layer_idx, _pos++);
    }

    // Final position with goal state
    createNext(initLayer[_pos-1]);
    SigSet goalSet = _htn.getGoals();
    for (Signature fact : goalSet) {
        initLayer[_pos].addFact(fact.abs());
        initLayer[_pos].addTrueFact(fact);
    }
    addNewFalseFacts();
    _enc.encode(_layer_idx, _pos++);

    initLayer.consolidate();

    bool solved = _enc.solve();
    
    // Next layers

    int maxIterations = 5;

    while (!solved && iteration < maxIterations) {
        _enc.printFailedVars(_layers.back());
        
        printf("Unsolvable at layer %i with assumptions\n", _layer_idx);  
        solved = _enc.solve();
        if (!solved) {
            printf("Unsolvable at layer %i even without assumptions!\n", _layer_idx);
            break;
        } else {
            printf("(solvable with assumptions)\n");
        }

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
                
                addNewFalseFacts();
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
        if (item.abstractTask == _htn._action_blank.getSignature()) continue;
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

    _enc.printSatisfyingAssignment();
}

void Planner::createNext(const Position& left) {
    Position& newPos = _layers[_layer_idx][_pos];
    newPos.setPos(_layer_idx, _pos);
    assert(left.getPos() == IntPair(_layer_idx, _pos-1));

    // Propagate occurring facts
    for (auto entry : left.getFacts()) {
        newPos.addFact(entry.first, Reason(_layer_idx, _pos-1, entry.first));
    }
    // Propagate fact decodings
    for (auto entry : left.getQFactDecodings()) {
        for (auto dec : entry.second) {
            newPos.addQFactDecoding(entry.first, dec);
        }
    }

    // Propagate state
    newPos.extendState(left.getState());

    // Propagate fact changes from operations from previous position
    for (auto entry : left.getActions()) {
        const Signature& aSig = entry.first; 
        for (Signature fact : _htn.getAllFactChanges(aSig)) {
            addEffect(aSig, fact);
            if (!left.getFacts().count(fact)) {
                // new fact
                newPos.addNewFact(fact);
            }
        }
    }
    for (auto entry : left.getReductions()) {
        const Signature& rSig = entry.first; 
        if (rSig == Position::NONE_SIG) continue;
        for (Signature fact : _htn.getAllFactChanges(rSig)) {
            addEffect(rSig, fact);
            if (!left.getFacts().count(fact)) {
                // new fact
                newPos.addNewFact(fact);
            }
        }
    }

    /*
    // Identify new q-free facts that should be set to false according to closed-world asmpt
    for (auto entry : newPos.getFacts()) {
        const Signature& factSig = entry.first;
        if (_htn.hasQConstants(factSig)) continue;

        // Check if this fact or one of its abstractions occurs as an effect
        if (newPos.getNewFacts().count(factSig)) continue; // effect!
        bool possiblyNew = true;
        if (newPos.getQFactAbstractions().count(factSig))
        for (Signature sig : newPos.getQFactAbstractions().at(factSig)) {
            if (newPos.getNewFacts().count(sig)) {possiblyNew = false; break;} // effect!
        }

        // Otherwise:
        if (possiblyNew) {
            if (!left.getFacts().count(factSig)) {
                // Fact did not occur at position to the left
                if (!newPos.getTrueFacts().count(factSig) && !newPos.getTrueFacts().count(factSig.opposite())) {
                    // Not an axiomatic new fact

                    // Add negated fact to set of true facts
                    newPos.addTrueFact(factSig.opposite());
                }
            }
        }
    }*/
        
    /*
        bool possiblyNew = true;
        for (Reason why : entry.second) {
            // axiomatic facts
            if (why.axiomatic) {
                possiblyNew = false; break; 
            }
            // effect / propagation from previous position
            if (why.layer == _layer_idx && why.pos == _pos-1) {
                possiblyNew = false; break; 
            }
        }
        if (!possiblyNew) continue;
        if (newPos.getQFactAbstractions().count(factSig))
        for (Signature qfactSig : newPos.getQFactAbstractions().at(factSig)) {
            for (Reason why : newPos.getFacts().at(qfactSig)) {
                // axiomatic facts
                if (why.axiomatic) {
                    possiblyNew = false; break; 
                }
                // effect / propagation from previous position
                if (why.layer == _layer_idx && why.pos == _pos-1) {
                    possiblyNew = false; break; 
                }
            }
        }

        if (!left.getFacts().count(factSig)) {
            // Fact did not occur at position to the left
            if (!left.getTrueFacts().count(factSig) && !left.getTrueFacts().count(factSig.opposite())) {
                // Not axiomatic

                // Add negated fact to set of true facts
                newPos.addTrueFact(factSig.opposite());
            }
        }
    }
    */
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
        // Propagate fact decodings
        for (auto entry : above.getQFactDecodings()) {
            for (auto dec : entry.second) {
                newPos.addQFactDecoding(entry.first, dec);
            }
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
                addPrecondition(aSig, fact);
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
                    addPrecondition(subRSig, fact);
                }
            }
            // action(s)?
            for (Signature aSig : getAllActionsOfTask(subtask, newPos.getState())) {
                numAdded++;
                newPos.addAction(aSig, why);
                // Add preconditions of action
                const Action& a = _htn._actions_by_sig[aSig];
                for (Signature fact : a.getPreconditions()) {
                    addPrecondition(aSig, fact);
                }
            }
        } else {
            // Blank
            numAdded++;
            newPos.addAction(_htn._action_blank.getSignature(), why);
        }

        if (numAdded == 0) {
            // Explicitly forbid the parent!
            //printf("FORBIDDING reduction %s@(%i,%i)\n", Names::to_string(rSig).c_str(), _layer_idx-1, oldPos);
            newPos.addReduction(Position::NONE_SIG, why);
        }
    }
}

void Planner::createNext(const Position& left, const Position& above, int oldPos) {
    createNext(left); // applies effects from left position
    createNext(above, oldPos); // instantiates new children using the "new" state
    addNewFalseFacts();
}

void Planner::addNewFalseFacts() {
    Position& newPos = _layers[_layer_idx][_pos];
    
    // For each action effect:
    for (auto entry : newPos.getActions()) {
        const Signature& aSig = entry.first;
        for (Signature eff : _htn.getAllFactChanges(aSig)) {

            if (!newPos.getFacts().count(eff.abs())) {
                // New fact: set to false before the action may happen
                newPos.addTrueFact(eff.abs().opposite());
            }
        }
    }

    // For each possible reduction effect: 
    for (auto entry : newPos.getReductions()) {
        const Signature& rSig = entry.first;
        for (Signature eff : _htn.getAllFactChanges(rSig)) {

            if (!newPos.getFacts().count(eff.abs())) {
                // New fact: set to false before the reduction may happen
                newPos.addTrueFact(eff.abs().opposite());
            }
        }
    }
}



void Planner::addPrecondition(const Signature& op, const Signature& fact) {
    Position& pos = _layers[_layer_idx][_pos];
    Signature factAbs = fact.abs();
    
    pos.addFact(factAbs, Reason(_layer_idx, _pos, op));

    for (Signature decFact : _htn.getDecodedObjects(factAbs)) {
        pos.addQFactDecoding(factAbs, decFact); // calls addFact(), too
    }
}

void Planner::addEffect(const Signature& op, const Signature& fact) {
    Position& pos = _layers[_layer_idx][_pos];
    Signature factAbs = fact.abs();

    pos.addFact(factAbs, Reason(_layer_idx, _pos-1, op));
    pos.addFactSupport(fact, op);
    pos.extendState(fact);

    Reason why = Reason(_layer_idx, _pos, fact);
    for (Signature decFact : _htn.getDecodedObjects(factAbs)) {
        Signature decFactSigned = fact._negated ? decFact.opposite() : decFact;
        assert(!decFact._negated && !factAbs._negated);
        assert(decFactSigned._negated == fact._negated);
        pos.addQFactDecoding(factAbs, decFact); // calls addFact(), too
        //pos.addConditionalFactSupport(decFactSigned, op, why);
        pos.extendState(decFactSigned);
    }
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
        //printf("   reduction %s ~> %i instantiations\n", Names::to_string(origSig).c_str(), reductions.size());
#ifdef Q_CONSTANTS
        std::vector<Reduction> reductions = _instantiator.getMinimalApplicableInstantiations(r, state);
#else
        std::vector<Reduction> reductions = _instantiator.getFullApplicableInstantiations(r, state);
#endif
        for (Reduction red : reductions) {
            // Rename any remaining variables in each action as unique q-constants 
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
    //printf("  task %s : action found: %s\n", Names::to_string(task).c_str(), Names::to_string(act).c_str());

#ifdef Q_CONSTANTS
    std::vector<Action> actions = _instantiator.getMinimalApplicableInstantiations(act, _layers[_layer_idx][_pos].getState());
#else
    std::vector<Action> actions = _instantiator.getFullApplicableInstantiations(act, _layers[_layer_idx][_pos].getState());
#endif
    for (Action action : actions) {
        action = _htn.replaceQConstants(action, _layer_idx, _pos);
        Signature sig = action.getSignature();
        _htn._actions_by_sig[sig] = action;
        result.push_back(sig);
    }
    return result;
}

