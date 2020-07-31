
#include <random>

#include "sat/encoding.h"
#include "sat/literal_tree.h"
#include "util/log.h"
#include "util/timer.h"


/*
encodePosition ()
*/

bool beganLine = false;

Encoding::Encoding(Parameters& params, HtnInstance& htn, std::vector<Layer*>& layers) : 
            _params(params), _htn(htn), _layers(layers), _print_formula(params.isNonzero("of")), 
            _use_q_constant_mutexes(_params.getIntParam("qcm") > 0), 
            _implicit_primitiveness(params.isNonzero("ip")) {
    _solver = ipasir_init();
    _substitute_name_id = _htn.nameId("__SUBSTITUTE___");
    _sig_substitution = USignature(_substitute_name_id, std::vector<int>(2));
    if (_print_formula) _out.open("formula.cnf");
    VariableDomain::init(params);
    _num_cls = 0;
    _num_lits = 0;
    _num_asmpts = 0;
}

void Encoding::encode(int layerIdx, int pos) {
    
    Log::v("  Encoding ...\n");
    int priorNumClauses = _num_cls;
    int priorNumLits = _num_lits;

    // Calculate relevant environment of the position
    Position NULL_POS;
    NULL_POS.setPos(-1, -1);
    Layer& newLayer = *_layers.at(layerIdx);
    Position& newPos = newLayer[pos];
    bool hasLeft = pos > 0;
    Position& left = (hasLeft ? newLayer[pos-1] : NULL_POS);
    int oldPos = 0, offset = 0;
    bool hasAbove = layerIdx > 0;
    if (hasAbove) {
        const Layer& oldLayer = *_layers.at(layerIdx-1);
        while (oldPos+1 < oldLayer.size() && oldLayer.getSuccessorPos(oldPos+1) <= pos) 
            oldPos++;
        offset = pos - oldLayer.getSuccessorPos(oldPos);
    }
    Position& above = (hasAbove ? (*_layers.at(layerIdx-1))[oldPos] : NULL_POS);

    // Variable determining whether this is a primitive (i.e. action) position
    int varPrim = varPrimitive(layerIdx, pos);
    
    // Encode all operations (actions and reductions) as variables
    for (const auto& [opSig, opId] : newPos.getOps()) {
        if (opSig == Position::NONE_SIG) continue;
        int opVar = newPos.encodeOp(opSig);

        bool isPrimitive = newPos.isPrimitive(opId);
        if (isPrimitive) {
            if (_implicit_primitiveness) _primitive_ops.push_back(opVar);
            else addClause(-opVar, varPrim);
        } else {
            bool trivialReduction = _htn.getReduction(opSig).getSubtasks().size() == 0;
            if (trivialReduction) {
                // If a trivial reduction occurs, the position is primitive
                if (_implicit_primitiveness) _primitive_ops.push_back(opVar);
                else addClause(-opVar, varPrim);
            } else {
                // If a non-trivial reduction occurs, the position is non-primitive
                if (_implicit_primitiveness) _nonprimitive_ops.push_back(opVar);
                addClause(-opVar, -varPrim);
            }
        }
    }

    // Encode true facts at this position and decide for each fact
    // whether to encode it or to reuse the previous variable
    encodeFactVariables(newPos, left, above, oldPos, offset);

    // Init substitution vars where necessary
    stage("initsubstitutions");
    for (const auto& a : newPos.getOps()) {
        for (int arg : a.first._args) initSubstitutionVars(newPos.getOpVariable(a.second), arg, newPos);
    }
    stage("initsubstitutions");

    // Link qfacts to their possible decodings
    stage("qfactsemantics");
    std::vector<int> substitutionVars; substitutionVars.reserve(128);
    for (const auto& entry : newPos.getQFacts()) for (const auto& qfactSig : entry.second) {
        assert(_htn.hasQConstants(qfactSig));

        int qfactVar = newPos.getFactVariable(qfactSig);

        // Already encoded earlier?
        if (!_new_fact_vars.count(qfactVar)) continue;

        std::vector<int> qargs, qargIndices; 
        for (int aIdx = 0; aIdx < qfactSig._args.size(); aIdx++) if (_htn.isQConstant(qfactSig._args[aIdx])) {
            qargs.push_back(qfactSig._args[aIdx]);
            qargIndices.push_back(aIdx);
        } 

        // For each possible fact decoding:
        for (const auto& decFactSig : _htn.getQFactDecodings(qfactSig)) {
            //if (!newPos.hasFact(decFactSig)) continue;

            // TODO Need to check if this fact decoding can occur here?

            if (_use_q_constant_mutexes) {
                // Check if this decoding is valid
                std::vector<int> decArgs; for (int idx : qargIndices) decArgs.push_back(decFactSig._args[idx]);
                if (!_htn.getQConstantDatabase().test(qargs, decArgs)) continue;
            }

            int decFactVar = newPos.getFactVariable(decFactSig);
            for (const auto& pair : Substitution(qfactSig._args, decFactSig._args)) {
                substitutionVars.push_back(varSubstitution(sigSubstitute(pair.first, pair.second)));
            }
            
            // If the substitution is chosen,
            // the q-fact and the corresponding actual fact are equivalent
            for (int sign = -1; sign <= 1; sign += 2) {
                for (const int& varSubst : substitutionVars) {
                    appendClause(-varSubst);
                }
                appendClause(sign*qfactVar, -sign*decFactVar);
                endClause();
            }
            substitutionVars.clear();
        }
    }
    stage("qfactsemantics");

    // Effects of "old" actions to the left
    stage("actioneffects");
    for (const auto& [aSig, aId] : left.getOps()) if (left.isPrimitive(aId)) {
        if (aSig == Position::NONE_SIG) continue;
        int aVar = left.getOpVariable(aSig);

        for (const Signature& eff : _htn.getAction(aSig).getEffects()) {
            
            std::vector<int> unifiersDnf;
            bool unifiedUnconditionally = false;
            if (eff._negated) {
                for (const auto& posEff : _htn.getAction(aSig).getEffects()) {
                    if (posEff._negated) continue;
                    if (posEff._usig._name_id != eff._usig._name_id) continue;
                    bool fits = true;
                    std::vector<int> s;
                    for (int i = 0; i < eff._usig._args.size(); i++) {
                        const int& effArg = eff._usig._args[i];
                        const int& posEffArg = posEff._usig._args[i];
                        if (effArg != posEffArg) {
                            bool effIsQ = _q_constants.count(effArg);
                            bool posEffIsQ = _q_constants.count(posEffArg);
                            if (effIsQ && posEffIsQ) {
                                s.push_back(varQConstEquality(effArg, posEffArg));
                            } else if (effIsQ) {
                                if (!_htn.getDomainOfQConstant(effArg).count(posEffArg)) fits = false;
                                else s.push_back(varSubstitution(sigSubstitute(effArg, posEffArg)));
                            } else if (posEffIsQ) {
                                if (!_htn.getDomainOfQConstant(posEffArg).count(effArg)) fits = false;
                                else s.push_back(varSubstitution(sigSubstitute(posEffArg, effArg)));
                            } else fits = false;
                        }
                    }
                    if (fits && s.empty()) {
                        // Empty substitution does the job
                        unifiedUnconditionally = true;
                        break;
                    }
                    s.push_back(0);
                    if (fits) unifiersDnf.insert(unifiersDnf.end(), s.begin(), s.end());
                }
            }
            if (unifiedUnconditionally) {
                //addClause(-aVar, -getVariable(newPos, eff._usig));
            } else if (unifiersDnf.empty()) {
                addClause(-aVar, (eff._negated?-1:1)*newPos.getFactVariable(eff._usig));
            } else {
                auto cnf = getCnf(unifiersDnf);
                for (const auto& clause : cnf) {
                    appendClause(-aVar, -newPos.getFactVariable(eff._usig));
                    for (int lit : clause) appendClause(lit);
                    endClause();
                }
            }
        }
    }
    stage("actioneffects");

    // Store all operations occurring here, for one big clause ORing them
    std::vector<int> aloElemClause(newPos.getOps().size(), 0);
    int numOccurringOps = 0;

    // preconditions
    for (const auto& [opSig, opId] : newPos.getOps()) {
        if (opSig == Position::NONE_SIG) continue;

        int opVar = newPos.getOpVariable(opId);
        aloElemClause[numOccurringOps++] = opVar;

        // Preconditions
        stage("preconditions");
        for (const Signature& pre : newPos.isPrimitive(opId) ? 
                _htn.getAction(opSig).getPreconditions() : _htn.getReduction(opSig).getPreconditions()
            ) {
            addClause(-opVar, (pre._negated?-1:1)*newPos.getFactVariable(pre._usig));
        }
        stage("preconditions");

        encodeSubstitutionConstraints(newPos, above, opSig);
    }
    
    if (numOccurringOps == 0 && pos+1 != newLayer.size()) {
        Log::e("No operations to encode at (%i,%i)! Considering this problem UNSOLVABLE.\n", layerIdx, pos);
        exit(1);
    }

    stage("atleastoneelement");
    int i = 0; 
    while (i < aloElemClause.size() && aloElemClause[i] != 0) 
        appendClause(aloElemClause[i++]);
    endClause();
    stage("atleastoneelement");

    stage("atmostoneelement");
    for (int i = 0; i < aloElemClause.size(); i++) {
        for (int j = i+1; j < aloElemClause.size(); j++) {
            if (aloElemClause[i] != 0 && aloElemClause[j] != 0)
                addClause(-aloElemClause[i], -aloElemClause[j]);
        }
    }
    stage("atmostoneelement");

    if (_params.isNonzero("svp")) setVariablePhases(aloElemClause);

    // Q-constants type constraints
    stage("qtypeconstraints");
    const auto& constraints = newPos.getQConstantsTypeConstraints();
    for (const auto& pair : constraints) {
        const USignature& opSig = pair.first;
        int opVar = newPos.getOpVariableOrZero(opSig);
        if (opVar != 0) {
            for (const TypeConstraint& c : pair.second) {
                int qconst = c.qconstant;
                bool positiveConstraint = c.sign;
                assert(_q_constants.count(qconst));

                if (positiveConstraint) {
                    // EITHER of the GOOD constants - one big clause
                    appendClause(-opVar);
                    for (int cnst : c.constants) {
                        appendClause(varSubstitution(sigSubstitute(qconst, cnst)));
                    }
                    endClause();
                } else {
                    // NEITHER of the BAD constants - many 2-clauses
                    for (int cnst : c.constants) {
                        addClause(-opVar, -varSubstitution(sigSubstitute(qconst, cnst)));
                    }
                }
            }
        }
    }
    stage("qtypeconstraints");

    // expansions
    for (const auto& [parent, opId] : above.getOps()) {
        if (parent == Position::NONE_SIG) continue;
        stage("expansions");
        for (const auto& child : above.getExpansions(parent, offset)) {
            if (child == Position::NONE_SIG) {
                // Forbid parent op that turned out to be impossible
                int oldOpVar = above.getOpVariable(parent);
                addClause(-oldOpVar);
                break;
            }
        }
        appendClause(-above.getOpVariable(parent));
        for (const auto& child : above.getExpansions(parent, offset)) {
            if (child == Position::NONE_SIG) continue;
            Log::d("EXPANSION %s %s\n", TOSTR(parent), TOSTR(child));
            appendClause(newPos.getOpVariable(child));
        }
        endClause();
        stage("expansions");
    }

    // Globally forbidden substitutions
    stage("substitutionconstraints");
    for (const auto& sub : _htn.getForbiddenSubstitutions()) {
        assert(!sub.empty());
        if (_forbidden_substitutions.count(sub)) continue;
        for (const auto& [src, dest] : sub) {
            appendClause(-varSubstitution(sigSubstitute(src, dest)));
        }
        endClause();
        _forbidden_substitutions.insert(sub);
    }
    _htn.clearForbiddenSubstitutions();
    stage("substitutionconstraints");

    Log::v("Encoding done. (%i clauses, total of %i literals)\n", (_num_cls-priorNumClauses), (_num_lits-priorNumLits));

    left.clearAtPastPosition();

    if (layerIdx == 0 || offset != 0) return;
    
    Position* positionToClear = nullptr;
    if (oldPos == 0) {
        // Clear rightmost position of "above above" layer
        if (layerIdx > 1) positionToClear = &_layers.at(layerIdx-2)->at(_layers.at(layerIdx-2)->size()-1);
    } else {
        // Clear previous parent position of "above" layer
        positionToClear = &_layers.at(layerIdx-1)->at(oldPos-1);
    }
    if (positionToClear != nullptr) {
        Log::v("  Freeing memory of (%i,%i) ...\n", positionToClear->getLayerIndex(), positionToClear->getPositionIndex());
        positionToClear->clearAtPastLayer();
    }
}

void Encoding::addAssumptions(int layerIdx) {
    Layer& l = *_layers.at(layerIdx);
    if (_implicit_primitiveness) {
        stage("assumptions");
        for (int pos = 0; pos < l.size(); pos++) {
            appendClause(-varPrimitive(layerIdx, pos));
            for (int var : _primitive_ops) appendClause(var);
            endClause();
        }
        stage("assumptions");
    }
    for (int pos = 0; pos < l.size(); pos++) {
        assume(varPrimitive(layerIdx, pos));
    }
}

void Encoding::setTerminateCallback(void * state, int (*terminate)(void * state)) {
    ipasir_set_terminate(_solver, state, terminate);
}

void Encoding::encodeFactVariables(Position& newPos, const Position& left, Position& above, int oldPos, int offset) {

    _new_fact_vars.clear();

    stage("factvarencoding");
    // Reuse variables from above position
    int reusedFacts = 0;
    if (newPos.getLayerIndex() > 0 && offset == 0) {
        for (const auto& [sig, factId] : above.getFacts()) {
            int var = above.getFactVariable(factId);
            newPos.setFactVariable(sig, var);
            reusedFacts++;
        }
    }
    stage("factvarencoding");

    if (newPos.getPositionIndex() == 0) {
        // Initialize all facts
        for (const auto& fact : newPos.getTrueFacts()) _new_fact_vars.insert(newPos.encodeFact(fact));
        for (const auto& fact : newPos.getFalseFacts()) _new_fact_vars.insert(newPos.encodeFact(fact));
    } else {
        // Encode frame axioms which will assign variables to all "normal" facts
        encodeFrameAxioms(newPos, left);
    }

    // Encode q-facts that are not encoded yet
    for ([[maybe_unused]] const auto& [nameId, qfacts] : newPos.getQFacts()) for (const auto& qfact : qfacts) {
        int qfactId = newPos.getFactIdOrNegative(qfact);
        if (qfactId >= 0 && newPos.hasFactVariable(qfactId)) continue;

        int leftVar = left.getFactVariableOrZero(qfact);
        bool reuseFromLeft = leftVar != 0 
                && newPos.getPosFactSupport(qfact).empty()
                && newPos.getNegFactSupport(qfact).empty();
        if (reuseFromLeft) for (const auto& decFact : _htn.getQFactDecodings(qfact)) {
            int decFactVar = newPos.getFactVariable(decFact);
            if (_new_fact_vars.count(decFactVar)) {
                reuseFromLeft = false;
                break;
            }
        }
        
        if (reuseFromLeft) {
            if (qfactId >= 0) newPos.setFactVariable(qfactId, leftVar);
            else newPos.setFactVariable(qfact, leftVar); 
        } 
        else _new_fact_vars.insert(qfactId >= 0 ? newPos.encodeFact(qfactId) : newPos.encodeFact(qfact));
    }

    // Facts that must hold at this position
    stage("truefacts");
    const USigSet* cHere[] = {&newPos.getTrueFacts(), &newPos.getFalseFacts()}; 
    for (int i = 0; i < 2; i++) 
    for (const USignature& factSig : *cHere[i]) {
        int var = newPos.getFactVariableOrZero(factSig);
        if (var == 0) {
            // Variable is not encoded yet.
            addClause((i == 0 ? 1 : -1) * newPos.encodeFact(factSig));
        } else {
            // Variable is already encoded.
            if (_new_fact_vars.count(var))
                addClause((i == 0 ? 1 : -1) * var);
        }
    }
    stage("truefacts");
}

void Encoding::encodeFrameAxioms(Position& newPos, const Position& left) {

    // Fact supports, frame axioms (only for non-new facts free of q-constants)
    stage("frameaxioms");

    bool nonprimFactSupport = _params.isNonzero("nps");

    int layerIdx = newPos.getLayerIndex();
    int pos = newPos.getPositionIndex();
    int prevVarPrim = varPrimitive(layerIdx, pos-1);

    std::vector<int> dnfSubs;

    for ([[maybe_unused]] const auto& [fact, leftId] : left.getFacts()) {
        if (_htn.hasQConstants(fact)) continue;
        
        const USigSet* directSupports[2] = {&newPos.getNegFactSupport(fact), &newPos.getPosFactSupport(fact)};
        FlatHashSet<int> indirectSupports[2];
        int oldFactVars[2] = {-left.getFactVariable(leftId), 0};
        oldFactVars[1] = -oldFactVars[0];

        // Do not commit on encoding the new fact yet, except if the variable already exists
        int rightId = newPos.getFactIdOrNegative(fact);
        int factVar = rightId >= 0 ? newPos.getFactVariableOrZero(rightId) : 0;
        if (oldFactVars[1] == factVar) continue;

        // Calculate indirect fact support for both polarities
        int i = -1;
        for (int sign = -1; sign <= 1; sign += 2) {
            i++;
            factVar *= -1; 

            // Calculate indirect support through qfact abstractions
            for (const USignature& qsig : newPos.getQFacts(fact._name_id)) {
                if (newPos.getFactSupport(qsig, sign<0).empty() || !_htn.isAbstraction(fact, qsig)) continue;

                // For each operation that supports some qfact abstraction of the fact:
                for (const USignature& opSig : newPos.getFactSupport(qsig, sign<0)) {
                    if (opSig == Position::NONE_SIG) continue;
                    int opVar = left.getOpVariable(opSig);
                    assert(opVar > 0);
                    
                    // Calculate and encode prerequisites for indirect support

                    // Find valid sets of substitutions for this operation causing the desired effect
                    auto subs = _htn.getInstantiator().getOperationSubstitutionsCausingEffect(
                        left.getFactChanges(opSig), fact, sign<0);
                    //assert(subMap.count(opSig));
                    if (subs.empty()) {
                        // No valid instantiations!
                        continue;
                    }

                    // Assemble possible substitution options to get the desired fact support
                    std::set<std::set<int>> substOptions;
                    bool unconditionalEffect = false;
                    for (const Substitution& s : subs) {
                        if (s.empty()) {
                            // Empty substitution does the job
                            unconditionalEffect = true;
                            break;
                        }
                        // An actual substitution is necessary
                        std::set<int> substOpt;
                        for (const auto& entry : s) {
                            int substVar = varSubstitution(sigSubstitute(entry.first, entry.second));
                            substOpt.insert(substVar);
                        }
                        substOptions.insert(substOpt);
                    }

                    // Add operation to indirect support
                    indirectSupports[i].insert(opVar); 
                    
                    if (unconditionalEffect) continue;

                    // Bring the found substitution sets to CNF and encode them
                    for (const auto& set : substOptions) {
                        dnfSubs.insert(dnfSubs.end(), set.begin(), set.end());
                        dnfSubs.push_back(0);
                    }
                    std::set<std::set<int>> cnfSubs = getCnf(dnfSubs);
                    dnfSubs.clear();
                    for (const std::set<int>& subsCls : cnfSubs) {
                        // IF fact change AND the operation is applied,
                        if (oldFactVars[i] != 0) appendClause(oldFactVars[i]);
                        if (factVar == 0) {
                            // Initialize fact variable, now that it is known 
                            // that there is some support for it to change
                            if (rightId < 0) rightId = newPos.factId(fact);
                            int v = newPos.encodeFact(rightId);
                            _new_fact_vars.insert(v);
                            factVar = sign*v;
                        }
                        appendClause(-factVar, -opVar);
                        if (!nonprimFactSupport) {
                            if (_implicit_primitiveness) {
                                for (int var : _nonprimitive_ops) appendClause(var);
                            } else appendClause(-prevVarPrim);
                        } 
                        // THEN either of the valid substitution combinations
                        for (int subVar : subsCls) appendClause(subVar); 
                        endClause();
                    }
                }
            }
        }

        // Retrieve (and possibly encode) fact variable
        if (factVar == 0) {
            if (rightId < 0) rightId = newPos.factId(fact);
            int leftVar = left.getFactVariableOrZero(leftId);
            if (leftVar != 0
                    && indirectSupports[0].empty() && indirectSupports[1].empty() 
                    && directSupports[0]->empty() && directSupports[1]->empty()) {
                // No support for this (yet unencoded) variable: reuse it from left position
                factVar = leftVar;
                newPos.setFactVariable(rightId, leftVar);
                continue; // Skip frame axiom encoding
            } else {
                // There is some support: use a new variable
                factVar = newPos.encodeFact(rightId);
                _new_fact_vars.insert(factVar);
            }
        }

        if (factVar == oldFactVars[1]) continue;

        // Encode frame axioms for this fact
        i = -1;
        for (int sign = -1; sign <= 1; sign += 2) {
            i++;
            // Fact change:
            if (oldFactVars[i] != 0) appendClause(oldFactVars[i]);
            appendClause(-sign*factVar);
            if (!newPos.getFactSupport(fact, sign<0).empty() || !indirectSupports[i].empty()) {
                // Non-primitiveness wildcard
                if (!nonprimFactSupport) {
                    if (_implicit_primitiveness) {
                        for (int var : _nonprimitive_ops) appendClause(var);
                    } else appendClause(-prevVarPrim);
                }
                // DIRECT support
                for (const USignature& opSig : *directSupports[i]) {
                        if (opSig == Position::NONE_SIG) continue;
                        int opVar = left.getOpVariable(opSig);
                        assert(opVar > 0);
                        appendClause(opVar);
                }
                // INDIRECT support
                for (int opVar : indirectSupports[i]) appendClause(opVar);
            }
            endClause();
        }
    }

    stage("frameaxioms");
}

void Encoding::initSubstitutionVars(int opVar, int arg, Position& pos) {

    if (_q_constants.count(arg)) return;
    if (!_htn.isQConstant(arg)) return;
    // arg is a *new* q-constant: initialize substitution logic

    _q_constants.insert(arg);

    std::vector<int> substitutionVars;
    for (int c : _htn.getDomainOfQConstant(arg)) {

        assert(!_htn.isVariable(c));

        // either of the possible substitutions must be chosen
        int varSubst = varSubstitution(sigSubstitute(arg, c));
        substitutionVars.push_back(varSubst);
    }
    assert(!substitutionVars.empty());

    // AT LEAST ONE substitution, or the parent op does NOT occur
    Log::d("INITSUBVARS @(%i,%i) op=%i qc=%s\n", pos.getLayerIndex(), pos.getPositionIndex(), opVar, TOSTR(arg));
    appendClause(-opVar);
    for (int vSub : substitutionVars) appendClause(vSub);
    endClause();

    // AT MOST ONE substitution
    for (int vSub1 : substitutionVars) {
        for (int vSub2 : substitutionVars) {
            if (vSub1 < vSub2) addClause(-vSub1, -vSub2);
        }
    }

    if (_params.isNonzero("svp")) setVariablePhases(substitutionVars);
}

void Encoding::encodeSubstitutionConstraints(Position& newPos, Position& above, const USignature& opSig) {

    int opVar = newPos.getOpVariable(opSig);
    
    stage("substitutionconstraints");
    for (const auto& s : newPos.getForbiddenSubstitutions(opSig)) {
        appendClause(-opVar);
        for (const auto& [src, dest] : s) {
            appendClause(-varSubstitution(sigSubstitute(src, dest)));
        }
        endClause();
    }
    for (const auto& subs : newPos.getValidSubstitutions(opSig)) {
        // Build literal tree from this set of valid substitution options for this op
        LiteralTree tree;
        // For each substitution option:
        for (const auto& s : subs) {
            std::vector<int> sVars(s.size()); 
            int i = 0;
            // For each atomic substitution:
            for (const auto& [src, dest] : s) {
                sVars[i++] = varSubstitution(sigSubstitute(src, dest));
            }
            tree.insert(sVars);
        }
        // Encode set of valid substitution options into CNF
        std::vector<std::vector<int>> cls = tree.encode(
            std::vector<int>(1, newPos.getOpVariable(opSig))
        );
        for (const auto& c : cls) {
            addClause(c);
        }
    }
    stage("substitutionconstraints");

    // predecessors
    if (newPos.getLayerIndex() > 0 && _params.isNonzero("p")) {
        stage("predecessors");
        appendClause(-opVar);
        for (const auto& parent : newPos.getPredecessors(opSig)) {
            appendClause(above.getOpVariable(parent));
        }
        endClause();
        stage("predecessors");
    }
}

void Encoding::setVariablePhases(const std::vector<int>& vars) {
    // Choose one positive phase variable at random, set negative phase for all others
    std::default_random_engine generator;
    std::uniform_int_distribution<int> distribution(0, vars.size()-1);
    int randomIdx = distribution(generator);
    for (int i = 0; i < vars.size(); i++) {
        ipasir_set_phase(_solver, vars[i], i == randomIdx);
    }
}

std::set<std::set<int>> Encoding::getCnf(const std::vector<int>& dnf) {
    std::set<std::set<int>> cnf;

    if (dnf.empty()) return cnf;

    int size = 1;
    int clsSize = 0;
    std::vector<int> clsStarts;
    for (int i = 0; i < dnf.size(); i++) {
        if (dnf[i] == 0) {
            size *= clsSize;
            clsSize = 0;
        } else {
            if (clsSize == 0) clsStarts.push_back(i);
            clsSize++;
        }
    }
    assert(size > 0);

    // Iterate over all possible combinations
    std::vector<int> counter(clsStarts.size(), 0);
    while (true) {
        // Assemble the combination
        std::set<int> newCls;
        for (int pos = 0; pos < counter.size(); pos++) {
            const int& lit = dnf[clsStarts[pos]+counter[pos]];
            assert(lit != 0);
            newCls.insert(lit);
        }
        cnf.insert(newCls);            

        // Increment exponential counter
        int x = 0;
        while (x < counter.size()) {
            if (dnf[clsStarts[x]+counter[x]+1] == 0) {
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

    if (cnf.size() > 1000) Log::w("CNF of size %i generated\n", cnf.size());
    //else Log::d("CNF of size %i generated\n", cnf.size());

    return cnf;
}

void Encoding::addClause(int lit) {
    assert(lit != 0);
    ipasir_add(_solver, lit); ipasir_add(_solver, 0);
    if (_print_formula) _out << lit << " 0\n";
    _num_lits++; _num_cls++;
}
void Encoding::addClause(int lit1, int lit2) {
    assert(lit1 != 0 && lit2 != 0);
    ipasir_add(_solver, lit1); ipasir_add(_solver, lit2); ipasir_add(_solver, 0);
    if (_print_formula) _out << lit1 << " " << lit2 << " 0\n";
    _num_lits += 2; _num_cls++;
}
void Encoding::addClause(int lit1, int lit2, int lit3) {
    assert(lit1 != 0 && lit2 != 0 && lit3 != 0);
    ipasir_add(_solver, lit1); ipasir_add(_solver, lit2); ipasir_add(_solver, lit3); ipasir_add(_solver, 0);
    if (_print_formula) _out << lit1 << " " << lit2 << " " << lit3 << " 0\n";
    _num_lits += 3; _num_cls++;
}
void Encoding::addClause(const std::initializer_list<int>& lits) {
    for (int lit : lits) {
        assert(lit != 0);
        ipasir_add(_solver, lit);
        if (_print_formula) _out << lit << " ";
    } 
    ipasir_add(_solver, 0);
    if (_print_formula) _out << "0\n";
    _num_cls++;
    _num_lits += lits.size();
}
void Encoding::addClause(const std::vector<int>& cls) {
    for (int lit : cls) {
        assert(lit != 0);
        ipasir_add(_solver, lit);
        if (_print_formula) _out << lit << " ";
    } 
    ipasir_add(_solver, 0);
    if (_print_formula) _out << "0\n";
    _num_cls++;
    _num_lits += cls.size();
}
void Encoding::appendClause(int lit) {
    if (!beganLine) beganLine = true;
    assert(lit != 0);
    ipasir_add(_solver, lit);
    if (_print_formula) _out << lit << " ";
    _num_lits++;
}
void Encoding::appendClause(int lit1, int lit2) {
    if (!beganLine) beganLine = true;
    assert(lit1 != 0 && lit2 != 0);
    ipasir_add(_solver, lit1); ipasir_add(_solver, lit2);
    if (_print_formula) _out << lit1 << " " << lit2 << " ";
    _num_lits += 2;
}
void Encoding::appendClause(const std::initializer_list<int>& lits) {
    if (!beganLine) {
        //log("CNF ");
        beganLine = true;
    }
    for (int lit : lits) {
        assert(lit != 0);
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

void onClauseLearnt(void* state, int* cls) {
    std::string str = "";
    int i = 0; while (cls[i] != 0) str += std::to_string(cls[i++]) + " ";
    Log::d("LEARNT_CLAUSE %s\n", str.c_str());
}

int Encoding::solve() {
    Log::i("Attempting to solve formula with %i clauses (%i literals) and %i assumptions\n", 
                _num_cls, _num_lits, _num_asmpts);
    
    ipasir_set_learn(_solver, this, /*maxLength=*/1, onClauseLearnt);

    //for (const int& v: _no_decision_variables) ipasir_set_decision_var(_solver, v, false);
    _no_decision_variables.clear();
    _primitive_ops.clear();
    _nonprimitive_ops.clear();

    _sat_call_start_time = Timer::elapsedSeconds();
    int result = ipasir_solve(_solver);
    _sat_call_start_time = 0;

    if (_num_asmpts == 0) _last_assumptions.clear();
    _num_asmpts = 0;
    return result;
}

float Encoding::getTimeSinceSatCallStart() {
    if (_sat_call_start_time == 0) return 0;
    return Timer::elapsedSeconds() - _sat_call_start_time;
}

int Encoding::varSubstitution(const USignature& sigSubst) {

    int var;
    if (!_substitution_variables.count(sigSubst)) {
        assert(!VariableDomain::isLocked() || Log::e("Unknown substitution variable %s queried!\n", TOSTR(sigSubst)));
        var = VariableDomain::nextVar();
        _substitution_variables[sigSubst] = var;
        VariableDomain::printVar(var, -1, -1, sigSubst);
        //_no_decision_variables.push_back(var);
    } else var = _substitution_variables[sigSubst];
    return var;
}

int Encoding::varQConstEquality(int q1, int q2) {
    std::pair<int, int> qPair(std::min(q1, q2), std::max(q1, q2));
    if (!_q_equality_variables.count(qPair)) {
        
        stage("qconstequality");
        FlatHashSet<int> good, bad1, bad2;
        for (int c : _htn.getDomainOfQConstant(q1)) {
            if (!_htn.getDomainOfQConstant(q2).count(c)) bad1.insert(c);
            else good.insert(c);
        }
        for (int c : _htn.getDomainOfQConstant(q2)) {
            if (_htn.getDomainOfQConstant(q1).count(c)) continue;
            bad2.insert(c);
        }
        int varEq = VariableDomain::nextVar();
        _no_decision_variables.push_back(varEq);
        if (good.empty()) {
            // Domains are incompatible -- equality never holds
            addClause(-varEq);
        } else {
            // If equality, then all "good" substitution vars are equivalent
            for (int c : good) {
                addClause(-varEq, varSubstitution(sigSubstitute(q1, c)), -varSubstitution(sigSubstitute(q2, c)));
                addClause(-varEq, -varSubstitution(sigSubstitute(q1, c)), varSubstitution(sigSubstitute(q2, c)));
            }
            // Any of the GOOD ones
            for (int c : good) addClause(-varSubstitution(sigSubstitute(q1, c)), -varSubstitution(sigSubstitute(q2, c)), varEq);
            // None of the BAD ones
            for (int c : bad1) addClause(-varSubstitution(sigSubstitute(q1, c)), -varEq);
            for (int c : bad2) addClause(-varSubstitution(sigSubstitute(q2, c)), -varEq);
        }
        stage("qconstequality");

        _q_equality_variables[qPair] = varEq;
    }
    return _q_equality_variables[qPair];
}

const USignature& Encoding::sigSubstitute(int qConstId, int trueConstId) {
    //assert(!_htn.isQConstant(trueConstId) || trueConstId < qConstId);
    auto& args = _sig_substitution._args;
    assert(_htn.isQConstant(qConstId));
    assert(!_htn.isQConstant(trueConstId));
    args[0] = qConstId;
    args[1] = trueConstId;
    return _sig_substitution;
}

std::string Encoding::varName(int layer, int pos, const USignature& sig) {
    return VariableDomain::varName(layer, pos, sig);
}

void Encoding::printVar(int layer, int pos, const USignature& sig) {
    Log::d("%s\n", VariableDomain::varName(layer, pos, sig).c_str());
}

int Encoding::varPrimitive(int layer, int pos) {
    return _layers.at(layer)->at(pos).encodePrimitiveness();
}

void Encoding::printFailedVars(Layer& layer) {
    Log::d("FAILED ");
    for (int pos = 0; pos < layer.size(); pos++) {
        int v = varPrimitive(layer.index(), pos);
        if (ipasir_failed(_solver, v)) Log::d("%i ", v);
    }
    Log::d("\n");
}

std::vector<PlanItem> Encoding::extractClassicalPlan() {

    Layer& finalLayer = *_layers.back();
    int li = finalLayer.index();
    VariableDomain::lock();

    std::vector<PlanItem> plan(finalLayer.size());
    //log("(actions at layer %i)\n", li);
    for (int pos = 0; pos < finalLayer.size(); pos++) {
        //log("%i\n", pos);
        Position& p = finalLayer.at(pos);
        if (!_implicit_primitiveness) assert(ipasir_val(_solver, p.encodePrimitiveness()) > 0 
                    || Log::e("Plan error: Position %i is not primitive!\n", pos));

        int chosenActions = 0;
        //State newState = state;
        SigSet effects;
        for (const auto& [aSig, opId] : p.getOps()) if (p.isPrimitive(opId)) {
            
            if (!p.hasOpVariable(aSig)) continue;
            //log("  %s ?\n", TOSTR(aSig));

            if (opValue(li, pos, aSig)) {
                chosenActions++;
                int aVar = p.getOpVariable(aSig);

                // Check fact consistency
                //checkAndApply(_htn._actions_by_sig[aSig], state, newState, li, pos);

                //if (aSig == _htn._action_blank.getSignature()) continue;

                // Decode q constants
                USignature aDec = getDecodedQOp(li, pos, aSig);
                if (aDec == Position::NONE_SIG) continue;

                if (aDec != aSig) {

                    const Action& a = _htn.getAction(aSig);
                    HtnOp opDecoded = a.substitute(Substitution(a.getArguments(), aDec._args));
                    Action aDecoded = (Action) opDecoded;

                    // Check fact consistency w.r.t. "actual" decoded action
                    //checkAndApply(aDecoded, state, newState, li, pos);
                }

                //Log::d("* %s @ %i\n", TOSTR(aDec), pos);
                plan[pos] = {aVar, aDec, aDec, std::vector<int>()};
            }
        }

        //for (Signature sig : newState) {
        //    assert(value(li, pos+1, sig));
        //}
        //state = newState;

        assert(chosenActions <= 1 || Log::e("Plan error: Added %i actions at step %i!\n", chosenActions, pos));
        if (chosenActions == 0) {
            plan[pos] = {-1, USignature(), USignature(), std::vector<int>()};
        }
    }

    //log("%i actions at final layer of size %i\n", plan.size(), _layers->back().size());
    return plan;
}

std::pair<std::vector<PlanItem>, std::vector<PlanItem>> Encoding::extractPlan() {

    auto result = std::pair<std::vector<PlanItem>, std::vector<PlanItem>>();
    auto& [classicalPlan, plan] = result;
    classicalPlan = extractClassicalPlan();
    
    std::vector<PlanItem> itemsOldLayer, itemsNewLayer;

    for (int layerIdx = 0; layerIdx < _layers.size(); layerIdx++) {
        Layer& l = *_layers.at(layerIdx);
        //log("(decomps at layer %i)\n", l.index());

        itemsNewLayer.resize(l.size());
        
        for (int pos = 0; pos < l.size(); pos++) {

            Position& p = l.at(pos);

            int predPos = 0;
            if (layerIdx > 0) {
                Layer& lastLayer = *_layers.at(layerIdx-1);
                while (predPos+1 < lastLayer.size() && lastLayer.getSuccessorPos(predPos+1) <= pos) 
                    predPos++;
            } 
            //log("%i -> %i\n", predPos, pos);

            int actionsThisPos = 0;
            int reductionsThisPos = 0;

            for (const auto& [rSig, opId] : p.getOps()) {

                if (!p.isPrimitive(opId)) {

                    if (!p.hasOpVariable(rSig) || rSig == Position::NONE_SIG) continue;

                    //log("? %s @ (%i,%i)\n", TOSTR(rSig), i, pos);

                    if (opValue(layerIdx, pos, rSig)) {

                        int v = p.getOpVariable(rSig);
                        const Reduction& r = _htn.getReduction(rSig);

                        // Check preconditions
                        /*
                        for (const Signature& pre : r.getPreconditions()) {
                            assert(value(layerIdx, pos, pre) || fail("Precondition " + Names::to_string(pre) + " of reduction "
                            + Names::to_string(r.getSignature()) + " does not hold at step " + std::to_string(pos) + "!\n"));
                        }*/

                        //log("%s:%s @ (%i,%i)\n", TOSTR(r.getTaskSignature()), TOSTR(rSig), layerIdx, pos);
                        USignature decRSig = getDecodedQOp(layerIdx, pos, rSig);
                        if (decRSig == Position::NONE_SIG) continue;

                        Reduction rDecoded = r.substituteRed(Substitution(r.getArguments(), decRSig._args));
                        Log::d("[%i] %s:%s @ (%i,%i)\n", v, TOSTR(rDecoded.getTaskSignature()), TOSTR(decRSig), layerIdx, pos);

                        if (layerIdx == 0) {
                            // Initial reduction
                            PlanItem root(0, 
                                USignature(_htn.nameId("root"), std::vector<int>()), 
                                decRSig, std::vector<int>());
                            itemsNewLayer[0] = root;
                            reductionsThisPos++;
                            continue;
                        }

                        // Lookup parent reduction
                        Reduction parentRed;
                        int offset = pos - _layers.at(layerIdx-1)->getSuccessorPos(predPos);
                        PlanItem& parent = itemsOldLayer[predPos];
                        assert(parent.id >= 0 || Log::e("Plan error: No parent at %i,%i!\n", layerIdx-1, predPos));
                        assert(_htn.isReduction(parent.reduction) || 
                            Log::e("Plan error: Invalid reduction id=%i at %i,%i!\n", parent.reduction._name_id, layerIdx-1, predPos));

                        parentRed = _htn.toReduction(parent.reduction._name_id, parent.reduction._args);

                        // Is the current reduction a proper subtask?
                        assert(offset < parentRed.getSubtasks().size());
                        if (parentRed.getSubtasks()[offset] == rDecoded.getTaskSignature()) {
                            if (itemsOldLayer[predPos].subtaskIds.size() > offset) {
                                // This subtask has already been written!
                                Log::d(" -- is a redundant child -> dismiss\n");
                                continue;
                            }
                            itemsNewLayer[pos] = PlanItem(v, rDecoded.getTaskSignature(), decRSig, std::vector<int>());
                            itemsOldLayer[predPos].subtaskIds.push_back(v);
                            reductionsThisPos++;
                        } else {
                            Log::d(" -- invalid : %s != %s\n", TOSTR(parentRed.getSubtasks()[offset]), TOSTR(rDecoded.getTaskSignature()));
                        } 
                    }

                } else {

                    const USignature& aSig = rSig;
                    if (!p.hasOpVariable(aSig)) continue;

                    if (opValue(layerIdx, pos, aSig)) {
                        actionsThisPos++;

                        if (aSig == HtnInstance::BLANK_ACTION.getSignature()) continue;
                        
                        int v = p.getOpVariable(aSig);
                        Action a = _htn.getAction(aSig);

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

                        Log::d("[%i] %s @ (%i,%i)\n", v, TOSTR(aSig), layerIdx, pos);                    

                        // Find the actual action variable at the final layer, not at this (inner) layer
                        int l = layerIdx;
                        int aPos = pos;
                        while (l+1 < _layers.size()) {
                            //log("(%i,%i) => ", l, aPos);
                            aPos = _layers.at(l)->getSuccessorPos(aPos);
                            l++;
                            //log("(%i,%i)\n", l, aPos);
                        }
                        v = classicalPlan[aPos].id; // *_layers.at(l-1)[aPos].getVariable(aSig);
                        assert(v > 0 || Log::e("%s : v=%i\n", TOSTR(aSig), v));

                        //itemsNewLayer[pos] = PlanItem({v, aSig, aSig, std::vector<int>()});
                        if (layerIdx > 0) itemsOldLayer[predPos].subtaskIds.push_back(v);
                    }

                }   
            }

            // At least an item per position 
            assert( (actionsThisPos+reductionsThisPos >= 1)
            || Log::e("Plan error: %i ops at (%i,%i)!\n", actionsThisPos+reductionsThisPos, layerIdx, pos));
            
            // At most one action per position
            assert(actionsThisPos <= 1 || Log::e("Plan error: %i actions at (%i,%i)!\n", actionsThisPos, layerIdx, pos));

            // Either actions OR reductions per position (not both)
            assert(actionsThisPos == 0 || reductionsThisPos == 0 
            || Log::e("Plan error: %i actions and %i reductions at (%i,%i)!\n", actionsThisPos, reductionsThisPos, layerIdx, pos));
        }

        plan.insert(plan.end(), itemsOldLayer.begin(), itemsOldLayer.end());

        itemsOldLayer = itemsNewLayer;
        itemsNewLayer.clear();
    }
    plan.insert(plan.end(), itemsOldLayer.begin(), itemsOldLayer.end());

    return result;
}

bool Encoding::factValue(int layer, int pos, const USignature& sig) {
    int v = _layers.at(layer)->at(pos).getFactVariable(sig);
    Log::d("VAL %s@(%i,%i)=%i %i\n", TOSTR(sig), layer, pos, v, ipasir_val(_solver, v));
    return (ipasir_val(_solver, v) > 0);
}
bool Encoding::opValue(int layer, int pos, const USignature& sig) {
    int v = _layers.at(layer)->at(pos).getOpVariable(sig);
    Log::d("VAL %s@(%i,%i)=%i %i\n", TOSTR(sig), layer, pos, v, ipasir_val(_solver, v));
    return (ipasir_val(_solver, v) > 0);
}

void Encoding::printSatisfyingAssignment() {
    Log::d("SOLUTION_VALS ");
    for (int v = 1; v <= VariableDomain::getMaxVar(); v++) {
        Log::d("%i ", ipasir_val(_solver, v));
    }
    Log::d("\n");
}

USignature Encoding::getDecodedQOp(int layer, int pos, const USignature& origSig) {
    assert(opValue(layer, pos, origSig));

    USignature sig = origSig;
    while (true) {
        bool containsQConstants = false;
        for (int arg : sig._args) if (_htn.isQConstant(arg)) {
            // q constant found
            containsQConstants = true;

            int numSubstitutions = 0;
            for (int argSubst : _htn.getDomainOfQConstant(arg)) {
                const USignature& sigSubst = sigSubstitute(arg, argSubst);
                if (_substitution_variables.count(sigSubst) && ipasir_val(_solver, varSubstitution(sigSubst)) > 0) {
                    //Log::d("%s/%s => %s ~~> ", TOSTR(arg), TOSTR(argSubst), TOSTR(sig));
                    numSubstitutions++;
                    Substitution sub;
                    sub[arg] = argSubst;
                    sig.apply(sub);
                    //Log::d("%s\n", TOSTR(sig));
                } else {
                    //Log::d("%i FALSE\n", varSubstitution(sigSubst));
                }
            }

            int opVar = _layers.at(layer)->at(pos).getOpVariable(origSig);
            if (numSubstitutions == 0) {
                Log::v("No substitutions for arg %s of %s (op=%i)\n", TOSTR(arg), TOSTR(origSig), opVar);
                return Position::NONE_SIG;
            }
            assert(numSubstitutions == 1 || Log::e("%i substitutions for arg %s of %s (op=%i)\n", numSubstitutions, TOSTR(arg), TOSTR(origSig), opVar));
        }

        if (!containsQConstants) break; // done
    }

    //if (origSig != sig) Log::d("%s ~~> %s\n", TOSTR(origSig), TOSTR(sig));
    
    return sig;
}

void Encoding::stage(std::string name) {
    if (_print_formula) _out << "c " << name << "\n";
    if (!_num_cls_per_stage.count(name)) {
        _num_cls_per_stage[name] = _num_cls;
        //log("    %s ...\n", name.c_str());
    } else {
        if (!_total_num_cls_per_stage.count(name)) _total_num_cls_per_stage[name] = 0;
        _total_num_cls_per_stage[name] += _num_cls - _num_cls_per_stage[name];
        _num_cls_per_stage.erase(name);
    }
}

void Encoding::printStages() {
    Log::i("Total amount of clauses encoded: %i\n", _num_cls);
    for (const auto& entry : _total_num_cls_per_stage) {
        Log::i(" %s : %i cls\n", entry.first.c_str(), entry.second);
    }
    _total_num_cls_per_stage.clear();
}

Encoding::~Encoding() {

    if (!_total_num_cls_per_stage.empty()) printStages();

    if (_params.isNonzero("of")) {

        // Append assumptions to written formula, close stream
        if (_last_assumptions.empty()) {
            addAssumptions(_layers.size()-1);
        }
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