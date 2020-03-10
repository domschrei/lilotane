
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

#define PRINT_TO_FILE true

typedef std::unordered_map<int, SigSet> State;

struct PlanItem {
    PlanItem() {
        id = -1;
        abstractTask = Position::NONE_SIG;
        reduction = Position::NONE_SIG;
        subtaskIds = std::vector<int>(0);
    }
    PlanItem(int id, const Signature& abstractTask, const Signature& reduction, const std::vector<int> subtaskIds) :
        id(id), abstractTask(abstractTask), reduction(reduction), subtaskIds(subtaskIds) {}
    int id = -1;
    Signature abstractTask;
    Signature reduction;
    std::vector<int> subtaskIds;
};

class Encoding {

private:
    Parameters& _params;
    HtnInstance& _htn;
    std::vector<Layer>* _layers;
    
    std::unordered_map<Signature, int, SignatureHasher> _substitution_variables;

    void* _solver;
    std::ofstream _out;

    Signature _sig_primitive;
    int _substitute_name_id;

    std::unordered_set<int> _q_constants;

    std::vector<int> _last_assumptions;

    const bool _print_formula;

    int _num_cls;
    int _num_lits;
    int _num_asmpts;

    int _prior_num_cls;
    int _prior_num_lits;

    std::map<std::string, int> _num_cls_per_stage;
    std::map<std::string, int> _total_num_cls_per_stage;

public:
    Encoding(Parameters& params, HtnInstance& htn, std::vector<Layer>& layers);
    ~Encoding();

    void encode(int layerIdx, int pos);
    void encodeInitialTaskNetwork();
    bool solve();

    void stage(std::string name);
    void printStages();

    std::pair<std::vector<PlanItem>, std::vector<PlanItem>> extractPlan();
    std::vector<PlanItem> extractClassicalPlan();
    std::vector<PlanItem> extractDecompositionPlan();

    void printFailedVars(Layer& layer);
    void printSatisfyingAssignment();

private:

    void initSubstitutionVars(int qconst, Position& pos);

    Signature sigSubstitute(int qConstId, int trueConstId) {
        assert(!_htn._q_constants.count(trueConstId) || trueConstId < qConstId);
        std::vector<int> args(2);
        args[0] = (qConstId);
        args[1] = (trueConstId);
        return Signature(_substitute_name_id, args);
    }

    std::set<std::set<int>> getCnf(const std::vector<std::vector<int>>& dnf);

    void addClause(const std::initializer_list<int>& lits);
    void appendClause(const std::initializer_list<int>& lits);
    void endClause();
    void assume(int lit);

    int varPrimitive(int layer, int pos);
    int varSubstitution(const Signature& sigSubst);
    bool isEncoded(int layer, int pos, const Signature& sig);
    bool isEncodedSubstitution(const Signature& sig);

    std::string varName(int layer, int pos, const Signature& sig);
    void printVar(int layer, int pos, const Signature& sig);

    bool value(int layer, int pos, const Signature& sig);
    Signature getDecodedQOp(int layer, int pos, const Signature& sig);
    void checkAndApply(const Action& a, State& state, State& newState, int layer, int pos);

};

#endif