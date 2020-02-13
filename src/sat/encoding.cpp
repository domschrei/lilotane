
#include "sat/encoding.h"
#include "util/log.h"

/*
encodePosition ()
*/

Encoding::Encoding(HtnInstance& htn, std::vector<Layer>& layers) : _htn(htn), 
            _layers(&layers) {
    _solver = ipasir_init();
    _sig_primitive = Signature(_htn.getNameId("__PRIMITIVE___"), std::vector<int>());
    _substitute_name_id = _htn.getNameId("__SUBSTITUTE___");
    _out.open("formula.cnf");
}

void Encoding::encode(int layerIdx, int pos) {
    
    printf("[ENC] position (%i,%i) ...\n", layerIdx, pos);

    // Calculate relevant environment of the position
    Position NULL_POS;
    NULL_POS.setPos(-1, -1);
    Layer& newLayer = _layers->at(layerIdx);
    Position& newPos = newLayer[pos];
    bool hasLeft = pos > 0;
    Position& left = (hasLeft ? newLayer[pos-1] : NULL_POS);
    int oldPos = 0, offset;
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
    
    // Facts that must hold at this position
    for (auto pair : newPos.getTrueFacts()) {
        const Signature& factSig = pair.first;
        int factVar = newPos.encode(factSig);
        addClause({factVar});
    }

    // General facts thay may hold
    std::unordered_map<int, std::unordered_set<int>> factSupport;
    std::unordered_set<int> newFacts;
    std::unordered_set<int> factsWithoutChange;
    std::unordered_map<int, std::unordered_set<int>> substitutionVarsPerFactVar;
    for (auto pair : newPos.getFacts()) {
        const Signature& factSig = pair.first;
        int factVar = newPos.encode(factSig);

        bool newFact = true;
        bool factChange = false;

        for (Reason why : pair.second) {

            if (why.axiomatic) continue;

            if (why.getOriginPos() == left.getPos()) {
                // Fact comes from the left
                newFact = false;

                if (why.sig == factSig) {
                    assert(!why.sig._negated);
                    factChange = true;

                    // frame axioms
                    //printVar(left.getPos().first, left.getPos().second, factSig);
                    int oldFactVar = left.getVariable(factSig);
                    factSupport[factVar];
                    factSupport[factVar].insert(-oldFactVar);
                    factSupport[-factVar];
                    factSupport[-factVar].insert(oldFactVar);

                } else if (_htn._actions_by_sig.count(why.sig.abs())) {

                    int aVar = left.getVariable(why.sig.abs());
                    int polarity = why.sig._negated ? -1 : 1;
                    
                    // action: effect
                    addClause({-aVar, polarity*factVar});

                    // fact support
                    factSupport[-polarity * factVar];
                    factSupport[-polarity * factVar].insert(aVar);

                } else if (_htn._reductions_by_sig.count(why.sig.abs())) {
                    
                    // reduction: possible fact change
                    int rVar = left.getVariable(why.sig.abs());
                    int polarity = why.sig._negated ? -1 : 1;
                    factSupport[-polarity * factVar];
                    factSupport[-polarity * factVar].insert(rVar);
                } else abort();

            } else if (why.getOriginPos() == above.getPos()) {
                // Fact comes from above: propagate meaning
                newFact = false;

                assert(offset == 0);
                assert(why.sig == factSig);

                int oldFactVar = above.getVariable(why.sig);
                addClause({-oldFactVar, factVar});
                addClause({oldFactVar, -factVar});

            } else if (why.getOriginPos() == newPos.getPos()) {
                // Fact comes from this position
                
                if (_htn._actions_by_sig.count(why.sig.abs())) {
                    
                    // action: precondition
                    int aVar = newPos.encode(why.sig.abs());
                    int polarity = why.sig._negated ? -1 : 1;
                    addClause({-aVar, polarity*factVar});

                } else if (_htn._reductions_by_sig.count(why.sig.abs())) {

                    // reduction: precondition
                    int rVar = newPos.encode(why.sig.abs());
                    int polarity = why.sig._negated ? -1 : 1;
                    addClause({-rVar, polarity*factVar});

                } else {
                    // a q-fact is the reason
                    assert(_htn.hasQConstants(why.sig));
                }
            } else abort();
        }

        if (newFact) {
            if (!_htn.hasQConstants(factSig))
                newFacts.insert(std::abs(factVar));
        }
        if (!factChange) {
            factsWithoutChange.insert(std::abs(factVar));
        }
        if (_htn.hasQConstants(factSig)) {
            // Q-Fact: 
                
            // initialize substitution logic where necessary
            for (int argPos = 0; argPos < factSig._args.size(); argPos++) {
                int arg = factSig._args[argPos];
                if (!_htn._q_constants.count(arg)) continue;
                
                // arg is a q-constant

                // if arg is a *new* q-constant: initialize substitution logic
                if (!_q_constants.count(arg)) {
                    _q_constants.insert(arg);

                    std::vector<int> substitutionVars;
                    for (int c : _htn._domains_of_q_constants[arg]) {

                        // either of the possible substitutions must be chosen
                        Signature sigSubst = sigSubstitute(arg, c);
                        int varSubst = varSubstitution(sigSubst);
                        substitutionVars.push_back(varSubst);
                        
                        _q_constants_per_arg[c];
                        _q_constants_per_arg[c].push_back(arg);
                    }

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
            }

            // Add equivalence to corresponding facts
            // relative to chosen substitution
            for (auto factsPair : newPos.getFacts()) {
                const Signature& substitutedFact = factsPair.first;

                if (substitutedFact._name_id != factSig._name_id) continue;

                // Check if the pair of facts form a valid substitution
                bool matches = true;
                bool differs = false;
                std::vector<int> substitutionVars;
                for (int i = 0; i < substitutedFact._args.size(); i++) {
                    int f = factSig._args[i];
                    int fSub = substitutedFact._args[i];

                    // If original arg is not a q constant, the substituted arg must be the same
                    if (!_htn._q_constants.count(f)) matches = (f == fSub);

                    // If original arg IS a q-constant, 
                    // the subst. arg is the same q-constant OR in the original q-constant's domain
                    if (_htn._q_constants.count(f)) {
                        matches = f == fSub || _htn._domains_of_q_constants[f].count(fSub);
                        if (!matches) break;
                        if (f != fSub) {
                            Signature sigSubst = sigSubstitute(f, fSub);
                            substitutionVars.push_back(varSubstitution(sigSubst));
                        }
                    } 

                    if (f != fSub) differs = true;
                    if (!matches) break;
                }
                if (!differs || !matches) continue;
                
                // Valid substitution found
                int varQFact = newPos.encode(factSig);
                int varSubstitutedFact = newPos.encode(substitutedFact);
                substitutionVarsPerFactVar[varSubstitutedFact];
                
                // If the substitution is chosen,
                // the q-fact and the corresponding actual fact are equivalent
                for (int sign = -1; sign <= 1; sign += 2) {
                    for (int varSubst : substitutionVars) {
                        appendClause({-varSubst});
                        substitutionVarsPerFactVar[varSubstitutedFact].insert(varSubst);
                    }
                    appendClause({sign*varQFact, -sign*varSubstitutedFact});
                    endClause();
                }
            }
        }
    }

    // apply fact supports, or initialize "new facts" to false
    for (auto pair : factSupport) {
        int factVar = pair.first;
        
        if (newFacts.count(std::abs(factVar))) {
            // New fact: initialize to false
            addClause({-std::abs(factVar)});
        } else if (factsWithoutChange.count(std::abs(factVar))) {
            // No fact change to encode
            continue;
        } else {
            // Normal fact change: encode support
            appendClause({factVar});
            for (int var : pair.second) {
                appendClause({var});
            }
            // If the fact is a possible substitution of a q-constant fact,
            // add corresponding substitution literal to frame axioms.
            for (int substitutionVar : substitutionVarsPerFactVar[factVar]) {
                appendClause({substitutionVar});
            }
            endClause();

            // TODO instead:
            // * basic frame axiom only contains fact change + all supporting ops
            // * additional clause for each (primitive?) supporting op:
            //   if fact change AND a certain substitution, then 
            //    (either of the ops whose according substitution actually causes the fact change)
            // would it work the other way round, too?
        }
    }

    std::unordered_map<int, std::vector<int>> expansions;
    std::vector<int> axiomaticOps;

    // actions
    int numOccurringOps = 0;
    for (auto pair : newPos.getActions()) {
        numOccurringOps++;
        const Signature& aSig = pair.first;
        int aVar = newPos.encode(aSig);
        if (pos == 0) printf(" POS0 "); printVar(layerIdx, pos, aSig);
        addClause({-aVar, varPrim});

        for (Reason why : pair.second) {
            assert(!why.sig._negated);

            if (why.axiomatic) {
                axiomaticOps.push_back(aVar);
            } else if (why.getOriginPos() == above.getPos()) {
                // Action is result of a propagation or expansion
                // from the layer above
                int oldOpVar = above.getVariable(why.sig.abs());
                expansions[oldOpVar];
                expansions[oldOpVar].push_back(aVar);
            } else abort();
        }

        // At-most-one action
        for (auto otherPair : newPos.getActions()) {
            const Signature& otherSig = otherPair.first;
            int otherVar = newPos.encode(otherSig);
            if (aVar < otherVar) {
                addClause({-aVar, -otherVar});
            }
        }
    }

    // reductions
    for (auto pair : newPos.getReductions()) {
        numOccurringOps++;
        const Signature& rSig = pair.first;
        int rVar = newPos.encode(rSig);
        if (pos == 0) printf(" POS0 "); printVar(layerIdx, pos, rSig);

        addClause({-rVar, -varPrim});

        // "Virtual children" forbidding parent reductions
        if (rSig == Position::NONE_SIG) {
            for (Reason why : pair.second) {
                int oldRVar = above.getVariable(why.sig.abs());
                if (pos == 0) printf(" POS0 ");
                addClause({-oldRVar});
            }
            continue;
        }

        // Actual children
        for (Reason why : pair.second) {
            assert(!why.sig._negated);
            if (why.axiomatic) {
                // axiomatic reduction
                axiomaticOps.push_back(rVar);
            } else if (why.getOriginPos() == above.getPos()) {
                // expansion
                int oldRVar = above.getVariable(why.sig.abs());
                expansions[oldRVar];
                expansions[oldRVar].push_back(rVar);
            } else abort();
        }

        // At-most-one reduction
        for (auto otherPair : newPos.getReductions()) {
            const Signature& otherSig = otherPair.first;
            if (otherSig == Position::NONE_SIG) continue;
            int otherVar = newPos.encode(otherSig);
            if (rVar < otherVar) {
                addClause({-rVar, -otherVar});
            }
        }
    }

    if (numOccurringOps == 0) {
        assert(pos+1 == newLayer.size() 
            || fail("No operations to encode at (" + std::to_string(layerIdx) + "," + std::to_string(pos) + ")!\n"));
    }

    // expansions
    for (auto pair : expansions) {
        int parent = pair.first;
        if (pos == 0) printf(" POS0 ");
        appendClause({-parent});
        for (int child : pair.second) {
            appendClause({child});
        }
        endClause();
    }
    // choice of axiomatic ops
    if (!axiomaticOps.empty()) {
        if (pos == 0) printf(" POS0 ");
        for (int var : axiomaticOps) {
            appendClause({var});
        }
        endClause();
    }

    // assume primitiveness
    assume(varPrim);

    printf("[ENC] position (%i,%i) done.\n", layerIdx, pos);
}

int numLits = 0;
int numClauses = 0;
int numAssumptions = 0;
bool beganLine = false;

void Encoding::addClause(std::initializer_list<int> lits) {
    printf("CNF ");
    for (int lit : lits) {
        ipasir_add(_solver, lit);
        _out << lit << " ";
        printf("%i ", lit);
    } 
    ipasir_add(_solver, 0);
    _out << "0\n";
    printf("0\n");

    numClauses++;
    numLits += lits.size();
}
void Encoding::appendClause(std::initializer_list<int> lits) {
    if (!beganLine) {
        printf("CNF ");
        beganLine = true;
    }
    for (int lit : lits) {
        ipasir_add(_solver, lit);
        _out << lit << " ";
        printf("%i ", lit);
    } 

    numLits += lits.size();
}
void Encoding::endClause() {
    assert(beganLine);
    ipasir_add(_solver, 0);
    _out << "0\n";
    printf("0\n");
    beganLine = false;

    numClauses++;
}
void Encoding::assume(int lit) {
    if (numAssumptions == 0) _last_assumptions.clear();
    ipasir_assume(_solver, lit);
    printf("CNF !%i\n", lit);
    _last_assumptions.push_back(lit);
    numAssumptions++;
}

bool Encoding::solve() {
    printf("Attempting to solve formula with %i clauses (%i literals) and %i assumptions\n", 
                numClauses, numLits, numAssumptions);
    numAssumptions = 0;
    return ipasir_solve(_solver) == 10;
}

bool Encoding::isEncoded(int layer, int pos, const Signature& sig) {
    return _layers->at(layer)[pos].hasVariable(sig);
}

bool Encoding::isEncodedSubstitution(Signature& sig) {
    return _substitution_variables.count(sig.abs());
}

int Encoding::varSubstitution(Signature sigSubst) {
    bool neg = sigSubst._negated;
    Signature sigAbs = neg ? sigSubst.abs() : sigSubst;
    if (!_substitution_variables.count(sigSubst)) {
        assert(!VariableDomain::isLocked() || fail("Unknown substitution variable " 
                    + Names::to_string(sigSubst) + " queried!\n"));
        _substitution_variables[sigAbs] = VariableDomain::nextVar();
        printf("VARMAP %i %s\n", _substitution_variables[sigAbs], Names::to_string(sigSubst).c_str());
    }
    return _substitution_variables[sigAbs];
}

std::string Encoding::varName(int layer, int pos, const Signature& sig) {
    return _layers->at(layer)[pos].varName(sig);
}

void Encoding::printVar(int layer, int pos, const Signature& sig) {
    printf("%s\n", varName(layer, pos, sig).c_str());
}

int Encoding::varPrimitive(int layer, int pos) {
    return _layers->at(layer)[pos].encode(_sig_primitive);
}

void Encoding::printFailedVars(Layer& layer) {
    printf("FAILED ");
    for (int pos = 0; pos < layer.size(); pos++) {
        int v = varPrimitive(layer.index(), pos);
        if (ipasir_failed(_solver, v)) printf("%i ", v);
    }
    printf("\n");
}

std::vector<PlanItem> Encoding::extractClassicalPlan() {

    Layer& finalLayer = _layers->back();
    int li = finalLayer.index();
    VariableDomain::lock();

    CausalSigSet state = finalLayer[0].getFacts();
    /*
    for (Signature f : state) {
        if (isEncoded(0, 0, f)) assert(value(0, 0, f));
    }*/

    std::vector<PlanItem> plan;
    printf("(actions at layer %i)\n", li);
    for (int pos = 0; pos+1 < finalLayer.size(); pos++) {
        //printf("%i\n", pos);
        assert(value(li, pos, _sig_primitive) || fail("Position " + std::to_string(pos) + " is not primitive!\n"));

        int chosenActions = 0;
        CausalSigSet newState = state;
        SigSet effects;
        for (auto pair : finalLayer[pos].getActions()) {
            const Signature& aSig = pair.first;

            if (!isEncoded(li, pos, aSig)) continue;
            //printf("  %s ?\n", Names::to_string(aSig).c_str());

            if (value(li, pos, aSig)) {
                chosenActions++;
                int aVar = finalLayer[pos].getVariable(aSig);

                // Check fact consistency
                checkAndApply(_htn._actions_by_sig[aSig], state, newState, li, pos);

                if (aSig == _htn._action_blank.getSignature()) continue;

                // Decode q constants
                Action& a = _htn._actions_by_sig[aSig];
                Signature aDec = getDecodedQOp(li, pos, aSig);
                HtnOp opDecoded = a.substitute(Substitution::get(a.getArguments(), aDec._args));
                Action aDecoded = (Action) opDecoded;

                // Check fact consistency w.r.t. "actual" decoded action
                checkAndApply(aDecoded, state, newState, li, pos);

                printf("* %s @ %i\n", Names::to_string(aDec).c_str(), pos);
                plan.push_back({aVar, aDec, aDec, std::vector<int>()});
            }
        }

        //for (Signature sig : newState) {
        //    assert(value(li, pos+1, sig));
        //}
        state = newState;

        assert(chosenActions == 1 || fail("Added " + std::to_string(chosenActions) + " actions at step " + std::to_string(pos) + "!\n"));
    }

    printf("%i actions at final layer of size %i\n", plan.size(), _layers->back().size());
    return plan;
}

void Encoding::checkAndApply(Action& a, CausalSigSet& state, CausalSigSet& newState, int layer, int pos) {
    //printf("%s\n", Names::to_string(a).c_str());
    for (Signature pre : a.getPreconditions()) {
        assert(value(layer, pos, pre) || fail("Precondition " + Names::to_string(pre) + " of action "
        + Names::to_string(a) + " does not hold at step " + std::to_string(pos) + "!\n"));
    }
    for (Signature eff : a.getEffects()) {
        assert(value(layer, pos+1, eff) || fail("Effect " + Names::to_string(eff) + " of action "
        + Names::to_string(a) + " does not hold at step " + std::to_string(pos+1) + "!\n"));

        // Apply effect
        eff.negate();
        if (state.count(eff)) newState.erase(eff);
        eff.negate();
        newState[eff];
    }
}

std::vector<PlanItem> Encoding::extractDecompositionPlan() {

    std::vector<PlanItem> plan;

    PlanItem root({0, 
                Signature(_htn.getNameId("root"), std::vector<int>()), 
                Signature(_htn.getNameId("root"), std::vector<int>()), 
                std::vector<int>()});
    
    std::vector<PlanItem> itemsOldLayer, itemsNewLayer;
    itemsOldLayer.push_back(root);

    for (int i = 0; i < _layers->size(); i++) {
        Layer& l = _layers->at(i);
        printf("(decomps at layer %i)\n", l.index());

        itemsNewLayer.resize(l.size());
        
        for (int pos = 0; pos < l.size(); pos++) {

            int predPos = 0;
            if (i > 0) {
                Layer& lastLayer = _layers->at(i-1);
                while (predPos+1 < lastLayer.size() && lastLayer.getSuccessorPos(predPos+1) <= pos) 
                    predPos++;
            } 
            //printf("%i -> %i\n", predPos, pos);

            int itemsThisPos = 0;

            for (auto pair : l[pos].getReductions()) {
                Signature rSig = pair.first;
                if (!isEncoded(i, pos, rSig) || rSig == Position::NONE_SIG) continue;

                if (value(i, pos, rSig)) {
                    itemsThisPos++;

                    int v = _layers->at(i)[pos].getVariable(rSig);
                    Reduction& r = _htn._reductions_by_sig[rSig];

                    // Check preconditions
                    for (Signature pre : r.getPreconditions()) {
                        assert(value(i, pos, pre) || fail("Precondition " + Names::to_string(pre) + " of reduction "
                        + Names::to_string(r) + " does not hold at step " + std::to_string(pos) + "!\n"));
                    }

                    rSig = getDecodedQOp(i, pos, rSig);
                    Reduction rDecoded = r.substituteRed(Substitution::get(r.getArguments(), rSig._args));
                    itemsNewLayer[pos] = PlanItem({v, rDecoded.getTaskSignature(), rSig, std::vector<int>()});

                    // TODO check this is a valid subtask relationship
                    itemsOldLayer[predPos].subtaskIds.push_back(v);
                }
            }

            for (auto pair : l[pos].getActions()) {
                Signature aSig = pair.first;
                if (!isEncoded(i, pos, aSig)) continue;

                if (value(i, pos, aSig)) {
                    itemsThisPos++;

                    if (aSig == _htn._action_blank.getSignature()) continue;
                    
                    int v = _layers->at(i)[pos].getVariable(aSig);
                    Action a = _htn._actions_by_sig[aSig];

                    // Check preconditions, effects
                    for (Signature pre : a.getPreconditions()) {
                        assert(value(i, pos, pre) || fail("Precondition " + Names::to_string(pre) + " of action "
                        + Names::to_string(aSig) + " does not hold at step " + std::to_string(pos) + "!\n"));
                    }
                    for (Signature eff : a.getEffects()) {
                        assert(value(i, pos+1, eff) || fail("Effect " + Names::to_string(eff) + " of action "
                        + Names::to_string(aSig) + " does not hold at step " + std::to_string(pos+1) + "!\n"));
                    }

                    // TODO check this is a valid subtask relationship

                    // Find the actual action variable at the final layer, not at this (inner) layer
                    int l = i;
                    int aPos = pos;
                    while (l < _layers->size()) {
                        //printf("(%i,%i) => ", l, aPos);
                        aPos = _layers->at(l).getSuccessorPos(aPos);
                        l++;
                        //printf("(%i,%i)\n", l, aPos);
                    }
                    v = _layers->at(l-1)[aPos].getVariable(aSig);

                    //itemsNewLayer[pos] = PlanItem({v, aSig, aSig, std::vector<int>()});
                    itemsOldLayer[predPos].subtaskIds.push_back(v);
                }
            }

            assert( ((itemsThisPos == 1) ^ (pos+1 == l.size()))
            || fail(std::to_string(itemsThisPos) 
                + " items at (" + std::to_string(i) + "," + std::to_string(pos) + ") !\n"));
        }

        plan.insert(plan.end(), itemsOldLayer.begin(), itemsOldLayer.end());

        itemsOldLayer = itemsNewLayer;
        itemsNewLayer.clear();
    }

    plan.insert(plan.end(), itemsOldLayer.begin(), itemsOldLayer.end());
    return plan;
}

bool Encoding::value(int layer, int pos, const Signature& sig) {
    int v = _layers->at(layer)[pos].getVariable(sig);
    int vAbs = std::abs(v);
    return (v < 0) ^ (ipasir_val(_solver, vAbs) > 0);
}

Signature Encoding::getDecodedQOp(int layer, int pos, Signature sig) {
    assert(isEncoded(layer, pos, sig));
    assert(value(layer, pos, sig));

    Signature origSig = sig;
    while (true) {
        bool containsQConstants = false;
        for (int arg : sig._args) if (_q_constants.count(arg)) {
            // q constant found
            containsQConstants = true;

            int numSubstitutions = 0;
            for (int argSubst : _htn._domains_of_q_constants[arg]) {
                Signature sigSubst = sigSubstitute(arg, argSubst);
                if (isEncodedSubstitution(sigSubst) && ipasir_val(_solver, varSubstitution(sigSubst)) > 0) {
                    //printf("%i TRUE\n", varSubstitution(sigSubst));
                    //printf("%s/%s => %s ~~> ", Names::to_string(arg).c_str(), 
                    //        Names::to_string(argSubst).c_str(), Names::to_string(sig).c_str());
                    numSubstitutions++;
                    substitution_t sub;
                    sub[arg] = argSubst;
                    sig = sig.substitute(sub);
                    //printf("%s\n", Names::to_string(sig).c_str());
                }
            }

            assert(numSubstitutions == 1);
        }

        if (!containsQConstants) break; // done
    }

    if (origSig != sig)
        printf("%s ~~> %s\n", Names::to_string(origSig).c_str(), Names::to_string(sig).c_str());
    return sig;
}

Encoding::~Encoding() {
    for (int asmpt : _last_assumptions) {
        _out << asmpt << " 0\n";
    }
    _out.flush();
    _out.close();

    std::ofstream headerfile;
    headerfile.open("header.cnf");
    VariableDomain::unlock();
    headerfile << "p cnf " << VariableDomain::nextVar() << " " << (numClauses+_last_assumptions.size()) << "\n";
    headerfile.flush();
    headerfile.close();

    ipasir_release(_solver);
}