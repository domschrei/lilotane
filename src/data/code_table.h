
#ifndef DOMPASCH_TREE_REXX_CODE_TABLE_H
#define DOMPASCH_TREE_REXX_CODE_TABLE_H

#include <string>
#include <vector>
#include <unordered_map>

#include "signature.h"

typedef std::unordered_map<Signature, int, SignatureHasher> SigToIntMap;

class CodeTable {

private:
    SigToIntMap _content;
    int _id = 1;

public:
    int operator()(Signature& sig) {
        if (!has(sig)) {
            _content[sig] = _id++;
        } 
        return (sig._negated ? -1 : 1) * _content[sig];
    }
    bool has(Signature& sig) {
        return _content.count(sig) > 0;
    }
};

class AssociativeCodeTable {

private:
    std::unordered_map<int, SigToIntMap> _content_per_predicate;
    int _id = 1;

public:
    int operator()(Signature& sig) {
        if (!hasPredicate(sig._name_id)) {
            _content_per_predicate[sig._name_id];
        }
        if (!has(sig)) {
            _content_per_predicate[sig._name_id][sig] = _id++;
        }
        return (sig._negated ? -1 : 1) * _content_per_predicate[sig._name_id][sig];
    }
    bool hasPredicate(int predId) {
        return _content_per_predicate.count(predId) > 0;
    }
    bool has(Signature& sig) {
        return _content_per_predicate[sig._name_id].count(sig) > 0;
    }
};


#endif