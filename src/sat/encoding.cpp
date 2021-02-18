
#include <random>

#include "sat/encoding.h"
#include "sat/literal_tree.h"
#include "sat/binary_amo.h"
#include "util/log.h"
#include "util/timer.h"

Encoding::Encoding(Parameters& params, HtnInstance& htn, FactAnalysis& analysis, std::vector<Layer*>& layers, std::function<void()> terminationCallback) : 
            _params(params), _htn(htn), _analysis(analysis), _layers(layers),
            _sat(params, _stats), _vars(_htn, _layers), _indir_support(_htn, _vars), 
            _termination_callback(terminationCallback),
            _use_q_constant_mutexes(_params.getIntParam("qcm") > 0), 
            _implicit_primitiveness(params.isNonzero("ip")) {
    VariableDomain::init(params);
}

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

    clearDonePositions();

    _stats.endPosition();
}

void Encoding::encodeOperationVariables(Position& newPos) {

    _primitive_ops.clear();
    _nonprimitive_ops.clear();

    _stats.begin(STAGE_ACTIONCONSTRAINTS);
    for (const auto& aSig : newPos.getActions()) {
        if (aSig == Sig::NONE_SIG) continue;
        int aVar = _vars.encodeVariable(VarType::OP, newPos, aSig);

        // If the action occurs, the position is primitive
        _primitive_ops.push_back(aVar);
    }
    _stats.end(STAGE_ACTIONCONSTRAINTS);

    _stats.begin(STAGE_REDUCTIONCONSTRAINTS);
    for (const auto& rSig : newPos.getReductions()) {
        if (rSig == Sig::NONE_SIG) continue;
        int rVar = _vars.encodeVariable(VarType::OP, newPos, rSig);

        bool trivialReduction = _htn.getReduction(rSig).getSubtasks().size() == 0;
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
        // Workaround for "x-1" ID assignment of surrogate actions
        VariableDomain::nextVar(); 
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
    // Retrieve indirect support substitutions
    auto [negIS, posIS] = _indir_support.computeFactSupports(newPos, left);
    IndirectSupportMap* iSuppTrees[2] = {&negIS, &posIS};

    // Find and encode frame axioms for each applicable fact from the left
    size_t skipped = 0;
    for ([[maybe_unused]] const auto& [fact, var] : left.getVariableTable(VarType::FACT)) {
        if (_htn.hasQConstants(fact)) continue;
        
        int oldFactVars[2] = {-var, var};
        const USigSet* dir[2] = {nullptr, nullptr};
        const IndirectFactSupportMapEntry* indir[2] = {nullptr, nullptr};
        const NodeHashMap<int, LiteralTree<int>>* tree[2] = {nullptr, nullptr};

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
            if (!iSupp[i]->empty()) { // Indirect support & tree
                auto it = iSupp[i]->find(fact);
                if (it != iSupp[i]->end()) {
                    auto itt = iSuppTrees[i]->find(fact);
                    if (itt != iSuppTrees[i]->end()) {
                        reuse = false;
                        indir[i] = &(it->second);
                        tree[i] = &(itt->second);
                    }
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

        // Skip frame axiom encoding if nothing can change
        if (var == factVar) continue; 
        // Skip frame axioms if they were already encoded
        if (skipRedundantFrameAxioms && above.hasVariable(VarType::FACT, fact)) continue;
        // No primitive ops at this position: No need for encoding frame axioms
        if (!hasPrimitiveOps) continue;

        // Encode general frame axioms for this fact
        int i = -1;
        for (int sign = -1; sign <= 1; sign += 2) {
            i++;
            // Fact change:
            if (oldFactVars[i] != 0) _sat.appendClause(oldFactVars[i]);
            _sat.appendClause(-sign*factVar);
            if (dir[i] != nullptr || indir[i] != nullptr) {
                // Non-primitiveness wildcard
                if (!nonprimFactSupport) {
                    if (_implicit_primitiveness) {
                        for (int var : _nonprimitive_ops) _sat.appendClause(var);
                    } else if (prevVarPrim != 0) _sat.appendClause(-prevVarPrim);
                }
                // DIRECT support
                if (dir[i] != nullptr) for (const USignature& opSig : *dir[i]) {
                    int opVar = left.getVariableOrZero(VarType::OP, opSig);
                    if (opVar > 0) _sat.appendClause(opVar);
                    USignature virt = opSig.renamed(_htn.getRepetitionNameOfAction(opSig._name_id));
                    int virtOpVar = left.getVariableOrZero(VarType::OP, virt);
                    if (virtOpVar > 0) _sat.appendClause(virtOpVar);
                }
                // INDIRECT support
                if (tree[i] != nullptr) for (const auto& [var, tree] : *tree[i]) {
                    _sat.appendClause(var);
                }
            }
            _sat.endClause();
        }

        // Encode substitutions enabling indirect support for this fact
        _stats.begin(STAGE_INDIRECTFRAMEAXIOMS);
        i = -1;
        for (int sign = -1; sign <= 1; sign += 2) {
            i++;
            factVar *= -1;
            if (tree[i] == nullptr) continue;

            // -- 1st part of each clause: "head literals"
            std::vector<int> headLits;
            // IF fact change AND the operation is applied,
            headLits.push_back(0); // operation var goes here
            if (oldFactVars[i] != 0) headLits.push_back(-oldFactVars[i]);
            headLits.push_back(factVar);
            if (!nonprimFactSupport) {
                if (_implicit_primitiveness) {
                    for (int var : _nonprimitive_ops) headLits.push_back(-var);
                } else if (prevVarPrim != 0) headLits.push_back(prevVarPrim);
            } 

            // Encode indirect support constraints
            for (const auto& [opVar, tree] : *tree[i]) {
                
                // Unconditional effect?
                if (tree.containsEmpty()) continue;

                headLits[0] = opVar;                
                for (const auto& cls : tree.encode(headLits)) _sat.addClause(cls);
            }
        }
        _stats.end(STAGE_INDIRECTFRAMEAXIOMS);
    }
    _stats.end(STAGE_DIRECTFRAMEAXIOMS);

    Log::d("Skipped %i frame axioms\n", skipped);
}

void Encoding::encodeOperationConstraints(Position& newPos) {

    size_t layerIdx = newPos.getLayerIndex();
    size_t pos = newPos.getPositionIndex();

    // Store all operations occurring here, for one big clause ORing them
    std::vector<int> elementVars(newPos.getActions().size() + newPos.getReductions().size(), 0);
    int numOccurringOps = 0;

    _stats.begin(STAGE_ACTIONCONSTRAINTS);
    for (const auto& aSig : newPos.getActions()) {
        if (aSig == Sig::NONE_SIG) continue;

        int aVar = _vars.getVariable(VarType::OP, newPos, aSig);
        elementVars[numOccurringOps++] = aVar;
        
        if (_htn.isActionRepetition(aSig._name_id)) continue;

        for (int arg : aSig._args) encodeSubstitutionVars(aSig, aVar, arg, newPos);

        // Preconditions
        for (const Signature& pre : _htn.getAction(aSig).getPreconditions()) {
            if (!_vars.isEncoded(VarType::FACT, layerIdx, pos, pre._usig)) continue;
            _sat.addClause(-aVar, (pre._negated?-1:1)*_vars.getVariable(VarType::FACT, newPos, pre._usig));
        }
    }
    _stats.end(STAGE_ACTIONCONSTRAINTS);
    _stats.begin(STAGE_REDUCTIONCONSTRAINTS);
    for (const auto& rSig : newPos.getReductions()) {
        if (rSig == Sig::NONE_SIG) continue;

        int rVar = _vars.getVariable(VarType::OP, newPos, rSig);
        for (int arg : rSig._args) encodeSubstitutionVars(rSig, rVar, arg, newPos);
        elementVars[numOccurringOps++] = rVar;

        // Preconditions
        for (const Signature& pre : _htn.getReduction(rSig).getPreconditions()) {
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

void Encoding::encodeSubstitutionVars(const USignature& opSig, int opVar, int arg, Position& pos) {

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
        if (aSig == Sig::NONE_SIG) continue;
        if (_htn.isActionRepetition(aSig._name_id)) continue;
        int aVar = _vars.getVariable(VarType::OP, left, aSig);

        for (const Signature& eff : _htn.getAction(aSig).getEffects()) {
            if (!_vars.isEncoded(VarType::FACT, _layer_idx, _pos, eff._usig)) continue;

            std::set<std::set<int>> unifiersDnf;
            bool unifiedUnconditionally = false;
            if (eff._negated) {
                for (const auto& posEff : _htn.getAction(aSig).getEffects()) {
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
                                s.insert(varQConstEquality(effArg, posEffArg));
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
                auto cnf = getCnf(dnf);
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

        // Get "good" and "bad" substitution options
        auto itValid = newPos.getValidSubstitutions().find(opSig);
        auto itForb = newPos.getForbiddenSubstitutions().find(opSig);
        
        // Encode forbidden substitutions

        if (itForb != newPos.getForbiddenSubstitutions().end()) {
            auto cls = itForb->second.encodeNegation();
            for (const auto& sub : cls) {
                _sat.appendClause(-_vars.getVariable(VarType::OP, newPos, opSig));
                for (const auto& [src, dest] : sub) {
                    _sat.appendClause(-_vars.varSubstitution(src, dest));
                }
                _sat.endClause();
            }
        }
        
        // Encode valid substitutions
        
        // For each set of valid substitution options
        if (itValid != newPos.getValidSubstitutions().end()) for (const auto& tree : itValid->second) {
            auto cls = tree.encode();
            for (auto& sub : cls) {
                _sat.appendClause(-_vars.getVariable(VarType::OP, newPos, opSig));
                for (auto& [src, dest] : sub) {
                    bool negated = src < 0;
                    if (negated) src *= -1;
                    _sat.appendClause((negated ? -1 : 1) * _vars.varSubstitution(src, dest));
                }
                _sat.endClause();
            }
        }
    }
    newPos.clearSubstitutions();

    // Globally forbidden substitutions
    for (const auto& sub : _htn.getForbiddenSubstitutions()) {
        assert(!sub.empty());
        if (_forbidden_substitutions.count(sub)) continue;
        for (const auto& [src, dest] : sub) {
            _sat.appendClause(-_vars.varSubstitution(src, dest));
        }
        _sat.endClause();
        _forbidden_substitutions.insert(sub);
    }
    _htn.clearForbiddenSubstitutions();
    
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
        bool forbidden = false;
        for (const USignature& child : children) {
            if (child == Sig::NONE_SIG) {
                // Forbid parent op that turned out to be impossible
                _sat.addClause(-parentVar);
                forbidden = true;
                break;
            }
        }

        if (forbidden) continue;

        _sat.appendClause(-parentVar);
        for (const USignature& child : children) {
            if (child == Sig::NONE_SIG) continue;
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
                        _sat.addClause(-parentVar, -childVar, varQConstEquality(dest, src));
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
            if (child == Sig::NONE_SIG) continue;

            _sat.appendClause(-_vars.getVariable(VarType::OP, newPos, child));
            for (const USignature& parent : parents) {
                _sat.appendClause(_vars.getVariable(VarType::OP, above, parent));
            }
            _sat.endClause();
        }
        _stats.end(STAGE_PREDECESSORS);
    }
}

void Encoding::clearDonePositions() {

    Position* positionToClearLeft = nullptr;
    if (_pos == 0 && _layer_idx > 0) {
        positionToClearLeft = &_layers.at(_layer_idx-1)->last();
    } else if (_pos > 0) positionToClearLeft = &_layers.at(_layer_idx)->at(_pos-1);
    if (positionToClearLeft != nullptr) {
        Log::v("  Freeing some memory of (%i,%i) ...\n", positionToClearLeft->getLayerIndex(), positionToClearLeft->getPositionIndex());
        positionToClearLeft->clearAtPastPosition();
    }

    if (_layer_idx == 0 || _offset != 0) return;
    
    Position* positionToClearAbove = nullptr;
    if (_old_pos == 0) {
        // Clear rightmost position of "above above" layer
        if (_layer_idx > 1) positionToClearAbove = &_layers.at(_layer_idx-2)->at(_layers.at(_layer_idx-2)->size()-1);
    } else {
        // Clear previous parent position of "above" layer
        positionToClearAbove = &_layers.at(_layer_idx-1)->at(_old_pos-1);
    }
    if (positionToClearAbove != nullptr) {
        Log::v("  Freeing most memory of (%i,%i) ...\n", positionToClearAbove->getLayerIndex(), positionToClearAbove->getPositionIndex());
        positionToClearAbove->clearAtPastLayer();
    }
}

void Encoding::optimizePlan(int upperBound, Plan& plan, ConstraintAddition mode) {

    int layerIdx = _layers.size()-1;
    Layer& l = *_layers.at(layerIdx);
    int currentPlanLength = upperBound;
    Log::v("PLO BEGIN %i\n", currentPlanLength);

    // Add counting mechanism
    _stats.begin(STAGE_PLANLENGTHCOUNTING);
    int minPlanLength = 0;
    int maxPlanLength = 0;
    std::vector<int> planLengthVars(1, VariableDomain::nextVar());
    Log::d("VARNAME %i (plan_length_equals %i %i)\n", planLengthVars[0], 0, 0);
    // At position zero, the plan length is always equal to zero
    _sat.addClause(planLengthVars[0]);
    for (size_t pos = 0; pos+1 < l.size(); pos++) {

        // Collect sets of potential operations
        FlatHashSet<int> emptyActions, actualActions;
        for (const auto& aSig : l.at(pos).getActions()) {
            Log::d("PLO %i %s?\n", pos, TOSTR(aSig));
            if (aSig == Sig::NONE_SIG) continue;
            int aVar = l.at(pos).getVariable(VarType::OP, aSig);
            if (isEmptyAction(aSig)) {
                emptyActions.insert(aVar);
            } else {
                actualActions.insert(aVar);
            }
        }
        for (const auto& rSig : l.at(pos).getReductions()) {
            Log::d("PLO %i %s?\n", pos, TOSTR(rSig));
            if (rSig == Sig::NONE_SIG) continue;
            if (_htn.getReduction(rSig).getSubtasks().size() == 0) {
                // Empty reduction
                emptyActions.insert(l.at(pos).getVariable(VarType::OP, rSig));
            }
        }

        if (emptyActions.empty()) {
            // Only actual actions here: Increment lower and upper bound, keep all variables.
            minPlanLength++;
            bool increaseUpperBound = maxPlanLength < currentPlanLength;
            if (increaseUpperBound) maxPlanLength++;
            else {
                // Upper bound hit!
                // Cut counter variables by one, forbid topmost one
                _sat.addClause(-planLengthVars.back());
                planLengthVars.resize(planLengthVars.size()-1);
            }
            Log::d("[no empty ops]\n");
        } else if (actualActions.empty()) {
            // Only empty actions here: Keep current bounds, keep all variables.
            Log::d("[only empty ops]\n");
        } else {
            // Mix of actual and empty actions here: Increment upper bound, 
            bool increaseUpperBound = maxPlanLength < currentPlanLength;
            if (increaseUpperBound) maxPlanLength++;

            int emptySpotVar = 0;
            bool encodeDirectly = emptyActions.size() <= 3 || actualActions.size() <= 3;
            bool encodeEmptiesOnly = emptyActions.size() < actualActions.size();
            bool encodeActualsOnly = emptyActions.size() > actualActions.size();
            if (!encodeDirectly) {
                // Encode with a helper variable
                emptySpotVar = VariableDomain::nextVar();

                // Define for each action var whether it implies an empty spot or not
                for (int v : emptyActions) {
                    // IF the empty action occurs, THEN the spot is empty.
                    _sat.addClause(-v, emptySpotVar);
                }
                for (int v : actualActions) {
                    // IF the actual action occurs, THEN the spot is not empty.
                    _sat.addClause(-v, -emptySpotVar);
                }
            }

            // create new variables and constraints.
            std::vector<int> newPlanLengthVars(planLengthVars.size()+(increaseUpperBound?1:0));
            for (size_t i = 0; i < newPlanLengthVars.size(); i++) {
                newPlanLengthVars[i] = VariableDomain::nextVar();
            }

            // Propagate plan length from previous position to new position
            for (size_t i = 0; i < planLengthVars.size(); i++) {
                int prevVar = planLengthVars[i];
                int keptPlanLengthVar = newPlanLengthVars[i];

                if (i+1 < newPlanLengthVars.size()) {
                    // IF previous plan length is X AND spot is empty
                    // THEN the plan length stays X 
                    if (encodeDirectly) {
                        if (encodeEmptiesOnly) {
                            for (int v : emptyActions) {
                                _sat.addClause(-prevVar, -v, keptPlanLengthVar);
                            }
                        } else {
                            _sat.appendClause(-prevVar, keptPlanLengthVar);
                            for (int v : actualActions) _sat.appendClause(v);
                            _sat.endClause();
                        }
                    } else {
                        _sat.addClause(-prevVar, -emptySpotVar, keptPlanLengthVar);
                    }
                    
                    // IF previous plan length is X AND here is a non-empty spot 
                    // THEN the plan length becomes X+1
                    int incrPlanLengthVar = newPlanLengthVars[i+1];
                    if (encodeDirectly) {
                        if (encodeActualsOnly) {
                            for (int v : actualActions) {
                                _sat.addClause(-prevVar, -v, incrPlanLengthVar);
                            }
                        } else {
                            _sat.appendClause(-prevVar, incrPlanLengthVar);
                            for (int v : emptyActions) _sat.appendClause(v);
                            _sat.endClause();
                        }
                    } else {
                        _sat.addClause(-prevVar, emptySpotVar, incrPlanLengthVar);
                    }

                } else {
                    // Limit hit -- no more actions are admitted
                    // IF previous plan length is X THEN this spot must be empty
                    if (encodeDirectly) {
                        if (encodeActualsOnly) {
                            for (int v : actualActions) {
                                _sat.addClause(-prevVar, -v);
                            }
                        } else {
                            _sat.appendClause(-prevVar);
                            for (int v : emptyActions) _sat.appendClause(v);
                            _sat.endClause();
                        }
                    } else {
                        _sat.addClause(-prevVar, emptySpotVar);
                    }
                    // IF previous plan length is X THEN the plan length stays X
                    _sat.addClause(-prevVar, keptPlanLengthVar);
                }
            }
            planLengthVars = newPlanLengthVars;
        }

        Log::v("Position %i: Plan length bounds [%i,%i]\n", pos, minPlanLength, maxPlanLength);
    }

    Log::i("Tightened initial plan length bounds at layer %i: [0,%i] => [%i,%i]\n",
            layerIdx, l.size()-1, minPlanLength, maxPlanLength);
    assert((int)planLengthVars.size() == maxPlanLength-minPlanLength+1 || Log::e("%i != %i-%i+1\n", planLengthVars.size(), maxPlanLength, minPlanLength));
    
    // Add primitiveness of all positions at the final layer
    // as unit literals (instead of assumptions)
    addAssumptions(layerIdx, /*permanent=*/mode == ConstraintAddition::PERMANENT);
    _stats.end(STAGE_PLANLENGTHCOUNTING);

    int curr = currentPlanLength;
    currentPlanLength = findMinBySat(minPlanLength, std::min(maxPlanLength, currentPlanLength), 
        // Variable mapping
        [&](int currentMax) {
            return planLengthVars[currentMax-minPlanLength];
        }, 
        // Bound update on SAT 
        [&]() {
            // SAT: Shorter plan found!
            plan = extractPlan();
            int newPlanLength = getPlanLength(std::get<0>(plan));
            Log::i("Shorter plan (length %i) found\n", newPlanLength);
            assert(newPlanLength < curr);
            curr = newPlanLength;
            return newPlanLength;
        }, mode);

    float factor = (float)currentPlanLength / minPlanLength;
    if (factor <= 1) {
        Log::v("Plan is globally optimal (static lower bound: %i)\n", minPlanLength);
    } else if (minPlanLength == 0) {
        Log::v("Plan may be arbitrarily suboptimal (static lower bound: 0)\n");
    } else {
        Log::v("Plan may be suboptimal by a maximum factor of %.2f (static lower bound: %i)\n", factor, minPlanLength);
    }
}

int Encoding::findMinBySat(int lower, int upper, std::function<int(int)> varMap, 
            std::function<int(void)> boundUpdateOnSat, ConstraintAddition mode) {

    int originalUpper = upper;
    int current = upper;

    // Solving iterations
    while (true) {
        // Hit lower bound of possible plan lengths? 
        if (current == lower) {
            Log::v("PLO END %i\n", current);
            Log::i("Length of current plan is at lower bound (%i): finished\n", lower);
            break;
        }

        // Assume a shorter plan by one
        _stats.begin(STAGE_PLANLENGTHCOUNTING);

        // Permanently forbid any plan lengths greater than / equal to the last found plan
        if (mode == TRANSIENT) {
            upper = originalUpper;
        }
        while (upper > current) {
            Log::d("GUARANTEE PL!=%i\n", upper);
            int probedVar = varMap(upper);
            if (mode == TRANSIENT) _sat.assume(-probedVar);
            else _sat.addClause(-probedVar);
            upper--;
        }
        assert(upper == current);
        
        // Assume a plan length shorter than the last found plan
        Log::d("GUARANTEE PL!=%i\n", upper);
        int probedVar = varMap(upper);
        if (mode == TRANSIENT) _sat.assume(-probedVar);
        else _sat.addClause(-probedVar);

        _stats.end(STAGE_PLANLENGTHCOUNTING);

        Log::i("Searching for a plan of length < %i\n", upper);
        int result = solve();

        // Check result
        if (result == 10) {
            // SAT: Shorter plan found!
            current = boundUpdateOnSat();
            Log::v("PLO UPDATE %i\n", current);
        } else if (result == 20) {
            // UNSAT
            Log::v("PLO END %i\n", current);
            break;
        } else {
            // UNKNOWN
            Log::v("PLO END %i\n", current);
            break;
        }
    }

    return current;
}

bool Encoding::isEmptyAction(const USignature& aSig) {
    if (_htn.isSecondPartOfSplitAction(aSig) || _htn.getBlankActionSig() == aSig)
        return true;
    if (_htn.getActionNameFromRepetition(aSig._name_id) == _htn.getBlankActionSig()._name_id)
        return true;
    return false;
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

std::set<std::set<int>> Encoding::getCnf(const std::vector<int>& dnf) {
    std::set<std::set<int>> cnf;

    if (dnf.empty()) return cnf;

    int size = 1;
    int clsSize = 0;
    std::vector<int> clsStarts;
    for (size_t i = 0; i < dnf.size(); i++) {
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
        for (size_t pos = 0; pos < counter.size(); pos++) {
            const int& lit = dnf[clsStarts[pos]+counter[pos]];
            assert(lit != 0);
            newCls.insert(lit);
        }
        cnf.insert(newCls);            

        // Increment exponential counter
        size_t x = 0;
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

int Encoding::varQConstEquality(int q1, int q2) {
    std::pair<int, int> qPair(std::min(q1, q2), std::max(q1, q2));
    if (!_q_equality_variables.count(qPair)) {
        
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
        int varEq = VariableDomain::nextVar();
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

        _q_equality_variables[qPair] = varEq;
    }
    return _q_equality_variables[qPair];
}

std::string Encoding::varName(int layer, int pos, const USignature& sig) {
    return VariableDomain::varName(layer, pos, sig);
}

void Encoding::printVar(int layer, int pos, const USignature& sig) {
    Log::d("%s\n", VariableDomain::varName(layer, pos, sig).c_str());
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

std::vector<PlanItem> Encoding::extractClassicalPlan(PlanExtraction mode) {

    Layer& finalLayer = *_layers.back();
    int li = finalLayer.index();
    //VariableDomain::lock();

    std::vector<PlanItem> plan(finalLayer.size());
    //log("(actions at layer %i)\n", li);
    for (size_t pos = 0; pos < finalLayer.size(); pos++) {
        //log("%i\n", pos);

        // Print out the state
        Log::d("PLANDBG %i,%i S ", li, pos);
        for (const auto& [sig, fVar] : finalLayer[pos].getVariableTable(VarType::FACT)) {
            if (_sat.holds(fVar)) Log::log_notime(Log::V4_DEBUG, "%s ", TOSTR(sig));
        }
        Log::log_notime(Log::V4_DEBUG, "\n");

        int chosenActions = 0;
        //State newState = state;
        for (const auto& [sig, aVar] : finalLayer[pos].getVariableTable(VarType::OP)) {
            if (!_sat.holds(aVar)) continue;

            USignature aSig = sig;
            if (mode == PRIMITIVE_ONLY && !_htn.isAction(aSig)) continue;

            if (_htn.isActionRepetition(aSig._name_id)) {
                aSig._name_id = _htn.getActionNameFromRepetition(sig._name_id);
            }

            //log("  %s ?\n", TOSTR(aSig));

            chosenActions++;
            
            Log::d("PLANDBG %i,%i A %s\n", li, pos, TOSTR(aSig));

            // Decode q constants
            USignature aDec = getDecodedQOp(li, pos, aSig);
            if (aDec == Sig::NONE_SIG) continue;
            plan[pos] = {aVar, aDec, aDec, std::vector<int>()};
        }

        assert(chosenActions <= 1 || Log::e("Plan error: Added %i actions at step %i!\n", chosenActions, pos));
        if (chosenActions == 0) {
            plan[pos] = {-1, USignature(), USignature(), std::vector<int>()};
        }
    }

    //log("%i actions at final layer of size %i\n", plan.size(), _layers->back().size());
    return plan;
}

bool holds(State& state, const Signature& fact) {

    // Positive fact
    if (!fact._negated) return state[fact._usig._name_id].count(fact);
    
    // Negative fact: fact is contained, OR counterpart is NOT contained
    return state[fact._usig._name_id].count(fact) || !state[fact._usig._name_id].count(fact.opposite());
}

Plan Encoding::extractPlan() {

    auto result = Plan();
    auto& [classicalPlan, plan] = result;
    classicalPlan = extractClassicalPlan();
    
    std::vector<PlanItem> itemsOldLayer, itemsNewLayer;

    for (size_t layerIdx = 0; layerIdx < _layers.size(); layerIdx++) {
        Layer& l = *_layers.at(layerIdx);
        //log("(decomps at layer %i)\n", l.index());

        itemsNewLayer.resize(l.size());
        
        for (size_t pos = 0; pos < l.size(); pos++) {

            size_t predPos = 0;
            if (layerIdx > 0) {
                Layer& lastLayer = *_layers.at(layerIdx-1);
                while (predPos+1 < lastLayer.size() && lastLayer.getSuccessorPos(predPos+1) <= pos) 
                    predPos++;
            } 
            //log("%i -> %i\n", predPos, pos);

            int actionsThisPos = 0;
            int reductionsThisPos = 0;

            for (const auto& [opSig, v] : l[pos].getVariableTable(VarType::OP)) {

                if (_sat.holds(v)) {

                    if (_htn.isAction(opSig)) {
                        // Action
                        actionsThisPos++;
                        const USignature& aSig = opSig;

                        if (aSig == _htn.getBlankActionSig()) continue;
                        if (_htn.isActionRepetition(aSig._name_id)) {
                            continue;
                        }
                        
                        int v = _vars.getVariable(VarType::OP, layerIdx, pos, aSig);
                        Action a = _htn.getAction(aSig);

                        // TODO check this is a valid subtask relationship

                        Log::d("[%i] %s @ (%i,%i)\n", v, TOSTR(aSig), layerIdx, pos);                    

                        // Find the actual action variable at the final layer, not at this (inner) layer
                        size_t l = layerIdx;
                        int aPos = pos;
                        while (l+1 < _layers.size()) {
                            //log("(%i,%i) => ", l, aPos);
                            aPos = _layers.at(l)->getSuccessorPos(aPos);
                            l++;
                            //log("(%i,%i)\n", l, aPos);
                        }
                        v = classicalPlan[aPos].id; // *_layers.at(l-1)[aPos]._vars.getVariable(aSig);
                        //assert(v > 0 || Log::e("%s : v=%i\n", TOSTR(aSig), v));
                        if (v > 0 && layerIdx > 0) {
                            itemsOldLayer[predPos].subtaskIds.push_back(v);
                        } else if (v <= 0) {
                            Log::d(" -- invalid: not part of classical plan\n");
                        }

                        //itemsNewLayer[pos] = PlanItem({v, aSig, aSig, std::vector<int>()});

                    } else if (_htn.isReduction(opSig)) {
                        // Reduction
                        const USignature& rSig = opSig;
                        const Reduction& r = _htn.getReduction(rSig);

                        //log("%s:%s @ (%i,%i)\n", TOSTR(r.getTaskSignature()), TOSTR(rSig), layerIdx, pos);
                        USignature decRSig = getDecodedQOp(layerIdx, pos, rSig);
                        if (decRSig == Sig::NONE_SIG) continue;

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
                        size_t offset = pos - _layers.at(layerIdx-1)->getSuccessorPos(predPos);
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
                }
            }

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

bool Encoding::value(VarType type, int layer, int pos, const USignature& sig) {
    int v = _vars.getVariable(type, layer, pos, sig);
    Log::d("VAL %s@(%i,%i)=%i %i\n", TOSTR(sig), layer, pos, v, _sat.holds(v));
    return _sat.holds(v);
}

int Encoding::getPlanLength(const std::vector<PlanItem>& classicalPlan) {
    int currentPlanLength = 0;
    for (size_t pos = 0; pos+1 < classicalPlan.size(); pos++) {
        const auto& aSig = classicalPlan[pos].abstractTask;
        Log::d("%s\n", TOSTR(aSig));
        // Reduction with an empty expansion?
        if (aSig._name_id < 0) continue;
        // No blank action, no second part of a split action?
        else if (!isEmptyAction(aSig)) {
            currentPlanLength++;
        }
    }
    return currentPlanLength;
}

void Encoding::printSatisfyingAssignment() {
    Log::d("SOLUTION_VALS ");
    for (int v = 1; v <= VariableDomain::getMaxVar(); v++) {
        Log::d("%i ", _sat.holds(v) ? v : -v);
    }
    Log::d("\n");
}

USignature Encoding::getDecodedQOp(int layer, int pos, const USignature& origSig) {
    //assert(isEncoded(VarType::OP, layer, pos, origSig));
    //assert(value(VarType::OP, layer, pos, origSig));

    USignature sig = origSig;
    while (true) {
        bool containsQConstants = false;
        for (int arg : sig._args) if (_htn.isQConstant(arg)) {
            // q constant found
            containsQConstants = true;

            int numSubstitutions = 0;
            for (int argSubst : _htn.getDomainOfQConstant(arg)) {
                const USignature& sigSubst = _vars.sigSubstitute(arg, argSubst);
                if (_vars.isEncodedSubstitution(sigSubst) && _sat.holds(_vars.varSubstitution(arg, argSubst))) {
                    Log::d("SUBSTVAR [%s/%s] TRUE => %s ~~> ", TOSTR(arg), TOSTR(argSubst), TOSTR(sig));
                    numSubstitutions++;
                    Substitution sub;
                    sub[arg] = argSubst;
                    sig.apply(sub);
                    Log::d("%s\n", TOSTR(sig));
                } else {
                    //Log::d("%i FALSE\n", varSubstitution(sigSubst));
                }
            }

            if (numSubstitutions == 0) {
                Log::v("(%i,%i) No substitutions for arg %s of %s\n", layer, pos, TOSTR(arg), TOSTR(origSig));
                return Sig::NONE_SIG;
            }
            assert(numSubstitutions == 1 || Log::e("%i substitutions for arg %s of %s\n", numSubstitutions, TOSTR(arg), TOSTR(origSig)));
        }

        if (!containsQConstants) break; // done
    }

    //if (origSig != sig) Log::d("%s ~~> %s\n", TOSTR(origSig), TOSTR(sig));
    
    return sig;
}
