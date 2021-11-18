
#include "data/signature.h"

USignature::USignature() = default;
USignature::USignature(int nameId, const std::vector<int>& args) : _name_id(nameId), _args(args) {}
USignature::USignature(int nameId, std::vector<int>&& args) : _name_id(nameId), _args(std::move(args)) {}
USignature::USignature(const USignature& sig) : _name_id(sig._name_id), _args(sig._args) {}
USignature::USignature(USignature&& sig) : _name_id(sig._name_id), _args(std::move(sig._args)) {}

Signature USignature::toSignature(bool negated) const {
    return Signature(*this, negated);
}

USignature USignature::substitute(const Substitution& s) const {
    USignature sig(*this);
    sig.apply(s);
    return sig;
}

void USignature::apply(const Substitution& s) {
    for (size_t i = 0; i < _args.size(); i++) {
        auto it = s.find(_args[i]);
        if (it != s.end()) _args[i] = it->second;
        assert(_args[i] != 0);
    }
}

USignature USignature::renamed(int nameId) const {
    USignature sig(*this);
    sig._name_id = nameId;
    return sig;
}

USignature& USignature::operator=(const USignature& sig) {
    _name_id = sig._name_id;
    _args = sig._args;
    return *this;
}
USignature& USignature::operator=(USignature&& sig) {
    _name_id = sig._name_id;
    _args = std::move(sig._args);
    return *this;
}

Signature::Signature() = default;
Signature::Signature(int nameId, const std::vector<int>& args, bool negated) : _usig(nameId, args), _negated(negated) {}
Signature::Signature(int nameId, std::vector<int>&& args, bool negated) : _usig(nameId, std::move(args)), _negated(negated) {}
Signature::Signature(const USignature& usig, bool negated) : _usig(usig), _negated(negated) {}
Signature::Signature(const Signature& sig) : _usig(sig._usig), _negated(sig._negated) {}
Signature::Signature(Signature&& sig) {
    _usig = std::move(sig._usig);
    _negated = sig._negated;
}

void Signature::negate() {
    _negated = !_negated;
}

const USignature& Signature::getUnsigned() const {
    return _usig;
}

Signature Signature::Signature::opposite() const {
    Signature out(*this);
    if (_negated) out.negate();
    return out;
}

Signature Signature::substitute(const Substitution& s) const {
    return Signature(_usig.substitute(s), _negated);
}

void Signature::apply(const Substitution& s) {
    _usig.apply(s);
}

Signature& Signature::operator=(const Signature& sig) {
    _usig = sig._usig;
    _negated = sig._negated;
    return *this;
}

Signature& Signature::operator=(Signature&& sig) {
    _usig = std::move(sig._usig);
    _negated = sig._negated;
    return *this;
}

int USignatureHasher::seed = 1;
