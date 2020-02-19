
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
        if (_pos > 0) createNext();

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
    createNext();
    SigSet goalSet = _htn.getGoals();
    for (Signature fact : goalSet) {
        assert(initLayer[_pos].containsInState(fact));
        assert(initLayer[_pos].getFacts().count(fact.abs()));
        initLayer[_pos].addTrueFact(fact);
    }
    //addNewFalseFacts();
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

        for (_old_pos = 0; _old_pos < oldLayer.size(); _old_pos++) {
            int newPos = oldLayer.getSuccessorPos(_old_pos);
            int maxOffset = oldLayer[_old_pos].getMaxExpansionSize();

            for (int offset = 0; offset < maxOffset; offset++) {
                assert(_pos == newPos + offset);

                //printf("%i,%i,%i,%i\n", oldPos, newPos, offset, newLayer.size());
                assert(newPos+offset < newLayer.size());

                createNext();
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

    //_enc.printSatisfyingAssignment();
}









void Planner::createNext() {
    Position& newPos = _layers[_layer_idx][_pos];

    // Set up all facts that may hold at this position.
    if (_pos == 0) {
        propagateInitialState();
    } else {
        Position& left = _layers[_layer_idx][_pos-1];
        createNextFromLeft(left);
    }

    // Generate this new position's content based on the facts and the position above.
    if (_layer_idx > 0) {
        Position& above = _layers[_layer_idx-1][_old_pos];
        createNextFromAbove(above);
    }

    // In preparation for the upcoming position,
    // add all effects of the actions and reductions occurring HERE
    // as (initially false) facts to THIS position.  
    addNewFalseFacts();
}

void Planner::createNextFromLeft(const Position& left) {
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
        }
    }
    for (auto entry : left.getReductions()) {
        const Signature& rSig = entry.first; 
        if (rSig == Position::NONE_SIG) continue;
        for (Signature fact : _htn.getAllFactChanges(rSig)) {
            addEffect(rSig, fact);
        }
    }
}

void Planner::propagateInitialState() {
    assert(_layer_idx > 0);
    assert(_pos == 0);

    Position& newPos = _layers[_layer_idx][0];
    Position& above = _layers[_layer_idx-1][0];

    // Propagate occurring facts
    for (auto entry : above.getFacts()) {
        newPos.addFact(entry.first, Reason(_layer_idx-1, _old_pos, entry.first));
    }
    // Propagate TRUE facts
    for (auto entry : above.getTrueFacts()) {
        assert(!_htn.hasQConstants(entry.first));
        newPos.addTrueFact(entry.first, Reason(_layer_idx-1, _old_pos, entry.first));
    }
    // Propagate state
    newPos.extendState(above.getState());
}

void Planner::createNextFromAbove(const Position& above) {
    Position& newPos = _layers[_layer_idx][_pos];
    newPos.setPos(_layer_idx, _pos);

    int offset = _pos - _layers[_layer_idx-1].getSuccessorPos(_old_pos);
    if (offset == 0) {
        // Propagate fact decodings
        for (auto entry : above.getQFactDecodings()) {
            for (auto dec : entry.second) {
                newPos.addQFactDecoding(entry.first, dec);
            }
        }
    }

    propagateActions(offset);
    propagateReductions(offset);
}

void Planner::propagateActions(int offset) {
    Position& newPos = _layers[_layer_idx][_pos];
    Position& above = _layers[_layer_idx-1][_old_pos];

    // Propagate actions
    for (auto entry : above.getActions()) {
        const Signature& aSig = entry.first;
        if (aSig == Position::NONE_SIG) continue;
        const Action& a = _htn._actions_by_sig[aSig];

        // Can the action occur here w.r.t. the current state?
        bool valid = true;
        for (Signature fact : a.getPreconditions()) {
            if (!_htn.hasQConstants(fact) && !newPos.containsInState(fact)) {
                // Action cannot occur here!
                valid = false; break;
            }
        }

        // If not: forbid the action, i.e., its parent action
        if (!valid) {
            newPos.addAction(Position::NONE_SIG, Reason(_layer_idx-1, _old_pos, aSig));
            continue;
        }

        Reason why = Reason(_layer_idx-1, _old_pos, aSig);
        if (offset < 1) {
            // proper action propagation
            newPos.addAction(aSig, why);
            // Add preconditions of action
            for (Signature fact : a.getPreconditions()) {
                addPrecondition(aSig, fact);
            }
        } else {
            // action expands to "blank" at non-zero offsets
            newPos.addAction(_htn._action_blank.getSignature(), why);
        }
    }
}

void Planner::propagateReductions(int offset) {
    Position& newPos = _layers[_layer_idx][_pos];
    Position& above = _layers[_layer_idx-1][_old_pos];

    // Expand reductions
    for (auto entry : above.getReductions()) {
        const Signature& rSig = entry.first;
        if (rSig == Position::NONE_SIG) continue;
        const Reduction& r = _htn._reductions_by_sig[rSig];
        Reason why = Reason(_layer_idx-1, _old_pos, rSig);

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

void introduceNewFalseFact(Position& newPos, const Signature& fact) {
    newPos.addTrueFact(fact.abs().opposite());
    newPos.addFact(fact.abs());
    newPos.extendState(fact.abs().opposite());
}

void Planner::addNewFalseFacts() {
    Position& newPos = _layers[_layer_idx][_pos];
    
    // For each action effect:
    for (auto entry : newPos.getActions()) {
        const Signature& aSig = entry.first;
        for (Signature eff : _htn.getAllFactChanges(aSig)) {

            if (!_htn.hasQConstants(eff) && !newPos.getFacts().count(eff.abs())) {
                // New fact: set to false before the action may happen
                introduceNewFalseFact(newPos, eff);
            }

            for (Signature decEff : _htn.getDecodedObjects(eff)) {
                if (!newPos.getFacts().count(decEff.abs())) {
                    // New fact: set to false before the action may happen
                    introduceNewFalseFact(newPos, decEff);
                }
            }
        }
    }

    // For each possible reduction effect: 
    for (auto entry : newPos.getReductions()) {
        const Signature& rSig = entry.first;
        if (rSig == Position::NONE_SIG) continue;
        for (Signature eff : _htn.getAllFactChanges(rSig)) {

            if (!_htn.hasQConstants(eff) && !newPos.getFacts().count(eff.abs())) {
                // New fact: set to false before the reduction may happen
                introduceNewFalseFact(newPos, eff);
            }

            for (Signature decEff : _htn.getDecodedObjects(eff)) {
                if (!newPos.getFacts().count(decEff.abs())) {
                    // New fact: set to false before the action may happen
                    introduceNewFalseFact(newPos, decEff);
                }
            }
        }
    }
}

void Planner::addPrecondition(const Signature& op, const Signature& fact) {
    Position& pos = _layers[_layer_idx][_pos];
    Signature factAbs = fact.abs();

    if (fact._negated && !_htn.hasQConstants(fact) && !pos.getFacts().count(fact.abs())) {
        // Negative precondition not contained in facts: initialize
        //printf("NEG_PRE %s\n", Names::to_string(fact).c_str());
        introduceNewFalseFact(pos, fact);
    }

    // Precondition must be valid (or a q fact)
    if (!_htn.hasQConstants(fact)) assert(pos.containsInState(fact));

    // Add additional reason for the fact / add it first if it's a q-constant
    pos.addFact(factAbs, Reason(_layer_idx, _pos, op));

    // For each fact decoded from the q-fact:
    bool somePossible = false;
    for (Signature decFact : _htn.getDecodedObjects(factAbs)) {
        if (fact._negated) decFact.negate();

        if (!pos.containsInState(decFact)) {
            // Not a possible fact here. Add as "false" fact to get a contradiction
            //printf("IMPOSSIBLE_DECFACT %s\n", Names::to_string(decFact).c_str());
            pos.addFact(decFact.abs());
            pos.addTrueFact(decFact.opposite());
            pos.extendState(decFact.opposite());
        } else somePossible = true;

        pos.addQFactDecoding(factAbs, decFact.abs());
        pos.addFact(factAbs, Reason(_layer_idx, _pos, op)); // also add fact as an (indirect) consequence of op
    }
    // TODO what if ALL are impossible? (!somePossible) => Forbid!
    // Right now, the q-fact will just run into a contradiction.
}

void Planner::addEffect(const Signature& op, const Signature& fact) {
    Position& pos = _layers[_layer_idx][_pos];
    Signature factAbs = fact.abs();

    pos.addFact(factAbs, Reason(_layer_idx, _pos-1, op));

    // Depending on whether fact supports are encoded for primitive ops only,
    // add the fact to the op's support accordingly
#ifdef NONPRIMITIVE_SUPPORT
    pos.addFactSupport(fact, op);
#else
    if (_htn._actions_by_sig.count(op)) pos.addFactSupport(fact, op);
#endif
    
    pos.extendState(fact);

    Reason why = Reason(_layer_idx, _pos, fact);
    for (Signature decFact : _htn.getDecodedObjects(factAbs)) {
        Signature decFactSigned = fact._negated ? decFact.opposite() : decFact;
        assert(!decFact._negated && !factAbs._negated);
        assert(decFactSigned._negated == fact._negated);

        pos.addQFactDecoding(factAbs, decFact);
        pos.addFact(factAbs, Reason(_layer_idx, _pos-1, op)); // also add fact as an (indirect) consequence of op
        
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

