
#ifndef DOMPASCH_TREE_REXX_SIGNATURE_H
#define DOMPASCH_TREE_REXX_SIGNATURE_H

#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <assert.h>

#include "util/hash.h"

typedef std::unordered_map<int, int> substitution_t;

namespace Substitution {
    substitution_t get(std::vector<int> src, std::vector<int> dest);

    struct Hasher {
        std::size_t operator()(const substitution_t& s) const {
            size_t hash = 0;
            for (auto pair : s) {
                hash_combine(hash, pair.first);
                hash_combine(hash, pair.second);
            }
            hash_combine(hash, s.size());
            return hash;
        }
    };
}

struct Signature {
    
    int _name_id;
    std::vector<int> _args;
    bool _negated = false;

    Signature() : _name_id(-1), _args() {}
    Signature(int nameId, std::vector<int> args, bool negated = false) : _name_id(nameId), _args(args), _negated(negated) {}

    void negate() {
        _negated = !_negated;
    }

    Signature abs() const {
        return Signature(_name_id, _args);
    }
    Signature opposite() const {
        Signature out = Signature(_name_id, _args);
        if (!_negated) out.negate();
        return out;
    }

    Signature substitute(std::unordered_map<int, int> s) const {
        Signature sig;
        sig._name_id = _name_id;
        assert(sig._name_id != 0);
        sig._args.resize(_args.size());
        for (int i = 0; i < _args.size(); i++) {
            if (s.count(_args[i])) sig._args[i] = s[_args[i]];
            else sig._args[i] = _args[i];
            assert(sig._args[i] != 0);
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

    bool operator!=(const Signature& b) const {
        return !(*this == b);
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

        /*
        int hash = 1337;
        hash ^= s._name_id;
        for (auto arg : s._args) {
            hash ^= arg;
        }
        return hash;*/

        size_t hash = 0;
        hash_combine(hash, s._name_id);
        for (auto arg : s._args) {
            hash_combine(hash, arg);
        }
        hash_combine(hash, s._negated);
        return hash;
    }
};

struct SignatureComparator {
    bool operator()(const Signature& a, const Signature& b) const {
        return a == b;
    }
};

typedef std::unordered_set<Signature, SignatureHasher> SigSet;

#endif