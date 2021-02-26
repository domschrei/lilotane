
#include <random>

#include "sat/encoding.h"
#include "sat/literal_tree.h"
#include "sat/binary_amo.h"
#include "sat/dnf2cnf.h"
#include "util/log.h"
#include "util/timer.h"

void Encoding::encode(size_t layerIdx, size_t pos) {
    _termination_callback();

    _stats.beginPosition();

    _layer_idx = layerIdx;
    _pos = pos;

    // Calculate relevant environment of the position
    Position NULL_POS;
    NULL_POS.setPos(-1, -1);
    Layer& newLayer = *_layers.at(layerIdx);
    Position& newPos = newLayer[pos];
    bool hasLeft = pos > 0;
    Position& left = (hasLeft ? newLayer[pos-1] : NULL_POS);
    bool hasAbove = layerIdx > 0;
    _offset = 0, _old_pos = 0;
    if (hasAbove) {
        const Layer& oldLayer = *_layers.at(layerIdx-1);
        while (_old_pos+1 < oldLayer.size() && oldLayer.getSuccessorPos(_old_pos+1) <= pos) 
            _old_pos++;
        _offset = pos - oldLayer.getSuccessorPos(_old_pos);
    }
    Position& above = (hasAbove ? (*_layers.at(layerIdx-1))[_old_pos] : NULL_POS);
    
    // 1st pass over all operations (actions and reductions): 
    // encode as variables, define primitiveness
    encodeOperationVariables(newPos);

    // Encode true facts at this position and decide for each fact
    // whether to encode it or to reuse the previous variable
    encodeFactVariables(newPos, left, above);

    // 2nd pass over all operations: Init substitution vars where necessary,
    // encode precondition constraints and at-{most,least}-one constraints
    encodeOperationConstraints(newPos);

    // Link qfacts to their possible decodings
    encodeQFactSemantics(newPos);

    // Effects of "old" actions to the left
    encodeActionEffects(newPos, left);

    // Type constraints and forbidden substitutions for q-constants
    // and (sets of) q-facts
    encodeQConstraints(newPos);

    // Expansion and predecessor specification for each element
    // and prohibition of impossible children
    encodeSubtaskRelationships(newPos, above);

    // choice of axiomatic ops
    _stats.begin(STAGE_AXIOMATICOPS);
    const USigSet& axiomaticOps = newPos.getAxiomaticOps();
    if (!axiomaticOps.empty()) {
        for (const USignature& op : axiomaticOps) {
            _sat.appendClause(_vars.getVariable(VarType::OP, newPos, op));
        }
        _sat.endClause();
    }
    _stats.end(STAGE_AXIOMATICOPS);

    _stats.endPosition();
}

void Encoding::encodeOperationVariables(Position& newPos) {

    _primitive_ops.clear();
    _nonprimitive_ops.clear();

    _stats.begin(STAGE_ACTIONCONSTRAINTS);
    for (const auto& aSig : newPos.getActions()) {
        int aVar = _vars.encodeVariable(VarType::OP, newPos, aSig);

        // If the action occurs, the position is primitive
        _primitive_ops.push_back(aVar);
    }
    _stats.end(STAGE_ACTIONCONSTRAINTS);

    _stats.begin(STAGE_REDUCTIONCONSTRAINTS);
    for (const auto& rSig : newPos.getReductions()) {
        int rVar = _vars.encodeVariable(VarType::OP, newPos, rSig);

        bool trivialReduction = _htn.getOpTable().getReduction(rSig).getSubtasks().size() == 0;
        if (trivialReduction) {
            // If a trivial reduction occurs, the position is primitive
            _primitive_ops.push_back(rVar);
        } else {
            // If another reduction occurs, the position is non-primitive
            _nonprimitive_ops.push_back(rVar);
        }
    }
    _stats.end(STAGE_REDUCTIONCONSTRAINTS);

    newPos.setHasPrimitiveOps(!_primitive_ops.empty());
    newPos.setHasNonprimitiveOps(!_nonprimitive_ops.empty());
    
    // Implicit primitiveness?
    if (_implicit_primitiveness) return;

    // Only primitive ops here? -> No primitiveness definition necessary
    if (_nonprimitive_ops.empty()) {
        // Workaround for "x-1" ID assignment of primitivizations
        _vars.skipVariable();
        return;
    }

    int varPrim = _vars.encodeVarPrimitive(newPos.getLayerIndex(), newPos.getPositionIndex());

    _stats.begin(STAGE_REDUCTIONCONSTRAINTS);
    if (_primitive_ops.empty()) {
        // Only non-primitive ops here
        _sat.addClause(-varPrim);
    } else {
        // Mix of primitive and non-primitive ops (default)
        _stats.begin(STAGE_ACTIONCONSTRAINTS);
        for (int aVar : _primitive_ops) _sat.addClause(-aVar, varPrim);
        _stats.end(STAGE_ACTIONCONSTRAINTS);
        for (int rVar : _nonprimitive_ops) _sat.addClause(-rVar, -varPrim);
    }
    _stats.end(STAGE_REDUCTIONCONSTRAINTS);
}

void Encoding::encodeFactVariables(Position& newPos, Position& left, Position& above) {

    _new_fact_vars.clear();

    _stats.begin(STAGE_FACTVARENCODING);

    // Reuse ground fact variables from above position
    if (newPos.getLayerIndex() > 0 && _offset == 0) {
        for (const auto& [factSig, factVar] : above.getVariableTable(VarType::FACT)) {
            if (!_htn.hasQConstants(factSig)) newPos.setVariable(VarType::FACT, factSig, factVar);
        }
    }

    if (_pos == 0) {
        // Encode all relevant definitive facts
        const USigSet* defFacts[] = {&newPos.getTrueFacts(), &newPos.getFalseFacts()};
        for (auto set : defFacts) for (const auto& fact : *set) {
            if (!newPos.hasVariable(VarType::FACT, fact) && _analysis.isRelevant(fact)) 
                _new_fact_vars.insert(_vars.encodeVariable(VarType::FACT, newPos, fact));
        }
    } else {
        // Encode frame axioms which will assign variables to all ground facts
        // that have some support to change at this position
        encodeFrameAxioms(newPos, left);
    }

    auto reuseQFact = [&](const USignature& qfact, int var, Position& otherPos, bool negated) {
        if (!newPos.hasQFactDecodings(qfact, negated)) return true;
        if (var == 0 || !otherPos.hasQFactDecodings(qfact, negated)
                || otherPos.getQFactDecodings(qfact, negated).size() < newPos.getQFactDecodings(qfact, negated).size())
            return false;
        const auto& otherDecodings = otherPos.getQFactDecodings(qfact, negated);
        for (const auto& decFact : newPos.getQFactDecodings(qfact, negated)) {
            int decFactVar = newPos.getVariableOrZero(VarType::FACT, decFact);
            int otherDecFactVar = otherPos.getVariableOrZero(VarType::FACT, decFact);
            if (decFactVar == 0 || otherDecFactVar == 0 
                    || decFactVar != otherDecFactVar 
                    || !otherDecodings.count(decFact)) {
                return false;
            }
        }
        return true;
    };

    // Encode q-facts that are not encoded yet
    for ([[maybe_unused]] const auto& qfact : newPos.getQFacts()) {
        if (!newPos.hasQFactDecodings(qfact, true) && !newPos.hasQFactDecodings(qfact, false)) continue;
        assert(!newPos.hasVariable(VarType::FACT, qfact));

        // Reuse variable from above?
        int aboveVar = above.getVariableOrZero(VarType::FACT, qfact);
        if (_offset == 0 && aboveVar != 0) {
            // Reuse qfact variable from above
            newPos.setVariable(VarType::FACT, qfact, aboveVar);

        } else {
            // Reuse variable from left?
            int leftVar = left.getVariableOrZero(VarType::FACT, qfact);           
            if (reuseQFact(qfact, leftVar, left, true) && reuseQFact(qfact, leftVar, left, false)) {
                // Reuse qfact variable from above
                newPos.setVariable(VarType::FACT, qfact, leftVar);

            } else {
                // Encode new variable
                _new_fact_vars.insert(_vars.encodeVariable(VarType::FACT, newPos, qfact));
            }
        }
    }

    _stats.end(STAGE_FACTVARENCODING);

    // Facts that must hold at this position
    _stats.begin(STAGE_TRUEFACTS);
    const USigSet* cHere[] = {&newPos.getTrueFacts(), &newPos.getFalseFacts()}; 
    for (int i = 0; i < 2; i++) 
    for (const USignature& factSig : *cHere[i]) if (_analysis.isRelevant(factSig)) {
        int var = newPos.getVariableOrZero(VarType::FACT, factSig);
        if (var == 0) {
            // Variable is not encoded yet.
            _sat.addClause((i == 0 ? 1 : -1) * _vars.encodeVariable(VarType::FACT, newPos, factSig));
        } else {
            // Variable is already encoded. If the variable is new, constrain it.
            if (_new_fact_vars.count(var)) _sat.addClause((i == 0 ? 1 : -1) * var);
        }
        Log::d("(%i,%i) DEFFACT %s\n", _layer_idx, _pos, TOSTR(factSig));
    }
    _stats.end(STAGE_TRUEFACTS);
}

void Encoding::encodeFrameAxioms(Position& newPos, Position& left) {
    static Position NULL_POS;

    using Supports = const NodeHashMap<USignature, USigSet, USignatureHasher>;

    _stats.begin(STAGE_DIRECTFRAMEAXIOMS);

    bool nonprimFactSupport = _params.isNonzero("nps");
    bool hasPrimitiveOps = left.hasPrimitiveOps();

    int layerIdx = newPos.getLayerIndex();
    int pos = newPos.getPositionIndex();
    int prevVarPrim = _vars.getVarPrimitiveOrZero(layerIdx, pos-1);

    // Check if frame axioms can be skipped because
    // the above position had a superset of operations
    Position& above = layerIdx > 0 ? _layers[layerIdx-1]->at(_old_pos) : NULL_POS;
    Position& leftOfAbove = layerIdx > 0 && _old_pos > 0 ? _layers[layerIdx-1]->at(_old_pos-1) : NULL_POS;
    bool skipRedundantFrameAxioms = _params.isNonzero("srfa") && _offset == 0
        && !left.hasNonprimitiveOps() && !leftOfAbove.hasNonprimitiveOps() 
        && left.getActions().size()+left.getReductions().size() <= leftOfAbove.getActions().size()+leftOfAbove.getReductions().size();

    // Retrieve supports from left position
    Supports* supp[2] = {&newPos.getNegFactSupports(), &newPos.getPosFactSupports()};
    IndirectFactSupportMap* iSupp[2] = {&newPos.getNegIndirectFactSupports(), &newPos.getPosIndirectFactSupports()};

    // Find and encode frame axioms for each applicable fact from the left
    size_t skipped = 0;
    for ([[maybe_unused]] const auto& [fact, var] : left.getVariableTable(VarType::FACT)) {
        if (_htn.hasQConstants(fact)) continue;
        
        int oldFactVars[2] = {-var, var};
        const USigSet* dir[2] = {nullptr, nullptr};
        const IndirectFactSupportMapEntry* indir[2] = {nullptr, nullptr};

        // Retrieve direct and indirect support for this fact
        bool reuse = true;
        for (int i = 0; i < 2; i++) {
            if (!supp[i]->empty()) { // Direct support
                auto it = supp[i]->find(fact);
                if (it != supp[i]->end()) {
                    dir[i] = &(it->second);
                    reuse = false;
                } 
            }
            if (!iSupp[i]->empty()) { // Indirect support
                auto it = iSupp[i]->find(fact);
                if (it != iSupp[i]->end()) {
                    indir[i] = &(it->second);
                    reuse = false;
                } 
            }
        }

        int factVar = newPos.getVariableOrZero(VarType::FACT, fact);

        // Decide on the fact variable to use (reuse or encode)
        if (factVar == 0) {
            if (reuse) {
                // No support for this fact -- variable can be reused from left
                factVar = var;
                newPos.setVariable(VarType::FACT, fact, var);
            } else {
                // There is some support for this fact -- need to encode new var
                int v = _vars.encodeVariable(VarType::FACT, newPos, fact);
                _new_fact_vars.insert(v);
                factVar = v;
            }
        }

        skipped++;
        // Skip frame axiom encoding if nothing can change
        if (var == factVar) continue; 
        // Skip frame axioms if they were already encoded
        if (skipRedundantFrameAxioms && above.hasVariable(VarType::FACT, fact)) continue;
        // No primitive ops at this position: No need for encoding frame axioms
        if (!hasPrimitiveOps) continue;
        skipped--;

        // Encode general frame axioms for this fact
        int i = -1;
        for (int sign = -1; sign <= 1; sign += 2) {
            i++;
            std::vector<int> cls;
            // Fact change:
            if (oldFactVars[i] != 0) cls.push_back(oldFactVars[i]);
            cls.push_back(-sign*factVar);
            if (dir[i] != nullptr || indir[i] != nullptr) {
                std::vector<int> headerLits = cls;
                // Non-primitiveness wildcard
                if (!nonprimFactSupport) {
                    if (_implicit_primitiveness) {
                        for (int var : _nonprimitive_ops) cls.push_back(var);
                    } else if (prevVarPrim != 0) cls.push_back(-prevVarPrim);
                }
                // INDIRECT support
                if (indir[i] != nullptr) {                    
                    for (const auto& [op, tree] : *indir[i]) {
                        // Skip if the operation is already a DIRECT support for the fact
                        if (dir[i] != nullptr && dir[i]->count(op)) continue;

                        // Encode substitutions enabling indirect support for this fact
                        int opVar = left.getVariableOrZero(VarType::OP, op);
                        USignature virtOp(_htn.getRepetitionNameOfAction(op._name_id), op._args);
                        int virtOpVar = left.getVariableOrZero(VarType::OP, virtOp);
                        if (opVar != 0) {
                            cls.push_back(opVar);
                            encodeIndirectFrameAxioms(headerLits, opVar, tree);
                        }
                        if (virtOpVar != 0) {
                            cls.push_back(virtOpVar);
                            encodeIndirectFrameAxioms(headerLits, virtOpVar, tree);
                        }
                    }
                }
                // DIRECT support
                if (dir[i] != nullptr) for (const USignature& opSig : *dir[i]) {
                    int opVar = left.getVariableOrZero(VarType::OP, opSig);
                    if (opVar != 0) cls.push_back(opVar);
                    USignature virt = opSig.renamed(_htn.getRepetitionNameOfAction(opSig._name_id));
                    int virtOpVar = left.getVariableOrZero(VarType::OP, virt);
                    if (virtOpVar != 0) cls.push_back(virtOpVar);
                }
            }
            _sat.addClause(cls);
        }
    }
    _stats.end(STAGE_DIRECTFRAMEAXIOMS);

    Log::d("Skipped %i frame axioms\n", skipped);
}

void Encoding::encodeIndirectFrameAxioms(const std::vector<int>& headerLits, int opVar, const IntPairTree& tree) {
       
    // Unconditional effect?
    if (tree.containsEmpty()) return;

    _stats.begin(STAGE_INDIRECTFRAMEAXIOMS);
            
    // Transform header and tree into a set of clauses
    for (const auto& cls : tree.encode()) {
        for (int lit : headerLits) _sat.appendClause(lit);
        _sat.appendClause(-opVar);
        for (const auto& [src, dest] : cls) {
            _sat.appendClause((src<0 ? -1 : 1) * _vars.varSubstitution(std::abs(src), dest));
        }
        _sat.endClause();
    }
    
    _stats.end(STAGE_INDIRECTFRAMEAXIOMS);
}

void Encoding::encodeOperationConstraints(Position& newPos) {

    size_t layerIdx = newPos.getLayerIndex();
    size_t pos = newPos.getPositionIndex();

    // Store all operations occurring here, for one big clause ORing them
    std::vector<int> elementVars(newPos.getActions().size() + newPos.getReductions().size(), 0);
    int numOccurringOps = 0;

    _stats.begin(STAGE_ACTIONCONSTRAINTS);
    for (const auto& aSig : newPos.getActions()) {

        int aVar = _vars.getVariable(VarType::OP, newPos, aSig);
        elementVars[numOccurringOps++] = aVar;
        
        if (_htn.isActionRepetition(aSig._name_id)) continue;

        for (int arg : aSig._args) encodeSubstitutionVars(aSig, aVar, arg);

        // Preconditions
        for (const Signature& pre : _htn.getOpTable().getAction(aSig).getPreconditions()) {
            if (!_vars.isEncoded(VarType::FACT, layerIdx, pos, pre._usig)) continue;
            _sat.addClause(-aVar, (pre._negated?-1:1)*_vars.getVariable(VarType::FACT, newPos, pre._usig));
        }
    }
    _stats.end(STAGE_ACTIONCONSTRAINTS);
    _stats.begin(STAGE_REDUCTIONCONSTRAINTS);
    for (const auto& rSig : newPos.getReductions()) {

        int rVar = _vars.getVariable(VarType::OP, newPos, rSig);
        for (int arg : rSig._args) encodeSubstitutionVars(rSig, rVar, arg);
        elementVars[numOccurringOps++] = rVar;

        // Preconditions
        for (const Signature& pre : _htn.getOpTable().getReduction(rSig).getPreconditions()) {
            if (!_vars.isEncoded(VarType::FACT, layerIdx, pos, pre._usig)) continue;
            _sat.addClause(-rVar, (pre._negated?-1:1)*_vars.getVariable(VarType::FACT, newPos, pre._usig));
        }
    }
    _stats.end(STAGE_REDUCTIONCONSTRAINTS);

    _q_constants.insert(_new_q_constants.begin(), _new_q_constants.end());
    _new_q_constants.clear();
    
    if (numOccurringOps == 0) return;

    if ((int)elementVars.size() >= _params.getIntParam("bamot")) {
        // Binary at-most-one

        _stats.begin(STAGE_ATMOSTONEELEMENT);
        auto bamo = BinaryAtMostOne(elementVars, elementVars.size()+1);
        for (const auto& c : bamo.encode()) _sat.addClause(c);
        _stats.end(STAGE_ATMOSTONEELEMENT);

    } else {
        // Naive at-most-one

        _stats.begin(STAGE_ATMOSTONEELEMENT);
        for (size_t i = 0; i < elementVars.size(); i++) {
            for (size_t j = i+1; j < elementVars.size(); j++) {
                _sat.addClause(-elementVars[i], -elementVars[j]);
            }
        }
        _stats.end(STAGE_ATMOSTONEELEMENT);
    }
}

void Encoding::encodeSubstitutionVars(const USignature& opSig, int opVar, int arg) {

    if (!_htn.isQConstant(arg)) return;
    if (_q_constants.count(arg)) return;

    // arg is a *new* q-constant: initialize substitution logic
    _new_q_constants.insert(arg);

    std::vector<int> substitutionVars;
    //Log::d("INITSUBVARS @(%i,%i) %s:%s [ ", pos.getLayerIndex(), pos.getPositionIndex(), TOSTR(opSig), TOSTR(arg));
    for (int c : _htn.popOperationDependentDomainOfQConstant(arg, opSig)) {

        assert(!_htn.isVariable(c));

        // either of the possible substitutions must be chosen
        int varSubst = _vars.varSubstitution(arg, c);
        substitutionVars.push_back(varSubst);
        //Log::log_notime(Log::V4_DEBUG, "%s ", TOSTR(sigSubstitute(arg, c)));
    }
    //Log::log_notime(Log::V4_DEBUG, "]\n");
    assert(!substitutionVars.empty());

    // AT LEAST ONE substitution, or the parent op does NOT occur
    _sat.appendClause(-opVar);
    for (int vSub : substitutionVars) _sat.appendClause(vSub);
    _sat.endClause();

    // AT MOST ONE substitution
    if ((int)substitutionVars.size() >= _params.getIntParam("bamot")) {
        // Binary at-most-one
        auto bamo = BinaryAtMostOne(substitutionVars, substitutionVars.size()+1);
        for (const auto& c : bamo.encode()) _sat.addClause(c);
    } else {
        // Naive at-most-one
        for (int vSub1 : substitutionVars) {
            for (int vSub2 : substitutionVars) {
                if (vSub1 < vSub2) _sat.addClause(-vSub1, -vSub2);
            }
        }
    }
}

void Encoding::encodeQFactSemantics(Position& newPos) {
    static Position NULL_POS;

    _stats.begin(STAGE_QFACTSEMANTICS);
    std::vector<int> substitutionVars; substitutionVars.reserve(128);
    for (const auto& qfactSig : newPos.getQFacts()) {
        assert(_htn.hasQConstants(qfactSig));
        
        int qfactVar = _vars.getVariable(VarType::FACT, newPos, qfactSig);

        for (int sign = -1; sign <= 1; sign += 2) {
            bool negated = sign < 0;
            if (!newPos.hasQFactDecodings(qfactSig, negated)) 
                continue;

            bool filterAbove = false;
            Position& above = _offset == 0 && _layer_idx > 0 ? _layers[_layer_idx-1]->at(_old_pos) : NULL_POS;
            if (!_new_fact_vars.count(qfactVar)) {
                if (_offset == 0 && _layer_idx > 0 && above.getVariableOrZero(VarType::FACT, qfactSig) == qfactVar
                                && above.hasQFactDecodings(qfactSig, negated)) {
                    filterAbove = true;

                    /*
                    TODO
                    aar=0 : qfact semantics are added once, then for each further layer
                    they are skipped because they were already encoded.
                    aar=1 : qfact semantics are added once, skipped once, then added again
                    because the qfact (and decodings) do not occur above any more.
                    */

                }
                if (!filterAbove && _pos > 0) {
                    Position& left = _layers[_layer_idx]->at(_pos-1);
                    if (left.getVariableOrZero(VarType::FACT, qfactSig) == qfactVar)
                        continue;
                }
            }

            // For each possible fact decoding:
            for (const auto& decFactSig : newPos.getQFactDecodings(qfactSig, negated)) {
                
                int decFactVar = newPos.getVariableOrZero(VarType::FACT, decFactSig);
                if (decFactVar == 0) continue;
                if (filterAbove && above.getQFactDecodings(qfactSig, negated).count(decFactSig)) continue;

                // Assemble list of substitution variables
                for (size_t i = 0; i < qfactSig._args.size(); i++) {
                    if (qfactSig._args[i] != decFactSig._args[i])
                        substitutionVars.push_back(
                            _vars.varSubstitution(qfactSig._args[i], decFactSig._args[i])
                        );
                }
                
                // If the substitution is chosen,
                // the q-fact and the corresponding actual fact are equivalent
                //Log::v("QFACTSEM (%i,%i) %s -> %s\n", _layer_idx, _pos, TOSTR(qfactSig), TOSTR(decFactSig));
                for (const int& varSubst : substitutionVars) {
                    _sat.appendClause(-varSubst);
                }
                _sat.appendClause(-sign*qfactVar, sign*decFactVar);
                _sat.endClause();
                substitutionVars.clear();
            }
        }
    }
    _stats.end(STAGE_QFACTSEMANTICS);
}

void Encoding::encodeActionEffects(Position& newPos, Position& left) {

    bool treeConversion = _params.isNonzero("tc");
    _stats.begin(STAGE_ACTIONEFFECTS);
    for (const auto& aSig : left.getActions()) {
        if (_htn.isActionRepetition(aSig._name_id)) continue;
        int aVar = _vars.getVariable(VarType::OP, left, aSig);

        const SigSet& effects = _htn.getOpTable().getAction(aSig).getEffects();

        for (const Signature& eff : effects) {
            if (!_vars.isEncoded(VarType::FACT, _layer_idx, _pos, eff._usig)) continue;

            std::set<std::set<int>> unifiersDnf;
            bool unifiedUnconditionally = false;
            if (eff._negated) {
                for (const auto& posEff : effects) {
                    if (posEff._negated) continue;
                    if (posEff._usig._name_id != eff._usig._name_id) continue;
                    if (!_vars.isEncoded(VarType::FACT, _layer_idx, _pos, posEff._usig)) continue;

                    bool fits = true;
                    std::set<int> s;
                    for (size_t i = 0; i < eff._usig._args.size(); i++) {
                        const int& effArg = eff._usig._args[i];
                        const int& posEffArg = posEff._usig._args[i];
                        if (effArg != posEffArg) {
                            bool effIsQ = _q_constants.count(effArg);
                            bool posEffIsQ = _q_constants.count(posEffArg);
                            if (effIsQ && posEffIsQ) {
                                s.insert(encodeQConstEquality(effArg, posEffArg));
                            } else if (effIsQ) {
                                if (!_htn.getDomainOfQConstant(effArg).count(posEffArg)) fits = false;
                                else s.insert(_vars.varSubstitution(effArg, posEffArg));
                            } else if (posEffIsQ) {
                                if (!_htn.getDomainOfQConstant(posEffArg).count(effArg)) fits = false;
                                else s.insert(_vars.varSubstitution(posEffArg, effArg));
                            } else fits = false;
                        }
                    }
                    if (fits && s.empty()) {
                        // Empty substitution does the job
                        unifiedUnconditionally = true;
                        break;
                    }
                    if (fits) unifiersDnf.insert(s);
                }
            }
            if (unifiedUnconditionally) continue; // Always unified
            if (unifiersDnf.empty()) {
                // Positive or ununifiable negative effect: enforce it
                _sat.addClause(-aVar, (eff._negated?-1:1)*_vars.getVariable(VarType::FACT, newPos, eff._usig));
                continue;
            }

            // Negative effect which only holds in certain cases
            if (treeConversion) {
                LiteralTree<int> tree;
                for (const auto& set : unifiersDnf) tree.insert(std::vector<int>(set.begin(), set.end()));
                std::vector<int> headerLits;
                headerLits.push_back(aVar);
                headerLits.push_back(_vars.getVariable(VarType::FACT, newPos, eff._usig));
                for (const auto& cls : tree.encode(headerLits)) _sat.addClause(cls);
            } else {
                std::vector<int> dnf;
                for (const auto& set : unifiersDnf) {
                    for (int lit : set) dnf.push_back(lit);
                    dnf.push_back(0);
                }
                auto cnf = Dnf2Cnf::getCnf(dnf);
                for (const auto& clause : cnf) {
                    _sat.appendClause(-aVar, -_vars.getVariable(VarType::FACT, newPos, eff._usig));
                    for (int lit : clause) _sat.appendClause(lit);
                    _sat.endClause();
                }
            }
        }
    }
    _stats.end(STAGE_ACTIONEFFECTS);
}

void Encoding::encodeQConstraints(Position& newPos) {

    // Q-constants type constraints
    _stats.begin(STAGE_QTYPECONSTRAINTS);
    const auto& constraints = newPos.getQConstantsTypeConstraints();
    for (const auto& [opSig, constraints] : constraints) {
        int opVar = newPos.getVariableOrZero(VarType::OP, opSig);
        if (opVar != 0) {
            for (const TypeConstraint& c : constraints) {
                int qconst = c.qconstant;
                bool positiveConstraint = c.sign;
                assert(_q_constants.count(qconst));

                if (positiveConstraint) {
                    // EITHER of the GOOD constants - one big clause
                    _sat.appendClause(-opVar);
                    for (int cnst : c.constants) {
                        _sat.appendClause(_vars.varSubstitution(qconst, cnst));
                    }
                    _sat.endClause();
                } else {
                    // NEITHER of the BAD constants - many 2-clauses
                    for (int cnst : c.constants) {
                        _sat.addClause(-opVar, -_vars.varSubstitution(qconst, cnst));
                    }
                }
            }
        }
    }
    _stats.end(STAGE_QTYPECONSTRAINTS);

    // Forbidden substitutions
    _stats.begin(STAGE_SUBSTITUTIONCONSTRAINTS);

    // For each operation (action or reduction)
    const USigSet* ops[2] = {&newPos.getActions(), &newPos.getReductions()};
    for (const auto& set : ops) for (auto opSig : *set) {

        auto it = newPos.getSubstitutionConstraints().find(opSig);
        if (it == newPos.getSubstitutionConstraints().end()) continue;
        
        for (const auto& c : it->second) {
            auto f = c.getEncoding();
            auto polarity = c.getPolarity();
            for (const auto& cls : f) {
                //std::string out = (polarity == SubstitutionConstraint::ANY_VALID ? "+" : "-") + std::string("SUBSTITUTION ") 
                //        + Names::to_string(opSig) + " ";
                _sat.appendClause(-_vars.getVariable(VarType::OP, newPos, opSig));
                for (const auto& [qArg, decArg] : cls) {
                    bool negated = qArg < 0;
                    //out += (negated ? "-" : "+")
                    //        + Names::to_string(involvedQConsts[idx]) + "/" + Names::to_string(std::abs(lit)) + " ";
                    _sat.appendClause((polarity == SubstitutionConstraint::NO_INVALID ? -1 : (negated ? -1 : 1)) 
                            * _vars.varSubstitution(std::abs(qArg), decArg));
                }
                _sat.endClause();
                //out += "\n";
                //Log::d(out.c_str());
            }
        }
    }
    newPos.clearSubstitutions();
    
    _stats.end(STAGE_SUBSTITUTIONCONSTRAINTS);
}

void Encoding::encodeSubtaskRelationships(Position& newPos, Position& above) {

    if (newPos.getActions().size() == 1 && newPos.getReductions().empty() 
            && newPos.hasAction(_htn.getBlankActionSig())) {
        // This position contains the blank action and nothing else.
        // No subtask relationships need to be encoded.
        return;
    }

    // expansions
    _stats.begin(STAGE_EXPANSIONS);
    for (const auto& [parent, children] : newPos.getExpansions()) {

        int parentVar = _vars.getVariable(VarType::OP, above, parent);
        _sat.appendClause(-parentVar);
        for (const USignature& child : children) {
            assert(child != Sig::NONE_SIG);
            _sat.appendClause(_vars.getVariable(VarType::OP, newPos, child));
        }
        _sat.endClause();

        if (newPos.getExpansionSubstitutions().count(parent)) {
            for (const auto& [child, s] : newPos.getExpansionSubstitutions().at(parent)) {
                int childVar = newPos.getVariableOrZero(VarType::OP, child);
                if (childVar == 0) continue;

                for (const auto& [src, dest] : s) {
                    assert(_htn.isQConstant(dest));

                    // Q-constant dest has a larger domain than (q-)constant src.
                    // Enforce that dest only takes values from the domain of src!
                    //Log::d("DOM %s->%s : Enforce %s only to take values from domain of %s\n", TOSTR(parent), TOSTR(child), TOSTR(dest), TOSTR(src));

                    if (!_htn.isQConstant(src)) {
                        _sat.addClause(-parentVar, -childVar, _vars.varSubstitution(dest, src));
                    } else {
                        _sat.addClause(-parentVar, -childVar, encodeQConstEquality(dest, src));
                    }
                }
            }
        }
    }
    _stats.end(STAGE_EXPANSIONS);

    // predecessors
    if (_params.isNonzero("p")) {
        _stats.begin(STAGE_PREDECESSORS);
        for (const auto& [child, parents] : newPos.getPredecessors()) {

            _sat.appendClause(-_vars.getVariable(VarType::OP, newPos, child));
            for (const USignature& parent : parents) {
                _sat.appendClause(_vars.getVariable(VarType::OP, above, parent));
            }
            _sat.endClause();
        }
        _stats.end(STAGE_PREDECESSORS);
    }
}

int Encoding::encodeQConstEquality(int q1, int q2) {

    if (!_vars.isQConstantEqualityEncoded(q1, q2)) {
        
        _stats.begin(STAGE_QCONSTEQUALITY);
        FlatHashSet<int> good, bad1, bad2;
        for (int c : _htn.getDomainOfQConstant(q1)) {
            if (!_htn.getDomainOfQConstant(q2).count(c)) bad1.insert(c);
            else good.insert(c);
        }
        for (int c : _htn.getDomainOfQConstant(q2)) {
            if (_htn.getDomainOfQConstant(q1).count(c)) continue;
            bad2.insert(c);
        }
        int varEq = _vars.encodeQConstantEqualityVar(q1, q2);
        if (good.empty()) {
            // Domains are incompatible -- equality never holds
            _sat.addClause(-varEq);
        } else {
            // If equality, then all "good" substitution vars are equivalent
            for (int c : good) {
                int v1 = _vars.varSubstitution(q1, c);
                int v2 = _vars.varSubstitution(q2, c);
                _sat.addClause(-varEq, v1, -v2);
                _sat.addClause(-varEq, -v1, v2);
            }
            // If any of the GOOD ones, then equality
            for (int c : good) _sat.addClause(-_vars.varSubstitution(q1, c), -_vars.varSubstitution(q2, c), varEq);
            // If any of the BAD ones, then inequality
            for (int c : bad1) _sat.addClause(-_vars.varSubstitution(q1, c), -varEq);
            for (int c : bad2) _sat.addClause(-_vars.varSubstitution(q2, c), -varEq);
        }
        _stats.end(STAGE_QCONSTEQUALITY);
    }
    return _vars.getQConstantEqualityVar(q1, q2);
}

void Encoding::addAssumptions(int layerIdx, bool permanent) {
    Layer& l = *_layers.at(layerIdx);
    if (_implicit_primitiveness) {
        _stats.begin(STAGE_ACTIONCONSTRAINTS);
        for (size_t pos = 0; pos < l.size(); pos++) {
            _sat.appendClause(-_vars.encodeVarPrimitive(layerIdx, pos));
            for (int var : _primitive_ops) _sat.appendClause(var);
            _sat.endClause();
        }
        _stats.end(STAGE_ACTIONCONSTRAINTS);
    }
    for (size_t pos = 0; pos < l.size(); pos++) {
        _stats.begin(STAGE_ASSUMPTIONS);
        int v = _vars.getVarPrimitiveOrZero(layerIdx, pos);
        if (v != 0) {
            if (permanent) _sat.addClause(v);
            else _sat.assume(v);
        }
        _stats.end(STAGE_ASSUMPTIONS);
    }
}

void Encoding::setTerminateCallback(void * state, int (*terminate)(void * state)) {
    _sat.setTerminateCallback(state, terminate);
}

void onClauseLearnt(void* state, int* cls) {
    std::string str = "";
    int i = 0; while (cls[i] != 0) str += std::to_string(cls[i++]) + " ";
    Log::d("LEARNT_CLAUSE %s\n", str.c_str());
}

int Encoding::solve() {
    Log::i("Attempting to solve formula with %i clauses (%i literals) and %i assumptions\n", 
                _stats._num_cls, _stats._num_lits, _stats._num_asmpts);
    
    if (_params.isNonzero("plc"))
        _sat.setLearnCallback(/*maxLength=*/100, this, onClauseLearnt);

    _sat_call_start_time = Timer::elapsedSeconds();
    int result = _sat.solve();
    _sat_call_start_time = 0;

    _termination_callback();

    return result;
}

void Encoding::addUnitConstraint(int lit) {
    _stats.begin(STAGE_FORBIDDENOPERATIONS);
    _sat.addClause(lit);
    _stats.end(STAGE_FORBIDDENOPERATIONS);
}

float Encoding::getTimeSinceSatCallStart() {
    if (_sat_call_start_time == 0) return 0;
    return Timer::elapsedSeconds() - _sat_call_start_time;
}

void Encoding::printFailedVars(Layer& layer) {
    Log::d("FAILED ");
    for (size_t pos = 0; pos < layer.size(); pos++) {
        int v = _vars.getVarPrimitiveOrZero(layer.index(), pos);
        if (v == 0) continue;
        if (_sat.didAssumptionFail(v)) Log::d("%i ", v);
    }
    Log::d("\n");
}

void Encoding::printSatisfyingAssignment() {
    Log::d("SOLUTION_VALS ");
    for (int v = 1; v <= _vars.getNumVariables(); v++) {
        Log::d("%i ", _sat.holds(v) ? v : -v);
    }
    Log::d("\n");
}
