
#ifndef DOMPASCH_LILOTANE_DECODER_H
#define DOMPASCH_LILOTANE_DECODER_H

#include "data/htn_instance.h"
#include "data/layer.h"
#include "sat/sat_interface.h"
#include "sat/variable_provider.h"
#include "data/plan.h"

class Decoder {

private:
    HtnInstance& _htn;
    std::vector<Layer*>& _layers;
    SatInterface& _sat;
    VariableProvider& _vars;

public:
    Decoder(HtnInstance& htn, std::vector<Layer*>& layers, SatInterface& sat, VariableProvider& vars) :
        _htn(htn), _layers(layers), _sat(sat), _vars(vars) {}

    enum PlanExtraction {ALL, PRIMITIVE_ONLY};
    std::vector<PlanItem> extractClassicalPlan(PlanExtraction mode = PRIMITIVE_ONLY) {

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

    Plan extractPlan() {

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
                            Action a = _htn.getOpTable().getAction(aSig);

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
                            const Reduction& r = _htn.getOpTable().getReduction(rSig);

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

    bool value(VarType type, int layer, int pos, const USignature& sig) {
        int v = _vars.getVariable(type, layer, pos, sig);
        Log::d("VAL %s@(%i,%i)=%i %i\n", TOSTR(sig), layer, pos, v, _sat.holds(v));
        return _sat.holds(v);
    }


    USignature getDecodedQOp(int layer, int pos, const USignature& origSig) {
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
};

#endif
