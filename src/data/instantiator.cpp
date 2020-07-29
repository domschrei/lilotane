
#include <assert.h>
#include <set>
#include <algorithm>

#include "data/instantiator.h"
#include "util/names.h"
#include "data/htn_instance.h"
#include "data/arg_iterator.h"

std::vector<Reduction> Instantiator::getApplicableInstantiations(
    const Reduction& r, const std::function<bool(const Signature&)>& state, int mode) {

    int oldMode = _inst_mode;
    if (mode >= 0) _inst_mode = mode;

    std::vector<Reduction> result;

    USigSet inst = instantiate(r, state);
    for (const USignature& sig : inst) {
        //log("%s\n", TOSTR(sig));
        result.push_back(r.substituteRed(Substitution(r.getArguments(), sig._args)));
    }

    _inst_mode = oldMode;

    return result;
}

std::vector<Action> Instantiator::getApplicableInstantiations(
    const Action& a, const std::function<bool(const Signature&)>& state, int mode) {

    int oldMode = _inst_mode;
    if (mode >= 0) _inst_mode = mode;

    std::vector<Action> result;

    USigSet inst = instantiate(a, state);
    for (const USignature& sig : inst) {
        //log("%s\n", TOSTR(sig));
        //assert(isFullyGround(sig) || Log::e("%s is not fully ground!\n", TOSTR(sig)));
        HtnOp newOp = a.substitute(Substitution(a.getArguments(), sig._args));
        result.push_back((Action) newOp);
    }

    _inst_mode = oldMode;

    return result;
}

bool Instantiator::hasSomeInstantiation(const USignature& sig) {

    const std::vector<int>& types = _htn->getSorts(sig._name_id);
    //log("%s , %i\n", TOSTR(sig), types.size());
    assert(types.size() == sig._args.size());
    for (int argPos = 0; argPos < sig._args.size(); argPos++) {
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

USigSet Instantiator::instantiate(const HtnOp& op, const std::function<bool(const Signature&)>& state) {
    __op = &op;

    // First try to naively ground the operation up to some limit
    std::vector<int> argsByPriority;
    for (const int& arg : op.getArguments()) {
        if (_htn->isVariable(arg)) argsByPriority.push_back(arg);
    }
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

    // Collect all arguments which should be instantiated
    FlatHashSet<int> argsToInstantiate;

    // a) All variable args according to the q-constant policy
    for (int i = 0; i < op.getArguments().size(); i++) {
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
        /*for (const auto& entry : ratings) {
            log("%s -- %s : rating %.3f\n", TOSTR(op.getSignature()), TOSTR(entry.first), entry.second);
        }*/
        if (_inst_mode != INSTANTIATE_FULL)
        for (int argIdx = 0; argIdx < op.getArguments().size(); argIdx++) {
            int arg = op.getArguments().at(argIdx);
            if (!_htn->isVariable(arg)) continue;

            int sort = _htn->getSorts(op.getSignature()._name_id).at(argIdx);
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
}

USigSet Instantiator::instantiateLimited(const HtnOp& op, const std::function<bool(const Signature&)>& state, 
            const std::vector<int>& argsByPriority, int limit, bool returnUnfinished) {

    USigSet instantiation;
    int doneInstSize = argsByPriority.size();
    
    if (doneInstSize == 0) {
        if (hasValidPreconditions(op.getPreconditions(), state) 
            && hasSomeInstantiation(op.getSignature())) 
            instantiation.insert(op.getSignature());
        //log("INST %s : %i instantiations X\n", TOSTR(op.getSignature()), instantiation.size());
        return instantiation;
    }

    // Create back transformation of argument positions
    FlatHashMap<int, int> argPosBackMapping;
    for (int j = 0; j < argsByPriority.size(); j++) {
        for (int i = 0; i < op.getArguments().size(); i++) {
            if (op.getArguments()[i] == argsByPriority[j]) {
                argPosBackMapping[j] = i;
                break;
            }
        }   
    }

    /*
    TODO
    For each assignment level, remember all of op's preconditions which *are to become ground* at that level.
    If this set is not empty at level x, then, instead of iterating over all valid constants to the current
    assignment, iterate over the according true facts in the state and use their corresponding constants.

    Prerequisite: Capability of state to iterate over all true facts of given predicate and polarity.
    */

    /*
    std::vector<int> qconstants, qconstIndices;
    for (int i = 0; i < argsByPriority; i++) {
        const int& arg = op.getArguments()[i];
        if (_htn->isQConstant(arg)) {
            qconstants.push_back(arg);
            qconstIndices.push_back(i);
        }
    }*/

    std::vector<std::vector<int>> assignmentsStack;
    assignmentsStack.push_back(std::vector<int>()); // begin with empty assignment
    while (!assignmentsStack.empty()) {
        const std::vector<int> assignment = assignmentsStack.back();
        assignmentsStack.pop_back();
        //for (int a : assignment) log("%i ", a); log("\n");

        // Loop over possible choices for the next argument position
        int argPos = argPosBackMapping[assignment.size()];
        int sort = _htn->getSorts(op.getSignature()._name_id).at(argPos);
        for (int c : _htn->getConstantsOfSort(sort)) {

            // Create new assignment
            std::vector<int> newAssignment = assignment;
            newAssignment.push_back(c);

            // Create corresponding op
            Substitution s;
            for (int i = 0; i < newAssignment.size(); i++) {
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
        
        NetworkTraversal(*_htn).traverse(normSig, [&](const USignature& nodeSig, int depth) {

            HtnOp op = (_htn->isAction(nodeSig) ? 
                        (HtnOp)_htn->toAction(nodeSig._name_id, nodeSig._args) : 
                        (HtnOp)_htn->toReduction(nodeSig._name_id, nodeSig._args));
            int numPrecondArgs = 0;
            int occs = 0;
            for (int i = 0; i < normSig._args.size(); i++) {
                int opArg = opSig._args[i];
                int normArg = normSig._args[i];
                if (!_htn->isVariable(opArg)) continue;
                
                ratings[opArg];
                numRatings[opArg];
                while (depth >= ratings[opArg].size()) {
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
            for (int depth = 0; depth < entry.second.size(); depth++) {
                const float& r = entry.second[depth];
                const int& numR = numRatings[arg][depth];
                if (numR > 0) _precond_ratings[nameId][arg] += 1.0f/(1 << depth) * r/numR;
            }
        }
    }

    return _precond_ratings.at(nameId);
}





// Given an unsigned ground fact signature and (a set of operations effects containing q constants),
// compute the possible sets of substitutions that are necessary to let the operation
// have the specified fact in its support.
NodeHashSet<Substitution, Substitution::Hasher> Instantiator::getOperationSubstitutionsCausingEffect(
            const SigSet& effects, const USignature& fact, bool negated) {

    //log("?= can %s be produced by %s ?\n", TOSTR(fact), TOSTR(opSig));
    NodeHashSet<Substitution, Substitution::Hasher> substitutions;

    // For each such effect: check if it is a valid result
    // of some series of q const substitutions
    for (const Signature& eff : effects) {
        if (eff._usig._name_id != fact._name_id) continue;
        if (eff._negated != negated) continue;
        bool matches = true;
        Substitution s;
        std::vector<int> qargs, decargs;
        //log("  %s ?= %s ", TOSTR(eff), TOSTR(fact));
        for (int argPos = 0; argPos < eff._usig._args.size(); argPos++) {
            int effArg = eff._usig._args[argPos];
            int substArg = fact._args[argPos];
            bool effIsQ = _htn->isQConstant(effArg);
            if (!effIsQ) {
                // If the effect fact has no q const here, the arg must be left unchanged
                matches &= effArg == substArg;
            } else {
                // If the effect fact has a q const here, the substituted arg must be in the q const's domain
                matches &= _htn->getDomainOfQConstant(effArg).count(substArg);
            }
            if (!matches) break;
            if (substArg != effArg) {
                // No two different substitution values for one arg!
                matches &= (!s.count(effArg) || s[effArg] == substArg);
                if (!matches) break;
                s[effArg] = substArg;
                if (effIsQ) {
                    qargs.push_back(effArg);
                    decargs.push_back(substArg);
                }
            }
        }
        if (matches) {
            // Matching substitution found. Valid?
            if (!_htn->getQConstantDatabase().test(qargs, decargs)) continue;

            // Matching, valid substitution
            if (!substitutions.count(s)) substitutions.insert(s);
            //log(" -- yes\n");
        } else {
            //log(" -- no\n");
        }
    }

    return substitutions;
}

SigSet Instantiator::getAllFactChanges(const USignature& sig) {    
    if (sig == Position::NONE_SIG) return SigSet();
    return getPossibleFactChanges(sig);
}

SigSet Instantiator::getPossibleFactChanges(const USignature& sig) {

    int nameId = sig._name_id;
    
    // Substitution mapping
    std::vector<int> placeholderArgs;
    USignature normSig = _htn->getNormalizedLifted(sig, placeholderArgs);
    Substitution sFromPlaceholder(placeholderArgs, sig._args);

    if (!_fact_changes.count(nameId)) {
        // Compute fact changes for origSig
        
        NodeHashSet<Signature, SignatureHasher> facts;

        _traversal.traverse(normSig.substitute(Substitution(normSig._args, placeholderArgs)), 
        [&](const USignature& nodeSig, int depth) {
            // If visited node is an action: add effects
            if (_htn->isAction(nodeSig)) {
                Action a = _htn->toAction(nodeSig._name_id, nodeSig._args);
                for (const Signature& eff : a.getEffects()) {
                    facts.insert(eff);
                }
            }
        });

        // Convert result to vector
        SigSet& result = _fact_changes[nameId];
        for (const Signature& sig : facts) {
            for (const Signature& sigGround : ArgIterator::getFullInstantiation(sig, *_htn)) {
                result.insert(sigGround);
            }
        }
    }

    // Get fact changes, substitute arguments
    SigSet out = _fact_changes[nameId];
    for (Signature& sig : out) {
        sig = sig.substitute(sFromPlaceholder);
    }
    return out;
}

bool Instantiator::isFullyGround(const USignature& sig) {
    for (int arg : sig._args) {
        if (_htn->isVariable(arg)) return false;
    }
    return true;
}

std::vector<int> Instantiator::getFreeArgPositions(const std::vector<int>& sigArgs) {
    std::vector<int> argPositions;
    for (int i = 0; i < sigArgs.size(); i++) {
        int arg = sigArgs[i];
        if (_htn->isVariable(arg)) argPositions.push_back(i);
    }
    return argPositions;
}

bool Instantiator::fits(USignature& sig, USignature& groundSig, FlatHashMap<int, int>* substitution) {
    assert(sig._name_id == groundSig._name_id);
    assert(sig._args.size() == groundSig._args.size());
    assert(isFullyGround(groundSig));
    //if (sig._negated != groundSig._negated) return false;
    for (int i = 0; i < sig._args.size(); i++) {
        if (!_htn->isVariable(sig._args[i])) {
            // Constant parameter: must be equal
            if (sig._args[i] != groundSig._args[i]) return false;
        }

        if (substitution != NULL) {
            assert(!substitution->count(sig._args[i]));
            (*substitution)[sig._args[i]] = groundSig._args[i];
        }
    }
    return true;
}

bool Instantiator::hasConsistentlyTypedArgs(const USignature& sig) {
    const std::vector<int>& taskSorts = _htn->getSorts(sig._name_id);
    for (int argPos = 0; argPos < sig._args.size(); argPos++) {
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
    for (int argPos = 0; argPos < sig._args.size(); argPos++) {
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
        FlatHashSet<int> validConstants = _htn->getConstantsOfSort(sigSort);
        // For each value the qconstant can assume:
        for (int c : _htn->getDomainOfQConstant(arg)) {
            // Is that constant of correct type?
            if (validConstants.count(c)) good.push_back(c);
            else bad.push_back(c);
        }

        if (good.size() >= bad.size()) {
            // arg must be EITHER of the GOOD ones
            constraints.emplace_back(arg, true, good);
        } else {
            // arg must be NEITHER of the BAD ones
            constraints.emplace_back(arg, false, bad);
        }
    }

    return constraints;
}

bool Instantiator::test(const Signature& sig, const std::function<bool(const Signature&)>& state) {
    
    if (!isFullyGround(sig._usig)) return true;

    bool positive = !sig._negated;
    
    // Q-Fact:
    if (_htn->hasQConstants(sig._usig)) {
        //log("QTEST %s\n", TOSTR(sig));
        for (const auto& decSig : _htn->decodeObjects(sig._usig, true)) {
            bool result = test(Signature(decSig, sig._negated), state);
            //log("QTEST -- %s : %s\n", TOSTR(decSig), result ? "TRUE" : "FALSE");    
            if (result) return true;
        }
        return false;
    }

    // fact positive : true iff contained in facts
    if (positive) return state(sig);
    
    // fact negative.

    // if contained in facts : return true
    //   (fact occurred negative)
    if (state(sig)) return true;
    
    // else: return true iff fact does NOT occur in positive form
    return !state(sig.opposite());
}

bool Instantiator::hasValidPreconditions(const SigSet& preconds, const std::function<bool(const Signature&)>& state) {

    for (const Signature& pre : preconds) {
        //Log::d("   %s ? ", TOSTR(pre));
        if (!test(pre, state)) {
            //Log::d("FALSE\n");
            return false;
        }
        //Log::d("TRUE\n");
    }
    return true;
}