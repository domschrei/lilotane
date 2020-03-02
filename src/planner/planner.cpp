
#include <iostream>
#include <functional>
#include <regex>
#include <string>

#include "parser/plan.hpp"

#include "planner.h"
#include "util/log.h"
//#include "parser/cwa.hpp"

int Planner::findPlan() {
    
    // Begin actual instantiation and solving loop
    int iteration = 0;
    log("ITERATION %i\n", iteration);

    int initSize = 2; //_htn._init_reduction.getSubtasks().size()+1;
    log("Creating initial layer of size %i\n", initSize);
    _layer_idx = 0;
    _pos = 0;
    _layers.push_back(Layer(iteration, initSize));
    Layer& initLayer = _layers[0];
    initLayer[_pos].setPos(_layer_idx, _pos);
    
    /***** LAYER 0, POSITION 0 ******/

    // Initial state
    SigSet initState = _htn.getInitState();
    for (Signature fact : initState) {
        initLayer[_pos].addFact(fact.abs());
        initLayer[_pos].addTrueFact(fact);
        initLayer[_pos].extendState(fact);
    }

    // Instantiate all possible init. reductions
    _htn._init_reduction_choices = _instantiator.getApplicableInstantiations(
            _htn._init_reduction, initLayer[0].getState());
    std::vector<Reduction> roots = _htn._init_reduction_choices;
    for (const Reduction& r : roots) {

        Reduction red = _htn.replaceQConstants(r, _layer_idx, _pos);
        Signature sig = red.getSignature();
        
        // Check validity
        if (!_instantiator.hasConsistentlyTypedArgs(sig)) continue;
        if (!_instantiator.hasValidPreconditions(red, initLayer[_pos].getState())) continue;
        
        // Remove unneeded rigid conditions from the reduction
        _htn.removeRigidConditions(red);

        _htn._reductions_by_sig[sig] = red;

        initLayer[_pos].addReduction(sig);
        initLayer[_pos].addExpansionSize(red.getSubtasks().size());
        // Add preconditions
        for (Signature fact : red.getPreconditions()) {
            addPrecondition(sig, fact);
        }
        addQConstantTypeConstraints(sig);
    }
    addNewFalseFacts();
    _enc.encode(_layer_idx, _pos++);

    /***** LAYER 0, POSITION 1 ******/

    createNext(); // position 1

    // Create virtual goal action
    Action goalAction(_htn.getNameId("_GOAL_ACTION_"), std::vector<int>());
    Signature goalSig = goalAction.getSignature();
    _htn._actions[goalSig._name_id] = goalAction;
    initLayer[_pos].addAction(goalSig);
    
    // Extract primitive goals, add to preconds of goal action
    SigSet goalSet = _htn.getGoals();
    for (Signature fact : goalSet) {
        assert(initLayer[_pos].containsInState(fact));
        assert(initLayer[_pos].getFacts().count(fact.abs()));
        goalAction.addPrecondition(fact);
        addPrecondition(goalSig, fact);
    }
    _htn._actions_by_sig[goalSig] = goalAction;
    
    _enc.encode(_layer_idx, _pos++);

    /***** LAYER 0 END ******/

    initLayer.consolidate();
    bool solved = _enc.solve();
    
    // Next layers

    int maxIterations = _params.getIntParam("d");

    while (!solved && (maxIterations == 0 || iteration < maxIterations)) {
        _enc.printFailedVars(_layers.back());
        
        log("Unsolvable at layer %i with assumptions\n", _layer_idx);

        // Attempt to solve formula again, now without assumptions
        // (is usually simple; if it fails, we know the entire problem is unsolvable)
        solved = _enc.solve();
        if (!solved) {
            log("Unsolvable at layer %i even without assumptions!\n", _layer_idx);
            break;
        } else {
            log("Solvable without assumptions - expanding by another layer\n");
        }

        iteration++;      
        log("ITERATION %i\n", iteration);
        
        _layers.push_back(Layer(iteration, _layers.back().getNextLayerSize()));
        Layer& newLayer = _layers.back();
        log(" NEW_LAYER_SIZE %i\n", newLayer.size());
        Layer& oldLayer = _layers[_layer_idx];
        _layer_idx++;
        _pos = 0;

        for (_old_pos = 0; _old_pos < oldLayer.size(); _old_pos++) {
            int newPos = oldLayer.getSuccessorPos(_old_pos);
            int maxOffset = oldLayer[_old_pos].getMaxExpansionSize();

            for (int offset = 0; offset < maxOffset; offset++) {
                assert(_pos == newPos + offset);
                log(" Position (%i,%i)\n", _layer_idx, _pos);
                log("  Instantiating ...\n");

                //log("%i,%i,%i,%i\n", oldPos, newPos, offset, newLayer.size());
                assert(newPos+offset < newLayer.size());

                createNext();
                log("  Instantiation done. (%i reductions, %i actions, %i facts)\n", 
                        _layers[_layer_idx][_pos].getReductions().size(),
                        _layers[_layer_idx][_pos].getActions().size(),
                        _layers[_layer_idx][_pos].getFacts().size()
                );
                _enc.encode(_layer_idx, _pos++);
             }
        }

        newLayer.consolidate();
        solved = _enc.solve();
    }

    if (!solved) {
        _enc.printFailedVars(_layers.back());
        //log("Limit exceeded. Solving without assumptions ...\n");
        //solved = _enc.solve();
        if (!solved) {
            log("No success. Exiting.\n");
            return 1;
        }
    }

    log("Found a solution at layer %i.\n", _layers.size()-1);

    // Extract solution
    auto planPair = _enc.extractPlan();

    // Create stringstream which is being fed the plan
    std::stringstream stream;

    // Print plan into stream

    // -- primitive part
    stream << "==>\n";
    std::unordered_set<int> actionIds;
    std::unordered_set<int> idsToRemove;
    for (PlanItem& item : planPair.first) {
        if (_htn._name_back_table[item.abstractTask._name_id].rfind("_SECOND") != std::string::npos) {
            // Second part of a split action: discard
            idsToRemove.insert(item.id);
            continue;
        }
        if (_htn._name_back_table[item.abstractTask._name_id].rfind("_FIRST") != std::string::npos) {
            // First part of a split action: change name, then handle normally
            item.abstractTask._name_id = _htn._split_action_from_first[item.abstractTask._name_id];
        }
        actionIds.insert(item.id);

        // Do not write blank actions or the virtual goal action
        if (item.abstractTask == _htn._action_blank.getSignature()) continue;
        if (item.abstractTask._name_id == _htn.getNameId("_GOAL_ACTION_")) continue;

        stream << item.id << " " << Names::to_string_nobrackets(item.abstractTask) << "\n";
    }
    // -- decomposition part
    bool root = true;
    for (PlanItem& item : planPair.second) {
        if (item.id < 0) continue;

        std::string subtaskIdStr = "";
        for (int subtaskId : item.subtaskIds) {
            if (!idsToRemove.count(subtaskId)) subtaskIdStr += " " + std::to_string(subtaskId);
        }
        
        if (root) {
            stream << "root " << subtaskIdStr << "\n";
            root = false;
            continue;
        } else if (item.id <= 0 || actionIds.count(item.id)) continue;
        
        stream << item.id << " " << Names::to_string_nobrackets(item.abstractTask) << " -> " 
            << Names::to_string_nobrackets(item.reduction) << subtaskIdStr << "\n";
    }
    stream << "<==\n";

    // Feed plan into parser to convert it into a plan to the original problem
    // (w.r.t. previous compilations the parser did) and output it
    std::ostringstream outstream;
    convert_plan(stream, outstream);
    log(outstream.str().c_str());
    log("<==\n");
    
    log("End of solution plan.\n");
    //_enc.printSatisfyingAssignment();
    return 0;
}



void introduceNewFalseFact(Position& newPos, const Signature& fact) {
    newPos.addTrueFact(fact.abs().opposite());
    newPos.addFact(fact.abs());
    newPos.extendState(fact.abs().opposite());
    //log("FALSE_FACT %s @(%i,%i)\n", Names::to_string(fact.abs().opposite()).c_str(), 
    //        newPos.getPos().first, newPos.getPos().second);
}

void Planner::createNext() {

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

    // Propagate state
    newPos.extendState(left.getState());

    std::unordered_set<int> relevantQConstants;

    // Propagate fact changes from operations from previous position
    for (auto entry : left.getActions()) {
        const Signature& aSig = entry.first; 
        for (Signature fact : _htn.getAllFactChanges(aSig)) {
            addEffect(aSig, fact);
        }
        for (const int& arg : aSig._args) {
            if (_htn._q_constants.count(arg)) relevantQConstants.insert(arg);
        }
    }
    for (auto entry : left.getReductions()) {
        const Signature& rSig = entry.first; 
        if (rSig == Position::NONE_SIG) continue;
        for (Signature fact : _htn.getAllFactChanges(rSig)) {
            addEffect(rSig, fact);
        }
        for (const int& arg : rSig._args) {
            if (_htn._q_constants.count(arg)) relevantQConstants.insert(arg);
        }
    }

    // Propagate occurring facts
    for (auto entry : left.getFacts()) {
        bool add = true;
        for (const int& arg : entry.first._args) {
            if (_htn._q_constants.count(arg) && !relevantQConstants.count(arg)) add = false;
        }
        if (!add) {
            // forget q-facts that have become irrelevant
            //log("  FORGET %s\n", Names::to_string(entry.first).c_str());
            continue;
        }
        newPos.addFact(entry.first, Reason(_layer_idx, _pos-1, entry.first));
    }
    // Propagate fact decodings
    for (auto entry : left.getQFactDecodings()) {
        bool add = true;
        for (const int& arg : entry.first._args) {
            if (_htn._q_constants.count(arg) && !relevantQConstants.count(arg)) add = false;
        }
        if (!add) {
            // forget q-fact decodings that have become irrelevant
            //log("  FORGET %s\n", Names::to_string(entry.first).c_str());
            continue;
        } 
        for (auto dec : entry.second) {
            newPos.addQFactDecoding(entry.first, dec);
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
        // Propagate facts
        for (auto entry : above.getFacts()) {
            //assert(newPos.getFacts().count(entry.first)); // already added as false fact!
            newPos.addFact(entry.first, Reason(_layer_idx-1, _old_pos, entry.first));
        }
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
            //log("FORBIDDING action %s@(%i,%i): no children at offset %i\n", 
            //        Names::to_string(aSig).c_str(), _layer_idx-1, _old_pos, offset);
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
                assert(_htn._reductions_by_sig.count(subRSig));
                const Reduction& subR = _htn._reductions_by_sig[subRSig];
                newPos.addReduction(subRSig, why);
                //if (_layer_idx <= 1) log("ADD %s:%s @ (%i,%i)\n", Names::to_string(subR.getTaskSignature()).c_str(), Names::to_string(subRSig).c_str(), _layer_idx, _pos);
                newPos.addExpansionSize(subR.getSubtasks().size());
                // Add preconditions of reduction
                //log("PRECONDS %s ", Names::to_string(subRSig).c_str());
                for (Signature fact : subR.getPreconditions()) {
                    addPrecondition(subRSig, fact);
                    //log("%s ", Names::to_string(fact).c_str());
                }
                addQConstantTypeConstraints(subRSig);
                //log("\n");
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
                addQConstantTypeConstraints(aSig);
            }
        } else {
            // Blank
            numAdded++;
            newPos.addAction(_htn._action_blank.getSignature(), why);
        }

        if (numAdded == 0) {
            // Explicitly forbid the parent!
            //log("FORBIDDING reduction %s@(%i,%i): no children at offset %i\n", 
            //        Names::to_string(rSig).c_str(), _layer_idx-1, _old_pos, offset);
            newPos.addReduction(Position::NONE_SIG, why);
        }
    }
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

    // For each fact from "above" the next position:
    if (_layer_idx == 0) return;
    if (_old_pos+1 < _layers[_layer_idx-1].size() && _layers[_layer_idx-1].getSuccessorPos(_old_pos+1) == _pos+1) {
        Position& newAbove = _layers[_layer_idx-1][_old_pos+1];

        for (auto entry : newAbove.getFacts()) {
            // If fact was not seen here before
            if (!newPos.getFacts().count(entry.first)) {
                // Add fact
                newPos.addFact(entry.first.abs());
                if (!_htn.hasQConstants(entry.first)) {
                    // Introduce as FALSE fact, if not a q-fact
                    introduceNewFalseFact(newPos, entry.first);
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
        //log("NEG_PRE %s\n", Names::to_string(fact).c_str());
        introduceNewFalseFact(pos, fact);
    }

    //log("pre %s of %s\n", Names::to_string(fact).c_str(), Names::to_string(op).c_str());
    // Precondition must be valid (or a q fact)
    if (!_htn.hasQConstants(fact)) assert(pos.containsInState(fact));

    // Add additional reason for the fact / add it first if it's a q-constant
    pos.addFact(factAbs, Reason(_layer_idx, _pos, op));

    // For each fact decoded from the q-fact:
    assert(!_htn.hasQConstants(factAbs) || !_htn.getDecodedObjects(factAbs).empty());
    for (Signature decFact : _htn.getDecodedObjects(factAbs)) {
        if (fact._negated) decFact.negate();

        if (!pos.getFacts().count(decFact.abs())) {
            // Decoded fact did not occur before.
            introduceNewFalseFact(pos, decFact);
        }

        pos.addQFactDecoding(factAbs, decFact.abs());
        pos.addFact(decFact.abs(), Reason(_layer_idx, _pos, op)); // also add fact as an (indirect) consequence of op
    }
}

void Planner::addEffect(const Signature& op, const Signature& fact) {
    Position& pos = _layers[_layer_idx][_pos];
    Signature factAbs = fact.abs();

    pos.addFact(factAbs, Reason(_layer_idx, _pos-1, op));

    // Depending on whether fact supports are encoded for primitive ops only,
    // add the fact to the op's support accordingly
    if (_params.isSet("nps") || _htn._actions_by_sig.count(op)) {
        pos.addFactSupport(fact, op);
    }
    
    pos.extendState(fact);

    Reason why = Reason(_layer_idx, _pos, fact);
    assert(!_htn.hasQConstants(factAbs) || !_htn.getDecodedObjects(factAbs).empty());
    for (Signature decFact : _htn.getDecodedObjects(factAbs)) {
        Signature decFactSigned = fact._negated ? decFact.opposite() : decFact;
        assert(!decFact._negated && !factAbs._negated);
        assert(decFactSigned._negated == fact._negated);

        pos.addQFactDecoding(factAbs, decFact);
        pos.addFact(factAbs, Reason(_layer_idx, _pos-1, op)); // also add fact as an (indirect) consequence of op
        
        pos.extendState(decFactSigned);
    }
}

void Planner::addQConstantTypeConstraints(const Signature& op) {
    // Add type constraints for q constants
    std::vector<TypeConstraint> cs = _instantiator.getQConstantTypeConstraints(op);
    // Add to this position's data structure
    for (const TypeConstraint& c : cs) {
        _layers[_layer_idx][_pos].addQConstantTypeConstraint(op, c);
    }
}


std::vector<Signature> Planner::getAllReductionsOfTask(const Signature& task, const State& state) {
    std::vector<Signature> result;

    if (!_htn._task_id_to_reduction_ids.count(task._name_id)) return result;

    // Check if the created reduction has consistent sorts
    //if (!_instantiator.hasConsistentlyTypedArgs(task)) return result;

    std::vector<int>& redIds = _htn._task_id_to_reduction_ids[task._name_id];
    //log("  task %s : %i reductions found\n", Names::to_string(task).c_str(), redIds.size());

    // Filter and minimally instantiate methods
    // applicable in current (super)state
    for (int redId : redIds) {
        Reduction r = _htn._reductions[redId];
        std::vector<substitution_t> subs = Substitution::getAll(r.getTaskArguments(), task._args);
        for (substitution_t s : subs) {

            //if (_layer_idx <= 1) log("SUBST %s\n", Names::to_string(s).c_str());
            Reduction rSub = r.substituteRed(s);
            Signature origSig = rSub.getSignature();
            if (!_instantiator.hasConsistentlyTypedArgs(origSig)) continue;
            
            //log("   reduction %s ~> %i instantiations\n", Names::to_string(origSig).c_str(), reductions.size());
            std::vector<Reduction> reductions = _instantiator.getApplicableInstantiations(rSub, state);
            for (Reduction red : reductions) {
                Signature sig = red.getSignature();

                // Check if the created reduction has consistent sorts
                //if (!_instantiator.hasConsistentlyTypedArgs(sig)) continue;

                // Rename any remaining variables in each action as new, unique q-constants 
                red = _htn.replaceQConstants(red, _layer_idx, _pos);
                
                // Check validity
                if (!_instantiator.hasConsistentlyTypedArgs(sig)) continue;
                if (!_instantiator.hasValidPreconditions(red, state)) continue;
                if (red.getTaskSignature() != task) continue;
                
                // Remove unneeded rigid conditions from the reduction
                _htn.removeRigidConditions(red);
                
                sig = red.getSignature();
                _htn._reductions_by_sig[sig] = red;
                result.push_back(sig);
            }
        }
    }
    return result;
}

std::vector<Signature> Planner::getAllActionsOfTask(const Signature& task, const State& state) {
    std::vector<Signature> result;

    if (!_htn._actions.count(task._name_id)) return result;

    // Check if the supplied task has consistent sorts
    //if (!_instantiator.hasConsistentlyTypedArgs(task)) return result;

    Action& a = _htn._actions[task._name_id];

    HtnOp op = a.substitute(Substitution::get(a.getArguments(), task._args));
    Action act = (Action) op;
    //log("  task %s : action found: %s\n", Names::to_string(task).c_str(), Names::to_string(act).c_str());
    
    std::vector<Action> actions = _instantiator.getApplicableInstantiations(act, _layers[_layer_idx][_pos].getState());
    for (Action action : actions) {
        Signature sig = action.getSignature();

        //if (!_instantiator.hasConsistentlyTypedArgs(sig)) continue;

        // Rename any remaining variables in each action as unique q-constants,
        action = _htn.replaceQConstants(action, _layer_idx, _pos);

        // Remove any inconsistent effects that were just created
        action.removeInconsistentEffects();

        // Check validity
        if (!_instantiator.hasConsistentlyTypedArgs(sig)) continue;
        if (!_instantiator.hasValidPreconditions(action, state)) continue;
        if (action.getSignature() != task) continue;
        
        // Remove unneeded rigid conditions from the reduction
        _htn.removeRigidConditions(action);

        sig = action.getSignature();
        _htn._actions_by_sig[sig] = action;
        result.push_back(sig);
    }
    return result;
}

