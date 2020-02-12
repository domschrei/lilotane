
#ifndef DOMPASCH_TREE_REXX_ARG_ITERATOR_H
#define DOMPASCH_TREE_REXX_ARG_ITERATOR_H

#include <unordered_map>
#include <vector>

#include "data/signature.h"
#include "util/log.h"

class HtnInstance;

class ArgIterator {

public:
    static std::vector<Signature> getFullInstantiation(Signature sig, HtnInstance& _htn);
    static std::vector<Signature> instantiate(Signature sig, std::vector<std::vector<int>> eligibleArgs);
};

#endif