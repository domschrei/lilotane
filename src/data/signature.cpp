
#include "data/signature.h"

USignature::USignature() : _name_id(-1) {}
USignature::USignature(int nameId, const std::vector<int>& args) : _name_id(nameId), _args(args) {}
USignature::USignature(const USignature& sig) : _name_id(sig._name_id), _args(sig._args) {}
USignature::USignature(USignature&& sig) {
    _name_id = sig._name_id;
    _args = std::move(sig._args);
}

Signature USignature::toSignature(bool negated) const {
    return Signature(*this, negated);
}

USignature USignature::substitute(const Substitution& s) const {
    USignature sig(*this);
    sig.apply(s);
    return sig;
}

void USignature::apply(const Substitution& s) {
    for (int i = 0; i < _args.size(); i++) {
        if (s.count(_args[i])) _args[i] = s.at(_args[i]);
        assert(_args[i] != 0);
    }
}

bool USignature::operator==(const USignature& b) const {
    if (_name_id != b._name_id) return false;
    if (_args.size() != b._args.size()) return false;
    for (int i = 0; i < _args.size(); i++) {
        if (_args[i] != b._args[i]) return false;
    }
    return true;
}
bool USignature::operator!=(const USignature& b) const {
    return !(*this == b);
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

std::size_t USignatureHasher::operator()(const USignature& s) const {
    size_t hash = s._args.size();
    for (const int& arg : s._args) {
        hash_combine(hash, arg);
    }
    hash_combine(hash, s._name_id);
    return hash;
}

Signature::Signature() {}
Signature::Signature(int nameId, const std::vector<int>& args, bool negated) : _usig(nameId, args), _negated(negated) {}
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

bool Signature::operator==(const Signature& b) const {
    if (_negated != b._negated) return false;
    if (_usig != b._usig) return false;
    return true;
}

bool Signature::operator!=(const Signature& b) const {
    return !(*this == b);
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

std::size_t SignatureHasher::operator()(const Signature& s) const {
    size_t hash = _usig_hasher(s._usig);
    hash_combine(hash, s._negated);
    return hash;
}