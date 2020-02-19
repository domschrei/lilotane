
#include <assert.h>

#include "data/instantiator.h"
#include "util/names.h"
#include "data/htn_instance.h"
#include "data/arg_iterator.h"

std::vector<Reduction> Instantiator::getMinimalApplicableInstantiations(
    Reduction& r, std::unordered_map<int, SigSet> facts) {

    std::vector<Reduction> reductions = instantiatePreconditions<Reduction>(r, facts);

    // TODO Investigate subtasks of the reduction?

    return reductions;
}

std::vector<Reduction> Instantiator::getFullApplicableInstantiations(
    Reduction& r, std::unordered_map<int, SigSet> facts) {

    std::vector<Reduction> result;

    std::vector<Reduction> reductions = instantiatePreconditions<Reduction>(r, facts);
    for (const Reduction& r : reductions) {
        std::vector<Signature> inst = ArgIterator::getFullInstantiation(r.getSignature(), *_htn);
        for (Signature sig : inst) {
            Reduction red = r.substituteRed(Substitution::get(r.getArguments(), sig._args));
            result.push_back(red);
        }
    }
    return result;
}

std::vector<Action> Instantiator::getMinimalApplicableInstantiations(
    Action& a, std::unordered_map<int, SigSet> facts) {

    return instantiatePreconditions<Action>(a, facts);
}

std::vector<Action> Instantiator::getFullApplicableInstantiations(
    Action& a, std::unordered_map<int, SigSet> facts) {

    std::vector<Action> result;

    std::vector<Action> actions = instantiatePreconditions<Action>(a, facts);
    for (const Action& a : actions) {
        std::vector<Signature> inst = ArgIterator::getFullInstantiation(a.getSignature(), *_htn);
        for (Signature sig : inst) {
            HtnOp op = a.substitute(Substitution::get(a.getArguments(), sig._args));
            result.push_back((Action) op);
        }
    }
    return result;
}



template<class T>
std::vector<T> Instantiator::instantiatePreconditions(T& r, std::unordered_map<int, SigSet> facts) {

    std::vector<T> result;
    HtnOp* op = static_cast<HtnOp*>(&r);

    // Check ground preconditions of the reduction
    const SigSet& pre = op->getPreconditions();
    //printf("    %i preconditions\n", pre.size());
    int numFreePreconds = 0;
    for (Signature sig : pre) {
        if (isFullyGround(sig)) {
            // This precondition must definitely hold
            if (!test(sig, facts)) {
                // does not hold -- no applicable reduction
                //printf("%s does not hold!\n", Names::to_string(sig).c_str());
                return result;
            } // else: precondition holds, nothing more to do
        } else numFreePreconds++;
    }

    // Fully ground preconditions -- finished, a single instantiation
    if (numFreePreconds == 0) {
        result.push_back(r);
        return result;
    }

    // Instantiate a lifted precondition
    for (Signature sig : pre) {
        //printf(" pre : %s\n", Names::to_string(sig).c_str());

        if (isFullyGround(sig)) continue;

        // This precondition must hold relative to its arguments:
        // Are there instantiations in the facts where it holds?

        // Find out which facts of the same predicate hold in the state
        int predId = sig._name_id;
        SigSet c = facts.count(predId) ? facts[predId] : SigSet();

        if (sig._negated) {
            // Negative preconditions for which some instantiation 
            // does not occur in posFacts nor in negFacts => need to be instantiated, too!
            
            // Get all constants of the respective type(s)
            std::vector<Signature> inst = ArgIterator::getFullInstantiation(sig, *_htn);
            for (Signature sigNew : inst) {
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
            }
        }

        // For each holding literal in the state, try an instantiation
        for (Signature groundSig : c) {

            // do not consider q facts for instantiation
            if (_htn->hasQConstants(groundSig)) continue;
            
            //printf("  ~> %s\n", Names::to_string(groundSig).c_str());

            std::unordered_map<int, int> s;
            if (!fits(sig, groundSig, &s)) continue;

            // Possible partial instantiation
            //printf("     %s\n", Names::to_string(s).c_str());

            if (std::is_same<Reduction, T>::value) {
                // Reduction
                Reduction newRed = dynamic_cast<Reduction*>(op)->substituteRed(s);
                
                // Recursively find all fitting instantiations for remaining preconditions
                std::vector<Reduction> newOps = instantiatePreconditions(newRed, facts);
                for (Reduction r : newOps) {
                    result.push_back(static_cast<T>(r));
                }
            } else if (std::is_same<Action, T>::value) {
                // Action
                HtnOp o = op->substitute(s);
                Action newA = Action(o);
                
                //printf("%s :: %s -> %s\n", Names::to_string(groundSig).c_str(), Names::to_string(op->getSignature()).c_str(), Names::to_string(newA.getSignature()).c_str());

                std::vector<Action> newOps = instantiatePreconditions(newA, facts);
                //printf("%s : %i\n", Names::to_string(groundSig).c_str(), newOps.size());
                for (Action a : newOps) {
                    Signature aSig = a.getSignature();
                    //printf("  - %s\n", Names::to_string(aSig).c_str());
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

    return result;
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
        //printf("?= can %s be produced by %s ?\n", Names::to_string(fact).c_str(), Names::to_string(opSig).c_str());
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
            //printf("  %s ?= %s ", Names::to_string(eff).c_str(), Names::to_string(fact).c_str());
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
                //printf(" -- yes\n");
            } else {
                //printf(" -- no\n");
            }
        }

        result[opSig] = substitutions;
    }
    return result;
}

bool Instantiator::isFullyGround(Signature& sig) {
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
    if (sig._negated != groundSig._negated) return false;
    for (int i = 0; i < sig._args.size(); i++) {
        if (_htn->_var_ids.count(sig._args[i]) == 0) {
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

    for (int arg : sig._args) {
        if (_htn->_q_constants.count(arg)) {
            // Q-Fact: assume that it holds
            return true;
        }
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