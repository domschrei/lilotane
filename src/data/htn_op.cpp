
#include "htn_op.h"

HtnOp::HtnOp() {}
HtnOp::HtnOp(int id, const std::vector<int>& args) : _id(id), _args(args) {}
HtnOp::HtnOp(const HtnOp& op) : _id(op._id), _args(op._args), _preconditions(op._preconditions), _effects(op._effects) {}
HtnOp::HtnOp(HtnOp&& op) : _id(op._id), _args(std::move(op._args)), 
        _preconditions(std::move(op._preconditions)), _effects(std::move(op._effects)) {}

void HtnOp::setPreconditions(const SigSet& set) {
    _preconditions = set;
}
void HtnOp::addPrecondition(const Signature& sig) {
    _preconditions.insert(sig);
}
void HtnOp::addPrecondition(Signature&& sig) {
    _preconditions.insert(std::move(sig));
}
void HtnOp::addEffect(const Signature& sig) {
    _effects.insert(sig);
}
void HtnOp::addEffect(Signature&& sig) {
    _effects.insert(std::move(sig));
}
void HtnOp::addArgument(int arg) {
    _args.push_back(arg);
}
void HtnOp::removeInconsistentEffects() {
    // Collect all neg. preconds for which the pos. precond is contained, too
    SigSet inconsistentEffs;
    for (const Signature& sig : _effects) {
        if (sig._negated && _effects.count(Signature(sig._usig, false))) {
            inconsistentEffs.insert(sig);
        }
    }
    // Remove each such neg. precond
    for (const Signature& sig : inconsistentEffs) {
        _effects.erase(sig);
    }
}

HtnOp HtnOp::substitute(const Substitution& s) const {
    HtnOp op;
    op._id = _id;
    op._args.resize(_args.size());
    for (size_t i = 0; i < _args.size(); i++) {
        if (s.count(_args[i])) op._args[i] = s.at(_args[i]);
        else op._args[i] = _args[i];
    }
    for (const Signature& sig : _preconditions) {
        op.addPrecondition(sig.substitute(s));
    }
    for (const Signature& sig : _effects) {
        op.addEffect(sig.substitute(s));
    }
    return op;
}

const SigSet& HtnOp::getPreconditions() const {
    return _preconditions;
}
const SigSet& HtnOp::getEffects() const {
    return _effects;
}
const std::vector<int>& HtnOp::getArguments() const {
    return _args;
}
USignature HtnOp::getSignature() const {
    return USignature(_id, _args);
}

HtnOp& HtnOp::operator=(const HtnOp& op) {
    _id = op._id;
    _args = op._args;
    _preconditions = op._preconditions;
    _effects = op._effects;
    return *this;
}