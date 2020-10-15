
#include <iostream>
#include <functional>
#include <string>
#include <sstream>

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
    // Termination due to initial planning time limit (-T)
    if (planner->_time_at_first_plan == 0 &&
        planner->_init_plan_time_limit > 0 &&
        Timer::elapsedSeconds() > planner->_init_plan_time_limit) {
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
    } else if (_time_at_first_plan == 0 
            && _init_plan_time_limit > 0
            && Timer::elapsedSeconds() > _init_plan_time_limit) {
        Log::i("Time limit to find an initial plan exceeded.\n");
        exitSet = true;
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
    _enc.setTerminateCallback(this, terminateSatCall);
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

    // Compute extra layers after initial solution as desired
    int extraLayers = _params.getIntParam("el");
    int upperBound = _layers.back()->size()-1;
    if (extraLayers > 0) {

        // Extract initial plan (for anytime purposes)
        _plan = _enc.extractPlan();
        _has_plan = true;
        upperBound = _enc.getPlanLength(std::get<0>(_plan));
        Log::i("Initial plan at most shallow layer has length %i\n", upperBound);

        // Extra layers without solving
        for (int x = 0; x < extraLayers; x++) {
            iteration++;      
            Log::i("Iteration %i. (extra)\n", iteration);
            createNextLayer();
        }

        // Solve again (to get another plan)
        _enc.addAssumptions(_layer_idx);
        int result = _enc.solve();
        assert(result == 10);
    }

    if (_optimization_factor != 0) {
        // Extract plan at final layer, update bound
        auto finalLayerPlan = _enc.extractPlan();
        int newLength = _enc.getPlanLength(std::get<0>(finalLayerPlan));
        // Update plan only if it is better than any previous plan
        if (newLength < upperBound || !_has_plan) {
            upperBound = newLength;
            _plan = finalLayerPlan;
            _has_plan = true;
        }
        Log::i("Initial plan at final layer has length %i\n", newLength);
        // Optimize
        _enc.optimizePlan(upperBound, _plan);

    } else {
        // Just extract plan
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
        if (item.abstractTask._name_id == _htn.nameId("<goal_action>")) continue;

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
    _init_state = _htn.getInitState();
    for (const USignature& fact : _init_state) _pos_layer_facts.insert(fact);

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
            IntPairTree badSubs;
            std::vector<IntPairTree> goodSubs;
            for (const Signature& fact : r.getPreconditions()) {
                addPrecondition(sig, fact, goodSubs, badSubs);
            }
            addSubstitutionConstraints(sig, goodSubs, badSubs);
            addQConstantTypeConstraints(sig);
            const PositionedUSig psig{_layer_idx,_pos,sig};
        }
    }
    introduceNewFacts();
    //_htn.getQConstantDatabase().backpropagateConditions(_layer_idx, _pos, (*_layers[_layer_idx])[_pos].getReductions());
    _pos++;

    /***** LAYER 0, POSITION 1 ******/

    createNextPosition(); // position 1

    // Create virtual goal action
    Action goalAction = _htn.getGoalAction();
    USignature goalSig = goalAction.getSignature();
    initLayer[_pos].addAction(goalSig);
    initLayer[_pos].addAxiomaticOp(goalSig);
    
    // Extract primitive goals, add to preconds of goal action
    IntPairTree badSubs;
    std::vector<IntPairTree> goodSubs;
    for (const Signature& fact : goalAction.getPreconditions()) {
        assert(getCurrentState(fact._negated).contains(fact._usig));
        addPrecondition(goalSig, fact, goodSubs, badSubs);
    }
    assert(goodSubs.empty() && badSubs.empty());
    
    /***** LAYER 0 END ******/

    initLayer[0].clearAfterInstantiation();
    initLayer[1].clearAfterInstantiation();

    _pos = 0;
    _enc.encode(_layer_idx, _pos++);
    _enc.encode(_layer_idx, _pos++);
    initLayer.consolidate();
}

void Planner::createNextLayer() {

    _layers.push_back(new Layer(_layers.size(), _layers.back()->getNextLayerSize()));
    Layer& newLayer = *_layers.back();
    Log::i("New layer size: %i\n", newLayer.size());
    Layer& oldLayer = (*_layers[_layer_idx]);
    _layer_idx++;
    _pos = 0;

    // Instantiate new layer
    Log::i("Instantiating ...\n");
    for (_old_pos = 0; _old_pos < oldLayer.size(); _old_pos++) {
        size_t newPos = oldLayer.getSuccessorPos(_old_pos);
        size_t maxOffset = oldLayer[_old_pos].getMaxExpansionSize();

        // Instantiate each new position induced by the old position
        for (size_t offset = 0; offset < maxOffset; offset++) {
            //Log::d("%i,%i,%i,%i\n", _old_pos, newPos, offset, newLayer.size());
            assert(_pos == newPos + offset);
            Log::v("- Position (%i,%i)\n", _layer_idx, _pos);

            assert(newPos+offset < newLayer.size());

            createNextPosition();
            Log::v("  Instantiation done. (r=%i a=%i qf=%i supp=%i)\n", 
                    (*_layers[_layer_idx])[_pos].getReductions().size(),
                    (*_layers[_layer_idx])[_pos].getActions().size(),
                    (*_layers[_layer_idx])[_pos].getQFacts().size(),
                    (*_layers[_layer_idx])[_pos].getPosFactSupports().size() + (*_layers[_layer_idx])[_pos].getNegFactSupports().size()
            );
            if (_pos > 0) _layers[_layer_idx]->at(_pos-1).clearAfterInstantiation();

            _pos++;
            checkTermination();
        }
    }
    if (_pos > 0) _layers[_layer_idx]->at(_pos-1).clearAfterInstantiation();

    Log::i("Collected %i relevant facts at this layer\n", _htn.getRelevantFacts().size());

    // Encode new layer
    Log::i("Encoding ...\n");
    for (_pos = 0; _pos < newLayer.size(); _pos++) {
        Log::v("- Position (%i,%i)\n", _layer_idx, _pos);
        _enc.encode(_layer_idx, _pos);
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
        createNextPositionFromAbove();
    }

    eliminateDominatedOperations();

    // In preparation for the upcoming position,
    // add all effects of the actions and reductions occurring HERE
    // as (initially false) facts to THIS position.  
    introduceNewFacts();
}

void Planner::createNextPositionFromAbove() {
    Position& newPos = (*_layers[_layer_idx])[_pos];
    newPos.setPos(_layer_idx, _pos);
    int offset = _pos - (*_layers[_layer_idx-1]).getSuccessorPos(_old_pos);
    propagateActions(offset);
    propagateReductions(offset);
}

void Planner::createNextPositionFromLeft(Position& left) {
    Position& newPos = (*_layers[_layer_idx])[_pos];
    newPos.setPos(_layer_idx, _pos);
    assert(left.getLayerIndex() == _layer_idx);
    assert(left.getPositionIndex() == _pos-1);

    // Propagate fact changes from operations from previous position
    USigSet actionsToRemove;
    const USigSet* ops[2] = {&left.getActions(), &left.getReductions()};
    bool isAction = true;
    for (const auto& set : ops) {
        for (const auto& aSig : *set) {

            bool repeatedAction = isAction && _htn.isVirtualizedChildOfAction(aSig._name_id);
            for (const Signature& fact : isAction ? _htn.getAction(aSig).getEffects() 
                                                  : _fact_changes_cache[aSig]) {
                if (isAction && !addEffect(
                        repeatedAction ? aSig.renamed(_htn.getActionNameOfVirtualizedChild(aSig._name_id)) : aSig, 
                        fact, 
                        repeatedAction ? EffectMode::DIRECT_NO_QFACT : EffectMode::DIRECT)) {
                    // Impossible direct effect: forbid action retroactively.
                    Log::w("Retroactively prune action %s due to impossible effect %s\n", TOSTR(aSig), TOSTR(fact));
                    //_enc.addUnitConstraint(-1*left.getVariable(VarType::OP, aSig));
                    actionsToRemove.insert(aSig);
                    break;
                }
                if (!isAction && !addEffect(aSig, fact, EffectMode::INDIRECT)) {
                    // Impossible indirect effect: ignore.
                }
            }
            if (!isAction) _fact_changes_cache.erase(aSig);

            // Remove larger substitution constraint structure
            auto goodSubs = left.getValidSubstitutions().count(aSig) ? &left.getValidSubstitutions().at(aSig) : nullptr;
            auto badSubs = left.getForbiddenSubstitutions().count(aSig) ? &left.getForbiddenSubstitutions().at(aSig) : nullptr;
            size_t goodSize = 0;
            if (goodSubs != nullptr) for (const auto& subs : *goodSubs) goodSize += subs.getSizeOfEncoding();
            size_t badSize = badSubs != nullptr ? badSubs->getSizeOfNegationEncoding() : 0;
            if (goodSubs != nullptr && badSize > goodSize) {
                // More bad subs than good ones:
                // Remember good ones instead (although encoding them can be more complex)
                left.getForbiddenSubstitutions().erase(aSig);
                Log::d("SUBCONSTR : %i good (instead of %i bad)\n", goodSize, badSize);
            } else if (badSubs != nullptr) {
                left.getValidSubstitutions().erase(aSig);
                Log::d("SUBCONSTR : %i bad (instead of %i good)\n", badSize, goodSize);
            }
        }
        isAction = false;
    }

    for (const auto& aSig : actionsToRemove) {
        left.removeActionOccurrence(aSig);
    }
}

void Planner::addPrecondition(const USignature& op, const Signature& fact, 
        std::vector<IntPairTree>& goodSubs, IntPairTree& badSubs, bool addQFact) {

    Position& pos = (*_layers[_layer_idx])[_pos];
    const USignature& factAbs = fact.getUnsigned();
    const auto& state = getStateEvaluator();

    if (!_htn.hasQConstants(factAbs)) { 
        assert(_instantiator.testWithNoVarsNoQConstants(factAbs, fact._negated, state)
                || Log::e("%s not contained in state!\n", TOSTR(fact)));
                
        if (_instantiator.testWithNoVarsNoQConstants(factAbs, !fact._negated, state)) {
            // Negated prec. is not impossible: not statically resolvable
            introduceNewFact(pos, factAbs);
            _htn.addRelevantFact(factAbs);
        }
        return;
    }

    // For each fact decoded from the q-fact:
    std::vector<int> sorts = _htn.getOpSortsForCondition(factAbs, op);
    std::vector<int> sortedArgIndices = getSortedSubstitutedArgIndices(factAbs._args, sorts);
    IntPairTree goods;
    bool staticallyResolvable = true;
    USigSet relevants;
    for (const USignature& decFactAbs : _htn.decodeObjects(factAbs, sorts)) {

        // Can the decoded fact occur as is?
        if (!_instantiator.testWithNoVarsNoQConstants(decFactAbs, fact._negated, state)) {
            // Fact cannot be true here
            if (addQFact) badSubs.insert(decodingToPath(factAbs._args, decFactAbs._args, sortedArgIndices));
            continue;
        } else {
            if (addQFact) goods.insert(decodingToPath(factAbs._args, decFactAbs._args, sortedArgIndices));
        }

        // If yes, can the decoded fact also occured in its opposite form?
        if (!_instantiator.testWithNoVarsNoQConstants(decFactAbs, !fact._negated, state)) {
            // No! This precondition is trivially satisfied 
            // with above substitution restrictions
            continue;
        }

        staticallyResolvable = false;
        relevants.insert(decFactAbs);
    }

    if (!staticallyResolvable) {
        if (addQFact) pos.addQFact(factAbs);
        for (const USignature& decFactAbs : relevants) {
            // Decoded fact may be new - initialize as necessary
            introduceNewFact(pos, decFactAbs);
            pos.addQFactDecoding(factAbs, decFactAbs);
            _htn.addRelevantFact(decFactAbs);
        }
    } // else : encoding the precondition is not necessary!

    if (addQFact) goodSubs.push_back(std::move(goods));
}

std::vector<int> Planner::getSortedSubstitutedArgIndices(const std::vector<int>& qargs, const std::vector<int>& sorts) const {

    // Collect indices of arguments which will be substituted
    std::vector<int> argIndices;
    for (size_t i = 0; i < qargs.size(); i++) {
        if (_htn.isQConstant(qargs[i])) argIndices.push_back(i);
    }

    // Sort argument indices by the potential size of their domain
    std::sort(argIndices.begin(), argIndices.end(), 
            [&](int i, int j) {return _htn.getConstantsOfSort(sorts[i]).size() < _htn.getConstantsOfSort(sorts[j]).size();});
    return argIndices;
}

std::vector<IntPair> Planner::decodingToPath(const std::vector<int>& qargs, const std::vector<int>& decArgs, const std::vector<int>& sortedIndices) const {
    
    // Write argument substitutions into the result in correct order
    std::vector<IntPair> path;
    for (size_t x = 0; x < sortedIndices.size(); x++) {
        size_t argIdx = sortedIndices[x];
        path.emplace_back(qargs[argIdx], decArgs[argIdx]);
    }
    return path;
}

void Planner::addSubstitutionConstraints(const USignature& op, 
            std::vector<IntPairTree>& goodSubs, IntPairTree& badSubs) {
    
    Position& newPos = _layers[_layer_idx]->at(_pos);
    newPos.setValidSubstitutions(op, std::move(goodSubs));
    newPos.setForbiddenSubstitutions(op, std::move(badSubs));
}

bool Planner::addEffect(const USignature& opSig, const Signature& fact, EffectMode mode) {
    Position& pos = (*_layers[_layer_idx])[_pos];
    assert(_pos > 0);
    Position& left = (*_layers[_layer_idx])[_pos-1];
    USignature factAbs = fact.getUnsigned();
    bool isQFact = _htn.hasQConstants(factAbs);
    const auto& state = getStateEvaluator();

    if (!isQFact) {
        if (!_instantiator.testWithNoVarsNoQConstants(factAbs, !fact._negated, state)) {
            // Always holds -- no need to encode
            return true;
        }

        if (mode != INDIRECT) _htn.addRelevantFact(factAbs);

        // Depending on whether fact supports are encoded for primitive ops only,
        // add the ground fact to the op's support accordingly
        if (_nonprimitive_support || _htn.isAction(opSig)) {
            pos.addFactSupport(fact, opSig);
        } else {
            // Remember that there is some (unspecified) support for this fact
            pos.touchFactSupport(fact);
        }
        
        getCurrentState(fact._negated).insert(fact._usig);
        return true;
    }

    // Get forbidden substitutions for this operation
    const auto* invalids = left.getForbiddenSubstitutions().count(opSig) ? 
            &left.getForbiddenSubstitutions().at(opSig) : nullptr;

    // Create the full set of valid decodings for this qfact
    std::vector<int> sorts = _htn.getOpSortsForCondition(factAbs, opSig);
    std::vector<int> argIndices = getSortedSubstitutedArgIndices(factAbs._args, sorts);
    bool anyGood = false;
    bool staticallyResolvable = true;
    for (const USignature& decFactAbs : _htn.decodeObjects(factAbs, sorts)) {
        
        // Check if this decoding is known to be invalid
        auto path = decodingToPath(factAbs._args, decFactAbs._args, argIndices);
        if (invalids != nullptr && invalids->contains(path)) continue;

        if (!_instantiator.testWithNoVarsNoQConstants(decFactAbs, !fact._negated, state)) {
            // Negation of this ground effect is impossible here -> effect holds trivially
            anyGood = true;
            continue;
        }
        
        // Valid effect decoding
        getCurrentState(fact._negated).insert(decFactAbs);
        if (_nonprimitive_support || _htn.isAction(opSig)) {
            pos.addIndirectFactSupport(decFactAbs, fact._negated, opSig, std::move(path));
        } else {
            pos.touchFactSupport(decFactAbs, fact._negated);
        }
        if (mode != INDIRECT) {
            pos.addQFactDecoding(factAbs, decFactAbs);
            _htn.addRelevantFact(decFactAbs);
        }
        anyGood = true;
        staticallyResolvable = false;
    }
    // Not a single valid decoding of the effect? -> Invalid effect.
    if (!anyGood) return false;

    if (!staticallyResolvable && mode == DIRECT) pos.addQFact(factAbs);
    
    return true;
}

void Planner::propagateInitialState() {
    assert(_layer_idx > 0);
    assert(_pos == 0);

    Position& newPos = (*_layers[_layer_idx])[0];
    Position& above = (*_layers[_layer_idx-1])[0];

    _defined_facts.clear();
    
    // Propagate TRUE facts
    for (const USignature& fact : above.getTrueFacts()) {
        newPos.addTrueFact(fact);
        _defined_facts.insert(fact);
    }
    for (const USignature& fact : above.getFalseFacts()) {
        newPos.addFalseFact(fact);
        _defined_facts.insert(fact);
    }

    // Set up layer facts: initial state only
    _pos_layer_facts.clear();
    _neg_layer_facts.clear();
    for (const USignature& fact : _init_state) {
        _pos_layer_facts.insert(fact);
    }    
}

void Planner::propagateActions(size_t offset) {
    Position& newPos = (*_layers[_layer_idx])[_pos];
    Position& above = (*_layers[_layer_idx-1])[_old_pos];

    // Propagate actions
    for (const auto& aSig : above.getActions()) {
        if (aSig == Sig::NONE_SIG) continue;
        const Action& a = _htn.getAction(aSig);

        // Can the action occur here w.r.t. the current state?
        bool valid = _instantiator.hasValidPreconditions(a.getPreconditions(), getStateEvaluator())
                && _instantiator.hasValidPreconditions(a.getExtraPreconditions(), getStateEvaluator());

        // If not: forbid the action, i.e., its parent action
        if (!valid) {
            Log::i("Forbidding action %s@(%i,%i): no children at offset %i\n", TOSTR(aSig), _layer_idx-1, _old_pos, offset);
            newPos.addExpansion(aSig, Sig::NONE_SIG);
            continue;
        }

        if (offset < 1) {
            // proper action propagation
            assert(_instantiator.isFullyGround(aSig));
            if (_params.isNonzero("vca") && !_htn.isVirtualizedChildOfAction(aSig._name_id)) {
                // Virtualize child of action
                USignature vChildSig = _htn.getVirtualizedChildOfAction(aSig);
                Action a = _htn.getAction(vChildSig);
                newPos.addAction(vChildSig);
                newPos.addExpansion(aSig, vChildSig);
            } else {
                // Treat as a normal action
                newPos.addAction(aSig);
                newPos.addExpansion(aSig, aSig);
            }
            // Add preconditions of action
            IntPairTree badSubs;
            std::vector<IntPairTree> goodSubs;
            for (const Signature& fact : a.getPreconditions()) {
                addPrecondition(aSig, fact, goodSubs, badSubs, 
                    /*addQFact=*/!_htn.isVirtualizedChildOfAction(aSig._name_id));
            }
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
        if (rSig == Sig::NONE_SIG) continue;
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
            IntPairTree badSubs;
            std::vector<IntPairTree> goodSubs;
            for (const Signature& fact : subR.getPreconditions()) {
                addPrecondition(subRSig, fact, goodSubs, badSubs);
                //log("%s ", TOSTR(fact));
            }
            addSubstitutionConstraints(subRSig, goodSubs, badSubs);
            addQConstantTypeConstraints(subRSig);

            for (const auto& rSig : parents) {
                reductionsWithChildren.insert(rSig);
                newPos.addExpansion(rSig, subRSig);
            }
            //log("\n");
        }

        // Any action(s) fitting the subtask?
        for (const USignature& aSig : allActions) {
            assert(_instantiator.isFullyGround(aSig));
            newPos.addAction(aSig);
            // Add preconditions of action
            const Action& a = _htn.getAction(aSig);
            IntPairTree badSubs;
            std::vector<IntPairTree> goodSubs;
            for (const Signature& fact : a.getPreconditions()) {
                addPrecondition(aSig, fact, goodSubs, badSubs);
            }
            addSubstitutionConstraints(aSig, goodSubs, badSubs);
            addQConstantTypeConstraints(aSig);

            for (const auto& rSig : parents) {
                reductionsWithChildren.insert(rSig);
                newPos.addExpansion(rSig, aSig);
            }
        }
    }

    // Check if any reduction has no valid children at all
    for (const auto& rSig : above.getReductions()) {
        if (!reductionsWithChildren.count(rSig)) {
            // Explicitly forbid the parent!
            Log::i("Forbidding reduction %s@(%i,%i): no children at offset %i\n", 
                    TOSTR(rSig), _layer_idx-1, _old_pos, offset);
            newPos.addExpansion(rSig, Sig::NONE_SIG);
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
    if (!_instantiator.hasValidPreconditions(action.getExtraPreconditions(), getStateEvaluator())) return false;
    
    sig = action.getSignature();
    _htn.addAction(action);
    
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
    if (!_instantiator.hasValidPreconditions(red.getExtraPreconditions(), getStateEvaluator())) return false;

    sig = red.getSignature();
    _htn.addReduction(red);
    
    return true;
}

void Planner::introduceNewFacts() {
    Position& newPos = (*_layers[_layer_idx])[_pos];
    
    // For each possible operation effect:
    const USigSet* ops[2] = {&newPos.getActions(), &newPos.getReductions()};
    bool isAction = true;
    for (const auto& set : ops) {
        for (const auto& aSig : *set) {
            if (aSig == Sig::NONE_SIG) continue;
            if (!isAction) {
                _fact_changes_cache[aSig] = _instantiator.getPossibleFactChanges(aSig);
            }
            for (const Signature& eff : isAction ? _htn.getAction(aSig).getEffects() 
                                                 : _fact_changes_cache[aSig]) {

                if (!_htn.hasQConstants(eff._usig)) {
                    // New fact: set before the action may happen
                    introduceNewFact(newPos, eff._usig); 
                } else {
                    std::vector<int> sorts = _htn.getOpSortsForCondition(eff._usig, aSig);
                    for (const USignature& decEff : _htn.decodeObjects(eff._usig, sorts)) {                    
                        // New fact: set before the action may happen
                        introduceNewFact(newPos, decEff);
                    }
                }
            }
        }
        isAction = false;
    }
}

void Planner::introduceNewFact(Position& newPos, const USignature& fact) {
    assert(!_htn.hasQConstants(fact));

    // New fact is to be introduce as initially FALSE
    // if its positive form is not contained in the initial state
    bool initiallyFalse = !_init_state.contains(fact);
    if (initiallyFalse) _neg_layer_facts.insert(fact);
    
    // Has the fact already been defined? -> Not new!
    if (_defined_facts.count(fact)) return;

    _defined_facts.insert(fact);
    if (initiallyFalse) newPos.addFalseFact(fact);
    else newPos.addTrueFact(fact);
}

void Planner::addQConstantTypeConstraints(const USignature& op) {
    // Add type constraints for q constants
    std::vector<TypeConstraint> cs = _instantiator.getQConstantTypeConstraints(op);
    // Add to this position's data structure
    for (const TypeConstraint& c : cs) {
        (*_layers[_layer_idx])[_pos].addQConstantTypeConstraint(op, c);
    }
}

Planner::DominationStatus Planner::getDominationStatus(const USignature& op, const USignature& other, 
        Substitution& qconstSubstitutions) {
    
    if (op._name_id != other._name_id) return DIFFERENT;
    assert(op._args.size() == other._args.size());
    
    DominationStatus status = EQUIVALENT;
    FlatHashSet<int> dummyDomain;

    for (size_t argIdx = 0; argIdx < op._args.size(); argIdx++) {
        int arg = op._args[argIdx];
        int otherArg = other._args[argIdx];

        // TODO Every q-constant must have a globally invariant domain for this to work.
        if (arg == otherArg) continue; 
        
        bool isQ = _htn.isQConstant(arg);
        bool isOtherQ = _htn.isQConstant(otherArg);
        if (!isQ && !isOtherQ) return DIFFERENT; // Different ground constants

        // Check whether both q-constants originate from the same position
        IntPair position(_layer_idx, _pos);
        if (isQ && isOtherQ && _htn.getOriginOfQConstant(arg) != _htn.getOriginOfQConstant(otherArg)) {
            return DIFFERENT;
        }
        
        // Compare domains of pseudo-constants
        
        const auto& domain = isQ ? _htn.getDomainOfQConstant(arg) : dummyDomain;
        if (!isQ) dummyDomain.insert(arg);
        const auto& otherDomain = isOtherQ ? _htn.getDomainOfQConstant(otherArg) : dummyDomain;
        if (!isOtherQ) dummyDomain.insert(otherArg);
        assert(dummyDomain.size() <= 1);

        if (domain.size() > otherDomain.size()) {
            // This op may dominate the other op

            // Contradicts previous argument indices -> ops are different
            if (status == DOMINATED) return DIFFERENT;
            // Check if this domain actually contains the other domain
            for (int c : otherDomain) if (!domain.count(c)) return DIFFERENT;
            // Yes: Dominating w.r.t. this position
            status = DOMINATING;
            qconstSubstitutions[otherArg] = arg;

        } else if (domain.size() < otherDomain.size()) {
            // This op may be dominated by the other op

            // Contradicts previous argument indices -> ops are different
            if (status == DOMINATED) return DIFFERENT;
            // Check if the other domain actually contains this domain
            for (int c : domain) if (!otherDomain.count(c)) return DIFFERENT;
            // Yes: Dominated w.r.t. this position
            status = DOMINATED;
            qconstSubstitutions[arg] = otherArg;

        } else if (domain != otherDomain) {
            // Different domains
            return DIFFERENT;
        }
        // Domains are equal

        if (!isQ || !isOtherQ) dummyDomain.clear();
    }

    if (status == EQUIVALENT) {
        // The operations are equal. Both dominate another.
        // Tie break using lexicographic ordering of arguments
        for (size_t argIdx = 0; argIdx < op._args.size(); argIdx++) {
            if (op._args[argIdx] != other._args[argIdx]) {
                status = op._args[argIdx] < other._args[argIdx] ? DOMINATED : DOMINATING;
                break;
            }    
        }
        for (size_t argIdx = 0; argIdx < op._args.size(); argIdx++) {
            if (_htn.isQConstant(status == DOMINATED ? other._args[argIdx] : op._args[argIdx])) {
                if (status == DOMINATED) qconstSubstitutions[op._args[argIdx]] = other._args[argIdx];
                else qconstSubstitutions[other._args[argIdx]] = op._args[argIdx];
            } 
        }
        return status;
    }
    return status;
}

void Planner::eliminateDominatedOperations() {
    Position& newPos = _layers.at(_layer_idx)->at(_pos);

    // Map of an op name id to (map of a dominating op to a set of dominated ops)
    NodeHashMap<int, NodeHashMap<USignature, USigSubstitutionMap, USignatureHasher>> dominatingActionsByName;
    NodeHashMap<int, NodeHashMap<USignature, USigSubstitutionMap, USignatureHasher>> dominatingReductionsByName;

    // For each operation
    const USigSet* ops[2] = {&newPos.getActions(), &newPos.getReductions()};
    NodeHashMap<int, NodeHashMap<USignature, USigSubstitutionMap, USignatureHasher>>* dMaps[2] = {
        &dominatingActionsByName, &dominatingReductionsByName
    };
    for (size_t i = 0; i < 2; i++) {

        for (const auto& op : *ops[i]) {
            auto& dominatingOps = (*dMaps[i])[op._name_id];

            // Compare operation with each currently dominating op of the same name
            USigSubstitutionMap dominated;
            
            if (dominatingOps.empty()) {
                dominatingOps[op];
                continue;
            }

            for (auto& [other, dominatedByOther] : dominatingOps) {
                Substitution s;
                auto status = getDominationStatus(op, other, s);
                if (status == DOMINATED) {
                    // This op is being dominated; mark for deletion
                    //Log::d("DOM %s << %s\n", TOSTR(op), TOSTR(other));
                    dominatedByOther[op] = s;
                    dominated.clear();
                    break;
                }
                if (status == DOMINATING) {
                    // This op dominates the other op
                    //Log::d("DOM %s >> %s\n", TOSTR(op), TOSTR(other));
                    dominated[other] = s;
                }
            }
            // Delete all ops transitively dominated by this op
            assert(!dominatingOps.count(op));
            for (const auto& [other, s] : dominated) {
                std::vector<USignature> dominatedVec(1, other);
                std::vector<Substitution> subVec(1, s);
                for (size_t j = 0; j < dominatedVec.size(); j++) {
                    auto dominatedOp = dominatedVec[j];
                    auto domS = subVec[j];
                    //Log::d("DOM %s, j=%i : %s\n", TOSTR(op), j, TOSTR(dominatedOp));
                    //Log::d("DOM sub: %s\n", TOSTR(domS));
                    if (!dominatingOps[op].count(dominatedOp) || domS.size() < dominatingOps[op][dominatedOp].size()) 
                        dominatingOps[op][dominatedOp] = domS;
                    //assert(dominatedOp.substitute(domS) == op || Log::d("%s -> %s != %s\n", TOSTR(dominatedOp), TOSTR(dominatedOp.substitute(domS)), TOSTR(op)));
                    if (dominatingOps.count(dominatedOp)) {
                        for (const auto& [domDomOp, domDomS] : dominatingOps[dominatedOp]) {
                            dominatedVec.push_back(domDomOp);
                            Substitution cat = domDomS.concatenate(domS);
                            //Log::d("DOM concat (%s , %s) ~> %s\n", TOSTR(domDomS), TOSTR(domS), TOSTR(cat));
                            subVec.push_back(cat); 
                        }
                        dominatingOps.erase(dominatedOp);
                    }
                }
            }
        }

        // Remove all dominated ops
        for (auto& [nameId, dMap] : *dMaps[i]) for (auto& [op, dominated] : dMap) {
            for (auto& [other, s] : dominated) {
                //Log::v("%s dominates %s (%s)\n", TOSTR(op), TOSTR(other), TOSTR(s));
                //assert(other.substitute(s) == op);

                auto predecessors = newPos.getPredecessors().at(other);
                for (const auto& parent : predecessors) {
                    newPos.addExpansion(parent, op);
                    newPos.addExpansionSubstitution(parent, op, std::move(s));
                }
                if (i == 0) {
                    newPos.removeActionOccurrence(other);
                } else {
                    newPos.removeReductionOccurrence(other);
                }
            }
        }
    }
    
}

USigSet& Planner::getCurrentState(bool negated) {
    return negated ? _neg_layer_facts : _pos_layer_facts;
}

Planner::StateEvaluator Planner::getStateEvaluator(int layer, int pos) {
    if (layer == -1) layer = _layer_idx;
    if (pos == -1) pos = _pos;
    return [this](const USignature& sig, bool negated) {
        return getCurrentState(negated).contains(sig);
    };
}
