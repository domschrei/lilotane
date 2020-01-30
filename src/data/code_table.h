
#ifndef DOMPASCH_TREE_REXX_CODE_TABLE_H
#define DOMPASCH_TREE_REXX_CODE_TABLE_H

#include <string>
#include <vector>
#include <unordered_map>

#include "signature.h"

class CodeTable {

private:
    std::unordered_map<Signature, int, SignatureComparator> _content;
    int _id = 1;

public:
    int operator()(Signature& sig) {
        if (!has(sig)) {
            _content[sig] = _id++;
        }
        return _content[sig];
    }
    bool has(Signature& sig) {
        return _content.count(sig) > 0;
    }
};


#endif