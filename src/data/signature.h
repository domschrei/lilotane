
#ifndef DOMPASCH_TREE_REXX_SIGNATURE_H
#define DOMPASCH_TREE_REXX_SIGNATURE_H

#include <vector>

struct Signature {
    int _name_id;
    std::vector<int>& _args;
    Signature(int nameId, std::vector<int>& args) : _name_id(nameId), _args(args) {}
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

#endif