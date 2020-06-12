
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

    //if (!hasConsistentlyTypedArgs(op.getSignature())) return SigSet();

    // Create structure for arguments ordered by priority
    std::vector<int> argsByPriority;
    for (int arg : op.getArguments()) {
        if (_htn->_var_ids.count(arg)) argsByPriority.push_back(arg);
    }

    int priorSize = argsByPriority.size();
    CompArgs comp;
    std::sort(argsByPriority.begin(), argsByPriority.end(), comp);
    assert(priorSize == argsByPriority.size());

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

    USigSet instantiation;
    int doneInstSize = argsByPriority.size(); // ALL
    if (_inst_mode == INSTANTIATE_NOTHING) doneInstSize = 0;
    if (_inst_mode == INSTANTIATE_PRECONDITIONS) {
        // Search the position where the rating becomes zero
        // (meaning that the args occur in no conditions)
        int lastRating = 999999;
        for (int i = 0; i < argsByPriority.size(); i++) {
            int rating = comp.rating(argsByPriority[i]);
            assert(lastRating >= rating);
            lastRating = rating;

            if (rating == 0) {
                doneInstSize = i;
                break;
            }
        }
        //log("Instantiate until arg %i/%i\n", doneInstSize, argsByPriority.size());
    }
    
    if (doneInstSize == 0 || argsByPriority.empty()) {
        if (hasValidPreconditions(op, state) 
            && hasSomeInstantiation(op.getSignature())) 
            instantiation.insert(op.getSignature());
        return instantiation;
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
            if (!hasValidPreconditions(newOp, state)) continue;

            // All ok -- add to stack
            if (newAssignment.size() == doneInstSize) {
                // If there are remaining variables: 
                // is there some valid constant for each of them?
                if (!hasSomeInstantiation(newOp.getSignature())) continue;

                // This instantiation is finished:
                // Assemble instantiated signature
                instantiation.insert(newOp.getSignature());
            } else {
                // Unfinished instantiation
                assignmentsStack.push_back(newAssignment);
            }
        }
    }

    __op = NULL;

    return instantiation;
}







// Given a fact (signature) and (a set of ground htn operations containing q constants),
// compute the possible sets of substitutions that are necessary to let each operation
// have the specified fact in its support.
HashMap<USignature, HashSet<substitution_t, Substitution::Hasher>, USignatureHasher> 
Instantiator::getOperationSubstitutionsCausingEffect(
    const HashSet<USignature, USignatureHasher>& operations, const USignature& fact, bool negated) {

    HashMap<USignature, HashSet<substitution_t, Substitution::Hasher>, USignatureHasher> result;

    // For each provided HtnOp:
    for (const USignature& opSig : operations) {
        //log("?= can %s be produced by %s ?\n", Names::to_string(fact).c_str(), Names::to_string(opSig).c_str());
        HashSet<substitution_t, Substitution::Hasher> substitutions;

        assert(isFullyGround(opSig));

        // Collect its (possible) effects
        SigSet effects = _htn->getAllFactChanges(opSig);

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

        result[opSig] = substitutions;
    }
    return result;
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
    if (_htn->hasQConstants(sig._usig)) return true;

    // fact positive : true iff contained in facts
    if (positive) return state(sig);
    
    // fact negative.

    // if contained in facts : return true
    //   (fact occurred negative)
    if (state(sig)) return true;
    
    // else: return true iff fact does NOT occur in positive form
    return !state(sig.opposite());
}

bool Instantiator::hasValidPreconditions(const HtnOp& op, const std::function<bool(const Signature&)>& state) {

    for (const Signature& pre : op.getPreconditions()) {
        if (isFullyGround(pre._usig) && !test(pre, state)) {
            return false;
        }
    }
    return true;
}