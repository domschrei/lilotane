
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
    HtnOp() {}
    HtnOp(int id, std::vector<int> args) : _id(id), _args(args) {}
    HtnOp(HtnOp& op) : _id(op._id), _args(op._args) {}

    void addPrecondition(Signature& sig) {
        _preconditions.insert(sig);
    }
    void addEffect(Signature& sig) {
        _effects.insert(sig);
    }

    virtual HtnOp substitute(std::unordered_map<int, int> s) {
        HtnOp op;
        op._id = _id;
        op._args.resize(_args.size());
        for (int i = 0; i < _args.size(); i++) {
            if (s.count(_args[i])) op._args[i] = s[_args[i]];
            else op._args[i] = _args[i];
        }
        for (Signature sig : _preconditions) {
            Signature sigSubst = sig.substitute(s);
            op.addPrecondition(sigSubst);
        }
        for (Signature sig : _effects) {
            Signature sigSubst = sig.substitute(s);
            op.addEffect(sigSubst);
        }
        return op;
    }

    const SigSet& getPreconditions() {
        return _preconditions;
    }
    const SigSet& getEffects() {
        return _effects;
    }
    std::vector<int> getArguments() {
        return _args;
    }
    Signature getSignature() {
        return Signature(_id, _args);
    }

    HtnOp& operator=(const HtnOp& op) {
        _id = op._id;
        _args = op._args;
        _preconditions = op._preconditions;
        _effects = op._effects;
        return *this;
    }
};


#endif