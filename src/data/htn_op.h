
#ifndef DOMPASCH_TREE_REXX_HTN_OP_H
#define DOMPASCH_TREE_REXX_HTN_OP_H

#include <vector>

#include "signature.h"


class HtnOp {

protected:
    int _id;
    std::vector<int> _args;

    SigSet _preconditions;
    SigSet _effects;

public:
    HtnOp();
    HtnOp(int id, const std::vector<int>& args);
    HtnOp(const HtnOp& op);
    HtnOp(HtnOp&& op);

    void setPreconditions(const SigSet& set);
    void addPrecondition(const Signature& sig);
    void addPrecondition(Signature&& sig);
    void addEffect(const Signature& sig);
    void addEffect(Signature&& sig);
    void addArgument(int arg);
    void removeInconsistentEffects();

    virtual HtnOp substitute(const Substitution& s) const;

    const SigSet& getPreconditions() const;
    const SigSet& getEffects() const;
    const std::vector<int>& getArguments() const;
    USignature getSignature() const;

    HtnOp& operator=(const HtnOp& op);
};


#endif