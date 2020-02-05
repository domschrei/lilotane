
#ifndef DOMPASCH_TREE_REXX_SIGNATURE_H
#define DOMPASCH_TREE_REXX_SIGNATURE_H

#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <assert.h>


typedef std::unordered_map<int, int> substitution_t;

namespace Substitution {
    substitution_t get(std::vector<int> src, std::vector<int> dest);
};

struct Signature {
    
    int _name_id;
    std::vector<int> _args;
    bool _negated = false;

    Signature() {}
    Signature(int nameId, std::vector<int> args) : _name_id(nameId), _args(args) {}

    void negate() {
        _negated = true;
    }

    Signature substitute(std::unordered_map<int, int> s) {
        Signature sig;
        sig._name_id = _name_id;
        sig._args.resize(_args.size());
        for (int i = 0; i < _args.size(); i++) {
            if (s.count(_args[i])) sig._args[i] = s[_args[i]];
            else sig._args[i] = _args[i];
        }
        sig._negated = _negated;
        return sig;
    }

    bool operator==(const Signature& b) const {
        if (_name_id != b._name_id) return false;
        if (_negated != b._negated) return false;
        if (_args.size() != b._args.size()) return false;
        for (int i = 0; i < _args.size(); i++) {
            if (_args[i] != b._args[i]) return false;
        }
        return true;
    }

    Signature& operator=(const Signature& sig) {
        _name_id = sig._name_id;
        _args = sig._args;
        _negated = sig._negated;
        return *this;
    }
};

struct SignatureHasher {
    std::size_t operator()(const Signature& s) const {
        int hash = 1337;
        hash ^= s._name_id;
        for (auto arg : s._args) {
            hash ^= arg;
        }
        return hash;
    }
};

struct SignatureComparator {
    bool operator()(const Signature& a, const Signature& b) const {
        if (a._name_id != b._name_id) return false;
        if (a._args.size() != b._args.size()) return false;
        for (int i = 0; i < a._args.size(); i++) {
            if (a._args[i] != b._args[i]) return false;
        }
        return true;
    }
};

typedef std::unordered_set<Signature, SignatureHasher> SigSet;

#endif