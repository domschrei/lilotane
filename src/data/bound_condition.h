
#ifndef DOMPASCH_TREE_REXX_BOUND_CONDITION_H
#define DOMPASCH_TREE_REXX_BOUND_CONDITION_H

#include <vector>

#include "data/hashmap.h"
#include "data/signature.h"

struct BoundSignature {

    int _id;
    std::vector<int> _arg_refs;
    bool _positive;

    BoundSignature() {}

    BoundSignature(int id, std::vector<int> argRefs, bool positive = true) 
        : _id(id), _arg_refs(argRefs), _positive(positive) {}

    BoundSignature(int id, std::vector<int> args, std::vector<int> parentArgs, bool positive = true) 
        : _id(id),  _positive(positive) {
        for (int arg : args) {
            for (int i = 0; i < parentArgs.size(); i++) {
                if (arg == parentArgs[i]) {
                    _arg_refs.push_back(i);
                }
            }
        }
    }

    Signature unbind(std::vector<int> parentArgs) {
        std::vector<int> unboundArgs;
        for (int ref : _arg_refs) {
            assert(ref >= 0 && ref < parentArgs.size());
            unboundArgs.push_back(parentArgs[ref]);
        }
        return Signature(_id, unboundArgs);
    }

    bool operator==(const BoundSignature& b) const {
        if (_id != b._id) return false;
        if (_arg_refs.size() != b._arg_refs.size()) return false;
        for (int i = 0; i < _arg_refs.size(); i++) {
            if (_arg_refs[i] != b._arg_refs[i]) return false;
        }
        return true;
    }
};

struct BoundSignatureHasher {
    std::size_t operator()(const BoundSignature& s) const {
        int hash = 1337;
        hash ^= s._id;
        for (auto arg : s._arg_refs) {
            hash ^= arg;
        }
        return hash;
    }
};

typedef HashSet<BoundSignature, BoundSignatureHasher> BoundConditionSet;

#endif