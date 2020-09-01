
#include <iostream>
#include <functional>
#include <regex>
#include <string>

#include "plan.hpp"
#include "verify.hpp"

#include "planner.h"
#include "util/log.h"
#include "util/signal_manager.h"
#include "util/timer.h"

int terminateSatCall(void* state) {
    Planner* planner = (Planner*) state;

    // Breaking out of first SAT call after some time
    if (planner->_sat_time_limit > 0 &&
        planner->_enc.getTimeSinceSatCallStart() > planner->_sat_time_limit) {
        return 1;
    }
    // Plan length optimization limit hit
    if (planner->cancelOptimization()) {
        return 1;
    }
    // Termination by interruption signal
    if (SignalManager::isExitSet()) return 1;
    return 0;
}

void Planner::checkTermination() {
    bool exitSet = SignalManager::isExitSet();
    bool cancelOpt = cancelOptimization();
    if (exitSet) {
        if (_has_plan) {
            Log::i("Termination signal caught - printing last found plan.\n");
            outputPlan();
        } else {
            Log::i("Termination signal caught.\n");
        }
    } else if (cancelOpt) {
        Log::i("Cancelling optimization according to provided limit.\n");
        outputPlan();
    }
    if (exitSet || cancelOpt) {
        _enc.printStages();
        Log::i("Exiting happily.\n");
        exit(0);
    }
}

bool Planner::cancelOptimization() {
    return _time_at_first_plan > 0 &&
            _optimization_factor > 0 &&
            Timer::elapsedSeconds() > (1+_optimization_factor) * _time_at_first_plan;
}

int Planner::findPlan() {
    
    int iteration = 0;
    Log::i("Iteration %i.\n", iteration);

    createFirstLayer();

    // Bounds on depth to solve / explore
    int firstSatCallIteration = _params.getIntParam("d");
    int maxIterations = _params.getIntParam("D");
    _sat_time_limit = _params.getFloatParam("stl");

    bool solved = false;
    if (_sat_time_limit > 0 || _optimization_factor != 0) _enc.setTerminateCallback(this, terminateSatCall);
    if (iteration >= firstSatCallIteration) {
        _enc.addAssumptions(_layer_idx);
        int result = _enc.solve();
        if (result == 0) {
            Log::w("Solver was interrupted. Discarding time limit for next solving attempts.\n");
            _sat_time_limit = 0;
        }
        solved = result == 10;
    } 
    
    // Next layers
    while (!solved && (maxIterations == 0 || iteration < maxIterations)) {

        if (iteration >= firstSatCallIteration) {

            _enc.printFailedVars(*_layers.back());

            if (_params.isNonzero("cs")) { // check solvability
                Log::i("Not solved at layer %i with assumptions\n", _layer_idx);

                // Attempt to solve formula again, now without assumptions
                // (is usually simple; if it fails, we know the entire problem is unsolvable)
                int result = _enc.solve();
                if (result == 20) {
                    Log::w("Unsolvable at layer %i even without assumptions!\n", _layer_idx);
                    break;
                } else {
                    Log::i("Not proven unsolvable - expanding by another layer\n");
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
            int result = _enc.solve();
            if (result == 0) {
                Log::w("Solver was interrupted. Discarding time limit for next solving attempts.\n");
                _sat_time_limit = 0;
            }
            solved = result == 10;
        } 
    }

    if (!solved) {
        if (iteration >= firstSatCallIteration) _enc.printFailedVars(*_layers.back());
        Log::w("No success. Exiting.\n");
        return 1;
    }

    Log::i("Found a solution at layer %i.\n", _layers.size()-1);
    _time_at_first_plan = Timer::elapsedSeconds();

    if (_optimization_factor != 0) {
        _has_plan = true;
        _enc.optimizePlan(_plan);
    } else {
        _plan = _enc.extractPlan();
        _has_plan = true;
    }
    outputPlan();

    _enc.printStages();
    
    return 0;
}

void Planner::outputPlan() {

    // Create stringstream which is being fed the plan
    std::stringstream stream;

    // Print plan into stream

    // -- primitive part
    stream << "==>\n";
    FlatHashSet<int> actionIds;
    FlatHashSet<int> idsToRemove;

    FlatHashSet<int> surrogateIds;
    std::vector<PlanItem> decompsToInsert;
    size_t decompsToInsertIdx = 0;
    size_t length = 0;
    
    for (PlanItem& item : std::get<0>(_plan)) {

        if (item.id < 0) continue;
        
        if (_htn.toString(item.abstractTask._name_id).rfind("__LLT_SECOND") != std::string::npos) {
            // Second part of a split action: discard
            idsToRemove.insert(item.id);
            continue;
        }
        if (_htn.toString(item.abstractTask._name_id).rfind("_FIRST") != std::string::npos) {
            // First part of a split action: change name, then handle normally
            item.abstractTask._name_id = _htn.getSplitAction(item.abstractTask._name_id);
        }
        
        if (_htn.toString(item.abstractTask._name_id).rfind("__SURROGATE") != std::string::npos) {
            // Surrogate action: Replace with actual action, remember represented method to include in decomposition

            [[maybe_unused]] const auto& [parentId, childId] = _htn.getParentAndChildFromSurrogate(item.abstractTask._name_id);
            const Reduction& parentRed = _htn.toReduction(parentId, item.abstractTask._args);
            surrogateIds.insert(item.id);
            
            PlanItem parent;
            parent.abstractTask = parentRed.getTaskSignature();  
            parent.id = item.id-1;
            parent.reduction = parentRed.getSignature();
            parent.subtaskIds = std::vector<int>(1, item.id);
            decompsToInsert.push_back(parent);

            const USignature& childSig = parentRed.getSubtasks()[0];
            item.abstractTask = childSig;
        }

        actionIds.insert(item.id);

        // Do not write blank actions or the virtual goal action
        if (item.abstractTask == _htn.getBlankActionSig()) continue;
        if (item.abstractTask._name_id == _htn.nameId("_GOAL_ACTION_")) continue;

        stream << item.id << " " << Names::to_string_nobrackets(_htn.cutNonoriginalTaskArguments(item.abstractTask)) << "\n";
        length++;
    }
    // -- decomposition part
    bool root = true;
    for (size_t itemIdx = 0; itemIdx < _plan.second.size() || decompsToInsertIdx < decompsToInsert.size(); itemIdx++) {

        // Pick next plan item to print
        PlanItem item;
        if (decompsToInsertIdx < decompsToInsert.size() && (itemIdx >= _plan.second.size() || decompsToInsert[decompsToInsertIdx].id < _plan.second[itemIdx].id)) {
            // Pick plan item from surrogate decompositions
            item = decompsToInsert[decompsToInsertIdx];
            decompsToInsertIdx++;
            itemIdx--;
        } else {
            // Pick plan item from "normal" plan list
            item = _plan.second[itemIdx];
        }
        if (item.id < 0) continue;

        std::string subtaskIdStr = "";
        for (int subtaskId : item.subtaskIds) {
            if (item.id+1 != subtaskId && surrogateIds.count(subtaskId)) subtaskId--;
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
    // (w.r.t. previous compilations the parser did)
    std::ostringstream outstream;
    convert_plan(stream, outstream);
    std::string planStr = outstream.str();

    if (_params.isNonzero("vp")) {
        // Verify plan (by copying converted plan stream and putting it back into panda)
        std::stringstream verifyStream;
        verifyStream << planStr << std::endl;
        bool ok = verify_plan(verifyStream, /*useOrderingInfo=*/true, /*lenientMode=*/false, /*debugMode=*/0);
        if (!ok) {
            Log::e("ERROR: Plan declared invalid by pandaPIparser! Exiting.\n");
            exit(1);
        }
    }
    
    // Print plan
    Log::log_notime(Log::V0_ESSENTIAL, planStr.c_str());
    Log::log_notime(Log::V0_ESSENTIAL, "<==\n");
    
    Log::i("End of solution plan. (counted length of %i)\n", length);
}

void Planner::createFirstLayer() {

    // Initial layer of size 2 (top level reduction + goal action)
    int initSize = 2;
    Log::i("Creating initial layer of size %i\n", initSize);
    _layer_idx = 0;
    _pos = 0;
    _layers.push_back(new Layer(0, initSize));
    Layer& initLayer = (*_layers[0]);
    initLayer[_pos].setPos(_layer_idx, _pos);
    
    /***** LAYER 0, POSITION 0 ******/

    // Initial state
    SigSet initState = _htn.getInitState();
    for (const Signature& fact : initState) {
        initLayer[_pos].addDefinitiveFact(fact);
        getLayerState().add(_pos, fact);
    }

    // Instantiate all possible init. reductions
    std::vector<Reduction> roots = _instantiator.getApplicableInstantiations(
            _htn.getInitReduction(), getStateEvaluator());
    for (Reduction& r : roots) {

        if (addReduction(r, USignature())) {
            USignature sig = r.getSignature();
            
            initLayer[_pos].addReduction(sig);
            initLayer[_pos].addAxiomaticOp(sig);
            initLayer[_pos].addExpansionSize(r.getSubtasks().size());
            // Add preconditions
            NodeHashSet<Substitution, Substitution::Hasher> badSubs;
            std::vector<NodeHashSet<Substitution, Substitution::Hasher>> goodSubs;
            for (const Signature& fact : r.getPreconditions()) {
                addPrecondition(sig, fact, goodSubs, badSubs);
            }
            addSubstitutionConstraints(sig, goodSubs, badSubs);
            addQConstantTypeConstraints(sig);
            const PositionedUSig psig{_layer_idx,_pos,sig};
            _htn.addQConstantConditions(r, psig, QConstantDatabase::PSIG_ROOT, 0, getStateEvaluator());
        }
    }
    addNewFalseFacts();
    //_htn.getQConstantDatabase().backpropagateConditions(_layer_idx, _pos, (*_layers[_layer_idx])[_pos].getReductions());
    _enc.encode(_layer_idx, _pos++);

    /***** LAYER 0, POSITION 1 ******/

    createNextPosition(); // position 1

    // Create virtual goal action
    Action goalAction = _htn.getGoalAction();
    USignature goalSig = goalAction.getSignature();
    initLayer[_pos].addAction(goalSig);
    initLayer[_pos].addAxiomaticOp(goalSig);
    
    // Extract primitive goals, add to preconds of goal action
    NodeHashSet<Substitution, Substitution::Hasher> badSubs;
    std::vector<NodeHashSet<Substitution, Substitution::Hasher>> goodSubs;
    for (const Signature& fact : goalAction.getPreconditions()) {
        assert(getLayerState().contains(_pos, fact));
        addPrecondition(goalSig, fact, goodSubs, badSubs);
    }
    assert(goodSubs.empty() && badSubs.empty());
    
    _enc.encode(_layer_idx, _pos++);

    /***** LAYER 0 END ******/

    initLayer.consolidate();
}

void Planner::createNextLayer() {

    _layers.push_back(new Layer(_layers.size(), _layers.back()->getNextLayerSize()));
    Layer& newLayer = *_layers.back();
    Log::i("New layer size: %i\n", newLayer.size());
    Layer& oldLayer = (*_layers[_layer_idx]);
    _layer_idx++;
    _pos = 0;

    for (_old_pos = 0; _old_pos < oldLayer.size(); _old_pos++) {
        size_t newPos = oldLayer.getSuccessorPos(_old_pos);
        size_t maxOffset = oldLayer[_old_pos].getMaxExpansionSize();

        for (size_t offset = 0; offset < maxOffset; offset++) {
            assert(_pos == newPos + offset);
            Log::v(" Position (%i,%i)\n", _layer_idx, _pos);
            Log::d("  Instantiating ...\n");

            //log("%i,%i,%i,%i\n", oldPos, newPos, offset, newLayer.size());
            assert(newPos+offset < newLayer.size());

            createNextPosition();
            Log::d("  Instantiation done. (r=%i a=%i qf=%i supp=%i)\n", 
                    (*_layers[_layer_idx])[_pos].getReductions().size(),
                    (*_layers[_layer_idx])[_pos].getActions().size(),
                    (*_layers[_layer_idx])[_pos].getQFacts().size(),
                    (*_layers[_layer_idx])[_pos].getPosFactSupports().size() + (*_layers[_layer_idx])[_pos].getNegFactSupports().size()
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
        Position& left = (*_layers[_layer_idx])[_pos-1];
        createNextPositionFromLeft(left);
    }

    // Generate this new position's content based on the facts and the position above.
    if (_layer_idx > 0) {
        Position& above = (*_layers[_layer_idx-1])[_old_pos];
        createNextPositionFromAbove(above);
    }

    // In preparation for the upcoming position,
    // add all effects of the actions and reductions occurring HERE
    // as (initially false) facts to THIS position.  
    addNewFalseFacts();

    if (_params.isNonzero("qcm")) {

        // Use new q-constant conditions from this position to infer conditions 
        // of the respective parent ops at the layer above. 
        //auto updatedOps = _htn.getQConstantDatabase().backpropagateConditions(_layer_idx, _pos, (*_layers[_layer_idx])[_pos].getActions());
        //auto updatedReductions = _htn.getQConstantDatabase().backpropagateConditions(_layer_idx, _pos, (*_layers[_layer_idx])[_pos].getReductions());
        //updatedOps.insert(updatedReductions.begin(), updatedReductions.end());

        //pruneRetroactively(updatedOps);

        // Remove all q fact decodings which have become invalid
        for (const auto& qfactSig : (*_layers[_layer_idx])[_pos].getQFacts()) {

            std::vector<int> qargs, qargIndices;
            for (size_t i = 0; i < qfactSig._args.size(); i++) {
                const int& arg = qfactSig._args[i];
                if (_htn.isQConstant(arg)) {
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
                _htn.addForbiddenSubstitution(qargs, decargs);
            } 
        }
    }
}

void Planner::createNextPositionFromAbove(const Position& above) {
    Position& newPos = (*_layers[_layer_idx])[_pos];
    newPos.setPos(_layer_idx, _pos);

    int offset = _pos - (*_layers[_layer_idx-1]).getSuccessorPos(_old_pos);
    if (offset == 0) {
        // Propagate facts
        for (const auto& fact : above.getQFacts()) {
            newPos.addQFact(fact);
        }
    }

    propagateActions(offset);
    propagateReductions(offset);
}

void Planner::createNextPositionFromLeft(Position& left) {
    Position& newPos = (*_layers[_layer_idx])[_pos];
    newPos.setPos(_layer_idx, _pos);
    assert(left.getLayerIndex() == _layer_idx);
    assert(left.getPositionIndex() == _pos-1);

    // Propagate state
    //newPos.extendState(left.getState());

    FlatHashSet<int> relevantQConstants;

    // Propagate fact changes from operations from previous position
    for (const auto& aSig : left.getActions()) {
        for (const Signature& fact : left.getFactChanges(aSig)) {
            if (!addEffect(aSig, fact)) {
                // Impossible direct effect: forbid action retroactively.
                _enc.addUnitConstraint(-1*left.getVariable(VarType::OP, aSig));
            }
        }
        for (const int& arg : aSig._args) {
            if (_htn.isQConstant(arg)) relevantQConstants.insert(arg);
        }
    }
    for (const auto& rSig : left.getReductions()) {
        if (rSig == Position::NONE_SIG) continue;
        for (const Signature& fact : left.getFactChanges(rSig)) {
            if (!addEffect(rSig, fact)) {
                // Impossible indirect effect: ignore.
            }
        }
        for (const int& arg : rSig._args) {
            if (_htn.isQConstant(arg)) relevantQConstants.insert(arg);
        }
    }

    // Propagate occurring facts
    for (const auto& fact : left.getQFacts()) {
        bool add = true;
        for (const int& arg : fact._args) {
            if (_htn.isQConstant(arg) && !relevantQConstants.count(arg)) add = false;
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

void Planner::addPrecondition(const USignature& op, const Signature& fact, 
        std::vector<NodeHashSet<Substitution, Substitution::Hasher>>& goodSubs, 
        NodeHashSet<Substitution, Substitution::Hasher>& badSubs) {

    Position& pos = (*_layers[_layer_idx])[_pos];
    const USignature& factAbs = fact.getUnsigned();

    bool isQFact = _htn.hasQConstants(factAbs);

    if (fact._negated && !isQFact) { // TODO
        // Negative precondition not contained in facts: initialize
        //log("NEG_PRE %s\n", TOSTR(fact));
        introduceNewFalseFact(pos, factAbs);
    }
    
    //log("pre %s of %s\n", TOSTR(fact), TOSTR(op));
    // Precondition must be valid (or a q fact)
    if (!isQFact) assert(getLayerState().contains(_pos, fact) 
            || Log::e("%s not contained in state!\n", TOSTR(fact)));

    if (!isQFact) return;
    pos.addQFact(factAbs);

    // For each fact decoded from the q-fact:
    std::vector<int> sorts = _htn.getOpSortsForCondition(factAbs, op);
    NodeHashSet<Substitution, Substitution::Hasher> goods;
    const auto& state = getStateEvaluator();
    for (const USignature& decFactAbs : _htn.decodeObjects(factAbs, false, sorts)) {
        
        if (!_instantiator.testWithNoVarsNoQConstants(decFactAbs, fact._negated, state)) {
            // Fact cannot be true here
            badSubs.emplace(factAbs._args, decFactAbs._args);
            continue;
        } else {
            goods.emplace(factAbs._args, decFactAbs._args);
        }

        if (fact._negated) {
            // Decoded fact did not occur before.
            introduceNewFalseFact(pos, decFactAbs);
        }

        _htn.addQFactDecoding(factAbs, decFactAbs);
    }
    goodSubs.push_back(std::move(goods));
}

void Planner::addSubstitutionConstraints(const USignature& op, 
            std::vector<NodeHashSet<Substitution, Substitution::Hasher>>& goodSubs, 
            NodeHashSet<Substitution, Substitution::Hasher>& badSubs) {
    
    Position& newPos = _layers[_layer_idx]->at(_pos);

    size_t goodSize = 0;
    for (const auto& subs : goodSubs) goodSize += subs.size();

    //if (badSubs.size() <= goodSize) {
        for (auto& s : badSubs) {
            //Log::d("(%i,%i) FORBIDDEN_SUBST NOR %s\n", _layer_idx, _pos, TOSTR(op));
            newPos.addForbiddenSubstitution(op, s);
        }
    //} else {
        // More bad subs than there are good ones:
        // Remember good ones instead (although encoding them can be more complex)
        for (auto& subs : goodSubs) {
            //Log::d("(%i,%i) VALID_SUBST OR %s\n", _layer_idx, _pos, TOSTR(op));
            newPos.addValidSubstitutions(op, subs);
        }
    //}
}

bool Planner::addEffect(const USignature& opSig, const Signature& fact) {
    Position& pos = (*_layers[_layer_idx])[_pos];
    assert(_pos > 0);
    Position& left = (*_layers[_layer_idx])[_pos-1];
    USignature factAbs = fact.getUnsigned();
    bool isQFact = _htn.hasQConstants(factAbs);

    if (isQFact) {
        // Get forbidden substitutions for this operation
        const auto* invalids = left.getForbiddenSubstitutions().count(opSig) ? 
                &left.getForbiddenSubstitutions().at(opSig) : nullptr;

        // Create the full set of valid decodings for this qfact
        std::vector<int> sorts = _htn.getOpSortsForCondition(factAbs, opSig);
        bool anyGood = false;
        for (const USignature& decFactAbs : _htn.decodeObjects(factAbs, true, sorts)) {

            // Check if this decoding is known to be invalid    
            Substitution s(factAbs._args, decFactAbs._args);
            if (invalids != nullptr && invalids->count(s)) continue;
            
            // Valid effect decoding
            _htn.addQFactDecoding(factAbs, decFactAbs);
            getLayerState().add(_pos, decFactAbs, fact._negated);
            pos.touchFactSupport(decFactAbs, fact._negated);
            anyGood = true;
        }
        // Not a single valid decoding of the effect? -> Invalid effect.
        if (!anyGood) return false;

        pos.addQFact(factAbs);
    }

    // Depending on whether fact supports are encoded for primitive ops only,
    // add the fact to the op's support accordingly
    if (_nonprimitive_support || _htn.isAction(opSig)) {
        pos.addFactSupport(fact, opSig);
    } else {
        // Remember that there is some (unspecified) support for this fact
        pos.touchFactSupport(fact);
    }
    
    getLayerState().add(_pos, fact);
    return true;
}

void Planner::propagateInitialState() {
    assert(_layer_idx > 0);
    assert(_pos == 0);

    Position& newPos = (*_layers[_layer_idx])[0];
    Position& above = (*_layers[_layer_idx-1])[0];

    // Propagate occurring facts
    for (const auto& fact : above.getQFacts()) {
        newPos.addQFact(fact);
    }
    // Propagate TRUE facts
    for (const USignature& fact : above.getTrueFacts())
        newPos.addTrueFact(fact);
    for (const USignature& fact : above.getFalseFacts())
        newPos.addFalseFact(fact);

    // Propagate state: initial position and all q-facts
    getLayerState(_layer_idx) = LayerState();
    const auto& oldState = getLayerState(_layer_idx-1);
    auto& newState = getLayerState(_layer_idx);
    for (bool neg : {true, false}) {
        for (const auto& entry : neg ? oldState.getNegFactOccurrences() : oldState.getPosFactOccurrences()) {
            const USignature& fact = entry.first;
            //log("  ~~~> %s\n", TOSTR(fact));
            const auto& range = entry.second;
            if (range.first == 0 || _htn.hasQConstants(fact)) {
                int newRangeFirst = (*_layers[_layer_idx-1]).getSuccessorPos(range.first);
                newState.add(newRangeFirst, fact, neg);
                if (range.second != INT32_MAX) {
                    int newRangeSecond = (*_layers[_layer_idx-1]).getSuccessorPos(range.second);    
                    newState.withdraw(newRangeSecond, fact, neg);
                }
            }
        }
    }
    Log::d("%i neg, %i pos ~~~> %i neg, %i pos\n", oldState.getNegFactOccurrences().size(), oldState.getPosFactOccurrences().size(), 
                                                newState.getNegFactOccurrences().size(), newState.getPosFactOccurrences().size());
    
}

void Planner::propagateActions(size_t offset) {
    Position& newPos = (*_layers[_layer_idx])[_pos];
    Position& above = (*_layers[_layer_idx-1])[_old_pos];

    // Propagate actions
    for (const auto& aSig : above.getActions()) {
        if (aSig == Position::NONE_SIG) continue;
        const Action& a = _htn.getAction(aSig);

        // Can the action occur here w.r.t. the current state?
        bool valid = _instantiator.hasValidPreconditions(a.getPreconditions(), getStateEvaluator());

        // If not: forbid the action, i.e., its parent action
        if (!valid) {
            Log::i("Forbidding action %s@(%i,%i): no children at offset %i\n", TOSTR(aSig), _layer_idx-1, _old_pos, offset);
            newPos.addExpansion(aSig, Position::NONE_SIG);
            continue;
        }

        if (offset < 1) {
            // proper action propagation
            assert(_instantiator.isFullyGround(aSig));
            newPos.addAction(aSig);
            newPos.addExpansion(aSig, aSig);
            above.moveFactChanges(newPos, aSig);
            // Add preconditions of action
            NodeHashSet<Substitution, Substitution::Hasher> badSubs;
            std::vector<NodeHashSet<Substitution, Substitution::Hasher>> goodSubs;
            for (const Signature& fact : a.getPreconditions()) {
                addPrecondition(aSig, fact, goodSubs, badSubs);
            }
            // Not necessary – were already added for above action!
            //addSubstitutionConstraints(aSig, goodSubs, badSubs);
        } else {
            // action expands to "blank" at non-zero offsets
            const USignature& blankSig = _htn.getBlankActionSig();
            newPos.addAction(blankSig);
            newPos.addExpansion(aSig, blankSig);
        }
    }
}

void Planner::propagateReductions(size_t offset) {
    Position& newPos = (*_layers[_layer_idx])[_pos];
    Position& above = (*_layers[_layer_idx-1])[_old_pos];

    NodeHashMap<USignature, USigSet, USignatureHasher> subtaskToParents;
    NodeHashSet<USignature, USignatureHasher> reductionsWithChildren;

    // Collect all possible subtasks and remember their possible parents
    for (const auto& rSig : above.getReductions()) {
        if (rSig == Position::NONE_SIG) continue;
        const Reduction r = _htn.getReduction(rSig);
        
        if (offset < r.getSubtasks().size()) {
            // Proper expansion
            const USignature& subtask = r.getSubtasks()[offset];
            subtaskToParents[subtask].insert(rSig);
        } else {
            // Blank
            reductionsWithChildren.insert(rSig);
            const USignature& blankSig = _htn.getBlankActionSig();
            newPos.addAction(blankSig);
            newPos.addExpansion(rSig, blankSig);
        }
    }

    // Iterate over all possible subtasks
    for (const auto& [subtask, parents] : subtaskToParents) {

        // Calculate all possible actions fitting the subtask.
        auto allActions = getAllActionsOfTask(subtask, getStateEvaluator());

        // Any reduction(s) fitting the subtask?
        for (const USignature& subRSig : getAllReductionsOfTask(subtask, getStateEvaluator())) {
            
            if (_htn.isAction(subRSig)) {
                // Actually an action, not a reduction: remember for later
                allActions.push_back(subRSig);
                continue;
            }

            assert(_htn.isReduction(subRSig));
            const Reduction& subR = _htn.getReduction(subRSig);

            assert(subRSig == subR.getSignature());
            assert(_instantiator.isFullyGround(subRSig));
            
            newPos.addReduction(subRSig);
            /*for (const auto& pre : subR.getPreconditions()) {
                Log::d(" -- pre %s\n", TOSTR(pre));
            }*/
            //if (_layer_idx <= 1) log("ADD %s:%s @ (%i,%i)\n", TOSTR(subR.getTaskSignature()), TOSTR(subRSig), _layer_idx, _pos);
            newPos.addExpansionSize(subR.getSubtasks().size());
            // Add preconditions of reduction
            //log("PRECONDS %s ", TOSTR(subRSig));
            NodeHashSet<Substitution, Substitution::Hasher> badSubs;
            std::vector<NodeHashSet<Substitution, Substitution::Hasher>> goodSubs;
            for (const Signature& fact : subR.getPreconditions()) {
                addPrecondition(subRSig, fact, goodSubs, badSubs);
                //log("%s ", TOSTR(fact));
            }
            addSubstitutionConstraints(subRSig, goodSubs, badSubs);
            addQConstantTypeConstraints(subRSig);

            for (const auto& rSig : parents) {
                reductionsWithChildren.insert(rSig);
                newPos.addExpansion(rSig, subRSig);
                //const PositionedUSig parentPSig(_layer_idx-1, _old_pos, rSig);
                //_htn.addQConstantConditions(subR, PositionedUSig(_layer_idx, _pos, subRSig), 
                //                        parentPSig, offset, getStateEvaluator());
            }
            //log("\n");
        }

        // Any action(s) fitting the subtask?
        for (const USignature& aSig : allActions) {
            assert(_instantiator.isFullyGround(aSig));
            newPos.addAction(aSig);
            // Add preconditions of action
            const Action& a = _htn.getAction(aSig);
            NodeHashSet<Substitution, Substitution::Hasher> badSubs;
            std::vector<NodeHashSet<Substitution, Substitution::Hasher>> goodSubs;
            for (const Signature& fact : a.getPreconditions()) {
                addPrecondition(aSig, fact, goodSubs, badSubs);
            }
            addSubstitutionConstraints(aSig, goodSubs, badSubs);
            addQConstantTypeConstraints(aSig);

            for (const auto& rSig : parents) {
                reductionsWithChildren.insert(rSig);
                newPos.addExpansion(rSig, aSig);
                //const PositionedUSig parentPSig(_layer_idx-1, _old_pos, rSig);
                //_htn.addQConstantConditions(a, PositionedUSig(_layer_idx, _pos, aSig), 
                //                        parentPSig, offset, getStateEvaluator());
            }
        }
    }

    // Check if any reduction has no valid children at all
    for (const auto& rSig : above.getReductions()) {
        if (!reductionsWithChildren.count(rSig)) {
            // Explicitly forbid the parent!
            Log::i("Forbidding reduction %s@(%i,%i): no children at offset %i\n", 
                    TOSTR(rSig), _layer_idx-1, _old_pos, offset);
            newPos.addExpansion(rSig, Position::NONE_SIG);
        }
    }
}

std::vector<USignature> Planner::getAllActionsOfTask(const USignature& task, const StateEvaluator& state) {
    std::vector<USignature> result;

    if (!_htn.isAction(task)) return result;

    const Action& a = _htn.toAction(task._name_id, task._args);
    
    std::vector<Action> actions = _instantiator.getApplicableInstantiations(a, state);
    for (Action& action : actions) {
        //Log::d("ADDACTION %s ?\n", TOSTR(action.getSignature()));
        if (addAction(action)) result.push_back(action.getSignature());
    }
    return result;
}

std::vector<USignature> Planner::getAllReductionsOfTask(const USignature& task, const StateEvaluator& state) {
    std::vector<USignature> result;

    if (!_htn.hasReductions(task._name_id)) return result;

    // Filter and minimally instantiate methods
    // applicable in current (super)state
    for (int redId : _htn.getReductionIdsOfTaskId(task._name_id)) {
        Reduction r = _htn.getReductionTemplate(redId);

        if (_htn.hasSurrogate(redId)) {
            const Action& a = _htn.getSurrogate(redId);

            std::vector<Substitution> subs = Substitution::getAll(r.getTaskArguments(), task._args);
            for (const Substitution& s : subs) {
                USignature surrSig = a.getSignature().substitute(s);
                //Log::d("SURROGATE %s \n     -> %s\n", TOSTR(task), TOSTR(surrSig));

                for (const auto& sig : getAllActionsOfTask(surrSig, state)) {
                    //Log::d("          => %s\n", TOSTR(sig));
                    result.push_back(sig);
                }
            }
            continue;
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

bool Planner::addAction(Action& action) {

    USignature sig = action.getSignature();

    //log("ADDACTION %s\n", TOSTR(sig));

    // Rename any remaining variables in each action as unique q-constants,
    action = _htn.replaceVariablesWithQConstants(action, _layer_idx, _pos, getStateEvaluator());

    //Log::d("ADDACTION %s\n", TOSTR(action.getSignature()));

    // Remove any inconsistent effects that were just created
    action.removeInconsistentEffects();

    // Check validity
    if (!_instantiator.isFullyGround(action.getSignature())) return false;
    if (!_instantiator.hasConsistentlyTypedArgs(sig)) return false;
    if (!_instantiator.hasValidPreconditions(action.getPreconditions(), getStateEvaluator())) return false;
    
    sig = action.getSignature();
    _htn.addAction(action);

    // Compute fact changes
    (*_layers[_layer_idx])[_pos].setFactChanges(sig, _instantiator.getPossibleFactChanges(sig));
    
    //Log::d("ADDACTION -- added\n");
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
    
    sig = red.getSignature();
    _htn.addReduction(red);

    // Compute fact changes
    (*_layers[_layer_idx])[_pos].setFactChanges(sig, _instantiator.getPossibleFactChanges(sig));
    
    return true;
}

void Planner::addNewFalseFacts() {
    Position& newPos = (*_layers[_layer_idx])[_pos];
    
    // For each possible operation effect:
    const USigSet* ops[2] = {&newPos.getActions(), &newPos.getReductions()};
    for (const auto& set : ops) for (const auto& aSig : *set) { 
        if (aSig == Position::NONE_SIG) continue;
        for (const Signature& eff : newPos.getFactChanges(aSig)) {

            if (!_htn.hasQConstants(eff._usig)) { // TODO
                // New fact: set to false before the action may happen
                introduceNewFalseFact(newPos, eff._usig);
            } else {
                std::vector<int> sorts = _htn.getOpSortsForCondition(eff._usig, aSig);
                for (const USignature& decEff : _htn.decodeObjects(eff._usig, true, sorts)) {                    
                    // New fact: set to false before the action may happen
                    introduceNewFalseFact(newPos, decEff);
                }
            }
        }
    }

    // For each fact from "above" the next position:
    if (_layer_idx == 0) return;
    if (_old_pos+1 < (*_layers[_layer_idx-1]).size() && (*_layers[_layer_idx-1]).getSuccessorPos(_old_pos+1) == _pos+1) {
        Position& newAbove = (*_layers[_layer_idx-1])[_old_pos+1];

        // TODO Propagation of facts from above to new layer ~> add as new false facts?

        for (const auto& fact : newAbove.getQFacts()) {
            // If fact was not seen here before
            newPos.addQFact(fact);
        }
    }
}

void Planner::introduceNewFalseFact(Position& newPos, const USignature& fact) {
    assert(!_htn.hasQConstants(fact));
    
    // Already a definitive fact? => Do not re-add false fact
    if ((*_layers[_layer_idx])[_pos].getTrueFacts().count(fact)) return;
    if ((*_layers[_layer_idx])[_pos].getFalseFacts().count(fact)) return;

    getLayerState(newPos.getLayerIndex()).add(newPos.getPositionIndex(), fact, /*negated=*/true);
    
    // Does position to the left already have the encoded fact? -> not new!
    if (_pos > 0 && (*_layers[_layer_idx])[_pos-1].hasVariable(VarType::FACT, fact)) return;
    
    newPos.addFalseFact(fact);
}

void Planner::addQConstantTypeConstraints(const USignature& op) {
    // Add type constraints for q constants
    std::vector<TypeConstraint> cs = _instantiator.getQConstantTypeConstraints(op);
    // Add to this position's data structure
    for (const TypeConstraint& c : cs) {
        (*_layers[_layer_idx])[_pos].addQConstantTypeConstraint(op, c);
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

            bool isReduction = _htn.isReduction(sig);
            if (isReduction) (*_layers[layerIdx])[pos].removeReductionOccurrence(sig);
            else (*_layers[layerIdx])[pos].removeActionOccurrence(sig);
        }
    }
}


LayerState& Planner::getLayerState(int layer) {
    if (layer == -1) layer = _layer_idx;
    return (*_layers[layer]).getState();
}

Planner::StateEvaluator Planner::getStateEvaluator(int layer, int pos) {
    if (layer == -1) layer = _layer_idx;
    if (pos == -1) pos = _pos;
    return [this,layer,pos](const USignature& sig, bool negated) {
        bool holds = getLayerState(layer).contains(pos, sig, negated);
        //log("STATEEVAL@(%i,%i) %s : %i\n", layer, pos, TOSTR(sig), holds);
        return holds;
    };
}
