
#include <assert.h>

#include "data/instantiator.h"
#include "util/names.h"

std::vector<Reduction> Instantiator::getMinimalApplicableInstantiations(
    Reduction& r, std::unordered_map<int, SigSet> facts) {

    std::vector<Reduction> reductions = instantiatePreconditions<Reduction>(r, facts);

    // TODO Investigate subtasks of the reduction

    // The preconditions of each reduction in the "results" set are ground and satisfied.
    // The reductions may still be (partially) lifted.
    // Add the remaining variables as pseudo-constants to the problem
    // and treat the reductions as ground.

    return reductions;
}

std::vector<Action> Instantiator::getApplicableInstantiations(
    Action& a, std::unordered_map<int, SigSet> facts) {

    std::vector<Action> actions = instantiatePreconditions<Action>(a, facts);
    return actions;
}

template<class T>
std::vector<T> Instantiator::instantiatePreconditions(T& r, std::unordered_map<int, SigSet> facts) {

    std::vector<T> result;
    HtnOp* op = static_cast<HtnOp*>(&r);

    // Check ground preconditions of the reduction
    const SigSet& pre = op->getPreconditions();
    printf("    %i preconditions\n", pre.size());
    for (Signature sig : pre) {
        if (isFullyGround(sig)) {
            // This precondition must definitely hold
            if (!test(sig, facts)) {
                // does not hold -- no applicable reduction
                return result;
            } // else: precondition holds, nothing more to do
        }
    }

    // Instantiate a lifted precondition
    for (Signature sig : pre) {
        printf(" pre : %s\n", Names::to_string(sig).c_str());

        if (!isFullyGround(sig)) {
            // This precondition must hold relative to its arguments:
            // Are there instantiations in the facts where it holds?

            // Find out which facts of the same predicate hold in the state
            int predId = sig._name_id;
            SigSet c = facts.count(predId) > 0 ? facts[predId] : SigSet();

            if (sig._negated) {
                // Negative preconditions for which some instantiation 
                // does not occur in posFacts nor in negFacts => need to be instantiated, too!
                
                // Get all constants of the respective type(s)
                assert(_predicate_sorts_table.count(sig._name_id) > 0);
                std::vector<int> sorts = _predicate_sorts_table[sig._name_id];
                std::vector<std::vector<int>> constantsPerArg;
                for (int sort : sorts) {
                    assert(_constants_by_sort.count(sort) > 0);
                    constantsPerArg.push_back(_constants_by_sort[sort]);
                }

                // Iterate over all possible assignments
                std::vector<int> counter(constantsPerArg.size(), 0);
                while (true) {
                    // Assemble the assignment
                    std::vector<int> newArgs(counter.size());
                    for (int argPos = 0; argPos < counter.size(); argPos++) {
                        
                        assert(argPos < constantsPerArg.size());
                        assert(counter[argPos] < constantsPerArg[argPos].size());

                        newArgs[argPos] = constantsPerArg[argPos][counter[argPos]];
                    }
                    Signature sigNew = sig.substitute(substitution(sig._args, newArgs));
                    
                    // Try the assignment
                    if (c.count(sigNew) == 0) {
                        sigNew.negate();
                        if (c.count(sigNew) == 0) {
                            // Occurs neither positive nor negative
                            sigNew.negate();
                            c.insert(sigNew);
                        } else {
                            sigNew.negate();
                        }
                    }

                    // Increment exponential counter
                    int x = 0;
                    while (x < counter.size()) {
                        if (counter[x]+1 == constantsPerArg[x].size()) {
                            // max value reached
                            counter[x] = 0;
                            if (x+1 == counter.size()) break;
                        } else {
                            // increment
                            counter[x]++;
                            break;
                        }
                        x++;
                    }
                    // Counter finished?
                    if (counter[x] == 0 && x+1 == counter.size()) break;
                }
            }

            // For each holding literal in the state, try an instantiation
            for (Signature groundSig : c) {
                printf("  ~> %s\n", Names::to_string(groundSig).c_str());

                std::unordered_map<int, int> s;
                if (!fits(sig, groundSig, &s)) continue;

                // Possible partial instantiation
                printf("     %s\n", Names::to_string(s).c_str());

                if (is_base_of<Reduction, T>()) {
                    // Reduction
                    Reduction newRed = dynamic_cast<Reduction*>(op)->substituteRed(s);
                    
                    // Recursively find all fitting instantiations for remaining preconditions
                    std::vector<Reduction> newOps = instantiatePreconditions(newRed, facts);
                    for (Reduction r : newOps) {
                        result.push_back(static_case<T>(r));
                    }
                } else {
                    // Action
                    HtnOp o = op->substitute(s);
                    Action newA = Action(o);
                    std::vector<Action> newOps = instantiatePreconditions(newA, facts);
                    for (Action a : newOps) {
                        result.push_back(static_cast<T>(a));
                    }
                }

            }
            // end after 1st successful substitution chain:
            // another recursive call did the remaining substitutions 
            if (!result.empty()) break; 
            // else: no successful substitution for this condition! Failure.
            else return std::vector<T>();
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
    assert(sig._name_id == groundSig._name_id);
    assert(sig._args.size() == groundSig._args.size());
    for (int i = 0; i < sig._args.size(); i++) {
        if (_var_ids.count(sig._args[i]) == 0) {
            // Constant parameter: must be equal
            if (sig._args[i] != groundSig._args[i]) return false;
        }

        if (substitution != NULL) {
            substitution->insert(std::pair<int, int>(sig._args[i], groundSig._args[i]));
        }
    }
    return true;
}

bool Instantiator::test(Signature& sig, std::unordered_map<int, SigSet> facts) {
    assert(isFullyGround(sig));
    bool positive = !sig._negated;
    
    if (facts.count(sig._name_id) == 0) {
        // Never saw such a predicate: cond. holds iff it is negative
        return !positive;
    }

    // fact positive : true iff contained in facts
    if (positive) return facts[sig._name_id].count(sig) > 0;
    
    // fact negative:
    //      if contained in facts : return true
    //              (fact occurred negative)
    if (facts[sig._name_id].count(sig) > 0) return true;
    
    // in posFacts, NOTin negFacts : return false
    //         (fact never occured negative, but positive)
    // NOTin posFacts, NOTin negFacts : return true !
    //         (fact assumed to be false due to closed-world-asmpt)
    sig.negate();
    bool contained = facts[sig._name_id].count(sig) == 0;
    sig.negate();
    return contained;
    
}