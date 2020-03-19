
#include "sat/encoding.h"
#include "util/log.h"

/*
encodePosition ()
*/

bool beganLine = false;

Encoding::Encoding(Parameters& params, HtnInstance& htn, std::vector<Layer>& layers) : 
            _params(params), _htn(htn), _layers(&layers), _print_formula(params.isSet("of")) {
    _solver = ipasir_init();
    _sig_primitive = Signature(_htn.getNameId("__PRIMITIVE___"), std::vector<int>());
    _substitute_name_id = _htn.getNameId("__SUBSTITUTE___");
    if (_print_formula) _out.open("formula.cnf");
    VariableDomain::init(params);
    _num_cls = 0;
    _num_lits = 0;
    _num_asmpts = 0;
}

void Encoding::encode(int layerIdx, int pos) {
    
    log("  Encoding ...\n");
    int priorNumClauses = _num_cls;
    int priorNumLits = _num_lits;

    // Calculate relevant environment of the position
    Position NULL_POS;
    NULL_POS.setPos(-1, -1);
    Layer& newLayer = _layers->at(layerIdx);
    Position& newPos = newLayer[pos];
    bool hasLeft = pos > 0;
    Position& left = (hasLeft ? newLayer[pos-1] : NULL_POS);
    int oldPos = 0, offset = 0;
    bool hasAbove = layerIdx > 0;
    if (hasAbove) {
        const Layer& oldLayer = _layers->at(layerIdx-1);
        while (oldPos+1 < oldLayer.size() && oldLayer.getSuccessorPos(oldPos+1) <= pos) 
            oldPos++;
        offset = pos - oldLayer.getSuccessorPos(oldPos);
    }
    Position& above = (hasAbove ? _layers->at(layerIdx-1)[oldPos] : NULL_POS);

    // Important variables for this position
    int varPrim = varPrimitive(layerIdx, pos);
    int prevVarPrim = hasLeft ? varPrimitive(layerIdx, pos-1) : 0;
    
    // Encode true facts at this position and decide for each fact
    // whether to encode it or to reuse the previous variable
    encodeFactVariables(newPos, left);

    // Init substitution vars where necessary
    stage("initsubstitutions");
    for (int qconst : _htn._q_constants) {
        initSubstitutionVars(qconst, newPos);
    }
    stage("initsubstitutions");

    // Link qfacts to their possible decodings
    stage("qfactsemantics");
    for (const auto& pair : newPos.getQFactDecodings()) {
        const Signature& qfactSig = pair.first;
        assert(!_htn.isRigidPredicate(qfactSig._name_id));
        assert(!qfactSig._negated);

        // Already encoded earlier?
        bool qVarReused = newPos.getPriorPosOfVariable(qfactSig) < pos;
        if (qVarReused) continue;
        
        int qfactVar = newPos.getVariable(qfactSig);

        // For each possible fact decoding:
        for (const auto& decFactSig : pair.second) {
            assert(!decFactSig._negated);
            assert(newPos.getFacts().count(decFactSig));

            // Already encoded earlier?
            //if (qVarReused && newPos.getPriorPosOfVariable(decFactSig) < pos) continue;
            
            int decFactVar = newPos.getVariable(decFactSig);

            // Get substitution from qfact to its decoded fact
            substitution_t s = Substitution::get(qfactSig._args, decFactSig._args);
            std::vector<int> substitutionVars;
            for (const auto& sPair : s) {
                Signature sigSubst = sigSubstitute(sPair.first, sPair.second);
                substitutionVars.push_back(varSubstitution(sigSubst));
            }
            
            // If the substitution is chosen,
            // the q-fact and the corresponding actual fact are equivalent
            for (int sign = -1; sign <= 1; sign += 2) {
                for (int varSubst : substitutionVars) {
                    appendClause({-varSubst});
                }
                appendClause({sign*qfactVar, -sign*decFactVar});
                endClause();
            }
        }
    }
    stage("qfactsemantics");

    // Propagate fact assignments from above
    stage("propagatefacts");
    for (const Signature& factSig : newPos.getFacts()) {
        if (_htn.isRigidPredicate(factSig._name_id)) continue;
        
        // Do not propagate fact if a q-fact 
        // TODO include this or not?
        //if (_htn.hasQConstants(factSig)) continue;

        int factVar = newPos.getVariable(factSig);

        if (hasAbove && offset == 0 && above.hasVariable(factSig)) {
            int oldFactVar = above.getVariable(factSig);

            // Find out whether the variables already occurred in an earlier propagation
            if (oldPos > 0) {
                int oldPriorPos = oldPos-1;
                int newPriorPos = _layers->at(layerIdx-1).getSuccessorPos(oldPos-1);
                
                bool oldReused = above.getPriorPosOfVariable(factSig) <= oldPriorPos;
                bool reused = newPos.getPriorPosOfVariable(factSig) <= newPriorPos;

                // Do not re-encode the propagation if both variables have already been reused
                //if (reused) log("NEW_REUSED\n");
                //if (oldReused) log("OLD_REUSED\n");
                if (reused && oldReused) {
                    //log("AVOID_REENCODE\n");
                    continue;
                } 
            }

            // Fact comes from above: propagate meaning
            addClause({-oldFactVar, factVar});
            addClause({oldFactVar, -factVar});
        }
    }
    stage("propagatefacts");

    // Fact supports, frame axioms (only for non-new facts free of q-constants)
    stage("frameaxioms");
    if (pos > 0)
    for (const Signature& fact : newPos.getFacts()) {
        assert(!fact._negated);

        if (_htn.hasQConstants(fact)) continue;
        if (_htn.isRigidPredicate(fact._name_id)) continue;

        bool firstOccurrence = !left.getFacts().count(fact);
        if (firstOccurrence) {
            // First time the fact occurs must be as a "false fact"
            assert(newPos.getTrueFacts().count(fact.opposite()));
            continue;
        }

        // Do not re-encode frame axioms if the fact has a reused variable
        if (newPos.getPriorPosOfVariable(fact) < pos) continue;

        for (int sign = -1; sign <= 1; sign += 2) {
            Signature factSig = fact;
            factSig._negated = sign < 0;
            int oldFactVar = left.getVariable(factSig);
            int factVar = newPos.getVariable(factSig);

            // Calculate indirect support through qfact abstractions
            std::unordered_set<int> indirectSupport;
            if (newPos.getQFactAbstractions().count(factSig.abs())) {
                for (const Signature& sig : newPos.getQFactAbstractions().at(factSig.abs())) {
                    const Signature qfactSig = (factSig._negated ? sig.opposite() : sig);

                    // For each operation that supports some qfact abstraction of the fact:
                    if (newPos.getFactSupports().count(qfactSig))
                    for (const Signature& opSig : newPos.getFactSupports().at(qfactSig)) {
                        int opVar = left.getVariable(opSig);
                        assert(opVar > 0);
                        
                        // Calculate and encode prerequisites for indirect support

                        // Find valid sets of substitutions for this operation causing the desired effect
                        SigSet opSet; opSet.insert(opSig);
                        auto subMap = _htn._instantiator->getOperationSubstitutionsCausingEffect(opSet, factSig);
                        assert(subMap.count(opSig));
                        if (subMap[opSig].empty()) {
                            // No valid instantiations!
                            continue;
                        }

                        // Assemble possible substitution options to get the desired fact support
                        std::set<std::set<int>> substOptions;
                        bool unconditionalEffect = false;
                        for (const substitution_t& s : subMap[opSig]) {
                            if (s.empty()) {
                                // Empty substitution does the job
                                unconditionalEffect = true;
                                break;
                            }
                            // An actual substitution is necessary
                            std::set<int> substOpt;
                            for (const std::pair<int,int>& entry : s) {
                                int substVar = varSubstitution(sigSubstitute(entry.first, entry.second));
                                substOpt.insert(substVar);
                            }
                            substOptions.insert(substOpt);
                        }

                        if (!unconditionalEffect) {
                            // Bring the found substitution sets to CNF and encode them
                            std::vector<std::vector<int>> dnfSubs;
                            for (const auto& set : substOptions) {
                                std::vector<int> vec;
                                vec.insert(vec.end(), set.begin(), set.end());
                                dnfSubs.push_back(vec);
                            }
                            std::set<std::set<int>> cnfSubs = getCnf(dnfSubs);
                            for (const std::set<int>& subsCls : cnfSubs) {
                                // IF fact change AND the operation is applied,
                                if (oldFactVar != 0) appendClause({oldFactVar});
                                #ifndef NONPRIMITIVE_SUPPORT
                                appendClause({-prevVarPrim});
                                #endif
                                appendClause({-factVar, -opVar});
                                //log("FRAME AXIOMS %i %i %i ", oldFactVar, -factVar, -opVar);
                                // THEN either of the valid substitution combinations
                                for (int subVar : subsCls) {
                                    appendClause({subVar});
                                    //log("%i ", subVar);  
                                } 
                                endClause();
                                //log("\n");
                            }
                        }

                        // Add operation to indirect support
                        indirectSupport.insert(opVar); 
                    }
                }
            }

            // Fact change:
            //log("FRAME AXIOMS %i %i ", oldFactVar, -factVar);
            if (oldFactVar != 0) appendClause({oldFactVar});
            appendClause({-factVar});
            #ifndef NONPRIMITIVE_SUPPORT
            // Non-primitiveness wildcard
            appendClause({-prevVarPrim});
            #endif
            // DIRECT support
            if (newPos.getFactSupports().count(factSig)) {
                for (const Signature& opSig : newPos.getFactSupports().at(factSig)) {
                    int opVar = left.getVariable(opSig);
                    assert(opVar > 0);
                    appendClause({opVar});
                    //log("%i ", opVar);
                }
            }
            // INDIRECT support
            for (int opVar : indirectSupport) {
                appendClause({opVar});
                //log("%i ", opVar);
            }
            endClause();
            //log("\n");
        }
    }
    stage("frameaxioms");

    // Effects of "old" actions to the left
    stage("actioneffects");
    for (const Signature& aSig : left.getActions()) {
        if (aSig == Position::NONE_SIG) continue;
        int aVar = left.encode(aSig);

        for (const Signature& eff : _htn._actions_by_sig[aSig].getEffects()) {
            
            // Check that the action is contained in the effect's support
            assert(!newPos.getFactSupports().empty());
            assert(newPos.getFactSupports().count(eff));
            assert(newPos.getFactSupports().at(eff).count(aSig));
            // Predicate must not be rigid
            assert(!_htn.isRigidPredicate(eff._name_id));

            addClause({-aVar, newPos.encode(eff)});
        }
    }
    stage("actioneffects");

    // New actions
    stage("actionconstraints");
    int numOccurringOps = 0;
    for (const Signature& aSig : newPos.getActions()) {
        if (aSig == Position::NONE_SIG) continue;

        numOccurringOps++;
        int aVar = newPos.encode(aSig);
        //printVar(layerIdx, pos, aSig);

        // If the action occurs, the position is primitive
        addClause({-aVar, varPrim});

        // Preconditions
        for (const Signature& pre : _htn._actions_by_sig[aSig].getPreconditions()) {
            assert(!_htn.isRigidPredicate(pre._name_id));
            addClause({-aVar, newPos.encode(pre)});
        }

        // At-most-one action
        for (const Signature& otherSig : newPos.getActions()) {
            int otherVar = newPos.encode(otherSig);
            if (aVar < otherVar) {
                addClause({-aVar, -otherVar});
            }
        }
    }
    stage("actionconstraints");

    // reductions
    stage("reductionconstraints");
    for (const Signature& rSig : newPos.getReductions()) {
        if (rSig == Position::NONE_SIG) continue;

        numOccurringOps++;
        int rVar = newPos.encode(rSig);

        bool trivialReduction = _htn._reductions_by_sig[rSig].getSubtasks().size() == 0;
        if (trivialReduction) {
            // If a trivial reduction occurs, the position is primitive
            addClause({-rVar, varPrim});

            // Add At-most-one constraints to "proper" actions
            for (const Signature& otherSig : newPos.getActions()) {
                int otherVar = newPos.encode(otherSig);
                addClause({-rVar, -otherVar});
            }
        } else {
            // If a non-trivial reduction occurs, the position is non-primitive
            addClause({-rVar, -varPrim});
        }

        // Preconditions
        for (const Signature& pre : _htn._reductions_by_sig[rSig].getPreconditions()) {
            assert(newPos.getFacts().count(pre.abs()));
            assert(!_htn.isRigidPredicate(pre._name_id));
            addClause({-rVar, newPos.encode(pre)});
        }

        // At-most-one reduction
        if (!_params.isSet("aamo")) continue;
        for (const Signature& otherSig : newPos.getReductions()) {
            if (otherSig == Position::NONE_SIG) continue;
            int otherVar = newPos.encode(otherSig);
            if (rVar < otherVar) {
                addClause({-rVar, -otherVar});
            }
        }
    }
    stage("reductionconstraints");
    
    if (numOccurringOps == 0) {
        assert(pos+1 == newLayer.size() 
            || fail("No operations to encode at (" + std::to_string(layerIdx) + "," + std::to_string(pos) + ")!\n"));
    }

    // Q-constants type constraints
    stage("qtypeconstraints");
    const auto& constraints = newPos.getQConstantsTypeConstraints();
    for (const auto& pair : constraints) {
        const Signature& opSig = pair.first;
        if (isEncoded(layerIdx, pos, opSig)) {
            for (const TypeConstraint& c : pair.second) {
                int qconst = c.qconstant;
                bool positiveConstraint = c.sign;
                assert(_q_constants.count(qconst));

                if (positiveConstraint) {
                    // EITHER of the GOOD constants - one big clause
                    appendClause({-newPos.getVariable(opSig)});
                    for (int cnst : c.constants) {
                        appendClause({varSubstitution(sigSubstitute(qconst, cnst))});
                    }
                    endClause();
                } else {
                    // NEITHER of the BAD constants - many 2-clauses
                    for (int cnst : c.constants) {
                        addClause({-newPos.getVariable(opSig), -varSubstitution(sigSubstitute(qconst, cnst))});
                    }
                }
            }
        }
    }
    stage("qtypeconstraints");

    // Forbidden substitutions per operator
    stage("forbiddensubstitutions");
    for (const auto& opPair : newPos.getForbiddenSubstitutions()) {
        const Signature& opSig = opPair.first;
        for (const substitution_t& s : opPair.second) {
            appendClause({-newPos.getVariable(opSig)});
            for (const auto& entry : s) {
                appendClause({-varSubstitution(sigSubstitute(entry.first, entry.second))});
            }
            endClause();
        }
    }
    stage("forbiddensubstitutions");

    // Forbid impossible parent ops
    stage("forbiddenparents");
    for (const auto& pair : newPos.getExpansions()) {
        const Signature& parent = pair.first;
        for (const Signature& child : pair.second) {
            if (child == Position::NONE_SIG) {
                // Forbid parent op that turned out to be impossible
                int oldOpVar = above.getVariable(parent);
                addClause({-oldOpVar});
                break;
            }
        }
    }
    stage("forbiddenparents");

    // expansions
    stage("expansions");
    for (const auto& pair : newPos.getExpansions()) {
        const Signature& parent = pair.first;

        appendClause({-above.getVariable(parent)});
        for (const Signature& child : pair.second) {
            if (child == Position::NONE_SIG) continue;
            appendClause({newPos.getVariable(child)});
        }
        endClause();
    }
    stage("expansions");

    // choice of axiomatic ops
    stage("axiomaticops");
    const SigSet& axiomaticOps = newPos.getAxiomaticOps();
    if (!axiomaticOps.empty()) {
        for (const Signature& op : axiomaticOps) {
            appendClause({newPos.getVariable(op)});
        }
        endClause();
    }
    stage("axiomaticops");

    log("  Encoding done. (%i clauses, total of %i literals)\n", (_num_cls-priorNumClauses), (_num_lits-priorNumLits));

    if (layerIdx > 1 && pos == 0) {
        // delete data from "above above" layer
        // except for necessary structures for decoding a plan
        Layer& layerToClear = _layers->at(layerIdx-2);
        log("  Freeing memory of (%i,{%i..%i}) ...\n", layerIdx-2, 0, layerToClear.size());
        for (int p = 0; p < layerToClear.size(); p++) {
            layerToClear[p].clearUnneeded();
        }
    }
}

void Encoding::addAssumptions(int layerIdx) {
    Layer& l = _layers->at(layerIdx);
    for (int pos = 0; pos < l.size(); pos++) {
        assume(varPrimitive(layerIdx, pos));
    }
}

void Encoding::encodeFactVariables(Position& newPos, const Position& left) {

    // Facts that must hold at this position
    stage("truefacts");
    for (const Signature& factSig : newPos.getTrueFacts()) {
        if (_htn.isRigidPredicate(factSig._name_id)) continue;
        int factVar = newPos.encode(factSig);
        addClause({factVar});
    }
    stage("truefacts");

    // Re-use all other fact variables where possible
    int reused = 0;

    // a) first check normal facts
    std::unordered_set<int> newFactVars;
    for (const Signature& factSig : newPos.getFacts()) {
        if (_htn.hasQConstants(factSig)) continue;
        
        // Fact must be contained to the left
        bool reuse = left.getFacts().count(factSig);
        // Fact must not have any support to change
        if (reuse) reuse = !newPos.getFactSupports().count(factSig) 
                && !newPos.getFactSupports().count(factSig.opposite());

        // None of its linked q-facts must have a support
        if (reuse && newPos.getQFactAbstractions().count(factSig)) {
            for (const Signature& qSig : newPos.getQFactAbstractions().at(factSig)) {
                if (reuse) reuse = !newPos.getFactSupports().count(qSig) 
                        && !newPos.getFactSupports().count(qSig.opposite());
            }
        }

        if (reuse) {
            newPos.setVariable(factSig, left.getVariable(factSig), left.getPriorPosOfVariable(factSig));
            //log("REUSE %s (%i,%i) ~> (%i,%i)\n", Names::to_string(factSig).c_str(), layerIdx, pos-1, layerIdx, pos);
            reused++;
        } else {
            newFactVars.insert(newPos.encode(factSig));
        }
    }

    // b) now check q-facts
    for (const Signature& factSig : newPos.getFacts()) {
        if (!_htn.hasQConstants(factSig)) continue;
        
        // Fact must be contained to the left
        bool reuse = left.getFacts().count(factSig);
        // Fact must not have any support to change
        if (reuse) reuse = !newPos.getFactSupports().count(factSig) 
                && !newPos.getFactSupports().count(factSig.opposite());

        // Possible fact decodings must be exactly the same
        // TODO expensive check!
        if (reuse) reuse = newPos.getQFactDecodings().count(factSig);
        if (reuse) reuse = left.getQFactDecodings().count(factSig);
        if (reuse) reuse = newPos.getQFactDecodings().at(factSig) == left.getQFactDecodings().at(factSig);

        // None of the decoded facts may have changed
        if (reuse) {
            for (const Signature& decSig : newPos.getQFactDecodings().at(factSig)) {
                // Decoded fact must have been reused from previous position
                if (reuse) reuse &= !newFactVars.count(newPos.getVariable(decSig));
            }
        }

        if (reuse) {
            newPos.setVariable(factSig, left.getVariable(factSig), left.getPriorPosOfVariable(factSig));
            //log("  REUSE %s\n", Names::to_string(factSig).c_str());
            reused++;
        } else {
            //log("  NO_REUSE %s\n", Names::to_string(factSig).c_str());
            newPos.encode(factSig);
        }
    }
    log("   %.2f%% of fact variables reused from previous position\n", ((float)100*reused/newPos.getFacts().size()));
}

void Encoding::initSubstitutionVars(int arg, Position& pos) {

    if (_q_constants.count(arg)) return;
    if (!_htn._q_constants.count(arg)) return;
    // arg is a *new* q-constant: initialize substitution logic

    _q_constants.insert(arg);

    std::vector<int> substitutionVars;
    for (int c : _htn.getDomainOfQConstant(arg)) {

        assert(!_htn._var_ids.count(c));

        // either of the possible substitutions must be chosen
        Signature sigSubst = sigSubstitute(arg, c);
        int varSubst = varSubstitution(sigSubst);
        substitutionVars.push_back(varSubst);
    }
    assert(!substitutionVars.empty());

    // AT LEAST ONE substitution
    for (int vSub : substitutionVars) appendClause({vSub});
    endClause();

    // AT MOST ONE substitution
    for (int vSub1 : substitutionVars) {
        for (int vSub2 : substitutionVars) {
            if (vSub1 < vSub2) addClause({-vSub1, -vSub2});
        }
    } 
}

std::set<std::set<int>> Encoding::getCnf(const std::vector<std::vector<int>>& dnf) {
    std::set<std::set<int>> cnf;

    if (dnf.empty()) return cnf;

    int size = 1;
    for (const auto& lits : dnf) {
        size *= lits.size();
    }
    assert(size > 0);

    // Iterate over all possible combinations
    std::vector<int> counter(dnf.size(), 0);
    while (true) {
        // Assemble the combination
        std::set<int> newCls;
        for (int pos = 0; pos < counter.size(); pos++) {
            
            assert(pos < dnf.size());
            assert(counter[pos] < dnf[pos].size());

            newCls.insert(dnf[pos][counter[pos]]);
        }
        cnf.insert(newCls);            

        // Increment exponential counter
        int x = 0;
        while (x < counter.size()) {
            if (counter[x]+1 == dnf[x].size()) {
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

    if (cnf.size() > 1000) log("CNF of size %i generated\n", cnf.size());

    return cnf;
}

void Encoding::addClause(const std::initializer_list<int>& lits) {
    //log("CNF ");
    for (int lit : lits) {
        ipasir_add(_solver, lit);
        if (_print_formula) _out << lit << " ";
        //log("%i ", lit);
    } 
    ipasir_add(_solver, 0);
    if (_print_formula) _out << "0\n";
    //log("0\n");

    _num_cls++;
    _num_lits += lits.size();
}
void Encoding::appendClause(const std::initializer_list<int>& lits) {
    if (!beganLine) {
        //log("CNF ");
        beganLine = true;
    }
    for (int lit : lits) {
        ipasir_add(_solver, lit);
        if (_print_formula) _out << lit << " ";
        //log("%i ", lit);
    } 

    _num_lits += lits.size();
}
void Encoding::endClause() {
    assert(beganLine);
    ipasir_add(_solver, 0);
    if (_print_formula) _out << "0\n";
    //log("0\n");
    beganLine = false;

    _num_cls++;
}
void Encoding::assume(int lit) {
    if (_num_asmpts == 0) _last_assumptions.clear();
    ipasir_assume(_solver, lit);
    //log("CNF !%i\n", lit);
    _last_assumptions.push_back(lit);
    _num_asmpts++;
}

bool Encoding::solve() {
    log("Attempting to solve formula with %i clauses (%i literals) and %i assumptions\n", 
                _num_cls, _num_lits, _num_asmpts);
    bool solved = ipasir_solve(_solver) == 10;
    if (_num_asmpts == 0) _last_assumptions.clear();
    _num_asmpts = 0;
    return solved;
}

bool Encoding::isEncoded(int layer, int pos, const Signature& sig) {
    return _layers->at(layer)[pos].hasVariable(sig);
}

bool Encoding::isEncodedSubstitution(const Signature& sig) {
    bool neg = sig._negated;
    sig._negated = false;
    bool encoded = _substitution_variables.count(sig);
    sig._negated = neg;
    return encoded;
}

int Encoding::varSubstitution(const Signature& sigSubst) {
    bool neg = sigSubst._negated;
    sigSubst._negated = false;

    int var;
    if (!_substitution_variables.count(sigSubst)) {
        assert(!VariableDomain::isLocked() || fail("Unknown substitution variable " 
                    + Names::to_string(sigSubst) + " queried!\n"));
        var = VariableDomain::nextVar();
        _substitution_variables[sigSubst] = var;
        VariableDomain::printVar(var, -1, -1, sigSubst);
    } else var = _substitution_variables[sigSubst];

    sigSubst._negated = neg;
    return (neg ? -1 : 1) * _substitution_variables[sigSubst];
}

std::string Encoding::varName(int layer, int pos, const Signature& sig) {
    return VariableDomain::varName(layer, pos, sig);
}

void Encoding::printVar(int layer, int pos, const Signature& sig) {
    log("%s\n", VariableDomain::varName(layer, pos, sig).c_str());
}

int Encoding::varPrimitive(int layer, int pos) {
    return _layers->at(layer)[pos].encode(_sig_primitive);
}

void Encoding::printFailedVars(Layer& layer) {
    log("FAILED ");
    for (int pos = 0; pos < layer.size(); pos++) {
        int v = varPrimitive(layer.index(), pos);
        if (ipasir_failed(_solver, v)) log("%i ", v);
    }
    log("\n");
}

std::vector<PlanItem> Encoding::extractClassicalPlan() {

    Layer& finalLayer = _layers->back();
    int li = finalLayer.index();
    VariableDomain::lock();

    /*
    State state = finalLayer[0].getState();
    for (const auto& pair : state) {
        for (const Signature& fact : pair.second) {
            if (_htn.isRigidPredicate(fact._name_id)) assert(!isEncoded(0, 0, fact));
            else if (!fact._negated) assert((isEncoded(0, 0, fact) && value(0, 0, fact)) || fail(Names::to_string(fact) + " does not hold initially!\n"));
            else if (fact._negated) assert(!isEncoded(0, 0, fact) || value(0, 0, fact) || fail(Names::to_string(fact) + " does not hold initially!\n"));
        } 
    }*/

    std::vector<PlanItem> plan;
    //log("(actions at layer %i)\n", li);
    for (int pos = 0; pos < finalLayer.size(); pos++) {
        //log("%i\n", pos);
        assert(value(li, pos, _sig_primitive) || fail("Position " + std::to_string(pos) + " is not primitive!\n"));

        int chosenActions = 0;
        //State newState = state;
        SigSet effects;
        for (const Signature& aSig : finalLayer[pos].getActions()) {
            
            if (!isEncoded(li, pos, aSig)) continue;
            //log("  %s ?\n", Names::to_string(aSig).c_str());

            if (value(li, pos, aSig)) {
                chosenActions++;
                int aVar = finalLayer[pos].getVariable(aSig);

                // Check fact consistency
                //checkAndApply(_htn._actions_by_sig[aSig], state, newState, li, pos);

                //if (aSig == _htn._action_blank.getSignature()) continue;

                // Decode q constants
                Action& a = _htn._actions_by_sig[aSig];
                Signature aDec = getDecodedQOp(li, pos, aSig);
                if (aDec != aSig) {

                    HtnOp opDecoded = a.substitute(Substitution::get(a.getArguments(), aDec._args));
                    Action aDecoded = (Action) opDecoded;

                    // Check fact consistency w.r.t. "actual" decoded action
                    //checkAndApply(aDecoded, state, newState, li, pos);
                }

                //log("* %s @ %i\n", Names::to_string(aDec).c_str(), pos);
                plan.push_back({aVar, aDec, aDec, std::vector<int>()});
            }
        }

        //for (Signature sig : newState) {
        //    assert(value(li, pos+1, sig));
        //}
        //state = newState;

        assert(chosenActions <= 1 || fail("Added " + std::to_string(chosenActions) + " actions at step " + std::to_string(pos) + "!\n"));
        if (chosenActions == 0) {
            plan.emplace_back(-1, Signature(), Signature(), std::vector<int>());
        }
    }

    //log("%i actions at final layer of size %i\n", plan.size(), _layers->back().size());
    return plan;
}

bool holds(State& state, const Signature& fact) {

    // Positive fact
    if (!fact._negated) return state[fact._name_id].count(fact);
    
    // Negative fact: fact is contained, OR counterpart is NOT contained
    return state[fact._name_id].count(fact) || !state[fact._name_id].count(fact.opposite());
}

void Encoding::checkAndApply(const Action& a, State& state, State& newState, int layer, int pos) {
    //log("%s\n", Names::to_string(a).c_str());
    for (const Signature& pre : a.getPreconditions()) {

        // Check assignment
        if (!_htn.isRigidPredicate(pre._name_id))
            assert((isEncoded(layer, pos, pre) && value(layer, pos, pre)) 
            || fail("Precondition " + Names::to_string(pre) + " of action "
        + Names::to_string(a) + " does not hold in assignment at step " + std::to_string(pos) + "!\n"));

        // Check state
        assert(_htn.hasQConstants(pre) || holds(state, pre) || fail("Precondition " + Names::to_string(pre) + " of action "
            + Names::to_string(a) + " does not hold in inferred state at step " + std::to_string(pos) + "!\n"));
        
        //log("Pre %s of action %s holds @(%i,%i)\n", Names::to_string(pre).c_str(), Names::to_string(a.getSignature()).c_str(), 
        //        layer, pos);
    }

    for (const Signature& eff : a.getEffects()) {
        assert((isEncoded(layer, pos+1, eff) && value(layer, pos+1, eff)) 
            || fail("Effect " + Names::to_string(eff) + " of action "
        + Names::to_string(a) + " does not hold in assignment at step " + std::to_string(pos+1) + "!\n"));

        // Apply effect
        if (holds(state, eff.opposite())) newState[eff._name_id].erase(eff.opposite());
        newState[eff._name_id];
        newState[eff._name_id].insert(eff);

        //log("Eff %s of action %s holds @(%i,%i)\n", Names::to_string(eff).c_str(), Names::to_string(a.getSignature()).c_str(), 
        //        layer, pos);
    }
}

std::pair<std::vector<PlanItem>, std::vector<PlanItem>> Encoding::extractPlan() {

    auto result = std::pair<std::vector<PlanItem>, std::vector<PlanItem>>();
    std::vector<PlanItem>& classicalPlan = result.first;
    std::vector<PlanItem>& plan = result.second;

    result.first = extractClassicalPlan();
    
    std::vector<PlanItem> itemsOldLayer, itemsNewLayer;

    for (int layerIdx = 0; layerIdx < _layers->size(); layerIdx++) {
        Layer& l = _layers->at(layerIdx);
        //log("(decomps at layer %i)\n", l.index());

        itemsNewLayer.resize(l.size());
        
        for (int pos = 0; pos < l.size(); pos++) {

            int predPos = 0;
            if (layerIdx > 0) {
                Layer& lastLayer = _layers->at(layerIdx-1);
                while (predPos+1 < lastLayer.size() && lastLayer.getSuccessorPos(predPos+1) <= pos) 
                    predPos++;
            } 
            //log("%i -> %i\n", predPos, pos);

            int actionsThisPos = 0;
            int reductionsThisPos = 0;

            for (const Signature& rSig : l[pos].getReductions()) {
                if (!isEncoded(layerIdx, pos, rSig) || rSig == Position::NONE_SIG) continue;

                //log("? %s @ (%i,%i)\n", Names::to_string(rSig).c_str(), i, pos);

                if (value(layerIdx, pos, rSig)) {

                    int v = _layers->at(layerIdx)[pos].getVariable(rSig);
                    const Reduction& r = _htn._reductions_by_sig[rSig];

                    // Check preconditions
                    /*
                    for (const Signature& pre : r.getPreconditions()) {
                        assert(value(layerIdx, pos, pre) || fail("Precondition " + Names::to_string(pre) + " of reduction "
                        + Names::to_string(r.getSignature()) + " does not hold at step " + std::to_string(pos) + "!\n"));
                    }*/

                    //log("%s:%s @ (%i,%i)\n", Names::to_string(r.getTaskSignature()).c_str(), Names::to_string(rSig).c_str(), layerIdx, pos);
                    Signature decRSig = getDecodedQOp(layerIdx, pos, rSig);
                    Reduction rDecoded = r.substituteRed(Substitution::get(r.getArguments(), decRSig._args));
                    log("[%i] %s:%s @ (%i,%i)\n", v, Names::to_string(rDecoded.getTaskSignature()).c_str(), Names::to_string(decRSig).c_str(), layerIdx, pos);

                    if (layerIdx == 0) {
                        // Initial reduction
                        PlanItem root(0, 
                            Signature(_htn.getNameId("root"), std::vector<int>()), 
                            decRSig, std::vector<int>());
                        itemsNewLayer[0] = root;
                        reductionsThisPos++;
                        continue;
                    }

                    // Lookup parent reduction
                    Reduction parentRed;
                    int offset = pos - _layers->at(layerIdx-1).getSuccessorPos(predPos);
                    PlanItem& parent = itemsOldLayer[predPos];
                    assert(parent.id >= 0 || fail("No parent at " + std::to_string(layerIdx-1) + "," + std::to_string(predPos) + "!\n"));
                    assert(_htn._reductions.count(parent.reduction._name_id) || 
                        fail("Invalid reduction id=" + std::to_string(parent.reduction._name_id) + " at " + std::to_string(layerIdx-1) + "," + std::to_string(predPos) + "\n"));

                    parentRed = _htn._reductions[parent.reduction._name_id];
                    parentRed = parentRed.substituteRed(Substitution::get(parentRed.getArguments(), parent.reduction._args));

                    // Is the current reduction a proper subtask?
                    assert(offset < parentRed.getSubtasks().size());
                    if (parentRed.getSubtasks()[offset] == rDecoded.getTaskSignature()) {
                        if (itemsOldLayer[predPos].subtaskIds.size() > offset) {
                            // This subtask has already been written!
                            log(" -- is a redundant child -> dismiss\n");
                            continue;
                        }
                        itemsNewLayer[pos] = PlanItem(v, rDecoded.getTaskSignature(), decRSig, std::vector<int>());
                        itemsOldLayer[predPos].subtaskIds.push_back(v);
                        reductionsThisPos++;
                    } else {
                        log(" -- invalid : %s != %s\n", Names::to_string(parentRed.getSubtasks()[offset]).c_str(), Names::to_string(rDecoded.getTaskSignature()).c_str());
                    } 
                }
            }

            for (const Signature& aSig : l[pos].getActions()) {
                if (!isEncoded(layerIdx, pos, aSig)) continue;

                if (value(layerIdx, pos, aSig)) {
                    actionsThisPos++;

                    if (aSig == _htn._action_blank.getSignature()) continue;
                    
                    int v = _layers->at(layerIdx)[pos].getVariable(aSig);
                    Action a = _htn._actions_by_sig[aSig];

                    /*
                    // Check preconditions, effects
                    for (const Signature& pre : a.getPreconditions()) {
                        assert(value(layerIdx, pos, pre) || fail("Precondition " + Names::to_string(pre) + " of action "
                        + Names::to_string(aSig) + " does not hold at step " + std::to_string(pos) + "!\n"));
                    }
                    for (const Signature& eff : a.getEffects()) {
                        assert(value(layerIdx, pos+1, eff) || fail("Effect " + Names::to_string(eff) + " of action "
                        + Names::to_string(aSig) + " does not hold at step " + std::to_string(pos+1) + "!\n"));
                    }*/

                    // TODO check this is a valid subtask relationship

                    log("[%i] %s @ (%i,%i)\n", v, Names::to_string(aSig).c_str(), layerIdx, pos);                    

                    // Find the actual action variable at the final layer, not at this (inner) layer
                    int l = layerIdx;
                    int aPos = pos;
                    while (l+1 < _layers->size()) {
                        //log("(%i,%i) => ", l, aPos);
                        aPos = _layers->at(l).getSuccessorPos(aPos);
                        l++;
                        //log("(%i,%i)\n", l, aPos);
                    }
                    v = classicalPlan[aPos].id; // _layers->at(l-1)[aPos].getVariable(aSig);
                    assert(v > 0);

                    //itemsNewLayer[pos] = PlanItem({v, aSig, aSig, std::vector<int>()});
                    if (layerIdx > 0) itemsOldLayer[predPos].subtaskIds.push_back(v);
                }
            }

            // At least an item per position 
            assert( (actionsThisPos+reductionsThisPos >= 1)
            || fail(std::to_string(actionsThisPos+reductionsThisPos) 
                + " ops at (" + std::to_string(layerIdx) + "," + std::to_string(pos) + ") !\n"));
            
            // At most one action per position
            assert(actionsThisPos <= 1 || fail(std::to_string(actionsThisPos) 
                + " actions at (" + std::to_string(layerIdx) + "," + std::to_string(pos) + ") !\n"));

            // Either actions OR reductions per position (not both)
            assert(actionsThisPos == 0 || reductionsThisPos == 0 || fail(std::to_string(actionsThisPos) 
                + " actions and " + std::to_string(reductionsThisPos) + " reductions at (" 
                + std::to_string(layerIdx) + "," + std::to_string(pos) + ") !\n"));
        }

        plan.insert(plan.end(), itemsOldLayer.begin(), itemsOldLayer.end());

        itemsOldLayer = itemsNewLayer;
        itemsNewLayer.clear();
    }
    plan.insert(plan.end(), itemsOldLayer.begin(), itemsOldLayer.end());

    return result;
}

bool Encoding::value(int layer, int pos, const Signature& sig) {
    int v = _layers->at(layer)[pos].getVariable(sig);
    int vAbs = std::abs(v);
    return (v < 0) ^ (ipasir_val(_solver, vAbs) > 0);
}

void Encoding::printSatisfyingAssignment() {
    log("SOLUTION_VALS ");
    for (int v = 1; v <= VariableDomain::getMaxVar(); v++) {
        log("%i ", ipasir_val(_solver, v));
    }
    log("\n");
}

Signature Encoding::getDecodedQOp(int layer, int pos, const Signature& origSig) {
    assert(isEncoded(layer, pos, origSig));
    assert(value(layer, pos, origSig));

    Signature sig = origSig;
    while (true) {
        bool containsQConstants = false;
        for (int arg : sig._args) if (_htn._q_constants.count(arg)) {
            // q constant found
            containsQConstants = true;

            int numSubstitutions = 0;
            for (int argSubst : _htn.getDomainOfQConstant(arg)) {
                Signature sigSubst = sigSubstitute(arg, argSubst);
                if (isEncodedSubstitution(sigSubst) && ipasir_val(_solver, varSubstitution(sigSubst)) > 0) {
                    //log("%i TRUE\n", varSubstitution(sigSubst));
                    //log("%s/%s => %s ~~> ", Names::to_string(arg).c_str(), 
                    //        Names::to_string(argSubst).c_str(), Names::to_string(sig).c_str());
                    numSubstitutions++;
                    substitution_t sub;
                    sub[arg] = argSubst;
                    sig = sig.substitute(sub);
                    //log("%s\n", Names::to_string(sig).c_str());
                } else {
                    //log("%i FALSE\n", varSubstitution(sigSubst));
                }
            }

            assert(numSubstitutions == 1);
        }

        if (!containsQConstants) break; // done
    }

    /*
    if (origSig != sig)
        log("%s ~~> %s\n", Names::to_string(origSig).c_str(), Names::to_string(sig).c_str());
    */
    return sig;
}

void Encoding::stage(std::string name) {
    if (!_num_cls_per_stage.count(name)) {
        _num_cls_per_stage[name] = _num_cls;
    } else {
        if (!_total_num_cls_per_stage.count(name)) _total_num_cls_per_stage[name] = 0;
        _total_num_cls_per_stage[name] += _num_cls - _num_cls_per_stage[name];
        _num_cls_per_stage.erase(name);
    }
}

void Encoding::printStages() {
    log("Total amount of clauses encoded: %i\n", _num_cls);
    for (const auto& entry : _total_num_cls_per_stage) {
        log(" %s : %i cls\n", entry.first.c_str(), entry.second);
    }
    _total_num_cls_per_stage.clear();
}

Encoding::~Encoding() {

    if (_params.isSet("of")) {

        // Append assumptions to written formula, close stream
        for (int asmpt : _last_assumptions) {
            _out << asmpt << " 0\n";
        }
        _out.flush();
        _out.close();

        // Create final formula file
        std::ofstream ffile;
        ffile.open("f.cnf");
        
        // Append header to formula file
        ffile << "p cnf " << VariableDomain::getMaxVar() << " " << (_num_cls+_last_assumptions.size()) << "\n";

        // Append main content to formula file (reading from "old" file)
        std::ifstream oldfile;
        oldfile.open("formula.cnf");
        std::string line;
        while (std::getline(oldfile, line)) {
            line = line + "\n";
            ffile.write(line.c_str(), line.size());
        }
        oldfile.close();
        remove("formula.cnf");

        // Finish
        ffile.flush();
        ffile.close();
    }

    // Release SAT solver
    ipasir_release(_solver);
}