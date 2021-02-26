
#ifndef DOMPASCH_TREE_REXX_ENCODING_H
#define DOMPASCH_TREE_REXX_ENCODING_H

#include "util/params.h"
#include "data/layer.h"
#include "data/signature.h"
#include "data/htn_instance.h"
#include "data/action.h"
#include "sat/literal_tree.h"
#include "sat/sat_interface.h"
#include "algo/fact_analysis.h"
#include "sat/variable_provider.h"
#include "sat/decoder.h"

typedef NodeHashMap<int, SigSet> State;

class Encoding {

private:
    Parameters& _params;
    HtnInstance& _htn;
    FactAnalysis& _analysis;
    std::vector<Layer*>& _layers;
    EncodingStatistics _stats;
    SatInterface _sat;
    VariableProvider _vars;
    Decoder _decoder;

    std::function<void()> _termination_callback;
    
    size_t _layer_idx;
    size_t _pos;
    size_t _old_pos;
    size_t _offset;

    NodeHashSet<Substitution, Substitution::Hasher> _forbidden_substitutions;
    FlatHashSet<int> _new_fact_vars;

    FlatHashSet<int> _q_constants;
    FlatHashSet<int> _new_q_constants;

    std::vector<int> _primitive_ops;
    std::vector<int> _nonprimitive_ops;

    const bool _use_q_constant_mutexes;
    const bool _implicit_primitiveness;

    float _sat_call_start_time;

public:
    Encoding(Parameters& params, HtnInstance& htn, FactAnalysis& analysis, std::vector<Layer*>& layers, std::function<void()> terminationCallback) : 
            _params(params), _htn(htn), _analysis(analysis), _layers(layers),
            _sat(params, _stats), _vars(_params, _htn, _layers),
            _decoder(_htn, _layers, _sat, _vars),
            _termination_callback(terminationCallback),
            _use_q_constant_mutexes(_params.getIntParam("qcm") > 0), 
            _implicit_primitiveness(params.isNonzero("ip")) {}

    void encode(size_t layerIdx, size_t pos);
    void addAssumptions(int layerIdx, bool permanent = false);
    void addUnitConstraint(int lit);
    
    void setTerminateCallback(void * state, int (*terminate)(void * state));
    int solve();
    float getTimeSinceSatCallStart();    

    void printFailedVars(Layer& layer);
    void printSatisfyingAssignment();

    Plan extractPlan() {
        return _decoder.extractPlan();
    }
    void printStatistics() {
        _stats.printStages();
    }
    SatInterface& getSatInterface() {return _sat;}
    EncodingStatistics& getEncodingStatistics() {return _stats;}

    ~Encoding() {
        // Append assumptions to written formula, close stream
        if (!_params.isNonzero("cs") && !_sat.hasLastAssumptions()) {
            addAssumptions(_layers.size()-1);
        }
    }

private:
    void encodeOperationVariables(Position& pos);
    void encodeFactVariables(Position& pos, Position& left, Position& above);
    void encodeFrameAxioms(Position& pos, Position& left);
    void encodeIndirectFrameAxioms(const std::vector<int>& headerLits, int opVar, const IntPairTree& tree);
    void encodeOperationConstraints(Position& pos);
    void encodeSubstitutionVars(const USignature& opSig, int opVar, int qconst);
    void encodeQFactSemantics(Position& pos);
    void encodeActionEffects(Position& pos, Position& left);
    void encodeQConstraints(Position& pos);
    void encodeSubtaskRelationships(Position& pos, Position& above);
    int encodeQConstEquality(int q1, int q2);
};

#endif