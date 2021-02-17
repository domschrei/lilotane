
#include <assert.h> 

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
            _plan_writer.outputPlan(_plan);
        } else {
            Log::i("Termination signal caught.\n");
        }
    } else if (cancelOpt) {
        Log::i("Cancelling optimization according to provided limit.\n");
        _plan_writer.outputPlan(_plan);
    } else if (_time_at_first_plan == 0 
            && _init_plan_time_limit > 0
            && Timer::elapsedSeconds() > _init_plan_time_limit) {
        Log::i("Time limit to find an initial plan exceeded.\n");
        exitSet = true;
    }
    if (exitSet || cancelOpt) {
        printStatistics();
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
    if (extraLayers != 0) {

        // Extract initial plan (for anytime purposes)
        _plan = _enc.extractPlan();
        _has_plan = true;
        upperBound = _enc.getPlanLength(std::get<0>(_plan));
        Log::i("Initial plan at most shallow layer has length %i\n", upperBound);
        
        if (extraLayers == -1) {

            // Indefinitely increase bound and solve until program is interrupted or max depth reached
            size_t el = 1;
            do {
                // Extra layers without solving
                for (size_t x = 0; x < el && (maxIterations == 0 || iteration < maxIterations); x++) {
                    iteration++;      
                    Log::i("Iteration %i. (extra)\n", iteration);
                    createNextLayer();
                }
                // Solve again (to get another plan)
                _enc.addAssumptions(_layer_idx);
                int result = _enc.solve();
                if (result != 10) break;
                // Extract plan at layer, update bound
                auto thisLayerPlan = _enc.extractPlan();
                int newLength = _enc.getPlanLength(std::get<0>(thisLayerPlan));
                // Update plan only if it is better than any previous plan
                if (newLength < upperBound || !_has_plan) {
                    upperBound = newLength;
                    _plan = thisLayerPlan;
                    _has_plan = true;
                }
                Log::i("Initial plan at layer %i has length %i\n", iteration, newLength);
                // Optimize
                _enc.optimizePlan(upperBound, _plan, Encoding::ConstraintAddition::TRANSIENT);
                // Double number of extra layers in next iteration
                el *= 2;
            } while (maxIterations == 0 || iteration < maxIterations);

        } else {
            // Extra layers without solving
            for (int x = 0; x < extraLayers; x++) {
                iteration++;      
                Log::i("Iteration %i. (extra)\n", iteration);
                createNextLayer();
            }

            // Solve again (to get another plan)
            _enc.addAssumptions(_layer_idx);
            _enc.solve();
        }
    }

    if (extraLayers != -1) {
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
            _enc.optimizePlan(upperBound, _plan, Encoding::ConstraintAddition::PERMANENT);

        } else {
            // Just extract plan
            _plan = _enc.extractPlan();
            _has_plan = true;
        }
    }

    _plan_writer.outputPlan(_plan);
    printStatistics();
    
    return 0;
}

void Planner::incrementPosition() {
    _num_instantiated_actions += _layers[_layer_idx]->at(_pos).getActions().size();
    _num_instantiated_reductions += _layers[_layer_idx]->at(_pos).getReductions().size();
    _pos++; _num_instantiated_positions++;
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

    // Instantiate all possible init. reductions
    for (Reduction& r : _instantiator.getApplicableInstantiations(_htn.getInitReduction())) {
        if (addReduction(r, USignature())) {
            USignature sig = r.getSignature();
            initLayer[_pos].addReduction(sig);
            initLayer[_pos].addAxiomaticOp(sig);
            initLayer[_pos].addExpansionSize(r.getSubtasks().size());
        }
    }
    addPreconditionConstraints();
    initializeNextEffects();
    
    incrementPosition();

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
        assert(_analysis.isReachable(fact));
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

            incrementPosition();
            checkTermination();
        }
    }
    if (_pos > 0) _layers[_layer_idx]->at(_pos-1).clearAfterInstantiation();

    Log::i("Collected %i relevant facts at this layer\n", _analysis.getRelevantFacts().size());

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

    // Eliminate operations which are dominated by another operation
    if (_params.isNonzero("edo")) 
        _domination_resolver.eliminateDominatedOperations(_layers[_layer_idx]->at(_pos));

    // In preparation for the upcoming position,
    // add all effects of the actions and reductions occurring HERE
    // as (initially false) facts to THIS position.  
    initializeNextEffects();
}

void Planner::createNextPositionFromAbove() {
    Position& newPos = (*_layers[_layer_idx])[_pos];
    newPos.setPos(_layer_idx, _pos);
    int offset = _pos - (*_layers[_layer_idx-1]).getSuccessorPos(_old_pos);
    //eliminateInvalidParentsAtCurrentState(offset);
    propagateActions(offset);
    propagateReductions(offset);
    addPreconditionConstraints();
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

            bool repeatedAction = isAction && _htn.isActionRepetition(aSig._name_id);

            for (const Signature& fact : _analysis.getPossibleFactChanges(aSig)) {
                if (isAction && !addEffect(
                        repeatedAction ? aSig.renamed(_htn.getActionNameFromRepetition(aSig._name_id)) : aSig, 
                        fact, 
                        repeatedAction ? EffectMode::DIRECT_NO_QFACT : EffectMode::DIRECT)) {
                    
                    // Impossible direct effect: forbid action retroactively.
                    Log::w("Retroactively prune action %s due to impossible effect %s\n", TOSTR(aSig), TOSTR(fact));
                    actionsToRemove.insert(aSig);

                    // Also remove any virtualized actions corresponding to this action
                    USignature repSig = aSig.renamed(_htn.getRepetitionNameOfAction(aSig._name_id));
                    if (left.hasAction(repSig)) actionsToRemove.insert(repSig);
                    
                    break;
                }
                if (!isAction && !addEffect(aSig, fact, EffectMode::INDIRECT)) {
                    // Impossible indirect effect: ignore.
                }
            }

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
        _pruning.prune(aSig, _layer_idx, _pos-1);
    }
}

void Planner::addPreconditionConstraints() {
    Position& newPos = _layers[_layer_idx]->at(_pos);

    for (const auto& aSig : newPos.getActions()) {
        const Action& a = _htn.getAction(aSig);
        // Add preconditions of action
        IntPairTree badSubs;
        std::vector<IntPairTree> goodSubs;
        bool isRepetition = _htn.isActionRepetition(aSig._name_id);
        for (const Signature& fact : a.getPreconditions()) {
            addPrecondition(aSig, fact, goodSubs, badSubs, 
                /*addQFact=*/!isRepetition);
        }
        if (isRepetition) {
            USignature origSig(_htn.getActionNameFromRepetition(aSig._name_id), aSig._args);
            newPos.setValidSubstitutions(origSig, std::move(goodSubs));
            newPos.setForbiddenSubstitutions(origSig, std::move(badSubs));
        } else {
            newPos.setValidSubstitutions(aSig, std::move(goodSubs));
            newPos.setForbiddenSubstitutions(aSig, std::move(badSubs));
            addQConstantTypeConstraints(aSig);
        }
    }

    for (const auto& rSig : newPos.getReductions()) {
        // Add preconditions of reduction
        IntPairTree badSubs;
        std::vector<IntPairTree> goodSubs;
        for (const Signature& fact : _htn.getReduction(rSig).getPreconditions()) {
            addPrecondition(rSig, fact, goodSubs, badSubs);
        }
        newPos.setValidSubstitutions(rSig, std::move(goodSubs));
        newPos.setForbiddenSubstitutions(rSig, std::move(badSubs));
        addQConstantTypeConstraints(rSig);
    }
}

void Planner::addPrecondition(const USignature& op, const Signature& fact, 
        std::vector<IntPairTree>& goodSubs, IntPairTree& badSubs, bool addQFact) {

    Position& pos = (*_layers[_layer_idx])[_pos];
    const USignature& factAbs = fact.getUnsigned();

    if (!_htn.hasQConstants(factAbs)) { 
        assert(_analysis.isReachable(fact) || Log::e("Precondition %s not reachable!\n", TOSTR(fact)));
                
        if (_analysis.isReachable(factAbs, !fact._negated)) {
            // Negated prec. is reachable: not statically resolvable
            initializeFact(pos, factAbs);
            _analysis.addRelevantFact(factAbs);
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
        if (_analysis.isReachable(decFactAbs, fact._negated)) {
            goods.insert(decodingToPath(factAbs._args, decFactAbs._args, sortedArgIndices));
        } else {
            // Fact cannot hold here
            badSubs.insert(decodingToPath(factAbs._args, decFactAbs._args, sortedArgIndices));
            continue;
        }

        // If the fact is reachable, is it even invariant?
        if (_analysis.isInvariant(decFactAbs, fact._negated)) {
            // Yes! This precondition is trivially satisfied 
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
            initializeFact(pos, decFactAbs);
            if (addQFact) pos.addQFactDecoding(factAbs, decFactAbs, fact._negated);
            _analysis.addRelevantFact(decFactAbs);
        }
    } // else : encoding the precondition is not necessary!

    goodSubs.push_back(std::move(goods));
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

bool Planner::addEffect(const USignature& opSig, const Signature& fact, EffectMode mode) {
    Position& pos = (*_layers[_layer_idx])[_pos];
    assert(_pos > 0);
    Position& left = (*_layers[_layer_idx])[_pos-1];
    USignature factAbs = fact.getUnsigned();
    bool isQFact = _htn.hasQConstants(factAbs);

    if (!isQFact) {
        // Invariant fact? --> no need to encode
        if (_analysis.isInvariant(fact)) return true;

        if (mode != INDIRECT) _analysis.addRelevantFact(factAbs);

        // Depending on whether fact supports are encoded for primitive ops only,
        // add the ground fact to the op's support accordingly
        if (_nonprimitive_support || _htn.isAction(opSig)) {
            pos.addFactSupport(fact, opSig);
        } else {
            // Remember that there is some (unspecified) support for this fact
            pos.touchFactSupport(fact);
        }
        
        _analysis.addReachableFact(fact);
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

        if (_analysis.isInvariant(decFactAbs, fact._negated)) {
            // Effect holds trivially
            anyGood = true;
            continue;
        }

        // Valid effect decoding
        _analysis.addReachableFact(decFactAbs, /*negated=*/fact._negated);
        if (_nonprimitive_support || _htn.isAction(opSig)) {
            pos.addIndirectFactSupport(decFactAbs, fact._negated, opSig, std::move(path));
        } else {
            pos.touchFactSupport(decFactAbs, fact._negated);
        }
        if (mode != INDIRECT) {
            if (mode == DIRECT) pos.addQFactDecoding(factAbs, decFactAbs, fact._negated);
            _analysis.addRelevantFact(decFactAbs);
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

    _analysis.resetReachability();

    // Propagate TRUE facts
    for (const USignature& fact : above.getTrueFacts()) {
        newPos.addTrueFact(fact);
        _analysis.addInitializedFact(fact);
    }
    for (const USignature& fact : above.getFalseFacts()) {
        newPos.addFalseFact(fact);
        _analysis.addInitializedFact(fact);
    }
}

void Planner::propagateActions(size_t offset) {
    Position& newPos = (*_layers[_layer_idx])[_pos];
    Position& above = (*_layers[_layer_idx-1])[_old_pos];

    // Check validity of actions at above position
    std::vector<USignature> actionsToPrune;
    size_t numActionsBefore = above.getActions().size();
    for (const auto& aSig : above.getActions()) {
        if (aSig == Sig::NONE_SIG) continue;
        const Action& a = _htn.getAction(aSig);

        // Can the action occur here w.r.t. the current state?
        bool valid = _analysis.hasValidPreconditions(a.getPreconditions())
                && _analysis.hasValidPreconditions(a.getExtraPreconditions());

        // If not: forbid the action, i.e., its parent action
        if (!valid) {
            Log::i("Retroactively prune action %s@(%i,%i): no children at offset %i\n", TOSTR(aSig), _layer_idx-1, _old_pos, offset);
            actionsToPrune.push_back(aSig);
        }
    }

    // Prune invalid actions at above position
    for (const auto& aSig : actionsToPrune) {
        _pruning.prune(aSig, _layer_idx-1, _old_pos);
    }
    assert(above.getActions().size() == numActionsBefore - actionsToPrune.size() 
        || Log::e("%i != %i-%i\n", above.getActions().size(), numActionsBefore, actionsToPrune.size()));

    // Propagate remaining (valid) actions from above
    for (const auto& aSig : above.getActions()) {
        if (offset < 1) {
            // proper action propagation
            assert(_htn.isFullyGround(aSig));
            if (_params.isNonzero("aar") && !_htn.isActionRepetition(aSig._name_id)) {
                // Virtualize child of action
                USignature vChildSig = _htn.getRepetitionOfAction(aSig);
                newPos.addAction(vChildSig);
                newPos.addExpansion(aSig, vChildSig);
            } else {
                // Treat as a normal action
                newPos.addAction(aSig);
                newPos.addExpansion(aSig, aSig);
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
        auto allActions = instantiateAllActionsOfTask(subtask);

        // Any reduction(s) fitting the subtask?
        for (const USignature& subRSig : instantiateAllReductionsOfTask(subtask)) {

            if (_htn.isAction(subRSig)) {
                // Actually an action, not a reduction: remember for later
                allActions.push_back(subRSig);
                continue;
            }

            const Reduction& subR = _htn.getReduction(subRSig);
            
            assert(_htn.isReduction(subRSig) && subRSig == subR.getSignature() && _htn.isFullyGround(subRSig));
            
            newPos.addReduction(subRSig);
            newPos.addExpansionSize(subR.getSubtasks().size());

            for (const auto& rSig : parents) {
                reductionsWithChildren.insert(rSig);
                newPos.addExpansion(rSig, subRSig);
            }
        }

        // Any action(s) fitting the subtask?
        for (const USignature& aSig : allActions) {
            assert(_htn.isFullyGround(aSig));
            newPos.addAction(aSig);

            for (const auto& rSig : parents) {
                reductionsWithChildren.insert(rSig);
                newPos.addExpansion(rSig, aSig);
            }
        }
    }

    // Check if any reduction has no valid children at all
    for (const auto& rSig : above.getReductions()) {
        if (!reductionsWithChildren.count(rSig)) {
            Log::i("Retroactively prune reduction %s@(%i,%i): no children at offset %i\n", 
                    TOSTR(rSig), _layer_idx-1, _old_pos, offset);
            _pruning.prune(rSig, _layer_idx-1, _old_pos);
        }
    }
}

std::vector<USignature> Planner::instantiateAllActionsOfTask(const USignature& task) {
    std::vector<USignature> result;

    if (!_htn.isAction(task)) return result;

    const Action& a = _htn.toAction(task._name_id, task._args);
    
    for (Action& action : _instantiator.getApplicableInstantiations(a)) {
        //Log::d("ADDACTION %s ?\n", TOSTR(action.getSignature()));

        USignature sig = action.getSignature();

        // Rename any remaining variables in each action as unique q-constants,
        action = _htn.replaceVariablesWithQConstants(action, _analysis.getReducedArgumentDomains(action), _layer_idx, _pos);

        // Remove any contradictory ground effects that were just created
        action.removeInconsistentEffects();

        // Check validity
        if (!_htn.isFullyGround(action.getSignature())) continue;
        if (!_htn.hasConsistentlyTypedArgs(sig)) continue;
        if (!_analysis.hasValidPreconditions(action.getPreconditions())) continue;
        if (!_analysis.hasValidPreconditions(action.getExtraPreconditions())) continue;
        
        // Action is valid
        sig = action.getSignature();
        _htn.addAction(action);
        result.push_back(action.getSignature());
    }
    return result;
}

std::vector<USignature> Planner::instantiateAllReductionsOfTask(const USignature& task) {
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
                for (const auto& sig : instantiateAllActionsOfTask(surrSig)) {
                    result.push_back(sig);
                }
            }
            continue;
        }

        std::vector<Substitution> subs = Substitution::getAll(r.getTaskArguments(), task._args);
        for (const Substitution& s : subs) {
            for (const auto& entry : s) assert(entry.second != 0);

            Reduction rSub = r.substituteRed(s);
            USignature origSig = rSub.getSignature();
            if (!_htn.hasConsistentlyTypedArgs(origSig)) continue;
            
            for (Reduction& red : _instantiator.getApplicableInstantiations(rSub)) {
                if (addReduction(red, task)) result.push_back(red.getSignature());
            }
        }
    }
    return result;
}

bool Planner::addReduction(Reduction& red, const USignature& task) {
    USignature sig = red.getSignature();

    // Rename any remaining variables in each action as new, unique q-constants 
    red = _htn.replaceVariablesWithQConstants(red, _analysis.getReducedArgumentDomains(red), _layer_idx, _pos);

    // Check validity
    if (task._name_id >= 0 && red.getTaskSignature() != task) return false;
    if (!_htn.isFullyGround(red.getSignature())) return false;
    if (!_htn.hasConsistentlyTypedArgs(sig)) return false;
    if (!_analysis.hasValidPreconditions(red.getPreconditions())) return false;
    if (!_analysis.hasValidPreconditions(red.getExtraPreconditions())) return false;

    sig = red.getSignature();
    _htn.addReduction(red);
    
    return true;
}

void Planner::initializeNextEffects() {
    Position& newPos = (*_layers[_layer_idx])[_pos];
    
    // For each possible operation effect:
    const USigSet* ops[2] = {&newPos.getActions(), &newPos.getReductions()};
    bool isAction = true;
    for (const auto& set : ops) {
        for (const auto& aSig : *set) {
            SigSet pfc = _analysis.getPossibleFactChanges(aSig, FactAnalysis::FULL, isAction ? FactAnalysis::ACTION : FactAnalysis::REDUCTION);
            for (const Signature& eff : pfc) {

                if (!_htn.hasQConstants(eff._usig)) {
                    // New ground fact: set before the action may happen
                    initializeFact(newPos, eff._usig); 
                } else {
                    std::vector<int> sorts = _htn.getOpSortsForCondition(eff._usig, aSig);
                    for (const USignature& decEff : _htn.decodeObjects(eff._usig, sorts)) {           
                        // New ground fact: set before the action may happen
                        initializeFact(newPos, decEff);
                    }
                }
            }
        }
        isAction = false;
    }
}

void Planner::initializeFact(Position& newPos, const USignature& fact) {
    assert(!_htn.hasQConstants(fact));

    // Has the fact already been defined? -> Not new!
    if (_analysis.isInitialized(fact)) return;

    _analysis.addInitializedFact(fact);

    if (_analysis.isReachable(fact, /*negated=*/true)) newPos.addFalseFact(fact);
    else newPos.addTrueFact(fact);
}

void Planner::addQConstantTypeConstraints(const USignature& op) {
    // Add type constraints for q constants
    std::vector<TypeConstraint> cs = _htn.getQConstantTypeConstraints(op);
    // Add to this position's data structure
    for (const TypeConstraint& c : cs) {
        (*_layers[_layer_idx])[_pos].addQConstantTypeConstraint(op, c);
    }
}

void Planner::printStatistics() {
    _enc.printStages();
    Log::i("# instantiated positions: %i\n", _num_instantiated_positions);
    Log::i("# instantiated actions: %i\n", _num_instantiated_actions);
    Log::i("# instantiated reductions: %i\n", _num_instantiated_reductions);
    Log::i("# introduced pseudo-constants: %i\n", _htn.getNumberOfQConstants());
    Log::i("# retroactive prunings: %i\n", _pruning.getNumRetroactivePunings());
    Log::i("# retroactively pruned operations: %i\n", _pruning.getNumRetroactivelyPrunedOps());
    Log::i("# dominated operations: %i\n", _domination_resolver.getNumDominatedOps());
}
