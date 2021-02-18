
#ifndef DOMPASCH_LILOTANE_ENCODING_STAGES_H
#define DOMPASCH_LILOTANE_ENCODING_STAGES_H

#include <vector>
#include <map>
#include <assert.h>

#include "util/log.h"

const int STAGE_ACTIONCONSTRAINTS = 0;
const int STAGE_ACTIONEFFECTS = 1;
const int STAGE_ATLEASTONEELEMENT = 2;
const int STAGE_ATMOSTONEELEMENT = 3;
const int STAGE_AXIOMATICOPS = 4;
const int STAGE_DIRECTFRAMEAXIOMS = 5;
const int STAGE_EXPANSIONS = 6;
const int STAGE_FACTPROPAGATION = 7;
const int STAGE_FACTVARENCODING = 8;
const int STAGE_FORBIDDENOPERATIONS = 9;
const int STAGE_INDIRECTFRAMEAXIOMS = 10;
const int STAGE_INITSUBSTITUTIONS = 11;
const int STAGE_PREDECESSORS = 12;
const int STAGE_QCONSTEQUALITY = 13;
const int STAGE_QFACTSEMANTICS = 14;
const int STAGE_QTYPECONSTRAINTS = 15;
const int STAGE_REDUCTIONCONSTRAINTS = 16;
const int STAGE_SUBSTITUTIONCONSTRAINTS = 17;
const int STAGE_TRUEFACTS = 18;
const int STAGE_ASSUMPTIONS = 19;
const int STAGE_PLANLENGTHCOUNTING = 20;

class EncodingStatistics {

public:
    int _num_cls = 0;
    int _num_lits = 0;
    int _num_asmpts = 0;
    int _prev_num_cls = 0;
    int _prev_num_lits = 0;

private:
    const char* STAGES_NAMES[21] = {"actionconstraints","actioneffects","atleastoneelement","atmostoneelement",
        "axiomaticops","directframeaxioms","expansions","factpropagation","factvarencoding","forbiddenoperations",
        "indirectframeaxioms", "initsubstitutions","predecessors","qconstequality","qfactsemantics",
        "qtypeconstraints","reductionconstraints","substitutionconstraints","truefacts","assumptions","planlengthcounting"};
    std::vector<int> _num_cls_per_stage;
    std::vector<int> _current_stages;
    int _num_cls_at_stage_start = 0;

public:
    EncodingStatistics() {
        _num_cls_per_stage.resize(sizeof(STAGES_NAMES)/sizeof(*STAGES_NAMES));
    }

    void beginPosition() {
        _prev_num_cls = _num_cls;
        _prev_num_lits = _num_lits;
    }

    void endPosition() {
        assert(_current_stages.empty());
        Log::v("  Encoded %i cls, %i lits\n", _num_cls-_prev_num_cls, _num_lits-_prev_num_lits);
    }

    void begin(int stage) {
        if (!_current_stages.empty()) {
            int oldStage = _current_stages.back();
            _num_cls_per_stage[oldStage] += _num_cls - _num_cls_at_stage_start;
        }
        _num_cls_at_stage_start = _num_cls;
        _current_stages.push_back(stage);
    }

    void end(int stage) {
        assert(!_current_stages.empty() && _current_stages.back() == stage);
        _current_stages.pop_back();
        _num_cls_per_stage[stage] += _num_cls - _num_cls_at_stage_start;
        _num_cls_at_stage_start = _num_cls;
    }

    void printStages() {
        Log::i("Total amount of clauses encoded: %i\n", _num_cls);
        std::map<int, int, std::greater<int>> stagesSorted;
        for (size_t stage = 0; stage < _num_cls_per_stage.size(); stage++) {
            if (_num_cls_per_stage[stage] > 0)
                stagesSorted[_num_cls_per_stage[stage]] = stage;
        }
        for (const auto& [num, stage] : stagesSorted) {
            Log::i("- %s : %i cls\n", STAGES_NAMES[stage], num);
        }
        _num_cls_per_stage.clear();
    }

    ~EncodingStatistics() {
        printStages();
    }
};

#endif
