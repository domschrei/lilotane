
#ifndef DOMPASCH_TREE_REXX_ARG_ITERATOR_H
#define DOMPASCH_TREE_REXX_ARG_ITERATOR_H

#include <vector>

#include <data/hashmap.h>
#include "data/signature.h"
#include "util/log.h"

class HtnInstance;

class ArgIterator {

public:
    static std::vector<Signature> getFullInstantiation(const Signature& sig, HtnInstance& _htn);
    static std::vector<Signature> instantiate(const Signature& sig, const std::vector<std::vector<int>>& eligibleArgs);
};

#endif