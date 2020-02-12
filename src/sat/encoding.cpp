
#include "sat/encoding.h"
#include "util/log.h"

Encoding::Encoding(HtnInstance& htn) : _htn(htn), _out("f.cnf") {
    _solver = ipasir_init();
    _sig_primitive = Signature(_htn.getNameId("__PRIMITIVE___"), std::vector<int>());
    _substitute_name_id = _htn.getNameId("__SUBSTITUTE___");
}

void Encoding::addTrueFacts(SigSet& facts, Layer& layer, int pos) {
    //printf("[ENC] addTrueFacts @ %i\n", pos);
    for (Signature fact : facts) {
        addClause({var(layer.index(), pos, fact)});
        _prior_facts[fact._name_id].insert(fact.abs());
        _posterior_facts[fact._name_id].insert(fact.abs());
    }
}

void Encoding::addInitialTasks(Layer& layer, int pos) {
    //printf("[ENC] addInitialTasks @ %i\n", pos);
    _var_domain_locked = true;
    for (Signature rSig : layer[pos].getReductions()) {
        appendClause({var(layer.index(), pos, rSig)});
    }
    for (Signature aSig : layer[pos].getActions()) {
        appendClause({var(layer.index(), pos, aSig)});
    }
    endClause();
    _var_domain_locked = false;
}

void Encoding::addAction(Action& a, Layer& layer, int pos) {
    int layerIdx = layer.index();
    Signature sig = a.getSignature();
    printf("[ENC] addAction %s @ %i\n", Names::to_string(sig).c_str(), pos);

    /*
    assert(!isEncoded(layerIdx, pos, sig) || fail(Names::to_string(a) + " is already encoded @ (" 
                + std::to_string(layerIdx) + "," + std::to_string(pos) + ") !\n"));
    */
    if (isEncoded(layerIdx, pos, sig)) return;

    int aVar = var(layerIdx, pos, sig);
    
    // If this action, then the pos is primitive
    addClause({-aVar, varPrimitive(layerIdx, pos)});

    // Preconditions must hold
    for (Signature precond : a.getPreconditions()) {
        //_prior_facts[pre._name_id].insert(pre.abs());
        addClause({-aVar, var(layerIdx, pos, precond)});

        // Expand any q constants
        std::vector<Signature> preDecoded = _htn.getDecodedFacts(precond);
        for (Signature pre : preDecoded) {
            _posterior_facts[pre._name_id].insert(pre.abs());
        }
    }
    // Effects must hold
    for (Signature effect : a.getEffects()) {
        addClause({-aVar, var(layerIdx, pos+1, effect)});

        // Add a to support of eff
        addToSupport(var(layerIdx, pos+1, effect), aVar);

        // Expand any q constants
        std::vector<Signature> effDecoded = _htn.getDecodedFacts(effect);
        for (Signature eff : effDecoded) {
            _posterior_facts[eff._name_id].insert(eff.abs());
        }
    }
}

void Encoding::addReduction(Reduction& r, SigSet& allFactChanges, Layer& layer, int pos) {
    int layerIdx = layer.index();
    Signature sig = r.getSignature();
    printf("[ENC] addReduction %s @ %i\n", Names::to_string(sig).c_str(), pos);

    /*
    assert(!isEncoded(layerIdx, pos, sig) || fail(Names::to_string(sig) + " is already encoded @ (" 
                + std::to_string(layerIdx) + "," + std::to_string(pos) + ") !\n"));
    */
    if (isEncoded(layerIdx, pos, sig)) return;

    int rVar = var(layerIdx, pos, sig);

    // If this reduction, then the pos is not primitive
    addClause({-rVar, -varPrimitive(layerIdx, pos)});

    // Preconditions must hold
    for (Signature precond : r.getPreconditions()) {
        //_prior_facts[pre._name_id].insert(pre.abs());
        addClause({-rVar, var(layerIdx, pos, precond)});
        
        // Expand any q constants
        std::vector<Signature> preDecoded = _htn.getDecodedFacts(precond);
        for (Signature pre : preDecoded) {
            _posterior_facts[pre._name_id].insert(pre.abs());
        }
    }

    // Add r to support of each eff in potential effects
    for (Signature effect : allFactChanges) {
        addToSupport(var(layerIdx, pos+1, effect), rVar);

        // Expand any q constants
        std::vector<Signature> effDecoded = _htn.getDecodedFacts(effect);
        for (Signature eff : effDecoded) {
            _posterior_facts[eff._name_id].insert(eff.abs());
        }
    }
}

void Encoding::addAction(Action& a, Reduction& parent, Layer& oldLayer, int oldPos, Layer& newLayer, int newPos) {
    Signature childSig = a.getSignature();
    Signature parentSig = parent.getSignature();
    printf("[ENC] addAction(expansion) %s <~ %s\n", Names::to_string(childSig).c_str(), Names::to_string(parentSig).c_str());
    
    // Add "universal" constraints of the new action
    addAction(a, newLayer, newPos);

    // Add a to expansion @(offset) of parent
    int offset = newPos-oldLayer.getSuccessorPos(oldPos);
    assert(offset >= 0 && offset < oldLayer.getSuccessorPos(oldPos+1));
    addToExpansion(var(oldLayer.index(), oldPos, parentSig), 
                   var(newLayer.index(), newPos, childSig), 
                   offset);
}

void Encoding::propagateAction(Action& a, Layer& oldLayer, int oldPos, Layer& newLayer, int newPos) {

    // Add "universal" constraints of the new action
    addAction(a, newLayer, newPos);
    
    Signature aSig = a.getSignature();
    printf("[ENC] propagateAction %s\n", Names::to_string(aSig).c_str());
    assert(isEncoded(oldLayer.index(), oldPos, aSig));

    Signature sigBlank = _htn._action_blank.getSignature();

    _var_domain_locked = true;
    // If the action was already at the old position, it will also be at the new position
    addClause({-var(oldLayer.index(), oldPos, aSig), var(newLayer.index(), newPos, aSig)});
    _var_domain_locked = false;

    // If the action was already at the old position, a BLANK action will be at all other child positions
    for (int offset = 1; offset < oldLayer[oldPos].getMaxExpansionSize(); offset++) {
        addClause({-var(oldLayer.index(), oldPos, aSig), var(newLayer.index(), newPos+offset, sigBlank)});
    }
}

void Encoding::propagateFacts(const SigSet& facts, Layer& oldLayer, int oldPos, Layer& newLayer, int newPos) {
    //printf("[ENC] propagateFacts\n");

    for (Signature fact : facts) {
        fact = fact.abs();
        int oldVar = var(oldLayer.index(), oldPos, fact);
        int newVar = var(newLayer.index(), newPos, fact);
        addClause({-oldVar, newVar});
        addClause({oldVar, -newVar});
        _prior_facts[fact._name_id].insert(fact.abs());
        _posterior_facts[fact._name_id].insert(fact.abs());
    }
}

void Encoding::addReduction(Reduction& child, Reduction& parent, SigSet& allFactChanges, 
                        Layer& oldLayer, int oldPos, Layer& newLayer, int newPos) {
    printf("[ENC] addReduction(propagate)\n");
    
    // Add "universal" constraints of the new (child) reduction
    addReduction(child, allFactChanges, newLayer, newPos);
    
    // Add child to expansion @(offset) of parent
    Signature parentSig = parent.getSignature();
    Signature childSig = child.getSignature();
    int offset = newPos-oldLayer.getSuccessorPos(oldPos);
    assert(offset >= 0 && offset < oldLayer.getSuccessorPos(oldPos+1));
    addToExpansion(var(oldLayer.index(), oldPos, parentSig), 
                   var(newLayer.index(), newPos, childSig), 
                   offset);
}

void Encoding::consolidateReductionExpansion(Reduction& r, 
        Layer& oldLayer, int oldPos, 
        Layer& newLayer, int newPos) {

    printf("[ENC] consolidateReductionExpansion\n");
    
    Signature sig = r.getSignature();
    int redVar = var(oldLayer.index(), oldPos, sig);

    // Add blank actions to offsets where the reduction has no children
    Signature sigBlank = _htn._action_blank.getSignature();
    for (int offset = r.getSubtasks().size(); offset < oldLayer[oldPos].getMaxExpansionSize(); offset++) {
        int blankVar = var(newLayer.index(), newPos+offset, sigBlank);
        addToExpansion(redVar, blankVar, offset);
    }

    // Add expansion rules to the reduction
    auto& expansion = _expansions[redVar];
    _var_domain_locked = true;
    // For each offset of the expansion
    for (int offset = 0; offset < expansion.size(); offset++) {
        // If the reduction was chosen, one of the children must be chosen
        appendClause({-redVar});
        for (int child : expansion[offset]) {
            appendClause({child});
        }
        endClause();
    }
    _var_domain_locked = false;
}

void Encoding::consolidateHtnOps(Layer& layer, int pos) {
    printf("[ENC] consolidateHtnOps\n");
    
    _var_domain_locked = true;
    for (Signature sigA1 : layer[pos].getActions()) {
        int varA1 = var(layer.index(), pos, sigA1);
        for (Signature sigA2 : layer[pos].getActions()) {
            int varA2 = var(layer.index(), pos, sigA2);
            if (varA1 < varA2) addClause({-varA1, -varA2});
        }
    }
    for (Signature sigR1 : layer[pos].getReductions()) {
        int varR1 = var(layer.index(), pos, sigR1);
        for (Signature sigR2 : layer[pos].getReductions()) {
            int varR2 = var(layer.index(), pos, sigR2);
            if (varR1 < varR2) addClause({-varR1, -varR2});
        }
    }
    _var_domain_locked = false;
}

void Encoding::consolidateFacts(Layer& layer, int pos) {
    printf("[ENC] consolidateFacts @ %i\n", pos);
    
    // For each occurring fact
    for (auto pair : _posterior_facts) for (Signature fact : pair.second) {

        // First time this fact occurs?
        if (!_prior_facts.count(fact._name_id) || !_prior_facts[fact._name_id].count(fact)) {
            Signature fAbs = fact.abs();
            addClause({-var(layer.index(), pos, fAbs)});
        }

        // Add substitution literals as necessary
        for (int argPos = 0; argPos < fact._args.size(); argPos++) {
            int arg = fact._args[argPos];
            if (!_htn._q_constants.count(arg)) continue;
            
            // arg is a q-constant

            // if arg is a *new* q-constant: initialize substitution logic
            if (!_q_constants.count(arg)) {

                std::vector<int> substitutionVars;
                for (int c : _htn._domains_of_q_constants[arg]) {
                    
                    // Do not include cyclic substitutions
                    if (_htn._q_constants.count(c)/* && c >= arg*/) continue;

                    // either of the possible substitutions must be chosen
                    Signature sigSubst = sigSubstitute(arg, c);
                    int varSubst = varSubstitution(sigSubst);
                    substitutionVars.push_back(varSubst);
                    
                    _q_constants_per_arg[c];
                    _q_constants_per_arg[c].push_back(arg);
                }
                for (int vSub : substitutionVars) appendClause({vSub});
                endClause();

                // AT MOST ONE substitution
                for (int vSub1 : substitutionVars) {
                    for (int vSub2 : substitutionVars) {
                        if (vSub1 < vSub2) addClause({-vSub1, -vSub2});
                    }
                }

                _q_constants.insert(arg);
            }

            // Add equivalence to corresponding facts
            // relative to chosen substitution
            for (auto factsPair : _posterior_facts) for (Signature substitutedFact : factsPair.second) {

                if (substitutedFact._name_id != fact._name_id) continue;
                int substArg = substitutedFact._args[argPos];

                // All args must be the same except for the qconst position;
                // at the qconst position, there must be a true constant 
                // or a qconst of smaller value.
                if (_htn._q_constants.count(substArg)/* && substArg >= arg*/) continue;
                bool isSubst = true;
                for (int i = 0; i < substitutedFact._args.size(); i++) {
                    isSubst = (i == argPos) ^ (substitutedFact._args[i] == fact._args[i]);
                    if (!isSubst) break;
                }
                if (!isSubst) continue;
                
                // -- yes: valid substitution found
                int varQFact = var(layer.index(), pos, fact);
                Signature sigSubst = sigSubstitute(arg, substitutedFact._args[argPos]);
                int varSubst = varSubstitution(sigSubst);
                int varSubstitutedFact = var(layer.index(), pos, substitutedFact);
                // If the substitution is chosen,
                // the q-fact and the corresponding actual fact are equivalent
                addClause({-varSubst, -varQFact, varSubstitutedFact});
                addClause({-varSubst, varQFact, -varSubstitutedFact});
            }
        }
    }

    // Frame axioms

    if (pos+1 >= layer.size()) return;

    for (auto pair : _posterior_facts) {
        int predId = pair.first;
        SigSet& facts = pair.second;
        for (Signature fact : facts) {

            for (int val = -1; val <= 1; val += 2) {
                
                // Check if facts at prior AND posterior position are encoded
                //if (!isEncoded(layer.index(), pos, fact) || !isEncoded(layer.index(), pos+1, fact)) 
                //    continue;
                
                //printf("%s\n", Names::to_string(fact).c_str());
                int fVarPre = val * var(layer.index(), pos, fact);
                int fVarPost = val * var(layer.index(), pos+1, fact);

                appendClause({fVarPre, -fVarPost});
                //printVar(layer.index(), pos, fact); printVar(layer.index(), pos+1, fact);
                
                // Allow any fact changes if the pos. is not primitive yet
                // (should not be necessary, but relaxes the encoding)
                //appendClause({-varPrimitive(layer.index(), pos)});

                _var_domain_locked = true;
                for (int opVar : _supports[fVarPost]) {
                    appendClause({opVar});
                    //printf("%i ", opVar);
                }

                // If the fact is a possible substitution of a q-constant fact,
                // add corresponding substitution literal to frame axioms.
                for (int arg : fact._args) {
                    if (_q_constants_per_arg.count(arg)) {
                        for (int qConst : _q_constants_per_arg[arg]) {

                            // do not consider cyclic substitution
                            if (_q_constants.count(arg) /*&& qConst >= arg*/) continue;
                            
                            Signature sigSubst = sigSubstitute(qConst, arg);
                            appendClause({varSubstitution(sigSubst)});
                            //printVar(layer.index(), pos, sigSubst);
                        }
                    } 
                }
                _var_domain_locked = false;

                endClause();
                //printf("\n");
            }
        }
    }

    _prior_facts = _posterior_facts;
    _supports.clear();
}

void Encoding::addAssumptions(Layer& layer) {
    //printf("[ENC] addAssumptions\n");
    
    for (int pos = 0; pos < layer.size(); pos++) {
        assume(varPrimitive(layer.index(), pos));
    }
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
    ipasir_add(_solver, 0);
    _out << "0\n";
    printf("0\n");
    beganLine = false;

    numClauses++;
}
void Encoding::assume(int lit) {
    ipasir_assume(_solver, lit);
    printf("CNF !%i\n", lit);
    numAssumptions++;
}

bool Encoding::solve() {
    printf("Attempting to solve formula with %i clauses (%i literals) and %i assumptions\n", 
                numClauses, numLits, numAssumptions);
    numAssumptions = 0;
    return ipasir_solve(_solver) == 10;
}

bool Encoding::isEncoded(int layer, int pos, Signature& sig) {
    if (layer >= _variables.size()) return false;
    if (pos >= _variables[layer].size()) return false;
    return _variables[layer][pos].count(sig.abs());
}

bool Encoding::isEncodedSubstitution(Signature& sig) {
    return _substitution_variables.count(sig.abs());
}

int Encoding::var(int layer, int pos, Signature& sig) {
    
    assert(layer >= 0 && pos >= 0);
    while (layer >= _variables.size()) _variables.push_back(std::vector<SigToIntMap>());
    while (pos >= _variables[layer].size()) _variables[layer].push_back(SigToIntMap());

    auto& vars = _variables[layer][pos];
    
    bool neg = sig._negated;
    Signature sigAbs = neg ? sig.abs() : sig;

    if (!vars.count(sigAbs)) {
        // introduce a new variable
        assert(!_var_domain_locked || fail("Unknown variable " + Names::to_string(sigAbs) 
                    + " @ (" + std::to_string(layer) + "," + std::to_string(pos) + ") queried!\n"));
        vars[sigAbs] = _running_var_id++;
        printf("VARMAP %i %s\n", vars[sigAbs], varName(layer, pos, sigAbs).c_str());
    }

    //printf("%i\n", vars[sig]);
    int val = (neg ? -1 : 1) * vars[sigAbs];
    return val;
}

int Encoding::varSubstitution(Signature sigSubst) {
    bool neg = sigSubst._negated;
    Signature sigAbs = neg ? sigSubst.abs() : sigSubst;
    if (!_substitution_variables.count(sigSubst)) {
        assert(!_var_domain_locked || fail("Unknown substitution variable " 
                    + Names::to_string(sigSubst) + " queried!\n"));
        _substitution_variables[sigAbs] = _running_var_id++;
        printf("VARMAP %i %s\n", _substitution_variables[sigAbs], Names::to_string(sigSubst).c_str());
    }
    return _substitution_variables[sigAbs];
}

std::string Encoding::varName(int layer, int pos, Signature& sig) {
    assert(isEncoded(layer, pos, sig));
    std::string out = Names::to_string(sig) + "@(" + std::to_string(layer) + "," + std::to_string(pos) + ")";
    return out;
}

void Encoding::printVar(int layer, int pos, Signature& sig) {
    printf("%s ", varName(layer, pos, sig).c_str());
}

int Encoding::varPrimitive(int layer, int pos) {
    return var(layer, pos, _sig_primitive);
}

void Encoding::printFailedVars(Layer& layer) {
    printf("FAILED ");
    for (int pos = 0; pos < layer.size(); pos++) {
        int v = varPrimitive(layer.index(), pos);
        if (ipasir_failed(_solver, v)) printf("%i ", v);
    }
    printf("\n");
}

std::vector<PlanItem> Encoding::extractClassicalPlan(Layer& finalLayer) {
    int li = finalLayer.index();
    _var_domain_locked = true;

    SigSet state = finalLayer[0].getFacts();
    for (Signature f : state) {
        assert(value(0, 0, f));
    }

    std::vector<PlanItem> plan;
    for (int pos = 0; pos+1 < finalLayer.size(); pos++) {
        //printf("%i\n", pos);
        assert(value(li, pos, _sig_primitive) || fail("Position " + std::to_string(pos) + " is not primitive!\n"));

        int chosenActions = 0;
        SigSet newState = state;
        SigSet effects;
        for (Signature aSig : finalLayer[pos].getActions()) {

            if (!isEncoded(li, pos, aSig)) continue;
            //printf("  %s ?\n", Names::to_string(aSig).c_str());

            if (value(li, pos, aSig)) {
                chosenActions++;
                int aVar = var(li, pos, aSig);

                // Check fact consistency
                checkAndApply(_htn._actions_by_sig[aSig], state, newState, li, pos);

                if (aSig != _htn._action_blank.getSignature()) {

                    // Decode q constants
                    Action& a = _htn._actions_by_sig[aSig];
                    aSig = getDecodedQOp(li, pos, aSig);
                    HtnOp opDecoded = a.substitute(Substitution::get(a.getArguments(), aSig._args));
                    Action aDecoded = (Action) opDecoded;

                    // Check fact consistency w.r.t. "actual" decoded action
                    checkAndApply(aDecoded, state, newState, li, pos);

                    //printf("%s @ %i\n", Names::to_string(aSig).c_str(), pos);
                    plan.push_back({aVar, aSig, aSig, std::vector<int>()});
                }
            }
        }

        //for (Signature sig : newState) {
        //    assert(value(li, pos+1, sig));
        //}
        state = newState;

        assert(chosenActions == 1 || fail("Added " + std::to_string(chosenActions) + " actions at step " + std::to_string(pos) + "!\n"));
    }

    return plan;
}

void Encoding::checkAndApply(Action& a, SigSet& state, SigSet& newState, int layer, int pos) {
    printf("%s\n", Names::to_string(a).c_str());
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
        newState.insert(eff);
    }
}

std::vector<PlanItem> Encoding::extractDecompositionPlan(std::vector<Layer>& allLayers) {

    std::vector<PlanItem> plan;

    PlanItem root({0, 
                Signature(_htn.getNameId("root"), std::vector<int>()), 
                Signature(_htn.getNameId("root"), std::vector<int>()), 
                std::vector<int>()});
    
    std::vector<PlanItem> itemsOldLayer, itemsNewLayer;
    itemsOldLayer.push_back(root);

    for (int i = 0; i < allLayers.size(); i++) {
        Layer& l = allLayers[i];
        printf("(layer %i)\n", l.index());

        itemsNewLayer.resize(l.size());
        
        for (int pos = 0; pos < l.size(); pos++) {

            int predPos = 0;
            if (i > 0) {
                Layer& lastLayer = allLayers[i-1];
                while (predPos+1 < lastLayer.size() && lastLayer.getSuccessorPos(predPos+1) <= pos) 
                    predPos++;
            } 
            //printf("%i -> %i\n", predPos, pos);

            int itemsThisPos = 0;

            for (Signature rSig : l[pos].getReductions()) {
                if (!isEncoded(i, pos, rSig)) continue;

                if (value(i, pos, rSig)) {
                    itemsThisPos++;

                    int v = var(i, pos, rSig);
                    Reduction& r = _htn._reductions_by_sig[rSig];

                    // Check preconditions
                    for (Signature pre : r.getPreconditions()) {
                        assert(value(i, pos, pre) || fail("Precondition " + Names::to_string(pre) + " of reduction "
                        + Names::to_string(r) + " does not hold at step " + std::to_string(pos) + "!\n"));
                    }

                    rSig = getDecodedQOp(i, pos, rSig);
                    itemsNewLayer[pos] = PlanItem({v, r.getTaskSignature(), rSig, std::vector<int>()});

                    // TODO check this is a valid subtask relationship
                    itemsOldLayer[predPos].subtaskIds.push_back(v);
                }
            }

            for (Signature aSig : l[pos].getActions()) {
                if (!isEncoded(i, pos, aSig)) continue;

                if (value(i, pos, aSig)) {
                    itemsThisPos++;

                    if (aSig == _htn._action_blank.getSignature()) continue;
                    
                    int v = var(i, pos, aSig);
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
    printf("%i items in decomp plan\n", plan.size());
    return plan;
}

bool Encoding::value(int layer, int pos, Signature& sig) {
    int v = var(layer, pos, sig);
    int vAbs = std::abs(v);
    return (v < 0) ^ ipasir_val(_solver, vAbs) > 0;
}

Signature Encoding::getDecodedQOp(int layer, int pos, Signature sig) {
    assert(isEncoded(layer, pos, sig));
    assert(value(layer, pos, sig));

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
                    printf("%s/%s => %s ~~> ", Names::to_string(arg).c_str(), 
                            Names::to_string(argSubst).c_str(), Names::to_string(sig).c_str());
                    numSubstitutions++;
                    substitution_t sub;
                    sub[arg] = argSubst;
                    sig = sig.substitute(sub);
                    printf("%s\n", Names::to_string(sig).c_str());
                }
            }

            assert(numSubstitutions == 1);
        }

        if (!containsQConstants) break; // done
    }

    return sig;
}

Encoding::~Encoding() {
    ipasir_release(_solver);
}