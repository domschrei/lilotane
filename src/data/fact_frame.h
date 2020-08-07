
#ifndef DOMPASCH_LILOTANE_FACT_FRAME_H
#define DOMPASCH_LILOTANE_FACT_FRAME_H

#include <vector>

#include "signature.h"

struct FactFrame {

    USignature sig;
    SigSet preconditions;
    SigSet flatEffects;
    NodeHashMap<std::vector<Signature>, SigSet, SigVecHasher> causalEffects;

    FactFrame substitute(const Substitution& s) const {
        FactFrame f;
        f.sig = sig.substitute(s);
        for (const auto& pre : preconditions) f.preconditions.insert(pre.substitute(s));
        for (const auto& [pres, effs] : causalEffects) {
            std::vector<Signature> fPres;
            SigSet fEffs;
            for (const auto& pre : pres) fPres.push_back(pre.substitute(s));
            for (const auto& eff : effs) {
                fEffs.insert(eff.substitute(s));
            }
            f.causalEffects[fPres] = fEffs;
        }
        for (const auto& eff : flatEffects) {
            f.flatEffects.insert(eff.substitute(s));
        }
        return f;
    }
};

#endif