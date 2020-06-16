
#ifndef DOMPASCH_TREE_REXX_SIGNATURE_H
#define DOMPASCH_TREE_REXX_SIGNATURE_H

#include <vector>
#include <assert.h>
#include <limits>

#include "data/hashmap.h"
#include "util/hash.h"

struct TypeConstraint {
    int qconstant;
    bool sign;
    std::vector<int> constants;
    TypeConstraint(int qconstant, bool sign, const std::vector<int>& constants) : 
        qconstant(qconstant), sign(sign), constants(constants) {}
};

typedef HashMap<int, int> substitution_t;

namespace Substitution {
    substitution_t get(const std::vector<int>& src, const std::vector<int>& dest);
    std::vector<substitution_t> getAll(const std::vector<int>& src, const std::vector<int>& dest);
    bool implies(const substitution_t& super, const substitution_t& sub);

    struct Hasher {
        std::size_t operator()(const substitution_t& s) const {
            size_t hash = 1337;
            for (const auto& pair : s) {
                hash_combine(hash, pair.first);
                hash_combine(hash, pair.second);
            }
            hash_combine(hash, s.size());
            return hash;
        }
    };
}

struct Signature;

struct USignature {

    int _name_id;
    std::vector<int> _args;

    USignature();
    USignature(int nameId, const std::vector<int>& args);
    USignature(const USignature& sig);
    USignature(USignature&& sig);

    Signature toSignature(bool negated = false) const;
    USignature substitute(const HashMap<int, int>& s) const;

    bool operator==(const USignature& b) const;
    bool operator!=(const USignature& b) const;

    USignature& operator=(const USignature& sig);
    USignature& operator=(USignature&& sig);
};

struct Signature {
    
    USignature _usig;
    mutable bool _negated = false;

    Signature() {}
    Signature(int nameId, const std::vector<int>& args, bool negated = false) : _usig(nameId, args), _negated(negated) {}
    Signature(const USignature& usig, bool negated = false) : _usig(usig), _negated(negated) {}
    Signature(const Signature& sig) : _usig(sig._usig), _negated(sig._negated) {}
    Signature(Signature&& sig);

    void negate() {
        _negated = !_negated;
    }

    const USignature& getUnsigned() const {
        return _usig;
    }

    Signature opposite() const {
        Signature out(*this);
        if (_negated) out.negate();
        return out;
    }

    Signature substitute(const HashMap<int, int>& s) const {
        Signature sig(_usig.substitute(s));
        sig._negated = _negated;
        return sig;
    }

    bool operator==(const Signature& b) const {
        if (_negated != b._negated) return false;
        if (_usig != b._usig) return false;
        return true;
    }

    bool operator!=(const Signature& b) const {
        return !(*this == b);
    }

    Signature& operator=(const Signature& sig) {
        _usig = sig._usig;
        _negated = sig._negated;
        return *this;
    }

    Signature& operator=(Signature&& sig) {
        _usig = std::move(sig._usig);
        _negated = sig._negated;
        return *this;
    }
};

struct USignatureHasher {
    std::size_t operator()(const USignature& s) const {
        size_t hash = 1337;
        for (auto arg : s._args) {
            hash_combine(hash, arg);
        }
        hash_combine(hash, s._name_id);
        return hash;
    }
};
struct SignatureHasher {
    USignatureHasher _usig_hasher;
    std::size_t operator()(const Signature& s) const {
        size_t hash = _usig_hasher(s._usig);
        hash_combine(hash, s._negated);
        return hash;
    }
};

typedef HashSet<Signature, SignatureHasher> SigSet;
typedef HashSet<USignature, USignatureHasher> USigSet;

#endif