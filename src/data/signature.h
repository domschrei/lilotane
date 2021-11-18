
#ifndef DOMPASCH_TREE_REXX_SIGNATURE_H
#define DOMPASCH_TREE_REXX_SIGNATURE_H

#include <vector>
#include <assert.h>
#include <limits>

#include "util/hashmap.h"
#include "util/hash.h"
#include "substitution.h"

struct TypeConstraint {
    int qconstant;
    bool sign;
    std::vector<int> constants;
    TypeConstraint(int qconstant, bool sign, const std::vector<int>& constants) : 
        qconstant(qconstant), sign(sign), constants(constants) {}
    TypeConstraint(int qconstant, bool sign, std::vector<int>&& constants) : 
        qconstant(qconstant), sign(sign), constants(constants) {}
    TypeConstraint(const TypeConstraint& other) : qconstant(other.qconstant), 
            sign(other.sign), constants(other.constants) {}
    TypeConstraint(TypeConstraint&& other) : qconstant(other.qconstant), 
            sign(other.sign), constants(std::move(other.constants)) {}
};

struct Signature;

struct USignature {

    int _name_id = -1;
    std::vector<int> _args;

    USignature();
    USignature(int nameId, const std::vector<int>& args);
    USignature(int nameId, std::vector<int>&& args);
    USignature(const USignature& sig);
    USignature(USignature&& sig);

    Signature toSignature(bool negated = false) const;
    USignature substitute(const Substitution& s) const;
    void apply(const Substitution& s);
    USignature renamed(int nameId) const;

    USignature& operator=(const USignature& sig);
    USignature& operator=(USignature&& sig);

    inline bool operator==(const USignature& b) const {
        if (_name_id != b._name_id) return false;
        if (_args != b._args) return false;
        return true;
    }
    inline bool operator!=(const USignature& b) const {
        return !(*this == b);
    }
};

struct Signature {
    
    USignature _usig;
    mutable bool _negated = false;

    Signature();
    Signature(int nameId, const std::vector<int>& args, bool negated = false);
    Signature(int nameId, std::vector<int>&& args, bool negated = false);
    Signature(const USignature& usig, bool negated);
    Signature(const Signature& sig);
    Signature(Signature&& sig);

    void negate();
    const USignature& getUnsigned() const;
    Signature opposite() const;
    Signature substitute(const Substitution& s) const;
    void apply(const Substitution& s);

    Signature& operator=(const Signature& sig);
    Signature& operator=(Signature&& sig);

    inline bool operator==(const Signature& b) const {
        if (_negated != b._negated) return false;
        if (_usig != b._usig) return false;
        return true;
    }

    inline bool operator!=(const Signature& b) const {
        return !(*this == b);
    }
};

struct PositionedUSig {
    size_t layer; size_t pos; USignature usig;
    PositionedUSig() : layer(-1), pos(-1) {}
    PositionedUSig(size_t layer, size_t pos, const USignature& usig) : layer(layer), pos(pos), usig(usig) {}
    bool operator==(const PositionedUSig& other) const {
        return layer == other.layer && pos == other.pos && usig == other.usig;
    }
};

struct USignatureHasher {
    static int seed;
    inline std::size_t operator()(const USignature& s) const {
        size_t hash = seed + s._args.size();
        for (const int& arg : s._args) {
            hash_combine(hash, arg);
        }
        hash_combine(hash, s._name_id);
        return hash;
    }
};
struct SignatureHasher {
    USignatureHasher _usig_hasher;
    inline std::size_t operator()(const Signature& s) const {
        size_t hash = _usig_hasher(s._usig);
        hash_combine(hash, s._negated);
        return hash;
    }
};
struct PositionedUSigHasher {
    USignatureHasher usigHasher;
    std::size_t operator()(const PositionedUSig& x) const {
        size_t hash = x.layer;
        hash_combine(hash, x.pos);
        hash_combine(hash, usigHasher(x.usig));
        return hash;
    }
};
struct SigVecHasher {
    SignatureHasher _sig_hasher;
    inline std::size_t operator()(const std::vector<Signature>& s) const {
        size_t hash = s.size();
        for (const Signature& sig : s) hash_combine(hash, _sig_hasher(sig));
        return hash;
    }
};

typedef FlatHashSet<Signature, SignatureHasher> SigSet;
typedef FlatHashSet<USignature, USignatureHasher> USigSet;

namespace Sig {
    const static USignature NONE_SIG = USignature(-1, std::vector<int>());
    const static SigSet EMPTY_SIG_SET;
    const static USigSet EMPTY_USIG_SET;
}

#endif