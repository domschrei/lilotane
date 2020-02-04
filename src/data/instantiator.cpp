
#include <assert.h>

#include "data/instantiator.h"

std::vector<Reduction> Instantiator::getMinimalApplicableInstantiations(
    Reduction& r, std::unordered_map<int, SigSet> posFacts, std::unordered_map<int, SigSet> negFacts) {

    std::vector<Reduction> reductions = instantiatePreconditions(r, posFacts, negFacts);

    // TODO Investigate subtasks of the reduction

    // The preconditions of each reduction in the "results" set are ground and satisfied.
    // The reductions may still be (partially) lifted.
    // Add the remaining variables as pseudo-constants to the problem
    // and treat the reductions as ground.

    return reductions;
}

std::vector<Reduction> Instantiator::instantiatePreconditions(
    Reduction& r, std::unordered_map<int, SigSet> posFacts, std::unordered_map<int, SigSet> negFacts) {

    std::vector<Reduction> result;

    // Check ground preconditions of the reduction
    const SignatureSet& pre = r.getPreconditions();
    for (Signature sig : pre) {
        if (isFullyGround(sig)) {
            // This precondition must definitely hold
            if (!test(sig, posFacts, negFacts)) {
                // does not hold -- no applicable reduction
                return result;
            } // else: precondition holds, nothing more to do
        }
    }

    // Instantiate a lifted precondition
    for (Signature sig : pre) {
        if (!isFullyGround(sig)) {
            // This precondition must hold relative to its arguments:
            // Are there instantiations in the facts where it holds?

            // Find out which facts of the same predicate hold in the state
            int predId = sig._name_id;
            SigSet c = (sig._negated ? negFacts : posFacts)[predId];

            // TODO: Negative preconditions for which some instantiation 
            // does not occur in posFacts nor in negFacts => need to be instantiated, too!
            // * Get all constants of the respective type(s)
            // * Enumerate
            std::vector<int> sorts = _predicate_sorts_table[sig._name_id];
            std::vector<std::vector<int>> constantsPerArg;
            for (int sort : sorts) {
                constantsPerArg.push_back(_constants_by_sort[sort]);
            }
            std::vector<int> counter(constantsPerArg.size(), 0);
            assignmentLoop : while (true) {
                // Assemble the assignment
                std::vector<int> newArgs(counter.size());
                for (int argPos = 0; argPos < counter.size(); argPos++) {
                    newArgs[argPos] = constantsPerArg[argPos][counter[argPos]];
                }
                Signature sigNew = sig.substitute(substitution(sig._args, newArgs));
                // TODO Try the assignment

                // Increment exponential counter
                int c = 0;
                while (c < counter.size()) {
                    if (counter[c]+1 == constantsPerArg[c].size()) {
                        // max value reached
                        counter[c] = 0;
                        if (c+1 == counter.size()) break assignmentLoop;
                    } else {
                        // increment
                        counter[c]++;
                        break;
                    }
                }
            }

            for (Signature groundSig : c) {
                std::unordered_map<int, int> s;
                if (!fits(sig, groundSig, &s)) continue;

                // Possible partial instantiation
                HtnOp newOp = r.substitute(s);
                Reduction& newRed = (Reduction&) newOp;
                
                // Recursively find all fitting instantiations for remaining preconditions
                std::vector<Reduction> newReductions = instantiatePreconditions(newRed, posFacts, negFacts);
                result.insert(result.end(), newReductions.begin(), newReductions.end());
            }
            // end after 1st successful substitution chain:
            // another recursive call did the remaining substitutions 
            if (!result.empty()) break; 
            // else: no successful substitution for this condition! Failure.
            else return std::vector<Reduction>();
        }
    }

    return result;
}

bool Instantiator::isFullyGround(Signature& sig) {
    for (int arg : sig._args) {
        if (_var_ids.count(arg) > 0) return false;
    }
    return true;
}

std::vector<int> Instantiator::getFreeArgPositions(Signature& sig) {
    std::vector<int> argPositions;
    for (int i = 0; i < sig._args.size(); i++) {
        int arg = sig._args[i];
        if (_var_ids.count(arg) > 0) argPositions.push_back(i);
    }
    return argPositions;
}

bool Instantiator::fits(Signature& sig, Signature& groundSig, std::unordered_map<int, int>* substitution) {
    assert(sig._name_id != groundSig._name_id);
    assert(sig._args.size() == groundSig._args.size());
    for (int i = 0; i < sig._args.size(); i++) {
        if (_var_ids.count(sig._args[i]) == 0) {
            // Constant parameter: must be equal
            if (sig._args[i] != groundSig._args[i]) return false;
        }

        if (substitution != NULL) {
            substitution->at(sig._args[i]) = groundSig._args[i];
        }
    }
    return true;
}

bool Instantiator::test(Signature& sig, std::unordered_map<int, SigSet> posFacts, std::unordered_map<int, SigSet> negFacts) {
    assert(isFullyGround(sig));
    bool positive = !sig._negated;
    if (positive) return posFacts[sig._name_id].count(sig) > 0;
    if (negFacts[sig._name_id].count(sig) > 0) return true;
    return posFacts[sig._name_id].count(sig) == 0;

    // fact positive : true iff contained in posFacts
    // fact negative:
    //      in negFacts : return true
    //              (fact occurred negative)
    //      in posFacts, NOTin negFacts : return false
    //              (fact never occured negative, but positive)
    //      NOTin posFacts, NOTin negFacts : return true !
    //              (fact assumed to be false due to closed-world-asmpt)
}