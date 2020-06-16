
#include "data/signature.h"

namespace Substitution {

    substitution_t get(const std::vector<int>& src, const std::vector<int>& dest) {
        substitution_t s;
        assert(src.size() == dest.size());
        for (int i = 0; i < src.size(); i++) {
            if (src[i] != dest[i]) {
                assert(!s.count(src[i]) || s[src[i]] == dest[i]);
                s[src[i]] = dest[i];
            }
        }
        return s;
    }

    std::vector<substitution_t> getAll(const std::vector<int>& src, const std::vector<int>& dest) {
        std::vector<substitution_t> ss;
        ss.push_back(substitution_t()); // start with empty substitution
        assert(src.size() == dest.size());
        for (int i = 0; i < src.size(); i++) {
            if (src[i] != dest[i]) {

                // Iterate over existing substitutions
                int priorSize = ss.size();
                for (int j = 0; j < priorSize; j++) {
                    substitution_t& s = ss[j];
                    
                    // Does the substitution already have such a key but with a different value? 
                    if (s.count(src[i]) && s[src[i]] != dest[i]) {
                        // yes -- branch: keep original substitution, add alternative

                        substitution_t s1 = s;
                        s1[src[i]] = dest[i];
                        ss.push_back(s1); // overwritten substitution

                    } else {
                        // Just add to substitution
                        s[src[i]] = dest[i];
                    }
                }
            }
        }
        return ss;
    }

    bool implies(const substitution_t& super, const substitution_t& sub) {
        for (const auto& e : sub) {
            if (!super.count(e.first) || super.at(e.first) != e.second) return false;
        }
        return true;
    }
}


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

USignature USignature::substitute(const HashMap<int, int>& s) const {
    USignature sig;
    sig._name_id = _name_id;
    assert(sig._name_id != 0);
    sig._args.resize(_args.size());
    for (int i = 0; i < _args.size(); i++) {
        if (s.count(_args[i])) sig._args[i] = s.at(_args[i]);
        else sig._args[i] = _args[i];
        assert(sig._args[i] != 0);
    }
    return sig;
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


Signature::Signature(Signature&& sig) {
    _usig = std::move(sig._usig);
    _negated = sig._negated;
}
