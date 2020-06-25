
#ifndef DOMPASCH_TREE_REXX_SIGNATURE_H
#define DOMPASCH_TREE_REXX_SIGNATURE_H

#include <vector>
#include <assert.h>
#include <limits>

#include "data/hashmap.h"
#include "util/hash.h"
#include "substitution.h"

struct TypeConstraint {
    int qconstant;
    bool sign;
    std::vector<int> constants;
    TypeConstraint(int qconstant, bool sign, const std::vector<int>& constants) : 
        qconstant(qconstant), sign(sign), constants(constants) {}
    TypeConstraint(const TypeConstraint& other) : qconstant(other.qconstant), 
            sign(other.sign), constants(other.constants) {}
    TypeConstraint(TypeConstraint&& other) : qconstant(other.qconstant), 
            sign(other.sign), constants(std::move(other.constants)) {}
};

struct Signature;

struct USignature {

    int _name_id;
    std::vector<int> _args;

    USignature();
    USignature(int nameId, const std::vector<int>& args);
    USignature(const USignature& sig);
    USignature(USignature&& sig);

    Signature toSignature(bool negated = false) const;
    USignature substitute(const Substitution& s) const;
    void apply(const Substitution& s);

    bool operator==(const USignature& b) const;
    bool operator!=(const USignature& b) const;

    USignature& operator=(const USignature& sig);
    USignature& operator=(USignature&& sig);
};

struct Signature {
    
    USignature _usig;
    mutable bool _negated = false;

    Signature();
    Signature(int nameId, const std::vector<int>& args, bool negated = false);
    Signature(const USignature& usig, bool negated);
    Signature(const Signature& sig);
    Signature(Signature&& sig);

    void negate();
    const USignature& getUnsigned() const;
    Signature opposite() const;
    Signature substitute(const Substitution& s) const;

    bool operator==(const Signature& b) const;
    bool operator!=(const Signature& b) const;

    Signature& operator=(const Signature& sig);
    Signature& operator=(Signature&& sig);
};

struct USignatureHasher {
    std::size_t operator()(const USignature& s) const;
};
struct SignatureHasher {
    USignatureHasher _usig_hasher;
    std::size_t operator()(const Signature& s) const;
};

typedef FlatHashSet<Signature, SignatureHasher> SigSet;
typedef FlatHashSet<USignature, USignatureHasher> USigSet;

#endif