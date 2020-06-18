
#ifndef DOMPASCH_TREE_REXX_VARIABLE_DOMAIN_H
#define DOMPASCH_TREE_REXX_VARIABLE_DOMAIN_H

#include "util/params.h"
#include "data/signature.h"

class VariableDomain {

private:
    static int _running_var_id;
    static bool _locked;

    static bool _print_variables;

public:
    static void init(const Parameters& params);

    static int nextVar();
    static int getMaxVar();

    static void printVar(int var, int layerIdx, int pos, const USignature& sig);
    static std::string varName(int layerIdx, int pos, const USignature& sig);
    
    static bool isLocked();
    static void lock();
    static void unlock();
};

#endif