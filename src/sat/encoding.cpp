
#include "sat/encoding.h"

Encoding::Encoding(HtnInstance& htn) : _htn(htn) {
    _solver = ipasir_init();
    _sig_primitive = Signature(_htn.getNameId("__PRIMITIVE___"), std::vector<int>());
    // blank action: no preconditions, no effects
    _action_blank = Action(_htn.getNameId("__BLANK___"), std::vector<int>());
    _substitute_name_id = _htn.getNameId("__SUBSTITUTE___");
}

void Encoding::addAction(Action& a, Layer& layer, int pos) {
    int layerIdx = layer.index();
    Signature sig = a.getSignature();

    int aVar = var(layerIdx, pos, sig);

    // If this action, then the pos is primitive
    addClause({-aVar, varPrimitive(layerIdx, pos)});

    // Preconditions must hold
    for (Signature pre : a.getPreconditions()) {
        addClause({-aVar, var(layerIdx, pos, pre)});
    }
    // Effects must hold
    for (Signature eff : a.getEffects()) {
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
        addClause({-rVar, var(layerIdx, pos, pre)});
    }

    // Add r to support of each eff in potential effects
    for (Signature eff : allFactChanges) {
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

void Encoding::addOccurringFacts(State& state, Layer& layer, int pos) {

    assert(pos == _occurring_facts.size());
    _occurring_facts.push_back(state);

    // For each contained literal
    for (auto pair : state) {
        int predId = pair.first;
        for (Signature sig : pair.second) {

            int fVar = var(layer.index(), pos, sig);

            // Frame axioms
            if (pos > 0) {
                int posBefore = getNearestPriorOccurrence(sig, pos);
                if (posBefore >= 0) {
                    
                    int fVarBefore = var(layer.index(), posBefore, sig);
                    appendClause({fVarBefore, -fVar});
                    for (int opVar : _supports[fVar]) {
                        appendClause({opVar});
                    }
                    // TODO add substitution vars as necessary
                }
            }

        }
    }
}

void Encoding::addFrameAxioms() {

    for (auto pair : _supports) {
        int factVar = pair.first;

        appendClause({});
        for (int opVar : pair.second) {

        }
    }
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