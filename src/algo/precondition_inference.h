
#ifndef DOMPASCH_LILOTANE_PRECONDITION_INFERENCE_H
#define DOMPASCH_LILOTANE_PRECONDITION_INFERENCE_H

#include "data/htn_instance.h"
#include "algo/fact_analysis.h"

class PreconditionInference {

public:
    enum MinePrecMode { NO_MINING, USE_FOR_INSTANTIATION, USE_EVERYWHERE };
    static void infer(HtnInstance& htn, FactAnalysis& analysis, MinePrecMode mode) {
        if (mode == NO_MINING) return;

        int precondsBefore = 0;
        int minedPreconds = 0;
        int initRedId = htn.getInitReduction().getSignature()._name_id;
        for (auto& [rId, r] : htn.getReductionTemplates()) {
            if (initRedId == rId) continue; // FIXME Skip init reduction

            precondsBefore += r.getPreconditions().size();
            // Mine additional preconditions, if possible
            for (auto& pre : analysis.inferPreconditions(r.getSignature())) {
                if (r.getPreconditions().count(pre)) continue;
                    
                bool hasFreeArgs = false;
                for (int arg : pre._usig._args) {
                    hasFreeArgs |= arg == htn.nameId("??_");
                    //if (!hasFreeArgs) assert(std::find(r.getSignature()._args.begin(), r.getSignature()._args.end(), arg) != r.getSignature()._args.end());
                }
                if (hasFreeArgs) continue;

                Log::d("%s : MINED_PRE %s\n", TOSTR(r.getSignature()), TOSTR(pre));
                if (mode == USE_FOR_INSTANTIATION) {
                    r.addExtraPrecondition(std::move(pre));
                }
                if (mode == USE_EVERYWHERE) {
                    r.addPrecondition(std::move(pre));
                }
                minedPreconds++;
            }
        }

        float newRatio = precondsBefore == 0 ? std::numeric_limits<float>::infinity() : 
                100.f * (((float)precondsBefore+minedPreconds) / precondsBefore - 1);

        Log::i("Mined %i new reduction preconditions (+%.1f%%).\n", minedPreconds, newRatio);
    }

};

#endif