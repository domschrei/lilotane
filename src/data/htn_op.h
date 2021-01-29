
#ifndef DOMPASCH_TREE_REXX_HTN_OP_H
#define DOMPASCH_TREE_REXX_HTN_OP_H

#include <vector>

#include "signature.h"


class HtnOp {

protected:
    int _id;
    std::vector<int> _args;

    SigSet _preconditions;
    
    // Preconditions which are not part of the problem description.
    // They are implied by the logic and thus are not necessary to be checked,
    // but they *can* be checked, helping to prune operations.
    SigSet _extra_preconditions;

    SigSet _effects;

public:
    HtnOp();
    HtnOp(int id, const std::vector<int>& args);
    HtnOp(int id, std::vector<int>&& args);
    HtnOp(const HtnOp& op);
    HtnOp(HtnOp&& op);

    void setPreconditions(const SigSet& set);
    void setExtraPreconditions(const SigSet& set);
    void setEffects(const SigSet& set);
    void addPrecondition(const Signature& sig);
    void addPrecondition(Signature&& sig);
    void addExtraPrecondition(const Signature& sig);
    void addExtraPrecondition(Signature&& sig);
    void addEffect(const Signature& sig);
    void addEffect(Signature&& sig);
    void addArgument(int arg);
    void removeInconsistentEffects();

    virtual HtnOp substitute(const Substitution& s) const;

    const SigSet& getPreconditions() const;
    const SigSet& getExtraPreconditions() const;
    const SigSet& getEffects() const;
    const std::vector<int>& getArguments() const;
    USignature getSignature() const;
    int getNameId() const;

    HtnOp& operator=(const HtnOp& op);
};


#endif