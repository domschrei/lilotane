
#ifndef DOMPASCH_TREE_REXX_NAMES_H
#define DOMPASCH_TREE_REXX_NAMES_H

#include <string>

#include "data/signature.h"
#include "data/action.h"

namespace Names {
    void init(HashMap<int, std::string>& nameBackTable);
    std::string to_string(int nameId);
    std::string to_string(const Signature& sig);
    std::string to_string_nobrackets(const Signature& sig);
    std::string to_string(const HashMap<int, int>& s);
    std::string to_string(const Action& a);
}

#endif