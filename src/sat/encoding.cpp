
#include "sat/encoding.h"

Encoding::Encoding(HtnInstance& htn) : _htn(htn) {
    _solver = ipasir_init();
    _sig_primitive = Signature(_htn.getNameId("__PRIMITIVE___"), std::vector<int>());
    // blank action: no preconditions, no effects
    _action_blank = Action(_htn.getNameId("__BLANK___"), std::vector<int>());
    _substitute_name_id = _htn.getNameId("__SUBSTITUTE___");
}

void Encoding::addTrueFacts(SigSet& facts, Layer& layer, int pos) {
    for (Signature fact : facts) {
        addClause({var(layer.index(), pos, fact)});
        _prior_facts.insert(fact.abs());
        _posterior_facts.insert(fact.abs());
    }
}

void Encoding::addAction(Action& a, Layer& layer, int pos) {
    int layerIdx = layer.index();
    Signature sig = a.getSignature();

    int aVar = var(layerIdx, pos, sig);

    // If this action, then the pos is primitive
    addClause({-aVar, varPrimitive(layerIdx, pos)});

    // Preconditions must hold
    for (Signature pre : a.getPreconditions()) {
        _prior_facts.insert(pre.abs());
        _posterior_facts.insert(pre.abs());
        addClause({-aVar, var(layerIdx, pos, pre)});
    }
    // Effects must hold
    for (Signature eff : a.getEffects()) {
        _posterior_facts.insert(eff.abs());
        addClause({-aVar, var(layerIdx, pos+1, eff)});

        // Add a to support of eff
        addToSupport(var(layerIdx, pos+1, eff), aVar);
    }
}

void Encoding::addReduction(Reduction& r, SigSet& allFactChanges, Layer& layer, int pos) {
    int layerIdx = layer.index();
    Signature sig = r.getSignature();

    int rVar = var(layerIdx, pos, sig);

    // If this reduction, then the pos is not primitive
    addClause({-rVar, -varPrimitive(layerIdx, pos)});

    // Preconditions must hold
    for (Signature pre : r.getPreconditions()) {
        _prior_facts.insert(pre.abs());
        _posterior_facts.insert(pre.abs());
        addClause({-rVar, var(layerIdx, pos, pre)});
    }

    // Add r to support of each eff in potential effects
    for (Signature eff : allFactChanges) {
        _posterior_facts.insert(eff.abs());
        addToSupport(var(layerIdx, pos+1, eff), rVar);
    }
}

void Encoding::addAction(Action& a, Reduction& parent, Layer& oldLayer, int oldPos, Layer& newLayer, int newPos) {
    
    // Add "universal" constraints of the new action
    addAction(a, newLayer, newPos);

    // Add a to expansion @(offset) of parent
    Signature parentSig = parent.getSignature();
    Signature childSig = a.getSignature();
    int offset = newPos-oldLayer.getSuccessorPos(oldPos);
    assert(offset >= 0 && offset < oldLayer.getSuccessorPos(oldPos+1));
    addToExpansion(var(oldLayer.index(), oldPos, parentSig), 
                   var(newLayer.index(), newPos, childSig), 
                   offset);
    
    // TODO check if the action was already present at the last layer, add propagation 
    // (here of somewhere else?)
}

void Encoding::addReduction(Reduction& child, Reduction& parent, SigSet& allFactChanges, 
                        Layer& oldLayer, int oldPos, Layer& newLayer, int newPos) {
    
    // Add "universal" constraints of the new (child) reduction
    addReduction(child, allFactChanges, newLayer, newPos);
    
    // TODO add child to expansion @(offset) of parent
    Signature parentSig = parent.getSignature();
    Signature childSig = child.getSignature();
    int offset = newPos-oldLayer.getSuccessorPos(oldPos);
    assert(offset >= 0 && offset < oldLayer.getSuccessorPos(oldPos+1));
    addToExpansion(var(oldLayer.index(), oldPos, parentSig), 
                   var(newLayer.index(), newPos, childSig), 
                   offset);
}

void Encoding::endReduction(Reduction& r, Layer& layer, int pos) {

    Signature sig = r.getSignature();
    int redVar = var(layer.index(), pos, sig);
    auto& expansion = _expansions[redVar];

    // For each offset of the expansion
    for (int offset = 0; offset < expansion.size(); offset++) {
        // If the reduction was chosen, one of the children must be chosen
        appendClause({-redVar});
        for (int child : expansion[offset]) {
            appendClause({child});
        }
        endClause();
    }
}

void Encoding::addFacts(Layer& layer, int pos) {

    for (auto pair : _posterior_facts) {
        int predId = pair.first;
        SigSet& facts = pair.second;

        // Maps an argument name ID to all q-constants that may assume this value.
        
        for (Signature fact : facts) {

            // Add substitution literals as necessary
            for (int argPos = 0; argPos < fact._args.size(); argPos++) {
                int arg = fact._args[argPos];

                if (_htn._q_constants.count(arg)) {
                    // arg is a q-constant

                    if (!_q_constants.count(arg)) {
                        // new q-constant: initialize substitution literals

                        // sort of q constant
                        int sort = _htn._predicate_sorts_table[fact._name_id][argPos];

                        // for each constant of same sort;
                        for (int c : _htn._constants_by_sort[sort]) {
                            
                            // Do not include cyclic substitutions
                            if (_htn._q_constants.count(c) && c >= arg) continue;

                            // either of the possible substitutions must be chosen
                            Signature sigSubst = sigSubstitute(arg, c);
                            appendClause(var(layer.index(), pos, sigSubst));
                            
                            _q_constants_per_arg[c];
                            _q_constants_per_arg[c].push_back(arg);
                        }
                        endClause();

                        // TODO AT MOST ONE substitution??

                        _q_constants.insert(arg);
                    }

                    // Add equivalence to corresponding facts
                    // relative to chosen substitution

                    // TODO extend formulae: ALL necessary substitutions at the left-hand side
                    // to directly lead to fully "ground" fact
                    // OR: add all necessary "in-between" facts to make all substitutions possible 
                    std::unordered_set<Signature, SignatureHasher> substitutedFacts;
                    for (Signature actualFact : _posterior_facts) {
                        if (actualFact._name_id != fact._name_id) continue;
                        int substArg = actualFact._args[argPos];

                        // All args must be the same except for the qconst position;
                        // at the qconst position, there must be a true constant 
                        // or a qconst of smaller value.
                        bool isSubstitution = !_htn._q_constants.count(substArg) || (substArg < arg);
                        for (int i = 0; i < actualFact._args.size(); i++) {
                            isSubstitution &= (i == argPos) ^ (actualFact._args[i] == fact._args[i]);
                        }

                        if (isSubstitution) {
                            // -- yes: valid substitution found
                            substitutedFacts.insert(actualFact);
                            Signature sigSubst = sigSubstitute(arg, actualFact._args[argPos]);
                            int varSubtitution = var(layer.index(), pos, sigSubst);
                            int varQFact = var(layer.index(), pos, fact);
                            int varActualFact = var(layer.index(), pos, actualFact);
                            // If the substitution is chosen,
                            // the q-fact and the corresponding actual fact are equivalent
                            addClause({-varSubtitution, -varQFact, varActualFact});
                            addClause({-varSubtitution, varQFact, -varActualFact});
                        }
                    }
                }
            }
        }

        for (Signature fact : facts) {

            if (!_prior_facts[predId].count(fact)) continue;
            
            for (int val = -1; val <= 1; val += 2) {
                
                // Frame axioms
                int fVarPre = val * var(layer.index(), pos-1, sig);
                int fVarPost = val * var(layer.index(), pos, sig);
                appendClause({fVarPre, -fVarPost});
                for (int opVar : _supports[val * fVarPost]) {
                    appendClause({opVar});
                }
                // If the fact is a possible substitution of a q-constant fact,
                // add corresponding substitution literal to frame axioms.
                for (int arg : fact._args) {
                    if (_q_constants_per_arg.count(arg)) {
                        for (int qConst : _q_constants_per_arg[arg]) {
                            
                            appendClause({var(layer.index(), pos, sigSubstitute(qConst, arg))});
                        }
                    } 
                }
                endClause();
            }
        }
    }

    _prior_facts = _posterior_facts;
}

void Encoding::addAssumptions(Layer& layer) {
    for (int pos = 0; pos < layer.size(); pos++) {
        assume(varPrimitive(layer.index(), pos));
    }
}

/*
void Encoding::addInitialClauses(Layer& layer) {
    addInitialState(layer);
    addInitialTaskNetwork(layer);
}

void Encoding::addUniversalClauses(Layer& layer) {
    addActionConstraints(layer);
    addFrameAxioms(layer);
    addQConstantsDefinition(layer);
    addPrimitivenessDefinition(layer);
}

void Encoding::addTransitionalClauses(Layer& oldLayer, Layer& newLayer) {
    addExpansions(oldLayer, newLayer);
    addActionPropagations(oldLayer, newLayer);
    addFactEquivalencies(oldLayer, newLayer);
}

*/
/*
void Encoding::addInitialTaskNetwork(Layer& layer) {}
void Encoding::addInitialState(Layer& layer) {}
void Encoding::addActionConstraints(Layer& layer) {}
void Encoding::addFrameAxioms(Layer& layer) {}
void Encoding::addQConstantsDefinition(Layer& layer) {}
void Encoding::addPrimitivenessDefinition(Layer& layer) {}
void Encoding::assertAllPrimitive(Layer& layer) {}
void Encoding::addExpansions(Layer& oldLayer, Layer& newLayer) {}
void Encoding::addActionPropagations(Layer& oldLayer, Layer& newLayer) {}
void Encoding::addFactEquivalencies(Layer& oldLayer, Layer& newLayer) {}
*/

bool Encoding::isInState(Signature fact, State& state) {
    if (!state.count(fact._name_id)) return false;
    if (fact._negated) fact.negate();
    return state[fact._name_id].count(fact);
}

void Encoding::addClause(std::initializer_list<int> lits) {
    for (int lit : lits) ipasir_add(_solver, lit);
    ipasir_add(_solver, 0);
}
void Encoding::appendClause(std::initializer_list<int> lits) {
    for (int lit : lits) ipasir_add(_solver, lit);
}
void Encoding::endClause() {
    ipasir_add(_solver, 0);
}
void Encoding::assume(int lit) {
    ipasir_assume(_solver, lit);
}

bool Encoding::solve() {
    return ipasir_solve(_solver) == 10;
}

int Encoding::var(int layer, int pos, Signature& sig) {
    assert(layer < _variables.size());
    assert(pos < _variables[layer].size());

    auto& vars = _variables[layer][pos]; 
    if (!vars.count(sig)) {
        
        // Is the opposite valued signature contained?
        sig.negate();
        int negVar = (vars.count(sig) ? vars[sig] : 0);
        sig.negate();

        if (negVar != 0) {
            // --yes: set the sig's coding to be the neg. signature's negation
            vars[sig] = -negVar;
        } else {
            // --no: introduce a new variable
            vars[sig] = _running_var_id++;
        }
    }
    return vars[sig];
}

bool Encoding::value(int layer, int pos, Signature& sig) {
    int v = var(layer, pos, sig);
    return ipasir_val(_solver, v);
}