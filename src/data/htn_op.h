
#ifndef DOMPASCH_TREE_REXX_HTN_OP_H
#define DOMPASCH_TREE_REXX_HTN_OP_H

#include <vector>

#include "bound_condition.h"

class HtnOp {

protected:
    int _id;
    std::vector<int> _args;

    SignatureSet _preconditions;
    SignatureSet _effects;

public:
    HtnOp() {}
    HtnOp(int id, std::vector<int> args) : _id(id), _args(args) {}

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
            op._args[i] = s[_args[i]];
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

    const SignatureSet& getPreconditions() {
        return _preconditions;
    }
    const SignatureSet& getEffects() {
        return _effects;
    }
    std::vector<int> getArguments() {
        return _args;
    }
    Signature getSignature() {
        return Signature(_id, _args);
    }
};


#endif