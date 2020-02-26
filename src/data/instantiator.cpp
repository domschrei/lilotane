
#include <assert.h>
#include <set>

#include "data/instantiator.h"
#include "util/names.h"
#include "data/htn_instance.h"
#include "data/arg_iterator.h"

std::vector<Reduction> Instantiator::getApplicableInstantiations(
    Reduction& r, std::unordered_map<int, SigSet> facts, int mode) {

    int oldMode = _inst_mode;
    if (mode >= 0) _inst_mode = mode;

    std::vector<Reduction> result;

    SigSet inst = instantiate(r, facts);
    for (const Signature& sig : inst) {
        //log("%s\n", Names::to_string(sig).c_str());
        result.push_back(r.substituteRed(Substitution::get(r.getArguments(), sig._args)));
    }

    _inst_mode = oldMode;

    return result;
}

std::vector<Action> Instantiator::getApplicableInstantiations(
    Action& a, std::unordered_map<int, SigSet> facts, int mode) {

    int oldMode = _inst_mode;
    if (mode >= 0) _inst_mode = mode;

    std::vector<Action> result;

    SigSet inst = instantiate(a, facts);
    for (const Signature& sig : inst) {
        //log("%s\n", Names::to_string(sig).c_str());
        assert(isFullyGround(sig));
        HtnOp newOp = a.substitute(Substitution::get(a.getArguments(), sig._args));
        result.push_back((Action) newOp);
    }

    _inst_mode = oldMode;

    return result;
}

bool Instantiator::hasSomeInstantiation(const Signature& sig) {

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
            for (int preArg : pre._args) {
                if (preArg == arg) r++;
            } 
        }
        for (const Signature& eff : __op->getEffects()) {
            for (int effArg : eff._args) {
                if (effArg == arg) r++;
            } 
        }
        return r;
    }
};

SigSet Instantiator::instantiate(const HtnOp& op, const std::unordered_map<int, SigSet>& facts) {
    __op = &op;

    if (!hasConsistentlyTypedArgs(op.getSignature())) return SigSet();

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
    std::unordered_map<int, int> argPosBackMapping;
    for (int j = 0; j < argsByPriority.size(); j++) {
        for (int i = 0; i < op.getArguments().size(); i++) {
            if (op.getArguments()[i] == argsByPriority[j]) {
                argPosBackMapping[j] = i;
                break;
            }
        }   
    }

    SigSet instantiation;
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
        if (hasValidPreconditions(op, facts) 
            && hasSomeInstantiation(op.getSignature())) 
            instantiation.insert(op.getSignature());
        return instantiation;
    }

    std::vector<std::vector<int>> assignmentsStack;
    assignmentsStack.push_back(std::vector<int>());
    while (!assignmentsStack.empty()) {
        const std::vector<int> assignment = assignmentsStack.back();
        assignmentsStack.pop_back();

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
            if (!hasValidPreconditions(newOp, facts)) continue;

            // All ok -- add to stack
            if (newAssignment.size() == doneInstSize) {
                // If there are remaining variables: 
                // is there some valid constant for each of them?
                if (!hasSomeInstantiation(newOp.getSignature())) continue;

                assert(hasConsistentlyTypedArgs(newOp.getSignature()));

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
std::unordered_map<Signature, std::unordered_set<substitution_t, Substitution::Hasher>, SignatureHasher> 
Instantiator::getOperationSubstitutionsCausingEffect(
    const std::unordered_set<Signature, SignatureHasher>& operations, const Signature& fact) {

    std::unordered_map<Signature, std::unordered_set<substitution_t, Substitution::Hasher>, SignatureHasher> result;

    // For each provided HtnOp:
    for (Signature opSig : operations) {
        //log("?= can %s be produced by %s ?\n", Names::to_string(fact).c_str(), Names::to_string(opSig).c_str());
        std::unordered_set<substitution_t, Substitution::Hasher> substitutions;

        // Decode it into a q constant free representation
        //for (Signature decOpSig : _htn->getDecodedObjects(opSig)) {
        assert(isFullyGround(opSig));

        // Collect its (possible) effects
        SigSet effects = _htn->getAllFactChanges(opSig);

        // For each such effect: check if it is a valid result
        // of some series of q const substitutions
        for (Signature eff : effects) {
            if (eff._name_id != fact._name_id) continue;
            if (eff._negated != fact._negated) continue;
            bool matches = true;
            substitution_t s;
            //log("  %s ?= %s ", Names::to_string(eff).c_str(), Names::to_string(fact).c_str());
            for (int argPos = 0; argPos < eff._args.size(); argPos++) {
                int effArg = eff._args[argPos];
                int substArg = fact._args[argPos];
                if (!_htn->_q_constants.count(effArg)) {
                    // If the effect fact has no q const here, the arg must be left unchanged
                    matches &= effArg == substArg;
                } else {
                    // If the effect fact has a q const here, the substituted arg must be in the q const's domain
                    matches &= _htn->_domains_of_q_constants[effArg].count(substArg);
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

bool Instantiator::isFullyGround(const Signature& sig) {
    for (int arg : sig._args) {
        if (_htn->_var_ids.count(arg)) return false;
    }
    return true;
}

std::vector<int> Instantiator::getFreeArgPositions(const Signature& sig) {
    std::vector<int> argPositions;
    for (int i = 0; i < sig._args.size(); i++) {
        int arg = sig._args[i];
        if (_htn->_var_ids.count(arg)) argPositions.push_back(i);
    }
    return argPositions;
}

bool Instantiator::fits(Signature& sig, Signature& groundSig, std::unordered_map<int, int>* substitution) {
    assert(sig._name_id == groundSig._name_id);
    assert(sig._args.size() == groundSig._args.size());
    assert(isFullyGround(groundSig));
    if (sig._negated != groundSig._negated) return false;
    for (int i = 0; i < sig._args.size(); i++) {
        if (!_htn->_var_ids.count(sig._args[i])) {
            // Constant parameter: must be equal
            if (sig._args[i] != groundSig._args[i]) return false;
        }

        if (substitution != NULL) {
            assert(!substitution->count(sig._args[i]));
            substitution->insert(std::pair<int, int>(sig._args[i], groundSig._args[i]));
        }
    }
    return true;
}

bool Instantiator::hasConsistentlyTypedArgs(const Signature& sig) {
    const std::vector<int>& taskSorts = _htn->_signature_sorts_table[sig._name_id];
    for (int argPos = 0; argPos < sig._args.size(); argPos++) {
        int sort = taskSorts[argPos];
        int arg = sig._args[argPos];
        if (_htn->_var_ids.count(arg)) continue; // skip variable
        bool valid = false;
        if (_htn->_q_constants.count(arg)) {
            // q constant: check if it has the correct sort
            for (int qsort : _htn->_sorts_of_q_constants[arg]) {
                if (qsort == sort) valid = true;
            }
        } else {
            // normal constant: check if it is contained in the correct sort
            for (int c : _htn->_constants_by_sort[sort]) {
                if (c == arg) valid = true; 
            }
        }
        if (!valid) return false;
    }
    return true;
}

bool Instantiator::test(const Signature& sig, std::unordered_map<int, SigSet> facts) {
    assert(isFullyGround(sig));
    bool positive = !sig._negated;
    
    if (!facts.count(sig._name_id)) {
        // Never saw such a predicate: cond. holds iff it is negative
        return !positive;
    }

    // Q-Fact: assume that it holds
    if (_htn->hasQConstants(sig)) return true;

    // fact positive : true iff contained in facts
    if (positive) return facts[sig._name_id].count(sig);
    
    // fact negative.

    // if contained in facts : return true
    //   (fact occurred negative)
    if (facts[sig._name_id].count(sig)) return true;
    
    // else: return true iff fact does NOT occur in positive form
    return !facts[sig._name_id].count(sig.opposite());
}

bool Instantiator::hasValidPreconditions(const HtnOp& op, std::unordered_map<int, SigSet> facts) {

    for (const Signature& pre : op.getPreconditions()) {
        if (isFullyGround(pre) && !test(pre, facts)) {
            return false;
        }
    }
    return true;
}