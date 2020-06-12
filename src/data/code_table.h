
#ifndef DOMPASCH_TREE_REXX_CODE_TABLE_H
#define DOMPASCH_TREE_REXX_CODE_TABLE_H

#include <string>
#include <vector>

#include "data/hashmap.h"
#include "signature.h"

typedef HashMap<USignature, int, USignatureHasher> SigToIntMap;

class CodeTable {

private:
    SigToIntMap _content;
    int _id = 1;

public:
    int operator()(USignature& sig) {
        if (!has(sig)) {
            _content[sig] = _id++;
        } 
        return _content[sig];
    }
    bool has(USignature& sig) {
        return _content.count(sig);
    }
};

class AssociativeCodeTable {

private:
    HashMap<int, SigToIntMap> _content_per_predicate;
    int _id = 1;

public:
    int operator()(USignature& sig) {
        if (!hasPredicate(sig._name_id)) {
            _content_per_predicate[sig._name_id];
        }
        if (!has(sig)) {
            _content_per_predicate[sig._name_id][sig] = _id++;
        }
        return _content_per_predicate[sig._name_id][sig];
    }
    bool hasPredicate(int predId) {
        return _content_per_predicate.count(predId) > 0;
    }
    bool has(USignature& sig) {
        return _content_per_predicate[sig._name_id].count(sig) > 0;
    }
};


#endif