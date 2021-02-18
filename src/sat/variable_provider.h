
#ifndef DOMPASCH_LILOTANE_VARIABLE_PROVIDER_H
#define DOMPASCH_LILOTANE_VARIABLE_PROVIDER_H

#include "data/htn_instance.h"
#include "data/layer.h"
#include "util/params.h"

class VariableProvider {

private:
    HtnInstance& _htn;
    std::vector<Layer*>& _layers;
    
    NodeHashMap<USignature, int, USignatureHasher> _substitution_variables;
    USignature _sig_primitive;
    USignature _sig_substitution;
    int _substitute_name_id;
    FlatHashMap<std::pair<int, int>, int, IntPairHasher> _q_equality_variables;
    
public:

    VariableProvider(Parameters& params, HtnInstance& htn, std::vector<Layer*>& layers) : _htn(htn), _layers(layers) {
        _sig_primitive = USignature(_htn.nameId("__PRIMITIVE___"), std::vector<int>());
        _substitute_name_id = _htn.nameId("__SUBSTITUTE___");
        _sig_substitution = USignature(_substitute_name_id, std::vector<int>(2));
        VariableDomain::init(params);
    }

    inline bool isEncoded(VarType type, int layer, int pos, const USignature& sig) {
        return _layers.at(layer)->at(pos).hasVariable(type, sig);
    }

    inline int getVariable(VarType type, int layer, int pos, const USignature& sig) {
        return getVariable(type, _layers[layer]->at(pos), sig);
    }

    inline int getVariable(VarType type, const Position& pos, const USignature& sig) {
        return pos.getVariable(type, sig);
    }

    inline int encodeVariable(VarType type, Position& pos, const USignature& sig) {
        int var = pos.getVariableOrZero(type, sig);
        if (var == 0) var = pos.encode(type, sig);
        return var;
    }

    bool isEncodedSubstitution(const USignature& sig) {
        return _substitution_variables.count(sig);
    }

    const USignature& sigSubstitute(int qConstId, int trueConstId) {
        //assert(!_htn.isQConstant(trueConstId) || trueConstId < qConstId);
        auto& args = _sig_substitution._args;
        assert(_htn.isQConstant(qConstId));
        assert(!_htn.isQConstant(trueConstId));

        args[0] = qConstId;
        args[1] = trueConstId;
        return _sig_substitution;
    }

    int varSubstitution(int qConstId, int trueConstId) {
        const USignature& sigSubst = sigSubstitute(qConstId, trueConstId);
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

    int encodeVarPrimitive(int layer, int pos) {
        return encodeVariable(VarType::OP, _layers.at(layer)->at(pos), _sig_primitive);
    }
    int getVarPrimitiveOrZero(int layer, int pos) {
        return _layers.at(layer)->at(pos).getVariableOrZero(VarType::OP, _sig_primitive);
    }

    bool isQConstantEqualityEncoded(int qconst1, int qconst2) {
        return _q_equality_variables.count(IntPair(qconst1, qconst2));
    }
    int encodeQConstantEqualityVar(int qconst1, int qconst2) {
        int var = VariableDomain::nextVar();
        _q_equality_variables[IntPair(qconst1, qconst2)] = var;
        return var;
    }
    int getQConstantEqualityVar(int qconst1, int qconst2) {
        return _q_equality_variables[IntPair(qconst1, qconst2)];
    }

    void skipVariable() {
        VariableDomain::nextVar();
    }
    int getNumVariables() {
        return VariableDomain::getMaxVar();
    }
};

#endif