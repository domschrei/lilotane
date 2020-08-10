
#ifndef DOMPASCH_TREE_REXX_ENCODING_H
#define DOMPASCH_TREE_REXX_ENCODING_H

extern "C" {
    #include "sat/ipasir.h"
}

#include <initializer_list>
#include <fstream>
#include <string>
#include <iostream>

#include "util/params.h"
#include "data/layer.h"
#include "data/signature.h"
#include "data/htn_instance.h"
#include "data/action.h"
#include "sat/variable_domain.h"

typedef NodeHashMap<int, SigSet> State;

struct PlanItem {
    PlanItem() {
        id = -1;
        abstractTask = Position::NONE_SIG;
        reduction = Position::NONE_SIG;
        subtaskIds = std::vector<int>(0);
    }
    PlanItem(int id, const USignature& abstractTask, const USignature& reduction, const std::vector<int> subtaskIds) :
        id(id), abstractTask(abstractTask), reduction(reduction), subtaskIds(subtaskIds) {}
    int id = -1;
    USignature abstractTask;
    USignature reduction;
    std::vector<int> subtaskIds;
};

class Encoding {

private:
    Parameters& _params;
    HtnInstance& _htn;
    std::vector<Layer*>& _layers;
    
    size_t _layer_idx;
    size_t _pos;
    size_t _old_pos;
    size_t _offset;

    NodeHashMap<USignature, int, USignatureHasher> _substitution_variables;
    NodeHashSet<Substitution, Substitution::Hasher> _forbidden_substitutions;
    FlatHashSet<int> _new_fact_vars;

    void* _solver;
    std::ofstream _out;

    USignature _sig_primitive;
    USignature _sig_substitution;
    int _substitute_name_id;

    FlatHashSet<int> _q_constants;
    FlatHashMap<std::pair<int, int>, int, IntPairHasher> _q_equality_variables;
    std::vector<int> _primitive_ops;
    std::vector<int> _nonprimitive_ops;
    bool _nonprimitive_only_at_prior_pos;

    std::vector<int> _last_assumptions;
    std::vector<int> _no_decision_variables;

    const bool _print_formula;
    const bool _use_q_constant_mutexes;
    const bool _implicit_primitiveness;

    int _num_cls;
    int _num_lits;
    int _num_asmpts;

    int STAGE_ACTIONCONSTRAINTS = 0;
    int STAGE_ACTIONEFFECTS = 1;
    int STAGE_ATLEASTONEELEMENT = 2;
    int STAGE_ATMOSTONEELEMENT = 3;
    int STAGE_AXIOMATICOPS = 4;
    int STAGE_EXPANSIONS = 5;
    int STAGE_FACTPROPAGATION = 6;
    int STAGE_FACTVARENCODING = 7;
    int STAGE_FORBIDDENPARENTS = 8;
    int STAGE_FRAMEAXIOMS = 9;
    int STAGE_INITSUBSTITUTIONS = 10;
    int STAGE_PREDECESSORS = 11;
    int STAGE_QCONSTEQUALITY = 12;
    int STAGE_QFACTSEMANTICS = 13;
    int STAGE_QTYPECONSTRAINTS = 14;
    int STAGE_REDUCTIONCONSTRAINTS = 15;
    int STAGE_SUBSTITUTIONCONSTRAINTS = 16;
    int STAGE_TRUEFACTS = 17;
    int STAGE_ASSUMPTIONS = 18;
    const char* STAGES_NAMES[19] = {"actionconstraints","actioneffects","atleastoneelement","atmostoneelement",
        "axiomaticops","expansions","factpropagation","factvarencoding","forbiddenparents","frameaxioms","initsubstitutions",
        "predecessors","qconstequality","qfactsemantics","qtypeconstraints","reductionconstraints",
        "substitutionconstraints","truefacts","assumptions"};
    std::map<int, int> _num_cls_per_stage;
    std::vector<int> _current_stages;
    int _num_cls_at_stage_start = 0; 

    float _sat_call_start_time;
    bool _began_line = false;

public:
    Encoding(Parameters& params, HtnInstance& htn, std::vector<Layer*>& layers);
    ~Encoding();

    void encode(size_t layerIdx, size_t pos);
    void addAssumptions(int layerIdx);
    void setTerminateCallback(void * state, int (*terminate)(void * state));
    int solve();

    float getTimeSinceSatCallStart();

    void begin(int stage);
    void end(int stage);
    void printStages();

    std::pair<std::vector<PlanItem>, std::vector<PlanItem>> extractPlan();
    std::vector<PlanItem> extractClassicalPlan();
    std::vector<PlanItem> extractDecompositionPlan();

    void printFailedVars(Layer& layer);
    void printSatisfyingAssignment();

private:

    void encodeOperationVariables(Position& pos);
    void encodeFactVariables(Position& pos, Position& left, Position& above);
    void encodeFrameAxioms(Position& pos, Position& left);
    void encodeOperationConstraints(Position& pos);
    void encodeSubstitutionVars(int opVar, int qconst, Position& pos);
    void encodeQFactSemantics(Position& pos);
    void encodeActionEffects(Position& pos, Position& left);
    void encodeQConstraints(Position& pos);
    void encodeSubtaskRelationships(Position& pos, Position& above);

    void setVariablePhases(const std::vector<int>& vars);
    void clearDonePositions();
    
    std::set<std::set<int>> getCnf(const std::vector<int>& dnf);

    int encodeVarPrimitive(int layer, int pos);
    int getVarPrimitiveOrZero(int layer, int pos);
    int varSubstitution(const USignature& sigSubst);
    int varQConstEquality(int q1, int q2);
    const USignature& sigSubstitute(int qConstId, int trueConstId);
    bool isEncodedSubstitution(const USignature& sig);

    bool value(VarType type, int layer, int pos, const USignature& sig);
    
    std::string varName(int layer, int pos, const USignature& sig);
    void printVar(int layer, int pos, const USignature& sig);

    USignature getDecodedQOp(int layer, int pos, const USignature& sig);
    



    inline bool isEncoded(VarType type, int layer, int pos, const USignature& sig) {
        return _layers.at(layer)->at(pos).hasVariable(type, sig);
    }

    inline int getVariable(VarType type, int layer, int pos, const USignature& sig) {
        return getVariable(type, _layers[layer]->at(pos), sig);
    }

    inline int getVariable(VarType type, const Position& pos, const USignature& sig) {
        return pos.getVariable(type, sig);
    }

    inline int encodeVariable(VarType type, Position& pos, const USignature& sig, bool decisionVar) {
        int var = pos.getVariableOrZero(type, sig);
        if (var == 0) {
            var = pos.encode(type, sig);
            if (!decisionVar) _no_decision_variables.push_back(var);
        }
        return var;
    }


    inline void addClause(int lit) {
        assert(lit != 0);
        ipasir_add(_solver, lit); ipasir_add(_solver, 0);
        if (_print_formula) _out << lit << " 0\n";
        _num_lits++; _num_cls++;
    }

    inline void addClause(int lit1, int lit2) {
        assert(lit1 != 0);
        assert(lit2 != 0);
        ipasir_add(_solver, lit1); ipasir_add(_solver, lit2); ipasir_add(_solver, 0);
        if (_print_formula) _out << lit1 << " " << lit2 << " 0\n";
        _num_lits += 2; _num_cls++;
    }

    inline void addClause(int lit1, int lit2, int lit3) {
        assert(lit1 != 0);
        assert(lit2 != 0);
        assert(lit3 != 0);
        ipasir_add(_solver, lit1); ipasir_add(_solver, lit2); ipasir_add(_solver, lit3); ipasir_add(_solver, 0);
        if (_print_formula) _out << lit1 << " " << lit2 << " " << lit3 << " 0\n";
        _num_lits += 3; _num_cls++;
    }

    inline void addClause(const std::initializer_list<int>& lits) {
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

    inline void addClause(const std::vector<int>& cls) {
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

    inline void addZeroSepClauses(const std::vector<int>& cls) {
        for (int lit : cls) {
            ipasir_add(_solver, lit);
            if (lit == 0) _num_cls++;
            else _num_lits++;
            if (_print_formula) {
                if (lit == 0) _out << "0\n";
                else _out << lit << " ";
            }
        }
    }

    inline void appendClause(int lit) {
        _began_line = true;

        assert(lit != 0);
        ipasir_add(_solver, lit);
        if (_print_formula) _out << lit << " ";
        _num_lits++;
    }

    inline void appendClause(int lit1, int lit2) {
        _began_line = true;
        
        assert(lit1 != 0);
        assert(lit2 != 0);
        ipasir_add(_solver, lit1); ipasir_add(_solver, lit2);
        if (_print_formula) _out << lit1 << " " << lit2 << " ";
        _num_lits += 2;
    }

    inline void appendClause(const std::initializer_list<int>& lits) {
        _began_line = true;

        for (int lit : lits) {
            assert(lit != 0);
            ipasir_add(_solver, lit);
            if (_print_formula) _out << lit << " ";
            //log("%i ", lit);
        } 

        _num_lits += lits.size();
    }

    inline void endClause() {
        assert(_began_line);
        ipasir_add(_solver, 0);
        if (_print_formula) _out << "0\n";
        //log("0\n");
        _began_line = false;

        _num_cls++;
    }

    inline void assume(int lit) {
        if (_num_asmpts == 0) _last_assumptions.clear();
        ipasir_assume(_solver, lit);
        //log("CNF !%i\n", lit);
        _last_assumptions.push_back(lit);
        _num_asmpts++;
    }
};

#endif