
#ifndef DOMPASCH_TREE_REXX_NAMES_H
#define DOMPASCH_TREE_REXX_NAMES_H

#include <string>

#include "data/signature.h"
#include "data/action.h"

namespace Names {
    void init(std::unordered_map<int, std::string>& nameBackTable);
    std::string to_string(int nameId);
    std::string to_string(Signature sig);
    std::string to_string(std::unordered_map<int, int> s);
    std::string to_string(Action a);
};
#endif