
#include <iostream>
#include <functional>
#include <regex>
#include <string>

#include "plan.hpp"

#include "planner.h"
#include "util/log.h"

int Planner::findPlan() {
    
    int iteration = 0;
    Log::i("Iteration %i.\n", iteration);

    createFirstLayer();

    // Bounds on depth to solve / explore
    int firstSatCallIteration = _params.getIntParam("d");
    int maxIterations = _params.getIntParam("D");

    bool solved = false;
    if (iteration >= firstSatCallIteration) {
        _enc.addAssumptions(_layer_idx);
        solved = _enc.solve();
    } 
    
    // Next layers
    while (!solved && (maxIterations == 0 || iteration < maxIterations)) {

        if (iteration >= firstSatCallIteration) {

            _enc.printFailedVars(_layers.back());

            if (_params.isSet("cs")) { // check solvability
                Log::i("Unsolvable at layer %i with assumptions\n", _layer_idx);

                // Attempt to solve formula again, now without assumptions
                // (is usually simple; if it fails, we know the entire problem is unsolvable)
                solved = _enc.solve();
                if (!solved) {
                    Log::w("Unsolvable at layer %i even without assumptions!\n", _layer_idx);
                    break;
                } else {
                    Log::i("Solvable without assumptions - expanding by another layer\n");
                }
            } else {
                Log::i("Unsolvable at layer %i -- expanding.\n", _layer_idx);
            }
        }

        iteration++;      
        Log::i("Iteration %i.\n", iteration);
        
        createNextLayer();

        if (iteration >= firstSatCallIteration) {
            _enc.addAssumptions(_layer_idx);
            solved = _enc.solve();
        } 
    }

    if (!solved) {
        if (iteration >= firstSatCallIteration) _enc.printFailedVars(_layers.back());
        Log::w("No success. Exiting.\n");
        return 1;
    }

    Log::i("Found a solution at layer %i.\n", _layers.size()-1);

    outputPlan();
    _enc.printStages();
    
    return 0;
}

void Planner::outputPlan() {

    // Extract solution
    auto planPair = _enc.extractPlan();

    // Create stringstream which is being fed the plan
    std::stringstream stream;

    // Print plan into stream

    // -- primitive part
    stream << "==>\n";
    FlatHashSet<int> actionIds;
    FlatHashSet<int> idsToRemove;
    for (PlanItem& item : planPair.first) {
        if (item.id < 0) continue;
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

        stream << item.id << " " << Names::to_string_nobrackets(_htn.cutNonoriginalTaskArguments(item.abstractTask)) << "\n";
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
        
        stream << item.id << " " << Names::to_string_nobrackets(_htn.cutNonoriginalTaskArguments(item.abstractTask)) << " -> " 
            << Names::to_string_nobrackets(item.reduction) << subtaskIdStr << "\n";
    }
    stream << "<==\n";

    // Feed plan into parser to convert it into a plan to the original problem
    // (w.r.t. previous compilations the parser did) and output it
    std::ostringstream outstream;
    convert_plan(stream, outstream);
    Log::log_notime(Log::V0_ESSENTIAL, outstream.str().c_str());
    Log::log_notime(Log::V0_ESSENTIAL, "<==\n");
    
    Log::i("End of solution plan.\n");
}

void Planner::createFirstLayer() {

    // Initial layer of size 2 (top level reduction + goal action)
    int initSize = 2;
    Log::i("Creating initial layer of size %i\n", initSize);
    _layer_idx = 0;
    _pos = 0;
    _layers.push_back(Layer(0, initSize));
    Layer& initLayer = _layers[0];
    initLayer[_pos].setPos(_layer_idx, _pos);
    
    /***** LAYER 0, POSITION 0 ******/

    // Initial state
    SigSet initState = _htn.getInitState();
    for (const Signature& fact : initState) {
        initLayer[_pos].addFact(fact._usig);
        initLayer[_pos].addDefinitiveFact(fact);
        getLayerState().add(_pos, fact);
    }

    // Instantiate all possible init. reductions
    _htn._init_reduction_choices = _instantiator.getApplicableInstantiations(
            _htn._init_reduction, getStateEvaluator());
    std::vector<Reduction> roots = _htn._init_reduction_choices;
    for (Reduction& r : roots) {

        if (addReduction(r, USignature())) {

            USignature sig = r.getSignature();
            
            initLayer[_pos].addReduction(sig);
            initLayer[_pos].addAxiomaticOp(sig);
            initLayer[_pos].addExpansionSize(r.getSubtasks().size());
            // Add preconditions
            for (const Signature& fact : r.getPreconditions()) {
                addPrecondition(sig, fact);
            }
            addQConstantTypeConstraints(sig);
            const PositionedUSig psig{_layer_idx,_pos,sig};
            _htn.addQConstantConditions(r, psig, QConstantDatabase::PSIG_ROOT, 0, getStateEvaluator());
        }
    }
    addNewFalseFacts();
    _htn._q_db.backpropagateConditions(_layer_idx, _pos, _layers[_layer_idx][_pos].getReductions());
    _enc.encode(_layer_idx, _pos++);

    /***** LAYER 0, POSITION 1 ******/

    createNextPosition(); // position 1

    // Create virtual goal action
    Action goalAction(_htn.getNameId("_GOAL_ACTION_"), std::vector<int>());
    USignature goalSig = goalAction.getSignature();
    _htn._actions[goalSig._name_id] = goalAction;
    initLayer[_pos].addAction(goalSig);
    initLayer[_pos].addAxiomaticOp(goalSig);
    
    // Extract primitive goals, add to preconds of goal action
    SigSet goalSet = _htn.getGoals();
    for (const Signature& fact : goalSet) {
        assert(getLayerState().contains(_pos, fact));
        assert(initLayer[_pos].hasFact(fact.getUnsigned()));
        goalAction.addPrecondition(fact);
        addPrecondition(goalSig, fact);
    }
    _htn._actions_by_sig[goalSig] = goalAction;
    
    _enc.encode(_layer_idx, _pos++);

    /***** LAYER 0 END ******/

    initLayer.consolidate();
}

void Planner::createNextLayer() {

    _layers.emplace_back(_layers.size(), _layers.back().getNextLayerSize());
    Layer& newLayer = _layers.back();
    Log::i("New layer size: %i\n", newLayer.size());
    Layer& oldLayer = _layers[_layer_idx];
    _layer_idx++;
    _pos = 0;

    for (_old_pos = 0; _old_pos < oldLayer.size(); _old_pos++) {
        int newPos = oldLayer.getSuccessorPos(_old_pos);
        int maxOffset = oldLayer[_old_pos].getMaxExpansionSize();

        for (int offset = 0; offset < maxOffset; offset++) {
            assert(_pos == newPos + offset);
            Log::i(" Position (%i,%i)\n", _layer_idx, _pos);
            Log::v("  Instantiating ...\n");

            //log("%i,%i,%i,%i\n", oldPos, newPos, offset, newLayer.size());
            assert(newPos+offset < newLayer.size());

            createNextPosition();
            Log::v("  Instantiation done. (r=%i a=%i f=%i qf=%i)\n", 
                    _layers[_layer_idx][_pos].getReductions().size(),
                    _layers[_layer_idx][_pos].getActions().size(),
                    _layers[_layer_idx][_pos].getFacts().size(),
                    _layers[_layer_idx][_pos].getNumQFacts()
            );
            _enc.encode(_layer_idx, _pos++);
        }
    }

    newLayer.consolidate();
}

void Planner::createNextPosition() {

    // Set up all facts that may hold at this position.
    if (_pos == 0) {
        propagateInitialState();
    } else {
        Position& left = _layers[_layer_idx][_pos-1];
        createNextPositionFromLeft(left);
    }

    // Generate this new position's content based on the facts and the position above.
    if (_layer_idx > 0) {
        Position& above = _layers[_layer_idx-1][_old_pos];
        createNextPositionFromAbove(above);
    }

    // In preparation for the upcoming position,
    // add all effects of the actions and reductions occurring HERE
    // as (initially false) facts to THIS position.  
    addNewFalseFacts();

    // Use new q-constant conditions from this position to infer conditions 
    // of the respective parent ops at the layer above. 
    auto updatedOps = _htn._q_db.backpropagateConditions(_layer_idx, _pos, _layers[_layer_idx][_pos].getActions());
    auto updatedReductions = _htn._q_db.backpropagateConditions(_layer_idx, _pos, _layers[_layer_idx][_pos].getReductions());
    updatedOps.insert(updatedReductions.begin(), updatedReductions.end());

    pruneRetroactively(updatedOps);

    // Remove all q fact decodings which have become invalid
    for (const auto& entry : _layers[_layer_idx][_pos].getQFacts()) for (const auto& qfactSig : entry.second) {

        std::vector<int> qargs, qargIndices;
        for (int i = 0; i < qfactSig._args.size(); i++) {
            const int& arg = qfactSig._args[i];
            if (_htn._q_constants.count(arg)) {
                qargs.push_back(arg);
                qargIndices.push_back(i);
            }
        }

        USigSet decodingsToRemove;
        for (const auto& decFactSig : _htn.getQFactDecodings(qfactSig)) {
            if (!_htn.isAbstraction(decFactSig, qfactSig)) {
                decodingsToRemove.insert(decFactSig);
                Log::d("REMOVE_DECODING %s@(%i,%i)\n", TOSTR(decFactSig), _layer_idx, _pos);

            }
        }
        // Remove all invalid q fact decodings
        for (const auto& decFactSig : decodingsToRemove) {
            _htn.removeQFactDecoding(qfactSig, decFactSig);
            
            std::vector<int> decargs; for (int idx : qargIndices) decargs.push_back(decFactSig._args[idx]);
            _htn._forbidden_substitutions.insert(Substitution(qargs, decargs));
        } 
    }
}

void Planner::createNextPositionFromAbove(const Position& above) {
    Position& newPos = _layers[_layer_idx][_pos];
    newPos.setPos(_layer_idx, _pos);

    int offset = _pos - _layers[_layer_idx-1].getSuccessorPos(_old_pos);
    if (offset == 0) {
        // Propagate facts
        for (const auto& entry : above.getQFacts()) for (const USignature& fact : entry.second) {
            newPos.addQFact(fact);
        }
        for (const USignature& fact : above.getFacts()) {
            //assert(newPos.getFacts().count(entry.first)); // already added as false fact!
            newPos.addFact(fact);
        }
    }

    propagateActions(offset);
    propagateReductions(offset);
}

void Planner::createNextPositionFromLeft(const Position& left) {
    Position& newPos = _layers[_layer_idx][_pos];
    newPos.setPos(_layer_idx, _pos);
    assert(left.getPos() == IntPair(_layer_idx, _pos-1));

    // Propagate state
    //newPos.extendState(left.getState());

    FlatHashSet<int> relevantQConstants;

    // Propagate fact changes from operations from previous position
    for (const auto& entry : left.getActions()) {
        const USignature& aSig = entry.first;
        for (const Signature& fact : left.getFactChanges(aSig)) {
            addEffect(aSig, fact);
        }
        for (const int& arg : aSig._args) {
            if (_htn._q_constants.count(arg)) relevantQConstants.insert(arg);
        }
    }
    for (const auto& entry : left.getReductions()) {
        const USignature& rSig = entry.first;
        if (rSig == Position::NONE_SIG) continue;
        for (const Signature& fact : left.getFactChanges(rSig)) {
            addEffect(rSig, fact);
        }
        for (const int& arg : rSig._args) {
            if (_htn._q_constants.count(arg)) relevantQConstants.insert(arg);
        }
    }

    // Propagate occurring facts
    for (const USignature& fact : left.getFacts()) newPos.addFact(fact);
    for (const auto& entry : left.getQFacts()) for (const USignature& fact : entry.second) {
        bool add = true;
        for (const int& arg : fact._args) {
            if (_htn._q_constants.count(arg) && !relevantQConstants.count(arg)) add = false;
        }
        if (!add) {
            // forget q-facts that have become irrelevant
            //log("  FORGET %s\n", TOSTR(entry.first));
            getLayerState().withdraw(_pos, fact, true);
            getLayerState().withdraw(_pos, fact, false);
            continue;
        }
        newPos.addQFact(fact);
    }
}

void Planner::addPrecondition(const USignature& op, const Signature& fact) {
    Position& pos = _layers[_layer_idx][_pos];
    const USignature& factAbs = fact.getUnsigned();

    bool isQFact = _htn.hasQConstants(factAbs);

    if (fact._negated && !isQFact && !pos.hasFact(factAbs)) {
        // Negative precondition not contained in facts: initialize
        //log("NEG_PRE %s\n", TOSTR(fact));
        introduceNewFalseFact(pos, factAbs);
    }
    
    //log("pre %s of %s\n", TOSTR(fact), TOSTR(op));
    // Precondition must be valid (or a q fact)
    if (!isQFact) assert(getLayerState().contains(_pos, fact) 
            || Log::e("%s not contained in state!\n", TOSTR(fact)));

    // Add additional reason for the fact / add it first if it's a q-constant
    if (isQFact) pos.addQFact(factAbs);
    else pos.addFact(factAbs);

    // For each fact decoded from the q-fact:
    assert(!isQFact || !_htn.getDecodedObjects(factAbs, false).empty());
    for (const USignature& decFactAbs : _htn.getDecodedObjects(factAbs, false)) {
        Signature decFact(decFactAbs, fact._negated);
        
        if (!_instantiator.test(decFact, getStateEvaluator())) {
            // Fact cannot be true here
            pos.addForbiddenSubstitution(op, factAbs._args, decFactAbs._args);
            continue;
        }

        if (!pos.hasFact(decFactAbs)) {
            // Decoded fact did not occur before.
            introduceNewFalseFact(pos, decFactAbs);
        }

        _htn.addQFactDecoding(factAbs, decFactAbs);
        pos.addFact(decFactAbs); // also add fact as an (indirect) consequence of op
    }
}

void Planner::addEffect(const USignature& opSig, const Signature& fact) {
    Position& pos = _layers[_layer_idx][_pos];
    assert(_pos > 0);
    Position& left = _layers[_layer_idx][_pos-1];
    //Position& left = _layers[_layer_idx][_pos-1];
    USignature factAbs = fact.getUnsigned();
    bool isQFact = _htn.hasQConstants(factAbs);
    if (isQFact) pos.addQFact(factAbs);
    else pos.addFact(factAbs);

    // Depending on whether fact supports are encoded for primitive ops only,
    // add the fact to the op's support accordingly
    if (_params.isSet("nps") || _htn._actions_by_sig.count(opSig)) {
        pos.addFactSupport(fact, opSig);
    } else {
        // Remember that there is some (unspecified) support for this fact
        pos.touchFactSupport(fact);
    }
    
    getLayerState().add(_pos, fact);

    if (!isQFact) return;

    //log("Calc decoded effects for %s:%s\n", TOSTR(opSig), TOSTR(fact));

    HtnOp& op = _htn.getOp(opSig);
    //assert(!_htn.getDecodedObjects(factAbs, true).empty());
    int numValid = 0;

    /*
    // Assemble reference list of contained q-constants for decodings validity check
    std::vector<int> qconsts, qconstIndices;
    for (int i = 0; i < fact._usig._args.size(); i++) {
        if (_htn._q_constants.count(fact._usig._args[i])) {
            qconsts.push_back(fact._usig._args[i]);
            qconstIndices.push_back(i);
        }
    }*/

    for (const USignature& decFactAbs : _htn.getDecodedObjects(factAbs, true)) {
        
        /*
        // Check if the preconditions of op subject to such a fact encoding can hold
        const auto& subs = _instantiator.getOperationSubstitutionsCausingEffect(left.getFactChanges(opSig), decFactAbs, fact._negated);
        bool valid = false;
        // For each possible substitution leading to the fact:
        for (const auto& sub : subs) {
            HtnOp opSub = op.substitute(sub);

            // Try to get a single valid (w.r.t. preconds) instantiation of the operator
            std::vector<int> varArgs;
            for (const int& arg : opSub.getArguments()) {
                if (_htn._var_ids.count(arg)) varArgs.push_back(arg);
            }
            USigSet inst = _instantiator.instantiateLimited(opSub, getStateEvaluator(_layer_idx, _pos-1), varArgs, 1, true);

            if (!inst.empty()) {
                // Success -- the effect may actually happen
                valid = true;
                break;
            }
        }
        if (!valid) continue;
        */

        /*
        // Test if this q constant substitution is valid
        std::vector<int> vals;
        for (const int& i : qconstIndices) vals.push_back(decFactAbs._args[i]);
        if (!_htn._q_db.test(qconsts, vals)) continue;
        */

        Signature decFact(decFactAbs, fact._negated);
        _htn.addQFactDecoding(factAbs, decFactAbs);
        getLayerState().add(_pos, decFact);
        numValid++;
    }

    if (numValid == 0) {
        //log("No valid decoded effects for %s!\n", TOSTR(opSig));
    }
}

void Planner::propagateInitialState() {
    assert(_layer_idx > 0);
    assert(_pos == 0);

    Position& newPos = _layers[_layer_idx][0];
    Position& above = _layers[_layer_idx-1][0];

    // Propagate occurring facts
    for (const USignature& fact : above.getFacts()) {
        newPos.addFact(fact);
    }
    for (const auto& entry : above.getQFacts()) for (const USignature& fact : entry.second) {
        newPos.addQFact(fact);
    }
    // Propagate TRUE facts
    for (const USignature& fact : above.getTrueFacts())
        newPos.addTrueFact(fact);
    for (const USignature& fact : above.getFalseFacts())
        newPos.addFalseFact(fact);

    // Propagate state: initial position and all q-facts
    getLayerState(_layer_idx) = //LayerState(getLayerState(_layer_idx-1), _layers[_layer_idx-1].getOffsets());
    LayerState();
    const auto& oldState = getLayerState(_layer_idx-1);
    auto& newState = getLayerState(_layer_idx);
    for (bool neg : {true, false}) {
        for (const auto& entry : neg ? oldState.getNegFactOccurrences() : oldState.getPosFactOccurrences()) {
            const USignature& fact = entry.first;
            //log("  ~~~> %s\n", TOSTR(fact));
            const auto& range = entry.second;
            if (range.first == 0 || _htn.hasQConstants(fact)) {
                int newRangeFirst = _layers[_layer_idx-1].getSuccessorPos(range.first);
                newState.add(newRangeFirst, fact, neg);
                if (range.second != INT32_MAX) {
                    int newRangeSecond = _layers[_layer_idx-1].getSuccessorPos(range.second);    
                    newState.withdraw(newRangeSecond, fact, neg);
                }
            }
        }
    }
    Log::d("%i neg, %i pos ~~~> %i neg, %i pos\n", oldState.getNegFactOccurrences().size(), oldState.getPosFactOccurrences().size(), 
                                                newState.getNegFactOccurrences().size(), newState.getPosFactOccurrences().size());
    
}

void Planner::propagateActions(int offset) {
    Position& newPos = _layers[_layer_idx][_pos];
    Position& above = _layers[_layer_idx-1][_old_pos];

    // Propagate actions
    for (const auto& entry : above.getActions()) {
        const USignature& aSig = entry.first;
        if (aSig == Position::NONE_SIG) continue;
        const Action& a = _htn._actions_by_sig[aSig];

        // Can the action occur here w.r.t. the current state?
        bool valid = _instantiator.hasValidPreconditions(a.getPreconditions(), getStateEvaluator());

        // If not: forbid the action, i.e., its parent action
        if (!valid) {
            //log("FORBIDDING action %s@(%i,%i): no children at offset %i\n", 
            //        TOSTR(aSig), _layer_idx-1, _old_pos, offset);
            newPos.addExpansion(aSig, Position::NONE_SIG);
            continue;
        }

        if (offset < 1) {
            // proper action propagation
            assert(_instantiator.isFullyGround(aSig));
            newPos.addAction(aSig);
            newPos.addExpansion(aSig, aSig);
            // Add preconditions of action
            for (const Signature& fact : a.getPreconditions()) {
                addPrecondition(aSig, fact);
            }
        } else {
            // action expands to "blank" at non-zero offsets
            USignature blankSig = _htn._action_blank.getSignature();
            newPos.addAction(blankSig);
            newPos.addExpansion(aSig, blankSig);
        }
    }
}

void Planner::propagateReductions(int offset) {
    Position& newPos = _layers[_layer_idx][_pos];
    Position& above = _layers[_layer_idx-1][_old_pos];

    // Expand reductions
    for (const auto& entry : above.getReductions()) {
        const USignature& rSig = entry.first;
        if (rSig == Position::NONE_SIG) continue;
        const Reduction r = _htn._reductions_by_sig[rSig];
        const PositionedUSig parentPSig(_layer_idx-1, _old_pos, rSig);
        
        int numAdded = 0;
        if (offset < r.getSubtasks().size()) {
            // Proper expansion
            const USignature& subtask = r.getSubtasks()[offset];
            auto allActions = getAllActionsOfTask(subtask, getStateEvaluator());
            // reduction(s)?
            for (const USignature& subRSig : getAllReductionsOfTask(subtask, getStateEvaluator())) {
                
                if (_htn._actions_by_sig.count(subRSig)) {
                    // Actually an action, not a reduction
                    allActions.push_back(subRSig);
                    continue;
                }

                numAdded++;
                assert(_htn._reductions_by_sig.count(subRSig));
                const Reduction& subR = _htn._reductions_by_sig[subRSig];

                assert(subRSig == _htn._reductions_by_sig[subRSig].getSignature());
                assert(_instantiator.isFullyGround(subRSig));
                
                newPos.addReduction(subRSig);
                newPos.addExpansion(rSig, subRSig);
                //if (_layer_idx <= 1) log("ADD %s:%s @ (%i,%i)\n", TOSTR(subR.getTaskSignature()), TOSTR(subRSig), _layer_idx, _pos);
                newPos.addExpansionSize(subR.getSubtasks().size());
                // Add preconditions of reduction
                //log("PRECONDS %s ", TOSTR(subRSig));
                for (const Signature& fact : subR.getPreconditions()) {
                    addPrecondition(subRSig, fact);
                    //log("%s ", TOSTR(fact));
                }
                addQConstantTypeConstraints(subRSig);
                _htn.addQConstantConditions(subR, PositionedUSig(_layer_idx, _pos, subRSig), 
                                        parentPSig, offset, getStateEvaluator());
                //log("\n");
            }
            // action(s)?
            for (const USignature& aSig : allActions) {
                numAdded++;
                assert(_instantiator.isFullyGround(aSig));
                newPos.addAction(aSig);
                newPos.addExpansion(rSig, aSig);
                // Add preconditions of action
                const Action& a = _htn._actions_by_sig[aSig];
                for (const Signature& fact : a.getPreconditions()) {
                    addPrecondition(aSig, fact);
                }
                addQConstantTypeConstraints(aSig);
                _htn.addQConstantConditions(a, PositionedUSig(_layer_idx, _pos, aSig), 
                                        parentPSig, offset, getStateEvaluator());
            }
        } else {
            // Blank
            numAdded++;
            USignature blankSig = _htn._action_blank.getSignature();
            newPos.addAction(blankSig);
            newPos.addExpansion(rSig, blankSig);
        }

        if (numAdded == 0) {
            // Explicitly forbid the parent!
            Log::i("Forbidding reduction %s@(%i,%i): no children at offset %i\n", 
                    TOSTR(rSig), _layer_idx-1, _old_pos, offset);
            newPos.addExpansion(rSig, Position::NONE_SIG);
        }
    }
}

std::vector<USignature> Planner::getAllActionsOfTask(const USignature& task, std::function<bool(const Signature&)> state) {
    std::vector<USignature> result;

    if (!_htn._actions.count(task._name_id)) return result;

    const Action& a = _htn._actions[task._name_id];

    HtnOp op = a.substitute(Substitution(a.getArguments(), task._args));
    Action act = (Action) op;
    
    std::vector<Action> actions = _instantiator.getApplicableInstantiations(act, state);
    for (Action& action : actions) {
        if (addAction(action, task)) result.push_back(action.getSignature());
    }
    return result;
}

std::vector<USignature> Planner::getAllReductionsOfTask(const USignature& task, std::function<bool(const Signature&)> state) {
    std::vector<USignature> result;

    if (!_htn._task_id_to_reduction_ids.count(task._name_id)) return result;

    // Filter and minimally instantiate methods
    // applicable in current (super)state
    for (int redId : _htn._task_id_to_reduction_ids[task._name_id]) {
        Reduction r = _htn._reductions[redId];

        if (_htn._reduction_to_surrogate.count(redId)) {
            assert(_htn._actions.count(_htn._reduction_to_surrogate.at(redId)));
            Action& a = _htn._actions.at(_htn._reduction_to_surrogate.at(redId));

            std::vector<Substitution> subs = Substitution::getAll(r.getTaskArguments(), task._args);
            for (const Substitution& s : subs) {
                USignature surrSig = a.getSignature().substitute(s);
                for (const auto& sig : getAllActionsOfTask(surrSig, state)) result.push_back(sig);
            }
            return result;
        }

        std::vector<Substitution> subs = Substitution::getAll(r.getTaskArguments(), task._args);
        for (const Substitution& s : subs) {
            for (const auto& entry : s) assert(entry.second != 0);

            //if (_layer_idx <= 1) log("SUBST %s\n", TOSTR(s));
            Reduction rSub = r.substituteRed(s);
            USignature origSig = rSub.getSignature();
            if (!_instantiator.hasConsistentlyTypedArgs(origSig)) continue;
            
            //log("   reduction %s ~> %i instantiations\n", TOSTR(origSig), reductions.size());
            std::vector<Reduction> reductions = _instantiator.getApplicableInstantiations(rSub, state);
            for (Reduction& red : reductions) {
                if (addReduction(red, task)) result.push_back(red.getSignature());
            }
        }
    }
    return result;
}

bool Planner::addAction(Action& action, const USignature& task) {

    USignature sig = action.getSignature();

    //log("ADDACTION %s\n", TOSTR(sig));

    // Rename any remaining variables in each action as unique q-constants,
    action = _htn.replaceVariablesWithQConstants(action, _layer_idx, _pos, getStateEvaluator());

    // Remove any inconsistent effects that were just created
    action.removeInconsistentEffects();

    // Check validity
    if (task._name_id >= 0 && action.getSignature() != task) return false;
    if (!_instantiator.isFullyGround(action.getSignature())) return false;
    if (!_instantiator.hasConsistentlyTypedArgs(sig)) return false;
    if (!_instantiator.hasValidPreconditions(action.getPreconditions(), getStateEvaluator())) return false;
    
    // Remove unneeded rigid conditions from the reduction
    _htn.removeRigidConditions(action);

    sig = action.getSignature();
    _htn._actions_by_sig[sig] = action;

    // Compute fact changes
    _layers[_layer_idx][_pos].setFactChanges(sig, _instantiator.getAllFactChanges(sig));

    Log::d("ADDACTION -- added\n");
    return true;
}

bool Planner::addReduction(Reduction& red, const USignature& task) {
    USignature sig = red.getSignature();

    // Rename any remaining variables in each action as new, unique q-constants 
    red = _htn.replaceVariablesWithQConstants(red, _layer_idx, _pos, getStateEvaluator());
    
    // Check validity
    if (task._name_id >= 0 && red.getTaskSignature() != task) return false;
    if (!_instantiator.isFullyGround(red.getSignature())) return false;
    if (!_instantiator.hasConsistentlyTypedArgs(sig)) return false;
    if (!_instantiator.hasValidPreconditions(red.getPreconditions(), getStateEvaluator())) return false;
    
    // Remove unneeded rigid conditions from the reduction
    _htn.removeRigidConditions(red);
    
    sig = red.getSignature();
    _htn._reductions_by_sig[sig] = red;

    // Compute fact changes
    _layers[_layer_idx][_pos].setFactChanges(sig, _instantiator.getAllFactChanges(sig));

    return true;
}

void Planner::addNewFalseFacts() {
    Position& newPos = _layers[_layer_idx][_pos];
    
    // For each action:
    for (const auto& entry : newPos.getActions()) {
        const USignature& aSig = entry.first;

        newPos.setFactChanges(aSig, _instantiator.getAllFactChanges(aSig));

        for (const Signature& eff : newPos.getFactChanges(aSig)) {

            if (!_htn.hasQConstants(eff._usig) && !newPos.hasFact(eff._usig)) {
                // New fact: set to false before the action may happen
                introduceNewFalseFact(newPos, eff._usig);
            }

            for (const USignature& decEff : _htn.getDecodedObjects(eff._usig, true)) {
                if (!newPos.hasFact(decEff)) {
                    // New fact: set to false before the action may happen
                    introduceNewFalseFact(newPos, decEff);
                }
            }
        }
    }

    // For each possible reduction effect: 
    for (const auto& entry : newPos.getReductions()) {
        const USignature& rSig = entry.first;
        if (rSig == Position::NONE_SIG) continue;

        newPos.setFactChanges(rSig, _instantiator.getAllFactChanges(rSig));

        for (const Signature& eff : newPos.getFactChanges(rSig)) {

            if (!_htn.hasQConstants(eff._usig) && !newPos.getFacts().count(eff._usig)) {
                // New fact: set to false before the reduction may happen
                introduceNewFalseFact(newPos, eff._usig);
            }

            for (const USignature& decEff : _htn.getDecodedObjects(eff._usig, true)) {
                if (!newPos.hasFact(decEff)) {
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

        for (const USignature& fact : newAbove.getFacts()) {
            // If fact was not seen here before
            if (!newPos.hasFact(fact)) {
                // Add fact
                newPos.addFact(fact);
                introduceNewFalseFact(newPos, fact);
            }
        }
        for (const auto& entry : newAbove.getQFacts()) for (const USignature& fact : entry.second) {
            // If fact was not seen here before
            if (!newPos.hasQFact(fact)) {
                newPos.addQFact(fact);
            }
        }
    }
}

void Planner::introduceNewFalseFact(Position& newPos, const USignature& fact) {
    Signature sig(fact, /*negated=*/true);
    assert(!_htn.hasQConstants(fact));
    newPos.addDefinitiveFact(sig);
    newPos.addFact(fact);
    getLayerState(newPos.getPos().first).add(newPos.getPos().second, sig);
    //log("FALSE_FACT %s @(%i,%i)\n", TOSTR(fact.abs().opposite()), 
    //        newPos.getPos().first, newPos.getPos().second);
}

void Planner::addQConstantTypeConstraints(const USignature& op) {
    // Add type constraints for q constants
    std::vector<TypeConstraint> cs = _instantiator.getQConstantTypeConstraints(op);
    // Add to this position's data structure
    for (const TypeConstraint& c : cs) {
        _layers[_layer_idx][_pos].addQConstantTypeConstraint(op, c);
    }
}

void Planner::pruneRetroactively(const NodeHashSet<PositionedUSig, PositionedUSigHasher>& updatedOps) {

    // TODO Retroactive pruning.
    // If they or some of their (recursive) children become impossible,
    // remove these ops, all of their children and recursively their parent
    // if the parent has no valid children at that position any more.

    if (!updatedOps.empty()) Log::d("%i ops to update\n", updatedOps.size());

    // For all ops which have become more restricted
    for (const auto& pusig : updatedOps) {
        
        const auto& sig = pusig.usig;
        int layerIdx = pusig.layer;
        int pos = pusig.pos;
        HtnOp& op = _htn.getOp(sig);

        // TODO What if op did not become completely impossible, but just more restricted?
        // => Iterate over possible children, add these to the "stack" of ops to be updated.

        if (!_instantiator.hasValidPreconditions(op.getPreconditions(), getStateEvaluator(layerIdx, pos))) {
            // Operation has become impossible to apply
            Log::d("Op %s became impossible!\n", TOSTR(sig));

            // COMPLETELY remove this op, disregarding witness counters.

            bool isReduction = _htn._reductions.count(sig._name_id);
            if (isReduction) _layers[layerIdx][pos].removeReductionOccurrence(sig);
            else _layers[layerIdx][pos].removeActionOccurrence(sig);
        }
    }
}


LayerState& Planner::getLayerState(int layer) {
    if (layer == -1) layer = _layer_idx;
    return _layers[layer].getState();
}

std::function<bool(const Signature&)> Planner::getStateEvaluator(int layer, int pos) {
    if (layer == -1) layer = _layer_idx;
    if (pos == -1) pos = _pos;
    return [this,layer,pos](const Signature& sig) {
        bool holds = getLayerState(layer).contains(pos, sig);
        //log("STATEEVAL@(%i,%i) %s : %i\n", layer, pos, TOSTR(sig), holds);
        return holds;
    };
}
