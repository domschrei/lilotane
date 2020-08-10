
#include <assert.h>
#include <set>
#include <algorithm>

#include "data/instantiator.h"
#include "util/names.h"
#include "data/htn_instance.h"
#include "data/arg_iterator.h"

USigSet Instantiator::EMPTY_USIG_SET;

std::vector<Reduction> Instantiator::getApplicableInstantiations(
    const Reduction& r, const StateEvaluator& state, int mode) {

    int oldMode = _inst_mode;
    if (mode >= 0) _inst_mode = mode;

    std::vector<Reduction> result;

    for (const USignature& sig : instantiate(r, state)) {
        //log("%s\n", TOSTR(sig));
        result.push_back(r.substituteRed(Substitution(r.getArguments(), sig._args)));
    }
    _inst_mode = oldMode;

    return result;
}

std::vector<Action> Instantiator::getApplicableInstantiations(
    const Action& a, const StateEvaluator& state, int mode) {

    int oldMode = _inst_mode;
    if (mode >= 0) _inst_mode = mode;

    std::vector<Action> result;

    for (const USignature& sig : instantiate(a, state)) {
        //log("%s\n", TOSTR(sig));
        //assert(isFullyGround(sig) || Log::e("%s is not fully ground!\n", TOSTR(sig)));
        HtnOp newOp = a.substitute(Substitution(a.getArguments(), sig._args));
        result.push_back((Action&&) std::move(newOp));
    }

    _inst_mode = oldMode;

    return result;
}

bool Instantiator::hasSomeInstantiation(const USignature& sig) {

    const std::vector<int>& types = _htn->getSorts(sig._name_id);
    //log("%s , %i\n", TOSTR(sig), types.size());
    assert(types.size() == sig._args.size());
    for (size_t argPos = 0; argPos < sig._args.size(); argPos++) {
        int sort = types[argPos];
        if (_htn->getConstantsOfSort(sort).empty()) {
            return false;
        }
    }
    return true;
}

const HtnOp* __op;
struct CompArgs {
    bool operator()(const int& a, const int& b) const {
        return rating(a) > rating(b);
    }
    int rating(int arg) const {
        int r = 0;
        for (const Signature& pre : __op->getPreconditions()) {
            for (int preArg : pre._usig._args) {
                if (preArg == arg) r++;
            } 
        }
        for (const Signature& eff : __op->getEffects()) {
            for (int effArg : eff._usig._args) {
                if (effArg == arg) r++;
            } 
        }
        return r;
    }
};

USigSet Instantiator::instantiate(const HtnOp& op, const StateEvaluator& state) {
    __op = &op;

    // First try to naively ground the operation up to some limit
    FlatHashSet<int> argsToInstantiate;
    for (const int& arg : op.getArguments()) {
        if (_htn->isVariable(arg)) argsToInstantiate.insert(arg);
    }
    std::vector<int> argsByPriority(argsToInstantiate.begin(), argsToInstantiate.end());
    std::sort(argsByPriority.begin(), argsByPriority.end(), CompArgs());
    
    // a) Try to naively ground _one single_ instantiation
    // -- if this fails, there is no valid instantiation at all
    USigSet inst = instantiateLimited(op, state, argsByPriority, 1, /*returnUnfinished=*/true);
    if (inst.empty()) return inst;

    // b) Try if the number of valid instantiations is below the user-defined threshold
    //    -- in that case, return that full instantiation
    if (_q_const_instantiation_limit > 0) {
        USigSet inst = instantiateLimited(op, state, argsByPriority, _q_const_instantiation_limit, /*returnUnfinished=*/false);
        if (!inst.empty()) return inst;
    }
    
    return instantiateLimited(op, state, argsByPriority, 0, false);

    /*
    // Collect all arguments which should be instantiated
    FlatHashSet<int> argsToInstantiate;

    // a) All variable args according to the q-constant policy
    for (size_t i = 0; i < op.getArguments().size(); i++) {
        const int& arg = op.getArguments().at(i);
        if (!_htn->isVariable(arg)) continue;

        if (_inst_mode == INSTANTIATE_FULL) {

            argsToInstantiate.insert(arg);

        } else if (_inst_mode == INSTANTIATE_PRECONDITIONS) {
    
            bool found = false;
            for (const auto& pre : op.getPreconditions()) {
                for (const int& preArg : pre._usig._args) {
                    if (arg == preArg) {
                        found = true;
                        break;
                    }
                }
                if (found) break;
            }

            if (found) argsToInstantiate.insert(arg);
        }
    }

    // b) All variable args whose domain is below the specified q constant threshold
    if (_q_const_rating_factor > 0) {
        const auto& ratings = getPreconditionRatings(op.getSignature());
        if (_inst_mode != INSTANTIATE_FULL)
        for (size_t argIdx = 0; argIdx < op.getArguments().size(); argIdx++) {
            int arg = op.getArguments().at(argIdx);
            if (!_htn->isVariable(arg)) continue;

            int sort = _htn->getSorts(op.getNameId()).at(argIdx);
            int domainSize = _htn->getConstantsOfSort(sort).size();
            float r = ratings.at(arg);
            if (_q_const_rating_factor*r > domainSize) {
                argsToInstantiate.insert(arg);
            }
        }
    }

    // Sort args to instantiate by their priority descendingly
    argsByPriority = std::vector<int>(argsToInstantiate.begin(), argsToInstantiate.end());
    std::sort(argsByPriority.begin(), argsByPriority.end(), CompArgs());

    return instantiateLimited(op, state, argsByPriority, 0, false);
    */
}

USigSet Instantiator::instantiateLimited(const HtnOp& op, const StateEvaluator& state, 
            const std::vector<int>& argsByPriority, size_t limit, bool returnUnfinished) {

    USigSet instantiation;
    size_t doneInstSize = argsByPriority.size();
    
    if (doneInstSize == 0) {
        if (hasValidPreconditions(op.getPreconditions(), state) 
            && hasSomeInstantiation(op.getSignature())) 
            instantiation.insert(op.getSignature());
        //log("INST %s : %i instantiations X\n", TOSTR(op.getSignature()), instantiation.size());
        return instantiation;
    }

    // Create back transformation of argument positions
    FlatHashMap<int, int> argPosBackMapping;
    for (size_t j = 0; j < argsByPriority.size(); j++) {
        for (size_t i = 0; i < op.getArguments().size(); i++) {
            if (op.getArguments()[i] == argsByPriority[j]) {
                argPosBackMapping[j] = i;
                break;
            }
        }   
    }

    std::vector<std::vector<int>> assignmentsStack;
    assignmentsStack.push_back(std::vector<int>()); // begin with empty assignment
    while (!assignmentsStack.empty()) {
        const std::vector<int> assignment = assignmentsStack.back();
        assignmentsStack.pop_back();
        //for (int a : assignment) log("%i ", a); log("\n");

        // Loop over possible choices for the next argument position
        int argPos = argPosBackMapping[assignment.size()];
        int sort = _htn->getSorts(op.getNameId()).at(argPos);
        for (int c : _htn->getConstantsOfSort(sort)) {

            // Create new assignment
            std::vector<int> newAssignment(assignment);
            newAssignment.push_back(c);

            // Create corresponding op
            Substitution s;
            for (size_t i = 0; i < newAssignment.size(); i++) {
                assert(i < argsByPriority.size());
                s[argsByPriority[i]] = newAssignment[i];
            }
            HtnOp newOp = op.substitute(s);

            // Test validity
            if (!hasValidPreconditions(newOp.getPreconditions(), state)) continue;

            // All ok -- add to stack
            if (newAssignment.size() == doneInstSize) {
                // If there are remaining variables: 
                // is there some valid constant for each of them?
                if (!hasSomeInstantiation(newOp.getSignature())) continue;

                // This instantiation is finished:
                // Assemble instantiated signature
                instantiation.insert(newOp.getSignature());

                if (limit > 0) {
                    if (returnUnfinished && instantiation.size() == limit) {
                        // Limit exceeded -- return unfinished instantiation
                        return instantiation;
                    }
                    if (!returnUnfinished && instantiation.size() > limit) {
                        // Limit exceeded -- return failure
                        return USigSet();
                    }
                }

            } else {
                // Unfinished instantiation
                assignmentsStack.push_back(newAssignment);
            }
        }
    }

    __op = NULL;

    //log("INST %s : %i instantiations\n", TOSTR(op.getSignature()), instantiation.size());
    return instantiation;
}

const FlatHashMap<int, float>& Instantiator::getPreconditionRatings(const USignature& opSig) {

    int nameId = opSig._name_id;
    
    // Substitution mapping
    std::vector<int> placeholderArgs;
    USignature normSig = _htn->getNormalizedLifted(opSig, placeholderArgs);
    Substitution sFromPlaceholder(placeholderArgs, opSig._args);

    if (!_precond_ratings.count(nameId)) {
        // Compute
        NodeHashMap<int, std::vector<float>> ratings;
        NodeHashMap<int, std::vector<int>> numRatings;
        
        NetworkTraversal(*_htn).traverse(normSig, NetworkTraversal::TRAVERSE_PREORDER, [&](const USignature& nodeSig, int depth) {

            HtnOp op = (_htn->isAction(nodeSig) ? 
                        (HtnOp)_htn->toAction(nodeSig._name_id, nodeSig._args) : 
                        (HtnOp)_htn->toReduction(nodeSig._name_id, nodeSig._args));
            int numPrecondArgs = 0;
            int occs = 0;
            for (size_t i = 0; i < normSig._args.size(); i++) {
                int opArg = opSig._args[i];
                int normArg = normSig._args[i];
                if (!_htn->isVariable(opArg)) continue;
                
                ratings[opArg];
                numRatings[opArg];
                while ((size_t)depth >= ratings[opArg].size()) {
                    ratings[opArg].push_back(0);
                    numRatings[opArg].push_back(0);
                }

                for (const Signature& pre : op.getPreconditions()) for (const int& preArg : pre._usig._args) {
                    if (normArg == preArg) occs++;
                    numPrecondArgs++;
                }

                ratings[opArg][depth] += (numPrecondArgs > 0) ? (float)occs / numPrecondArgs : 0;
                numRatings[opArg][depth]++;
            }
        });

        _precond_ratings[nameId];
        for (const auto& entry : ratings) {
            const int& arg = entry.first;
            _precond_ratings[nameId][arg] = 0;
            for (size_t depth = 0; depth < entry.second.size(); depth++) {
                const float& r = entry.second[depth];
                const int& numR = numRatings[arg][depth];
                if (numR > 0) _precond_ratings[nameId][arg] += 1.0f/(1 << depth) * r/numR;
            }
        }
    }

    return _precond_ratings.at(nameId);
}

SigSet Instantiator::getPossibleFactChanges(const USignature& sig, bool fullyInstantiate) {
    if (sig == Position::NONE_SIG) return SigSet();

    int nameId = sig._name_id;
    
    // Substitution mapping
    std::vector<int> placeholderArgs;
    USignature normSig = _htn->getNormalizedLifted(sig, placeholderArgs);
    Substitution sFromPlaceholder(placeholderArgs, sig._args);

    auto& factChanges = fullyInstantiate ? _fact_changes : _lifted_fact_changes;
    if (!factChanges.count(nameId)) {
        // Compute fact changes for origSig
        
        NodeHashSet<Signature, SignatureHasher> facts;

        _traversal.traverse(normSig.substitute(Substitution(normSig._args, placeholderArgs)), 
        NetworkTraversal::TRAVERSE_PREORDER,
        [&](const USignature& nodeSig, int depth) { // NOLINT
            // If visited node is an action: add effects
            if (_htn->isAction(nodeSig)) {
                Action a = _htn->toAction(nodeSig._name_id, nodeSig._args);
                for (const Signature& eff : a.getEffects()) {
                    facts.insert(eff);
                }
            }
        });

        // Convert result to vector
        SigSet& liftedResult = _lifted_fact_changes[nameId];
        SigSet& result = _fact_changes[nameId];
        for (const Signature& sig : facts) {
            liftedResult.insert(sig);
            for (const Signature& sigGround : ArgIterator::getFullInstantiation(sig, *_htn)) {
                result.insert(sigGround);
            }
        }
    }

    // Get fact changes, substitute arguments
    SigSet out = factChanges.at(nameId);
    for (Signature& sig : out) {
        sig.apply(sFromPlaceholder);
    }
    return out;
}

FactFrame Instantiator::getFactFrame(const USignature& sig, bool simpleMode, USigSet& currentOps) {

    Log::d("GET_FACT_FRAME %s\n", TOSTR(sig));

    int nameId = sig._name_id;
    if (!_fact_frames.count(nameId)) {

        FactFrame result;

        std::vector<int> newArgs(sig._args.size());
        for (size_t i = 0; i < sig._args.size(); i++) {
            newArgs[i] = _htn->nameId("c" + std::to_string(i));
        }
        USignature op(sig._name_id, std::move(newArgs));
        result.sig = op;

        if (_htn->isAction(op)) {

            // Action
            const Action& a = _htn->toAction(op._name_id, op._args);
            result.preconditions = a.getPreconditions();
            result.flatEffects = a.getEffects();
            if (!simpleMode) result.causalEffects[std::vector<Signature>()] = a.getEffects();

        } else if (currentOps.count(op)) {

            // Handle recursive call of same reduction: Conservatively add preconditions and effects
            // without recursing on subtasks
            const Reduction& r = _htn->toReduction(op._name_id, op._args);
            result.preconditions = r.getPreconditions();
            result.flatEffects = getPossibleFactChanges(r.getSignature(), /*fullyInstantiate=*/false);
            Log::d("RECURSIVE_FACT_FRAME %s\n", TOSTR(result.flatEffects));
            if (!simpleMode) result.causalEffects[std::vector<Signature>()] = result.flatEffects;

        } else {

            currentOps.insert(op);
            const Reduction& r = _htn->toReduction(op._name_id, op._args);
            result.sig = op;
            result.preconditions.insert(r.getPreconditions().begin(), r.getPreconditions().end());
            
            // For each subtask position ("offset")
            for (size_t offset = 0; offset < r.getSubtasks().size(); offset++) {
                
                FactFrame frameOfOffset;
                std::vector<USignature> children;
                _traversal.getPossibleChildren(r.getSubtasks(), offset, children);
                bool firstChild = true;

                // Assemble fact frame of this offset by iterating over all possible children at the offset
                for (const auto& child : children) {

                    // Assemble unified argument names
                    std::vector<int> newChildArgs(child._args);
                    for (size_t i = 0; i < child._args.size(); i++) {
                        if (_htn->isVariable(child._args[i])) newChildArgs[i] = _htn->nameId("??_");
                    }

                    // Recursively get child frame of the child
                    FactFrame childFrame = getFactFrame(USignature(child._name_id, std::move(newChildArgs)), simpleMode);
                    
                    if (firstChild) {
                        // Add all preconditions of child that are not yet part of the parent's effects
                        for (const auto& pre : childFrame.preconditions) {
                            bool isNew = true;
                            for (const auto& eff : result.flatEffects) {
                                if (fits(eff, pre) || fits(pre, eff)) {
                                    isNew = false;
                                    Log::d("FACT_FRAME Precondition %s absorbed by effect %s of %s\n", TOSTR(pre), TOSTR(eff), TOSTR(child));
                                    break;
                                } 
                            }
                            if (isNew) frameOfOffset.preconditions.insert(std::move(pre));
                        }
                        firstChild = false;
                    } else {
                        // Intersect preconditions
                        SigSet newPrec;
                        for (auto& pre : childFrame.preconditions) {
                            if (frameOfOffset.preconditions.count(pre)) {
                                newPrec.insert(std::move(pre));
                            }
                        }
                        frameOfOffset.preconditions = std::move(newPrec);
                    }

                    if (simpleMode) {
                        // Add all of the child's effects to the parent's effects
                        frameOfOffset.flatEffects.insert(childFrame.flatEffects.begin(), childFrame.flatEffects.end());
                    } else {
                        // Add all effects with (child's precondition + eff. preconditions) minus the parent's effects
                        for (const auto& [pres, effs] : childFrame.causalEffects) {
                            SigSet newPres;
                            for (const auto& pre : childFrame.preconditions) {
                                if (!result.flatEffects.count(pre) && !frameOfOffset.preconditions.count(pre)) 
                                    newPres.insert(pre);
                            }
                            for (const auto& pre : pres) {
                                if (!result.flatEffects.count(pre) && !frameOfOffset.preconditions.count(pre)) 
                                    newPres.insert(pre);
                            }
                            frameOfOffset.causalEffects[std::vector<Signature>(newPres.begin(), newPres.end())] = effs;
                            frameOfOffset.flatEffects.insert(effs.begin(), effs.end());
                        }
                    }
                }

                // Write into parent's fact frame
                result.preconditions.insert(frameOfOffset.preconditions.begin(), frameOfOffset.preconditions.end());
                if (simpleMode) {
                    result.flatEffects.insert(frameOfOffset.flatEffects.begin(), frameOfOffset.flatEffects.end());
                } else {
                    for (const auto& [pres, effs] : frameOfOffset.causalEffects) {
                        result.causalEffects[pres].insert(effs.begin(), effs.end());
                        result.flatEffects.insert(effs.begin(), effs.end());
                    }
                }
            }
        }

        _fact_frames[nameId] = std::move(result);
        currentOps.erase(sig);

        Log::d("FACT_FRAME %s\n", TOSTR(_fact_frames[nameId]));
    }

    const FactFrame& f = _fact_frames[nameId];
    return f.substitute(Substitution(f.sig._args, sig._args));
}

std::vector<int> Instantiator::getFreeArgPositions(const std::vector<int>& sigArgs) {
    std::vector<int> argPositions;
    for (size_t i = 0; i < sigArgs.size(); i++) {
        int arg = sigArgs[i];
        if (_htn->isVariable(arg)) argPositions.push_back(i);
    }
    return argPositions;
}

bool Instantiator::fits(const Signature& from, const Signature& to, FlatHashMap<int, int>* substitution) {
    if (from._negated != to._negated) return false;
    return fits(from._usig, to._usig, substitution);
}

bool Instantiator::fits(const USignature& from, const USignature& to, FlatHashMap<int, int>* substitution) {
    if (from._name_id != to._name_id) return false;
    if (from._args.size() != to._args.size()) return false;

    for (size_t i = 0; i < from._args.size(); i++) {

        if (!_htn->isVariable(from._args[i])) {
            // Constant parameter: must be equal
            if (from._args[i] != to._args[i]) return false;

        } else if (_htn->isVariable(to._args[i])) {
            // Both are variables: fine
            if (substitution != nullptr) {
                (*substitution)[from._args[i]] = to._args[i];
            }
        
        } else {
            // Variable to constant: fine
            if (substitution != nullptr) {
                (*substitution)[from._args[i]] = to._args[i];
            }
        }
    }
    return true;
}

bool Instantiator::hasConsistentlyTypedArgs(const USignature& sig) {
    const std::vector<int>& taskSorts = _htn->getSorts(sig._name_id);
    for (size_t argPos = 0; argPos < sig._args.size(); argPos++) {
        int sort = taskSorts[argPos];
        int arg = sig._args[argPos];
        if (_htn->isVariable(arg)) continue; // skip variable
        bool valid = false;
        if (_htn->isQConstant(arg)) {
            // q constant: TODO check if SOME SUBSTITUTEABLE CONSTANT has the correct sort
            for (int cnst : _htn->getDomainOfQConstant(arg)) {
                if (_htn->getConstantsOfSort(sort).count(cnst)) {
                    valid = true;
                    break;
                }
            }
        } else {
            // normal constant: check if it is contained in the correct sort
            valid = _htn->getConstantsOfSort(sort).count(arg);
        }
        if (!valid) {
            //log("arg %s not of sort %s => %s invalid\n", TOSTR(arg), TOSTR(sort), TOSTR(sig));
            return false;
        } 
    }
    return true;
}

std::vector<TypeConstraint> Instantiator::getQConstantTypeConstraints(const USignature& sig) {

    std::vector<TypeConstraint> constraints;

    const std::vector<int>& taskSorts = _htn->getSorts(sig._name_id);
    for (size_t argPos = 0; argPos < sig._args.size(); argPos++) {
        int sigSort = taskSorts[argPos];
        int arg = sig._args[argPos];
        
        // Not a q-constant here
        if (!_htn->isQConstant(arg)) {
            // Must be of valid type
            assert(_htn->getConstantsOfSort(sigSort).count(arg));
            continue;
        }

        // Type is fine no matter which substitution is chosen
        if (_htn->getSortsOfQConstant(arg).count(sigSort)) continue;

        // Type is NOT fine, at least for some substitutions
        std::vector<int> good;
        std::vector<int> bad;
        const FlatHashSet<int>& validConstants = _htn->getConstantsOfSort(sigSort);
        // For each value the qconstant can assume:
        for (int c : _htn->getDomainOfQConstant(arg)) {
            // Is that constant of correct type?
            if (validConstants.count(c)) good.push_back(c);
            else bad.push_back(c);
        }

        if (good.size() >= bad.size()) {
            // arg must be EITHER of the GOOD ones
            constraints.emplace_back(arg, true, std::move(good));
        } else {
            // arg must be NEITHER of the BAD ones
            constraints.emplace_back(arg, false, std::move(bad));
        }
    }

    return constraints;
}
