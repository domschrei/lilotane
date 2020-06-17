
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
        //log("%s\n", Names::to_string(sig).c_str());
        result.push_back(r.substituteRed(Substitution::get(r.getArguments(), sig._args)));
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
        //log("%s\n", Names::to_string(sig).c_str());
        assert(isFullyGround(sig));
        HtnOp newOp = a.substitute(Substitution::get(a.getArguments(), sig._args));
        result.push_back((Action) newOp);
    }

    _inst_mode = oldMode;

    return result;
}

bool Instantiator::hasSomeInstantiation(const USignature& sig) {

    const std::vector<int>& types = _htn->_signature_sorts_table[sig._name_id];
    for (int argPos = 0; argPos < sig._args.size(); argPos++) {
        int sort = types[argPos];
        if (_htn->_constants_by_sort[sort].empty()) {
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

    if (_q_const_instantiation_limit > 0) {
        // First try to naively ground the operation up to some limit
        std::vector<int> argsByPriority;
        for (const int& arg : op.getArguments()) {
            if (_htn->_var_ids.count(arg)) argsByPriority.push_back(arg);
        }
        std::sort(argsByPriority.begin(), argsByPriority.end(), CompArgs());
        USigSet inst = instantiateLimited(op, state, argsByPriority, _q_const_instantiation_limit);
        if (!inst.empty()) return inst;
    }

    // Collect all arguments which should be instantiated
    HashSet<int> argsToInstantiate;

    // a) All variable args according to the q-constant policy
    for (int i = 0; i < op.getArguments().size(); i++) {
        const int& arg = op.getArguments().at(i);
        if (!_htn->_var_ids.count(arg)) continue;

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
            log("%s -- %s : rating %.3f\n", Names::to_string(op.getSignature()).c_str(), Names::to_string(entry.first).c_str(), entry.second);
        }*/
        if (_inst_mode != INSTANTIATE_FULL)
        for (int argIdx = 0; argIdx < op.getArguments().size(); argIdx++) {
            int arg = op.getArguments().at(argIdx);
            if (!_htn->_var_ids.count(arg)) continue;

            int sort = _htn->_signature_sorts_table[op.getSignature()._name_id][argIdx];
            int domainSize = _htn->getConstantsOfSort(sort).size();
            float r = ratings.at(arg);
            if (_q_const_rating_factor*r > domainSize) {
                argsToInstantiate.insert(arg);
            }
        }
    }

    // Sort args to instantiate by their priority descendingly
    std::vector<int> argsByPriority(argsToInstantiate.begin(), argsToInstantiate.end());
    std::sort(argsByPriority.begin(), argsByPriority.end(), CompArgs());

    return instantiateLimited(op, state, argsByPriority, 0);
}

USigSet Instantiator::instantiateLimited(const HtnOp& op, const std::function<bool(const Signature&)>& state, 
            const std::vector<int>& argsByPriority, int limit) {

    USigSet instantiation;
    int doneInstSize = argsByPriority.size();
    
    if (doneInstSize == 0) {
        if (hasValidPreconditions(op.getPreconditions(), state) 
            && hasSomeInstantiation(op.getSignature())) 
            instantiation.insert(op.getSignature());
        return instantiation;
    }

    // Create back transformation of argument positions
    HashMap<int, int> argPosBackMapping;
    for (int j = 0; j < argsByPriority.size(); j++) {
        for (int i = 0; i < op.getArguments().size(); i++) {
            if (op.getArguments()[i] == argsByPriority[j]) {
                argPosBackMapping[j] = i;
                break;
            }
        }   
    }

    std::vector<std::vector<int>> assignmentsStack;
    assignmentsStack.push_back(std::vector<int>());
    while (!assignmentsStack.empty()) {
        const std::vector<int> assignment = assignmentsStack.back();
        assignmentsStack.pop_back();
        //for (int a : assignment) log("%i ", a); log("\n");

        // Pick constant for next argument position
        int argPos = argPosBackMapping[assignment.size()];
        int sort = _htn->_signature_sorts_table[op.getSignature()._name_id][argPos];
        for (int c : _htn->_constants_by_sort[sort]) {

            // Create new assignment
            std::vector<int> newAssignment = assignment;
            newAssignment.push_back(c);

            // Create corresponding op
            substitution_t s;
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

                if (limit > 0 && instantiation.size() > limit) {
                    // Limit exceeded -- failure
                    return USigSet();
                }

            } else {
                // Unfinished instantiation
                assignmentsStack.push_back(newAssignment);
            }
        }
    }

    __op = NULL;

    return instantiation;
}

const HashMap<int, float>& Instantiator::getPreconditionRatings(const USignature& opSig) {

    int nameId = opSig._name_id;
    
    // Substitution mapping
    std::vector<int> placeholderArgs;
    USignature normSig = _htn->getNormalizedLifted(opSig, placeholderArgs);
    HashMap<int, int> sFromPlaceholder = Substitution::get(placeholderArgs, opSig._args);

    if (!_precond_ratings.count(nameId)) {
        // Compute
        HashMap<int, std::vector<float>> ratings;
        HashMap<int, std::vector<int>> numRatings;
        
        NetworkTraversal(*_htn).traverse(normSig, [&](const USignature& nodeSig, int depth) {

            HtnOp& op = (_htn->_actions.count(nodeSig._name_id) ? (HtnOp&)_htn->_actions.at(nodeSig._name_id) : (HtnOp&)_htn->_reductions.at(nodeSig._name_id));
            HtnOp opSub = op.substitute(Substitution::get(op.getArguments(), nodeSig._args));
            int numPrecondArgs = 0;
            int occs = 0;
            for (int i = 0; i < normSig._args.size(); i++) {
                int opArg = opSig._args[i];
                int normArg = normSig._args[i];
                if (!_htn->_var_ids.count(opArg)) continue;
                
                ratings[opArg];
                numRatings[opArg];
                while (depth >= ratings[opArg].size()) {
                    ratings[opArg].push_back(0);
                    numRatings[opArg].push_back(0);
                }

                for (const Signature& pre : opSub.getPreconditions()) for (const int& preArg : pre._usig._args) {
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





// Given a fact (signature) and (a set of ground htn operations containing q constants),
// compute the possible sets of substitutions that are necessary to let each operation
// have the specified fact in its support.
HashSet<substitution_t, Substitution::Hasher> Instantiator::getOperationSubstitutionsCausingEffect(
            const SigSet& effects, const USignature& fact, bool negated) {

    //log("?= can %s be produced by %s ?\n", Names::to_string(fact).c_str(), Names::to_string(opSig).c_str());
    HashSet<substitution_t, Substitution::Hasher> substitutions;

    // For each such effect: check if it is a valid result
    // of some series of q const substitutions
    for (const Signature& eff : effects) {
        if (eff._usig._name_id != fact._name_id) continue;
        if (eff._negated != negated) continue;
        bool matches = true;
        substitution_t s;
        //log("  %s ?= %s ", Names::to_string(eff).c_str(), Names::to_string(fact).c_str());
        for (int argPos = 0; argPos < eff._usig._args.size(); argPos++) {
            int effArg = eff._usig._args[argPos];
            int substArg = fact._args[argPos];
            if (!_htn->_q_constants.count(effArg)) {
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
            }
        }
        if (matches) {
            // Valid, matching substitution found (possibly empty)
            if (!substitutions.count(s)) substitutions.insert(s);
            //log(" -- yes\n");
        } else {
            //log(" -- no\n");
        }
    }

    return substitutions;
}

bool Instantiator::isFullyGround(const USignature& sig) {
    for (int arg : sig._args) {
        if (_htn->_var_ids.count(arg)) return false;
    }
    return true;
}

std::vector<int> Instantiator::getFreeArgPositions(const std::vector<int>& sigArgs) {
    std::vector<int> argPositions;
    for (int i = 0; i < sigArgs.size(); i++) {
        int arg = sigArgs[i];
        if (_htn->_var_ids.count(arg)) argPositions.push_back(i);
    }
    return argPositions;
}

bool Instantiator::fits(USignature& sig, USignature& groundSig, HashMap<int, int>* substitution) {
    assert(sig._name_id == groundSig._name_id);
    assert(sig._args.size() == groundSig._args.size());
    assert(isFullyGround(groundSig));
    //if (sig._negated != groundSig._negated) return false;
    for (int i = 0; i < sig._args.size(); i++) {
        if (!_htn->_var_ids.count(sig._args[i])) {
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
    const std::vector<int>& taskSorts = _htn->_signature_sorts_table[sig._name_id];
    for (int argPos = 0; argPos < sig._args.size(); argPos++) {
        int sort = taskSorts[argPos];
        int arg = sig._args[argPos];
        if (_htn->_var_ids.count(arg)) continue; // skip variable
        bool valid = false;
        if (_htn->_q_constants.count(arg)) {
            // q constant: TODO check if SOME SUBSTITUTEABLE CONSTANT has the correct sort
            for (int cnst : _htn->getDomainOfQConstant(arg)) {
                if (_htn->getConstantsOfSort(sort).count(cnst)) {
                    valid = true;
                    break;
                }
            }
        } else {
            // normal constant: check if it is contained in the correct sort
            for (int c : _htn->_constants_by_sort[sort]) {
                if (c == arg) valid = true; 
            }
        }
        if (!valid) {
            //log("arg %s not of sort %s => %s invalid\n", Names::to_string(arg).c_str(), Names::to_string(sort).c_str(), Names::to_string(sig).c_str());
            return false;
        } 
    }
    return true;
}

std::vector<TypeConstraint> Instantiator::getQConstantTypeConstraints(const USignature& sig) {

    std::vector<TypeConstraint> constraints;

    const std::vector<int>& taskSorts = _htn->_signature_sorts_table[sig._name_id];
    for (int argPos = 0; argPos < sig._args.size(); argPos++) {
        int sigSort = taskSorts[argPos];
        int arg = sig._args[argPos];
        
        // Not a q-constant here
        if (!_htn->_q_constants.count(arg)) {
            // Must be of valid type
            assert(_htn->getConstantsOfSort(sigSort).count(arg));
            continue;
        }

        // Type is fine no matter which substitution is chosen
        if (_htn->getSortsOfQConstant(arg).count(sigSort)) continue;

        // Type is NOT fine, at least for some substitutions
        std::vector<int> good;
        std::vector<int> bad;
        HashSet<int> validConstants = _htn->getConstantsOfSort(sigSort);
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
    assert(isFullyGround(sig._usig));
    bool positive = !sig._negated;
    
    // Q-Fact: assume that it holds
    if (_htn->hasQConstants(sig._usig)) {
        for (const auto& decSig : _htn->getDecodedObjects(sig._usig)) {
            if (test(Signature(decSig, sig._negated), state)) return true;
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
        if (isFullyGround(pre._usig) && !test(pre, state)) {
            return false;
        }
    }
    return true;
}