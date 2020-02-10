
#include "sat/encoding.h"
#include "util/log.h"

Encoding::Encoding(HtnInstance& htn) : _htn(htn), _out("f.cnf") {
    _solver = ipasir_init();
    _sig_primitive = Signature(_htn.getNameId("__PRIMITIVE___"), std::vector<int>());
    _substitute_name_id = _htn.getNameId("__SUBSTITUTE___");
}

void Encoding::addTrueFacts(SigSet& facts, Layer& layer, int pos) {
    printf("[ENC] addTrueFacts @ %i\n", pos);
    for (Signature fact : facts) {
        addClause({var(layer.index(), pos, fact)});
        //_prior_facts[fact._name_id].insert(fact.abs());
        _posterior_facts[fact._name_id].insert(fact.abs());
    }
}

void Encoding::addInitialTasks(Layer& layer, int pos) {
    printf("[ENC] addInitialTasks @ %i\n", pos);
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
    _current_action_vars.insert(aVar);

    // If this action, then the pos is primitive
    addClause({-aVar, varPrimitive(layerIdx, pos)});

    // Preconditions must hold
    for (Signature pre : a.getPreconditions()) {
        //_prior_facts[pre._name_id].insert(pre.abs());
        _posterior_facts[pre._name_id].insert(pre.abs());
        addClause({-aVar, var(layerIdx, pos, pre)});
    }
    // Effects must hold
    for (Signature eff : a.getEffects()) {
        _posterior_facts[eff._name_id].insert(eff.abs());
        addClause({-aVar, var(layerIdx, pos+1, eff)});

        // Add a to support of eff
        addToSupport(var(layerIdx, pos+1, eff), aVar);
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
    _current_reduction_vars.insert(rVar);

    // If this reduction, then the pos is not primitive
    addClause({-rVar, -varPrimitive(layerIdx, pos)});

    // Preconditions must hold
    for (Signature pre : r.getPreconditions()) {
        //_prior_facts[pre._name_id].insert(pre.abs());
        _posterior_facts[pre._name_id].insert(pre.abs());
        addClause({-rVar, var(layerIdx, pos, pre)});
    }

    // Add r to support of each eff in potential effects
    for (Signature eff : allFactChanges) {
        _posterior_facts[eff._name_id].insert(eff.abs());
        addToSupport(var(layerIdx, pos+1, eff), rVar);
    }
}

void Encoding::addAction(Action& a, Reduction& parent, Layer& oldLayer, int oldPos, Layer& newLayer, int newPos) {
    Signature childSig = a.getSignature();
    Signature parentSig = parent.getSignature();
    printf("[ENC] addAction(propagate) %s <~ %s\n", Names::to_string(childSig).c_str(), Names::to_string(parentSig).c_str());
    
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
    printf("[ENC] propagateFacts\n");

    for (Signature fact : facts) {
        fact = fact.abs();
        int oldVar = var(oldLayer.index(), oldPos, fact);
        int newVar = var(newLayer.index(), newPos, fact);
        addClause({-oldVar, newVar});
        addClause({oldVar, -newVar});
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
    for (int aVar1 : _current_action_vars) {
        for (int aVar2 : _current_action_vars) {
            if (aVar1 < aVar2) addClause({-aVar1, -aVar2});
        }
    }
    for (int rVar1 : _current_reduction_vars) {
        for (int rVar2 : _current_reduction_vars) {
            if (rVar1 < rVar2) addClause({-rVar1, -rVar2});
        }
    }
    _var_domain_locked = false;

    _current_action_vars.clear();
    _current_reduction_vars.clear();
}

void Encoding::consolidateFacts(Layer& layer, int pos) {
    printf("[ENC] consolidateFacts @ %i\n", pos);
    
    // For each occurring fact
    for (auto pair : _posterior_facts) {
        int predId = pair.first;
        SigSet& facts = pair.second;
        for (Signature fact : facts) {

            // Add substitution literals as necessary
            for (int argPos = 0; argPos < fact._args.size(); argPos++) {
                int arg = fact._args[argPos];
                if (!_htn._q_constants.count(arg)) continue;
                
                // arg is a q-constant

                if (!_q_constants.count(arg)) {
                    // new q-constant: initialize substitution literals

                    // sort of q constant
                    int sort = _htn._signature_sorts_table[fact._name_id][argPos];

                    // for each constant of same sort
                    assert(_htn._constants_by_sort.count(sort));
                    for (int c : _htn._constants_by_sort[sort]) {
                        
                        // Do not include cyclic substitutions
                        if (_htn._q_constants.count(c) && c >= arg) continue;

                        // either of the possible substitutions must be chosen
                        Signature sigSubst = sigSubstitute(arg, c);
                        appendClause({var(layer.index(), pos, sigSubst)});
                        
                        _q_constants_per_arg[c];
                        _q_constants_per_arg[c].push_back(arg);
                    }
                    endClause();

                    // TODO AT MOST ONE substitution??

                    _q_constants.insert(arg);
                }

                // Add equivalence to corresponding facts
                // relative to chosen substitution
                std::unordered_set<Signature, SignatureHasher> substitutedFacts;
                for (auto factsPair : _posterior_facts) {
                    for (Signature actualFact : factsPair.second) {

                        if (actualFact._name_id != fact._name_id) continue;
                        int substArg = actualFact._args[argPos];

                        // All args must be the same except for the qconst position;
                        // at the qconst position, there must be a true constant 
                        // or a qconst of smaller value.
                        bool isSubstitution = !_htn._q_constants.count(substArg) || (substArg < arg);
                        for (int i = 0; i < actualFact._args.size(); i++) {
                            isSubstitution &= (i == argPos) ^ (actualFact._args[i] == fact._args[i]);
                        }
                        if (!isSubstitution) continue;
                        
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
                            Signature sigSubst = sigSubstitute(qConst, arg);
                            appendClause({var(layer.index(), pos, sigSubst)});
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
    printf("[ENC] addAssumptions\n");
    
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
    return ipasir_solve(_solver) == 10;
}

bool Encoding::isEncoded(int layer, int pos, Signature& sig) {
    if (layer >= _variables.size()) return false;
    if (pos >= _variables[layer].size()) return false;
    return _variables[layer][pos].count(sig.abs());
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

std::vector<PlanItem> Encoding::extractClassicalPlan(Layer& finalLayer) {
    int li = finalLayer.index();
    _var_domain_locked = true;

    SigSet state = finalLayer[0].getFacts();
    for (Signature f : state) {
        assert(value(0, 0, f));
    }

    std::vector<PlanItem> plan;
    for (int pos = 0; pos+1 < finalLayer.size(); pos++) {
        printf("%i\n", pos);
        assert(value(li, pos, _sig_primitive) || fail("Position " + std::to_string(pos) + " is not primitive!\n"));

        int addedActions = 0;
        SigSet newState = state;
        SigSet effects;
        for (Signature aSig : finalLayer[pos].getActions()) {

            if (!isEncoded(li, pos, aSig)) continue;
            //printf("  %s ?\n", Names::to_string(aSig).c_str());

            if (value(li, pos, aSig)) {

                // Check fact consistency
                Action& a = _htn._actions_by_sig[aSig];
                for (Signature pre : a.getPreconditions()) {
                    assert(value(li, pos, pre) || fail("Precondition " + Names::to_string(pre) + " of action "
                    + Names::to_string(a) + " does not hold at step " + std::to_string(pos) + "!\n"));
                }
                for (Signature eff : a.getEffects()) {
                    assert(value(li, pos+1, eff) || fail("Effect " + Names::to_string(eff) + " of action "
                    + Names::to_string(a) + " does not hold at step " + std::to_string(pos+1) + "!\n"));

                    // Apply effect
                    eff.negate();
                    if (state.count(eff)) newState.erase(eff);
                    eff.negate();
                    newState.insert(eff);
                    effects.insert(eff);
                }

                addedActions++;
                printf("%s @ %i\n", Names::to_string(aSig).c_str(), pos);
                if (aSig != _htn._action_blank.getSignature()) {
                    plan.push_back({var(li, pos, aSig), aSig, aSig, std::vector<int>()});
                }

            }
        }

        for (Signature sig : newState) {
            assert(value(li, pos+1, sig));
        }
        state = newState;

        assert(addedActions <= 1 || fail("Added " + std::to_string(addedActions) + " actions at step " + std::to_string(pos) + "!\n"));
    }

    return plan;
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
            printf("%i -> %i\n", predPos, pos);

            int itemsThisPos = 0;

            for (Signature rSig : l[pos].getReductions()) {
                if (!isEncoded(i, pos, rSig)) continue;

                if (value(i, pos, rSig)) {
                    itemsThisPos++;

                    // Check preconditions
                    Reduction& r = _htn._reductions_by_sig[rSig];
                    for (Signature pre : r.getPreconditions()) {
                        assert(value(i, pos, pre) || fail("Precondition " + Names::to_string(pre) + " of reduction "
                        + Names::to_string(r) + " does not hold at step " + std::to_string(pos) + "!\n"));
                    }

                    int v = var(i, pos, rSig);
                    itemsNewLayer[pos] = PlanItem({v, _htn._reductions_by_sig[rSig].getTaskSignature(), rSig, std::vector<int>()});

                    // TODO check this is a valid subtask relationship
                    itemsOldLayer[predPos].subtaskIds.push_back(v);
                }
            }

            for (Signature aSig : l[pos].getActions()) {
                if (!isEncoded(i, pos, aSig)) continue;

                if (value(i, pos, aSig)) {
                    itemsThisPos++;

                    if (aSig == _htn._action_blank.getSignature()) continue;
                    
                    // Check preconditions, effects
                    Action& a = _htn._actions_by_sig[aSig];
                    for (Signature pre : a.getPreconditions()) {
                        assert(value(i, pos, pre) || fail("Precondition " + Names::to_string(pre) + " of action "
                        + Names::to_string(aSig) + " does not hold at step " + std::to_string(pos) + "!\n"));
                    }
                    for (Signature eff : a.getEffects()) {
                        assert(value(i, pos+1, eff) || fail("Effect " + Names::to_string(eff) + " of action "
                        + Names::to_string(aSig) + " does not hold at step " + std::to_string(pos+1) + "!\n"));
                    }

                    int v = var(i, pos, aSig);
                    
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

Encoding::~Encoding() {
    ipasir_release(_solver);
}