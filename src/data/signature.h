
#ifndef DOMPASCH_TREE_REXX_SIGNATURE_H
#define DOMPASCH_TREE_REXX_SIGNATURE_H

#include <vector>

#include "data/arg.h"

struct Signature {
    
    int _name_id;
    std::vector<Argument> _args;
    bool _negated = false;

    Signature() {}
    Signature(int nameId, std::vector<Argument>& args) : _name_id(nameId), _args(args) {}

    void negate() {
        _negated = true;
    }

    bool operator==(const Signature& b) const {
        if (_name_id != b._name_id) return false;
        if (_args.size() != b._args.size()) return false;
        for (int i = 0; i < _args.size(); i++) {
            if (_args[i]._name_id != b._args[i]._name_id) return false;
            if (_args[i]._sort != b._args[i]._sort) return false; // TODO typing
        }
        return true;
    }

    Signature& operator=(const Signature& sig) {
        _name_id = sig._name_id;
        _args = sig._args;
    } 
};

struct SignatureHasher {
    std::size_t operator()(const Signature& s) const {
        int hash = 1337;
        hash ^= s._name_id;
        for (auto arg : s._args) {
            hash ^= arg._name_id;
        }
        return hash;
    }
};

struct SignatureComparator {
    bool operator()(const Signature& a, const Signature& b) const {
        if (a._name_id != b._name_id) return false;
        if (a._args.size() != b._args.size()) return false;
        for (int i = 0; i < a._args.size(); i++) {
            if (a._args[i]._name_id != b._args[i]._name_id) return false;
            if (a._args[i]._sort != b._args[i]._sort) return false; // TODO typing
        }
        return true;
    }
};

#endif