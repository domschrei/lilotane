
#include <random>

#include "sat/encoding.h"
#include "sat/literal_tree.h"
#include "sat/binary_amo.h"
#include "util/log.h"
#include "util/timer.h"


Encoding::Encoding(Parameters& params, HtnInstance& htn, std::vector<Layer*>& layers, std::function<void()> terminationCallback) : 
            _params(params), _htn(htn), _layers(layers), 
            _termination_callback(terminationCallback), 
            _print_formula(params.isNonzero("wf")),
            _use_q_constant_mutexes(_params.getIntParam("qcm") > 0), 
            _implicit_primitiveness(params.isNonzero("ip")) {
    _solver = ipasir_init();
    ipasir_set_seed(_solver, _params.getIntParam("s"));
    _sig_primitive = USignature(_htn.nameId("__PRIMITIVE___"), std::vector<int>());
    _substitute_name_id = _htn.nameId("__SUBSTITUTE___");
    _sig_substitution = USignature(_substitute_name_id, std::vector<int>(2));
    if (_print_formula) _out.open("formula.cnf");
    VariableDomain::init(params);
    _num_cls = 0;
    _num_lits = 0;
    _num_asmpts = 0;
}

void Encoding::encode(size_t layerIdx, size_t pos) {
    _termination_callback();

    //Log::v("Encoding ...\n");
    int priorNumClauses = _num_cls;
    int priorNumLits = _num_lits;
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
    begin(STAGE_AXIOMATICOPS);
    const USigSet& axiomaticOps = newPos.getAxiomaticOps();
    if (!axiomaticOps.empty()) {
        for (const USignature& op : axiomaticOps) {
            appendClause(getVariable(VarType::OP, newPos, op));
        }
        endClause();
    }
    end(STAGE_AXIOMATICOPS);

    Log::v("  Encoded (%i,%i): %i clauses, total of %i literals\n", layerIdx, pos, _num_cls-priorNumClauses, _num_lits-priorNumLits);

    clearDonePositions();
}

void Encoding::encodeOperationVariables(Position& newPos) {

    _primitive_ops.clear();
    _nonprimitive_ops.clear();

    begin(STAGE_ACTIONCONSTRAINTS);
    for (const auto& aSig : newPos.getActions()) {
        if (aSig == Position::NONE_SIG) continue;
        int aVar = encodeVariable(VarType::OP, newPos, aSig, true);

        // If the action occurs, the position is primitive
        _primitive_ops.push_back(aVar);
    }
    end(STAGE_ACTIONCONSTRAINTS);

    begin(STAGE_REDUCTIONCONSTRAINTS);
    for (const auto& rSig : newPos.getReductions()) {
        if (rSig == Position::NONE_SIG) continue;
        int rVar = encodeVariable(VarType::OP, newPos, rSig, true);

        bool trivialReduction = _htn.getReduction(rSig).getSubtasks().size() == 0;
        if (trivialReduction) {
            // If a trivial reduction occurs, the position is primitive
            _primitive_ops.push_back(rVar);
        } else {
            // If another reduction occurs, the position is non-primitive
            _nonprimitive_ops.push_back(rVar);
        }
    }
    end(STAGE_REDUCTIONCONSTRAINTS);

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

    int varPrim = encodeVarPrimitive(newPos.getLayerIndex(), newPos.getPositionIndex());

    if (_primitive_ops.empty()) {
        // Only non-primitive ops here
        addClause(-varPrim);
    } else {
        // Mix of primitive and non-primitive ops (default)
        for (int aVar : _primitive_ops) addClause(-aVar, varPrim);
        for (int rVar : _nonprimitive_ops) addClause(-rVar, -varPrim);
    }
}

void Encoding::encodeFactVariables(Position& newPos, Position& left, Position& above) {

    _new_fact_vars.clear();

    begin(STAGE_FACTVARENCODING);

    // Reuse variables from above position
    if (newPos.getLayerIndex() > 0 && _offset == 0) {
        newPos.setVariableTable(VarType::FACT, above.getVariableTable(VarType::FACT));
        //above.moveVariableTable(VarType::FACT, newPos);
    }

    if (_pos == 0) {
        // Encode all definitive facts
        const USigSet* defFacts[] = {&newPos.getTrueFacts(), &newPos.getFalseFacts()};
        for (auto set : defFacts) for (const auto& fact : *set) {
            if (!newPos.hasVariable(VarType::FACT, fact) && _htn.isRelevant(fact)) 
                _new_fact_vars.insert(encodeVariable(VarType::FACT, newPos, fact, false));
        }
    } else {
        // Encode frame axioms which will assign variables to all ground facts
        // that have some support to change at this position
        encodeFrameAxioms(newPos, left);
    }

    // Encode q-facts that are not encoded yet
    for ([[maybe_unused]] const auto& qfact : newPos.getQFacts()) {
        if (newPos.hasVariable(VarType::FACT, qfact)) continue;
        if (!newPos.hasQFactDecodings(qfact)) continue;
        // Encode new variable
        _new_fact_vars.insert(encodeVariable(VarType::FACT, newPos, qfact, false));        
    }
    end(STAGE_FACTVARENCODING);

    // Facts that must hold at this position
    begin(STAGE_TRUEFACTS);
    const USigSet* cHere[] = {&newPos.getTrueFacts(), &newPos.getFalseFacts()}; 
    for (int i = 0; i < 2; i++) 
    for (const USignature& factSig : *cHere[i]) if (_htn.isRelevant(factSig)) {
        int var = newPos.getVariableOrZero(VarType::FACT, factSig);
        if (var == 0) {
            // Variable is not encoded yet.
            addClause((i == 0 ? 1 : -1) * encodeVariable(VarType::FACT, newPos, factSig, false));
        } else {
            // Variable is already encoded. If the variable is new, constrain it.
            if (_new_fact_vars.count(var)) addClause((i == 0 ? 1 : -1) * var);
        }
        Log::d("DEFFACT %s\n", TOSTR(factSig));
    }
    end(STAGE_TRUEFACTS);
}

std::pair<IndirectSupport, IndirectSupport> Encoding::computeFactSupports(Position& newPos, Position& left) {
    static Position NULL_POS;

    // Retrieve supports
    NodeHashMap<USignature, USigSet, USignatureHasher>* dirSupports[2] 
            = {&newPos.getNegFactSupports(), &newPos.getPosFactSupports()};
    IndirectFactSupportMap* indirSupports[2] 
            = {&newPos.getNegIndirectFactSupports(), &newPos.getPosIndirectFactSupports()};
    
    // Structures for indirect supports:
    // Maps each fact to a map of operation variables to a tree of substitutions 
    // which make the operation (possibly) change the variable.
    std::pair<IndirectSupport, IndirectSupport> result;
    auto& [negResult, posResult] = result;

    // For both fact polarities
    for (size_t i = 0; i < 2; i++) {
        
        // For each fact with some indirect support, for each action in the support
        auto& output = i == 0 ? negResult : posResult;
        for (const auto& [fact, entry] : *indirSupports[i]) for (const auto& [op, subs] : entry) {
            assert(!_htn.isVirtualizedChildOfAction(op._name_id));

            // Skip if the operation is already a DIRECT support for the fact
            auto it = dirSupports[i]->find(fact);
            if (it != dirSupports[i]->end() && it->second.count(op)) continue;

            int opVar = left.getVariableOrZero(VarType::OP, op);
            USignature virtOp(_htn.getVirtualizedChildNameOfAction(op._name_id), op._args);
            int virtOpVar = left.getVariableOrZero(VarType::OP, virtOp);
            
            // Not an encoded action? (May have been pruned away)
            if (opVar == 0 && virtOpVar == 0) continue;

            // For each substitution leading to the desired ground effect:
            for (const auto& s : subs) {
                assert(!s.empty());
                std::vector<int> sVars(s.size());
                size_t i = 0;
                for (const auto& [src, dest] : s) {
                    sVars[i++] = varSubstitution(sigSubstitute(src, dest));
                }
                std::sort(sVars.begin(), sVars.end());

                // Insert into according support tree
                if (opVar != 0) output[fact][opVar].insert(sVars);
                if (virtOpVar != 0) output[fact][virtOpVar].insert(sVars);
            }
        }
    }

    return result;
}

void Encoding::encodeFrameAxioms(Position& newPos, Position& left) {
    static Position NULL_POS;

    using Supports = const NodeHashMap<USignature, USigSet, USignatureHasher>;

    begin(STAGE_DIRECTFRAMEAXIOMS);

    bool nonprimFactSupport = _params.isNonzero("nps");
    bool hasPrimitiveOps = left.hasPrimitiveOps();

    int layerIdx = newPos.getLayerIndex();
    int pos = newPos.getPositionIndex();
    int prevVarPrim = getVarPrimitiveOrZero(layerIdx, pos-1);

    Position& above = layerIdx > 0 ? _layers[layerIdx-1]->at(_old_pos) : NULL_POS;
    Position& leftOfAbove = layerIdx > 0 && _old_pos > 0 ? _layers[layerIdx-1]->at(_old_pos-1) : NULL_POS;
    bool skipRedundantFrameAxioms = _params.isNonzero("srfa") && _offset == 0
        && hasPrimitiveOps && !left.hasNonprimitiveOps()
        && leftOfAbove.hasPrimitiveOps() && !leftOfAbove.hasNonprimitiveOps();

    // Retrieve supports from left position
    Supports* supp[2] = {&newPos.getNegFactSupports(), &newPos.getPosFactSupports()};
    IndirectFactSupportMap* iSupp[2] = {&newPos.getNegIndirectFactSupports(), &newPos.getPosIndirectFactSupports()};
    // Retrieve indirect support substitutions
    auto [negIS, posIS] = computeFactSupports(newPos, left);
    IndirectSupport* iSuppTrees[2] = {&negIS, &posIS};

    // Retrieve supports from above position
    Supports* aboveSupp[2] = {&above.getNegFactSupports(), &above.getPosFactSupports()};
    IndirectFactSupportMap* aboveISupp[2] = {&above.getNegIndirectFactSupports(), &above.getPosIndirectFactSupports()};
    
    // Find and encode frame axioms for each applicable fact from the left
    size_t skipped = 0;
    for ([[maybe_unused]] const auto& [fact, var] : left.getVariableTable(VarType::FACT)) {
        if (_htn.hasQConstants(fact)) continue;
        
        int oldFactVars[2] = {-var, var};
        const USigSet* dir[2] = {nullptr, nullptr};
        const IndirectFactSupportMapEntry* indir[2] = {nullptr, nullptr};
        const NodeHashMap<int, LiteralTree>* tree[2] = {nullptr, nullptr};

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
                    indir[i] = &(it->second);
                    reuse = false;
                    auto itt = iSuppTrees[i]->find(fact);
                    if (itt != iSuppTrees[i]->end()) {
                        tree[i] = &(itt->second);
                    }
                } 
            }
        }

        int factVar = newPos.getVariableOrZero(VarType::FACT, fact);

        // Check if there is already an equivalent fact support ABOVE
        // (only if at offset 0, and if the frame axioms are not trivial either way)
        bool skip[2] = {false, false};
        if (skipRedundantFrameAxioms && !reuse) {
            int aboveVar = above.getVariableOrZero(VarType::FACT, fact);
            if (aboveVar != 0) {
                // Should reuse variable from above
                assert(factVar == aboveVar);

                // Check for equivalence of fact support
                for (int i = 0; i < 2; i++) {
                    int suppSize = 0;
                    {
                        // Equivalent direct support at the two positions?
                        auto aboveIt = aboveSupp[i]->find(fact);
                        if (aboveIt == aboveSupp[i]->end() && dir[i] != nullptr)
                            // No support above, but support to the left
                            continue;
                        if (aboveIt != aboveSupp[i]->end()) {
                            if (dir[i] == nullptr)
                                // Support above, but no support to the left
                                continue;
                            // Both have some support
                            auto* aboveDir = &aboveIt->second;
                            if (*dir[i] != *aboveDir) continue; // Support not equal
                            suppSize += dir[i]->size();
                        }
                    }
                    {
                        // Equivalent indirect support at the two positions?
                        auto aboveIt = aboveISupp[i]->find(fact);
                        if (aboveIt == aboveISupp[i]->end() && indir[i] != nullptr)
                            // No support above, but support to the left
                            continue;
                        if (aboveIt != aboveISupp[i]->end()) {
                            if (indir[i] == nullptr)
                                // Support above, but no support to the left
                                continue;
                            // Both have some support
                            auto* aboveIndir = &aboveIt->second;
                            if (indir[i]->size() != aboveIndir->size()) continue; // different #entries
                            bool ok = true;
                            for (const auto& [op, subs] : *indir[i]) {
                                if (!aboveIndir->count(op)) {ok = false; break;} // entry not in both
                            }
                            if (!ok) continue;
                            suppSize += indir[i]->size();
                        }
                    }

                    // All equivalence checks passed:
                    // Frame axiom was already encoded equivalently for fact above
                    skip[i] = true;
                    Log::d("Skipping frame axiom of %s; %i supporting ops\n", TOSTR(fact), suppSize);
                    skipped++;
                }
            } 
        }

        // Decide on the fact variable to use (reuse or encode)
        if (factVar == 0) {
            if (reuse) {
                // No support for this fact -- variable can be reused from left
                factVar = var;
                newPos.setVariable(VarType::FACT, fact, var);
            } else {
                // There is some support for this fact -- need to encode new var
                int v = encodeVariable(VarType::FACT, newPos, fact, false);
                _new_fact_vars.insert(v);
                factVar = v;
            }
        }
        if (var == factVar) continue; // Skip frame axiom encoding
        
        // No primitive ops at this position: No need for encoding frame axioms
        if (!hasPrimitiveOps) continue;

        // Encode general frame axioms for this fact
        int i = -1;
        for (int sign = -1; sign <= 1; sign += 2) {
            i++;
            if (skip[i]) continue;
            // Fact change:
            if (oldFactVars[i] != 0) appendClause(oldFactVars[i]);
            appendClause(-sign*factVar);
            if (dir[i] != nullptr || indir[i] != nullptr) {
                // Non-primitiveness wildcard
                if (!nonprimFactSupport) {
                    if (_implicit_primitiveness) {
                        for (int var : _nonprimitive_ops) appendClause(var);
                    } else if (prevVarPrim != 0) appendClause(-prevVarPrim);
                }
                // DIRECT support
                if (dir[i] != nullptr) for (const USignature& opSig : *dir[i]) {
                    int opVar = left.getVariableOrZero(VarType::OP, opSig);
                    if (opVar > 0) appendClause(opVar);
                    USignature virt = opSig.renamed(_htn.getVirtualizedChildNameOfAction(opSig._name_id));
                    int virtOpVar = left.getVariableOrZero(VarType::OP, virt);
                    if (virtOpVar > 0) appendClause(virtOpVar);
                }
                // INDIRECT support
                if (tree[i] != nullptr) for (const auto& [var, tree] : *tree[i]) {
                    appendClause(var);
                }
            }
            endClause();
        }

        // Encode substitutions enabling indirect support for this fact
        begin(STAGE_INDIRECTFRAMEAXIOMS);
        i = -1;
        for (int sign = -1; sign <= 1; sign += 2) {
            i++;
            factVar *= -1;
            if (skip[i]) continue;
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
                for (const auto& cls : tree.encode(headLits)) addClause(cls);
            }
        }
        end(STAGE_INDIRECTFRAMEAXIOMS);
    }
    end(STAGE_DIRECTFRAMEAXIOMS);

    Log::d("Skipped %i frame axioms\n", skipped);
}

void Encoding::encodeOperationConstraints(Position& newPos) {

    size_t layerIdx = newPos.getLayerIndex();
    size_t pos = newPos.getPositionIndex();

    // Store all operations occurring here, for one big clause ORing them
    std::vector<int> elementVars(newPos.getActions().size() + newPos.getReductions().size(), 0);
    int numOccurringOps = 0;

    begin(STAGE_ACTIONCONSTRAINTS);
    for (const auto& aSig : newPos.getActions()) {
        if (aSig == Position::NONE_SIG) continue;

        int aVar = getVariable(VarType::OP, newPos, aSig);
        elementVars[numOccurringOps++] = aVar;
        
        if (_htn.isVirtualizedChildOfAction(aSig._name_id)) continue;

        for (int arg : aSig._args) encodeSubstitutionVars(aVar, arg, newPos);

        // Preconditions
        for (const Signature& pre : _htn.getAction(aSig).getPreconditions()) {
            if (!isEncoded(VarType::FACT, layerIdx, pos, pre._usig)) continue;
            addClause(-aVar, (pre._negated?-1:1)*getVariable(VarType::FACT, newPos, pre._usig));
        }
    }
    end(STAGE_ACTIONCONSTRAINTS);
    begin(STAGE_REDUCTIONCONSTRAINTS);
    for (const auto& rSig : newPos.getReductions()) {
        if (rSig == Position::NONE_SIG) continue;

        int rVar = getVariable(VarType::OP, newPos, rSig);
        for (int arg : rSig._args) encodeSubstitutionVars(rVar, arg, newPos);
        elementVars[numOccurringOps++] = rVar;

        // Preconditions
        for (const Signature& pre : _htn.getReduction(rSig).getPreconditions()) {
            if (!isEncoded(VarType::FACT, layerIdx, pos, pre._usig)) continue;
            addClause(-rVar, (pre._negated?-1:1)*getVariable(VarType::FACT, newPos, pre._usig));
        }
    }
    end(STAGE_REDUCTIONCONSTRAINTS);
    
    if (numOccurringOps == 0 && pos+1 != _layers.back()->size()) {
        Log::e("No operations to encode at (%i,%i)! Considering this problem UNSOLVABLE.\n", layerIdx, pos);
        exit(1);
    }

    if ((int)elementVars.size() >= _params.getIntParam("bamot")) {
        // Binary exactly-one

        begin(STAGE_ATMOSTONEELEMENT);
        auto bamo = BinaryAtMostOne(elementVars, elementVars.size());
        for (const auto& c : bamo.encode()) addClause(c);
        end(STAGE_ATMOSTONEELEMENT);

    } else {
        // Naive exactly-one

        if (_params.isNonzero("alo")) {
            begin(STAGE_ATLEASTONEELEMENT);
            size_t i = 0; 
            while (i < elementVars.size() && elementVars[i] != 0) 
                appendClause(elementVars[i++]);
            endClause();
            end(STAGE_ATLEASTONEELEMENT);
        }

        begin(STAGE_ATMOSTONEELEMENT);
        for (size_t i = 0; i < elementVars.size(); i++) {
            for (size_t j = i+1; j < elementVars.size(); j++) {
                addClause(-elementVars[i], -elementVars[j]);
            }
        }
        end(STAGE_ATMOSTONEELEMENT);
    }

    if (_params.isNonzero("svp")) setVariablePhases(elementVars);
}

void Encoding::encodeSubstitutionVars(int opVar, int arg, Position& pos) {

    if (!_htn.isQConstant(arg)) return;
    if (_q_constants.count(arg)) return;
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
    if ((int)substitutionVars.size() >= _params.getIntParam("bamot")) {
        // Binary at-most-one
        auto bamo = BinaryAtMostOne(substitutionVars, substitutionVars.size()+1);
        for (const auto& c : bamo.encode()) addClause(c);
    } else {
        // Naive at-most-one
        for (int vSub1 : substitutionVars) {
            for (int vSub2 : substitutionVars) {
                if (vSub1 < vSub2) addClause(-vSub1, -vSub2);
            }
        }
    }

    if (_params.isNonzero("svp")) setVariablePhases(substitutionVars);
}

void Encoding::encodeQFactSemantics(Position& newPos) {

    begin(STAGE_QFACTSEMANTICS);
    std::vector<int> substitutionVars; substitutionVars.reserve(128);
    std::vector<int> qargs, qargIndices; 
    for (const auto& qfactSig : newPos.getQFacts()) {
        assert(_htn.hasQConstants(qfactSig));
        if (!newPos.hasQFactDecodings(qfactSig)) continue;

        int qfactVar = getVariable(VarType::FACT, newPos, qfactSig);

        // Already encoded earlier?
        if (!_new_fact_vars.count(qfactVar)) continue;

        // For each possible fact decoding:
        for (const auto& decFactSig : newPos.getQFactDecodings(qfactSig)) {

            // Assemble list of substitution variables
            int decFactVar = getVariable(VarType::FACT, newPos, decFactSig);
            for (size_t i = 0; i < qfactSig._args.size(); i++) {
                if (qfactSig._args[i] != decFactSig._args[i])
                    substitutionVars.push_back(
                        varSubstitution(sigSubstitute(qfactSig._args[i], decFactSig._args[i]))
                    );
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
    end(STAGE_QFACTSEMANTICS);
}

void Encoding::encodeActionEffects(Position& newPos, Position& left) {

    bool treeConversion = _params.isNonzero("tc");
    begin(STAGE_ACTIONEFFECTS);
    for (const auto& aSig : left.getActions()) {
        if (aSig == Position::NONE_SIG) continue;
        if (_htn.isVirtualizedChildOfAction(aSig._name_id)) continue;
        int aVar = getVariable(VarType::OP, left, aSig);

        for (const Signature& eff : _htn.getAction(aSig).getEffects()) {
                        
            std::set<std::set<int>> unifiersDnf;
            bool unifiedUnconditionally = false;
            if (eff._negated) {
                for (const auto& posEff : _htn.getAction(aSig).getEffects()) {
                    if (posEff._negated) continue;
                    if (posEff._usig._name_id != eff._usig._name_id) continue;
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
                                else s.insert(varSubstitution(sigSubstitute(effArg, posEffArg)));
                            } else if (posEffIsQ) {
                                if (!_htn.getDomainOfQConstant(posEffArg).count(effArg)) fits = false;
                                else s.insert(varSubstitution(sigSubstitute(posEffArg, effArg)));
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
            if (unifiedUnconditionally) {
                //addClause(-aVar, -getVariable(newPos, eff._usig));
            } else if (unifiersDnf.empty()) {
                addClause(-aVar, (eff._negated?-1:1)*getVariable(VarType::FACT, newPos, eff._usig));
            } else {
                if (treeConversion) {
                    LiteralTree tree;
                    for (const auto& set : unifiersDnf) tree.insert(std::vector<int>(set.begin(), set.end()));
                    std::vector<int> headerLits;
                    headerLits.push_back(aVar);
                    headerLits.push_back(getVariable(VarType::FACT, newPos, eff._usig));
                    for (const auto& cls : tree.encode(headerLits)) addClause(cls);
                } else {
                    std::vector<int> dnf;
                    for (const auto& set : unifiersDnf) {
                        for (int lit : set) dnf.push_back(lit);
                        dnf.push_back(0);
                    }
                    auto cnf = getCnf(dnf);
                    for (const auto& clause : cnf) {
                        appendClause(-aVar, -getVariable(VarType::FACT, newPos, eff._usig));
                        for (int lit : clause) appendClause(lit);
                        endClause();
                    }
                }
            }
        }
    }
    end(STAGE_ACTIONEFFECTS);
}

void Encoding::encodeQConstraints(Position& newPos) {

    // Q-constants type constraints
    begin(STAGE_QTYPECONSTRAINTS);
    const auto& constraints = newPos.getQConstantsTypeConstraints();
    for (const auto& pair : constraints) {
        const USignature& opSig = pair.first;
        int opVar = newPos.getVariableOrZero(VarType::OP, opSig);
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
    end(STAGE_QTYPECONSTRAINTS);

    // Forbidden substitutions per operator
    begin(STAGE_SUBSTITUTIONCONSTRAINTS);

    // For each operation (action or reduction)
    const USigSet* ops[2] = {&newPos.getActions(), &newPos.getReductions()};
    for (const auto& set : ops) for (auto opSig : *set) {

        // Get number of "good" and "bad" substitution options
        size_t goodSize = 0;
        auto itValid = newPos.getValidSubstitutions().find(opSig);
        if (itValid != newPos.getValidSubstitutions().end()) {
            for (const auto& s : itValid->second) {
                goodSize += s.size();
            }
        }
        auto itForb = newPos.getForbiddenSubstitutions().find(opSig);
        size_t badSize = itForb != newPos.getForbiddenSubstitutions().end() ? itForb->second.size() : 0;
        if (badSize == 0) continue;

        // Which one to encode?
        if (badSize <= goodSize) {
            // Use forbidden substitutions
            for (const Substitution& s : itForb->second) {
                appendClause(-getVariable(VarType::OP, newPos, opSig));
                for (const auto& [src, dest] : s) {
                    appendClause(-varSubstitution(sigSubstitute(src, dest)));
                }
                endClause();
            }
        } else {
            // Use valid substitutions
            
            // For each set of valid substitution options
            for (const auto& subs : itValid->second) {
                // Build literal tree from this set of valid substitution options for this op
                LiteralTree tree;
                // For each substitution option:
                for (const auto& s : subs) {
                    std::vector<int> sVars(s.size()); 
                    size_t i = 0;
                    // For each atomic substitution:
                    for (const auto& [src, dest] : s) {
                        sVars[i++] = varSubstitution(sigSubstitute(src, dest));
                    }
                    tree.insert(sVars);
                }
                // Encode set of valid substitution options into CNF
                std::vector<std::vector<int>> cls = tree.encode(
                    std::vector<int>(1, getVariable(VarType::OP, newPos, opSig))
                );
                for (const auto& c : cls) {
                    addClause(c);
                }
            }
        }
    }
    // Globally forbidden substitutions
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
    end(STAGE_SUBSTITUTIONCONSTRAINTS);
}

void Encoding::encodeSubtaskRelationships(Position& newPos, Position& above) {

    // expansions
    begin(STAGE_EXPANSIONS);
    for (const auto& pair : newPos.getExpansions()) {
        const USignature& parent = pair.first;

        bool forbidden = false;
        for (const USignature& child : pair.second) {
            if (child == Position::NONE_SIG) {
                // Forbid parent op that turned out to be impossible
                int oldOpVar = getVariable(VarType::OP, above, parent);
                addClause(-oldOpVar);
                forbidden = true;
                break;
            }
        }
        if (forbidden) continue;

        appendClause(-getVariable(VarType::OP, above, parent));
        for (const USignature& child : pair.second) {
            if (child == Position::NONE_SIG) continue;
            appendClause(getVariable(VarType::OP, newPos, child));
        }
        endClause();
    }
    end(STAGE_EXPANSIONS);

    // predecessors
    if (_params.isNonzero("p")) {
        begin(STAGE_PREDECESSORS);
        for (const auto& pair : newPos.getPredecessors()) {
            const USignature& child = pair.first;
            if (child == Position::NONE_SIG) continue;

            appendClause(-getVariable(VarType::OP, newPos, child));
            for (const USignature& parent : pair.second) {
                appendClause(getVariable(VarType::OP, above, parent));
            }
            endClause();
        }
        end(STAGE_PREDECESSORS);
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

void Encoding::optimizePlan(int upperBound, Plan& plan) {

    int layerIdx = _layers.size()-1;
    Layer& l = *_layers.at(layerIdx);
    int currentPlanLength = upperBound;
    Log::v("PLO BEGIN %i\n", currentPlanLength);

    // Add counting mechanism
    begin(STAGE_PLANLENGTHCOUNTING);
    int minPlanLength = 0;
    int maxPlanLength = 0;
    std::vector<int> planLengthVars(1, VariableDomain::nextVar());
    Log::d("VARNAME %i (plan_length_equals %i %i)\n", planLengthVars[0], 0, 0);
    // At position zero, the plan length is always equal to zero
    addClause(planLengthVars[0]);
    for (size_t pos = 0; pos+1 < l.size(); pos++) {

        // Collect sets of potential operations
        USigSet emptyActions, actualActions;
        for (const auto& aSig : l.at(pos).getActions()) {
            Log::d("PLO %i %s?\n", pos, TOSTR(aSig));
            if (aSig == Position::NONE_SIG) continue;
            (isEmptyAction(aSig) ? emptyActions : actualActions).insert(aSig);
        }
        for (const auto& rSig : l.at(pos).getReductions()) {
            Log::d("PLO %i %s?\n", pos, TOSTR(rSig));
            if (rSig == Position::NONE_SIG) continue;
            if (_htn.getReduction(rSig).getSubtasks().size() == 0) {
                // Empty reduction
                emptyActions.insert(rSig);
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
                addClause(-planLengthVars.back());
                planLengthVars.resize(planLengthVars.size()-1);
            }
            Log::d("[no empty actions]\n");
        } else if (actualActions.empty()) {
            // Only empty actions here: Keep current bounds, keep all variables.
            Log::d("[only empty actions]\n");
        } else {
            // Mix of actual and empty actions here: Increment upper bound, 
            bool increaseUpperBound = maxPlanLength < currentPlanLength;
            if (increaseUpperBound) maxPlanLength++;
            // create new variables and constraints.
            int emptySpotVar = VariableDomain::nextVar();
            std::vector<int> newPlanLengthVars(planLengthVars.size()+(increaseUpperBound?1:0));
            for (size_t i = 0; i < newPlanLengthVars.size(); i++) {
                newPlanLengthVars[i] = VariableDomain::nextVar();
            }

            // Define for each action var whether it implies an empty spot or not
            for (const auto& aSig : emptyActions) {
                // IF the empty action occurs, THEN the spot is empty.
                addClause(-l.at(pos).getVariable(VarType::OP, aSig), emptySpotVar);
            }
            for (const auto& aSig : actualActions) {
                // IF the actual action occurs, THEN the spot is not empty.
                addClause(-l.at(pos).getVariable(VarType::OP, aSig), -emptySpotVar);
            }

            // Propagate plan length from previous position to new position
            for (size_t i = 0; i < planLengthVars.size(); i++) {
                int prevVar = planLengthVars[i];
                int keptPlanLengthVar = newPlanLengthVars[i];

                if (i+1 < newPlanLengthVars.size()) {
                    // IF previous plan length is X THEN
                    // (here is an empty spot IFF the plan length stays X) 
                    addClause(-prevVar, -emptySpotVar, keptPlanLengthVar);
                    addClause(-prevVar, emptySpotVar, -keptPlanLengthVar);

                    // IF previous plan length is X THEN
                    // (here is a non-empty spot IFF the plan length becomes X+1)
                    int incrPlanLengthVar = newPlanLengthVars[i+1];
                    addClause(-prevVar, emptySpotVar, incrPlanLengthVar);
                    addClause(-prevVar, -emptySpotVar, -incrPlanLengthVar);
                } else {
                    // Limit hit -- no more actions are admitted
                    // IF previous plan length is X THEN this spot must be empty
                    addClause(-prevVar, emptySpotVar);
                    // IF previous plan length is X THEN the plan length stays X
                    addClause(-prevVar, keptPlanLengthVar);
                }
            }
            planLengthVars = newPlanLengthVars;
        }

        Log::v("Position %i: Plan length bounds [%i,%i]\n", pos, minPlanLength, maxPlanLength);
    }

    Log::i("Tightened initial plan length bounds at layer %i: [0,%i] => [%i,%i]\n",
            layerIdx, l.size()-1, minPlanLength, maxPlanLength);
    assert(planLengthVars.size() == maxPlanLength-minPlanLength+1 || Log::e("%i != %i-%i+1\n", planLengthVars.size(), maxPlanLength, minPlanLength));
    
    // Add primitiveness of all positions at the final layer
    // as unit literals (instead of assumptions)
    addAssumptions(layerIdx, /*permanent=*/true);
    end(STAGE_PLANLENGTHCOUNTING);

    // Solving iterations
    while (true) {
        // Hit lower bound of possible plan lengths? 
        if (currentPlanLength == minPlanLength) {
            Log::v("PLO END %i\n", currentPlanLength);
            Log::i("Length of current plan is at lower bound (%i): finished\n", minPlanLength);
            break;
        }

        // Assume a shorter plan by one
        begin(STAGE_PLANLENGTHCOUNTING);

        // Permanently forbid any plan lengths greater than / equal to the last found plan
        while (maxPlanLength > currentPlanLength) {
            Log::d("GUARANTEE PL!=%i\n", maxPlanLength);
            int probedVar = planLengthVars[maxPlanLength-minPlanLength];
            addClause(-probedVar);
            maxPlanLength--;
        }
        assert(maxPlanLength == currentPlanLength);
        //assert(solve() != 20 || Log::e("Problem became unsolvable through plan counter!\n"));

        // Assume a plan length shorter than the last found plan
        Log::d("GUARANTEE PL!=%i\n", maxPlanLength);
        int probedVar = planLengthVars[maxPlanLength-minPlanLength];
        addClause(-probedVar);

        end(STAGE_PLANLENGTHCOUNTING);

        Log::i("Searching for a plan of length < %i\n", maxPlanLength);
        int result = solve();

        // Check result
        if (result == 10) {
            // SAT: Shorter plan found!
            plan = extractPlan();
            int newPlanLength = getPlanLength(std::get<0>(plan));
            assert(newPlanLength < currentPlanLength || Log::e("New found plan has length %i!\n", newPlanLength));
            Log::i("Shorter plan (length %i) found\n", newPlanLength);
            currentPlanLength = newPlanLength;
            Log::v("PLO UPDATE %i\n", currentPlanLength);
        } else if (result == 20) {
            // UNSAT
            Log::v("PLO END %i\n", currentPlanLength);
            Log::i("No plan of length < %i exists at this layer\n", currentPlanLength);
            break;
        } else {
            // UNKNOWN
            Log::v("PLO END %i\n", currentPlanLength);
            break;
        }
    }
}

bool Encoding::isEmptyAction(const USignature& aSig) {
    if (_htn.isSecondPartOfSplitAction(aSig) || _htn.getBlankActionSig() == aSig)
        return true;
    if (_htn.getActionNameOfVirtualizedChild(aSig._name_id) == _htn.getBlankActionSig()._name_id)
        return true;
    return false;
}

void Encoding::addAssumptions(int layerIdx, bool permanent) {
    Layer& l = *_layers.at(layerIdx);
    if (_implicit_primitiveness) {
        begin(STAGE_ASSUMPTIONS);
        for (size_t pos = 0; pos < l.size(); pos++) {
            appendClause(-encodeVarPrimitive(layerIdx, pos));
            for (int var : _primitive_ops) appendClause(var);
            endClause();
        }
        end(STAGE_ASSUMPTIONS);
    }
    for (size_t pos = 0; pos < l.size(); pos++) {

        int v = getVarPrimitiveOrZero(layerIdx, pos);
        if (v != 0) {
            if (permanent) addClause(v);
            else assume(v);
        }
    }
}

void Encoding::setVariablePhases(const std::vector<int>& vars) {
    // Choose one positive phase variable at random, set negative phase for all others
    std::default_random_engine generator;
    std::uniform_int_distribution<size_t> distribution(0, vars.size()-1);
    size_t randomIdx = distribution(generator);
    for (size_t i = 0; i < vars.size(); i++) {
        ipasir_set_phase(_solver, vars[i], i == randomIdx);
    }
}

void Encoding::setTerminateCallback(void * state, int (*terminate)(void * state)) {
    ipasir_set_terminate(_solver, state, terminate);
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
                _num_cls, _num_lits, _num_asmpts);
    
    ipasir_set_learn(_solver, this, /*maxLength=*/1, onClauseLearnt);

    //for (const int& v: _no_decision_variables) ipasir_set_decision_var(_solver, v, false);
    _no_decision_variables.clear();

    _sat_call_start_time = Timer::elapsedSeconds();
    int result = ipasir_solve(_solver);
    _sat_call_start_time = 0;

    if (_num_asmpts == 0) _last_assumptions.clear();
    _num_asmpts = 0;

    _termination_callback();

    return result;
}

void Encoding::addUnitConstraint(int lit) {
    addClause(lit);
}

float Encoding::getTimeSinceSatCallStart() {
    if (_sat_call_start_time == 0) return 0;
    return Timer::elapsedSeconds() - _sat_call_start_time;
}

bool Encoding::isEncodedSubstitution(const USignature& sig) {
    return _substitution_variables.count(sig);
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
        
        begin(STAGE_QCONSTEQUALITY);
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
                int v1 = varSubstitution(sigSubstitute(q1, c));
                int v2 = varSubstitution(sigSubstitute(q2, c));
                addClause(-varEq, v1, -v2);
                addClause(-varEq, -v1, v2);
            }
            // Any of the GOOD ones
            for (int c : good) addClause(-varSubstitution(sigSubstitute(q1, c)), -varSubstitution(sigSubstitute(q2, c)), varEq);
            // None of the BAD ones
            for (int c : bad1) addClause(-varSubstitution(sigSubstitute(q1, c)), -varEq);
            for (int c : bad2) addClause(-varSubstitution(sigSubstitute(q2, c)), -varEq);
        }
        end(STAGE_QCONSTEQUALITY);

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

int Encoding::encodeVarPrimitive(int layer, int pos) {
    return encodeVariable(VarType::OP, _layers.at(layer)->at(pos), _sig_primitive, false);
}
int Encoding::getVarPrimitiveOrZero(int layer, int pos) {
    return _layers.at(layer)->at(pos).getVariableOrZero(VarType::OP, _sig_primitive);
}

void Encoding::printFailedVars(Layer& layer) {
    Log::d("FAILED ");
    for (size_t pos = 0; pos < layer.size(); pos++) {
        int v = getVarPrimitiveOrZero(layer.index(), pos);
        if (v == 0) continue;
        if (ipasir_failed(_solver, v)) Log::d("%i ", v);
    }
    Log::d("\n");
}

std::vector<PlanItem> Encoding::extractClassicalPlan() {

    Layer& finalLayer = *_layers.back();
    int li = finalLayer.index();
    //VariableDomain::lock();

    std::vector<PlanItem> plan(finalLayer.size());
    //log("(actions at layer %i)\n", li);
    for (size_t pos = 0; pos < finalLayer.size(); pos++) {
        //log("%i\n", pos);

        int chosenActions = 0;
        //State newState = state;
        for (const auto& [sig, aVar] : finalLayer[pos].getVariableTable(VarType::OP)) {
            if (ipasir_val(_solver, aVar) <= 0) continue;

            USignature aSig = sig;
            if (!_htn.isAction(aSig)) continue;

            if (_htn.isVirtualizedChildOfAction(aSig._name_id)) {
                aSig._name_id = _htn.getActionNameOfVirtualizedChild(sig._name_id);
            }

            //log("  %s ?\n", TOSTR(aSig));

            chosenActions++;

            // Decode q constants
            USignature aDec = getDecodedQOp(li, pos, aSig);
            if (aDec == Position::NONE_SIG) continue;

            if (aDec != aSig) {

                const Action& a = _htn.getAction(aSig);
                HtnOp opDecoded = a.substitute(Substitution(a.getArguments(), aDec._args));
                Action aDecoded = (Action) opDecoded;
            }

            //Log::d("* %s @ %i\n", TOSTR(aDec), pos);
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

                if (ipasir_val(_solver, v) > 0) {

                    if (_htn.isAction(opSig)) {
                        // Action
                        actionsThisPos++;
                        const USignature& aSig = opSig;

                        if (aSig == _htn.getBlankActionSig()) continue;
                        if (_htn.isVirtualizedChildOfAction(aSig._name_id)) {
                            continue;
                        }
                        
                        int v = getVariable(VarType::OP, layerIdx, pos, aSig);
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
                        v = classicalPlan[aPos].id; // *_layers.at(l-1)[aPos].getVariable(aSig);
                        assert(v > 0 || Log::e("%s : v=%i\n", TOSTR(aSig), v));

                        //itemsNewLayer[pos] = PlanItem({v, aSig, aSig, std::vector<int>()});
                        if (layerIdx > 0) itemsOldLayer[predPos].subtaskIds.push_back(v);

                    } else if (_htn.isReduction(opSig)) {
                        // Reduction
                        const USignature& rSig = opSig;
                        const Reduction& r = _htn.getReduction(rSig);

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

bool Encoding::value(VarType type, int layer, int pos, const USignature& sig) {
    int v = getVariable(type, layer, pos, sig);
    Log::d("VAL %s@(%i,%i)=%i %i\n", TOSTR(sig), layer, pos, v, ipasir_val(_solver, v));
    return (ipasir_val(_solver, v) > 0);
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
        Log::d("%i ", ipasir_val(_solver, v));
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
                const USignature& sigSubst = sigSubstitute(arg, argSubst);
                if (isEncodedSubstitution(sigSubst) && ipasir_val(_solver, varSubstitution(sigSubst)) > 0) {
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

            if (numSubstitutions == 0) {
                Log::v("No substitutions for arg %s of %s\n", TOSTR(arg), TOSTR(origSig));
                return Position::NONE_SIG;
            }
            assert(numSubstitutions == 1 || Log::e("%i substitutions for arg %s of %s\n", numSubstitutions, TOSTR(arg), TOSTR(origSig)));
        }

        if (!containsQConstants) break; // done
    }

    //if (origSig != sig) Log::d("%s ~~> %s\n", TOSTR(origSig), TOSTR(sig));
    
    return sig;
}

void Encoding::begin(int stage) {
    if (!_current_stages.empty()) {
        int oldStage = _current_stages.back();
        _num_cls_per_stage[oldStage] += _num_cls - _num_cls_at_stage_start;
    }
    _num_cls_at_stage_start = _num_cls;
    _current_stages.push_back(stage);
}

void Encoding::end(int stage) {
    assert(!_current_stages.empty() && _current_stages.back() == stage);
    _current_stages.pop_back();
    _num_cls_per_stage[stage] += _num_cls - _num_cls_at_stage_start;
    _num_cls_at_stage_start = _num_cls;
}

void Encoding::printStages() {
    Log::i("Total amount of clauses encoded: %i\n", _num_cls);
    std::map<int, int, std::greater<int>> stagesSorted;
    for (const auto& [stage, num] : _num_cls_per_stage) if (num > 0) {
        stagesSorted[num] = stage;
    }
    for (const auto& [num, stage] : stagesSorted) {
        Log::i("- %s : %i cls\n", STAGES_NAMES[stage], num);
    }
    _num_cls_per_stage.clear();
}

Encoding::~Encoding() {

    if (!_num_cls_per_stage.empty()) printStages();

    if (_params.isNonzero("wf")) {

        // Append assumptions to written formula, close stream
        if (!_params.isNonzero("cs") && _last_assumptions.empty()) {
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