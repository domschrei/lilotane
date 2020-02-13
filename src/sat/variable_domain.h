
#ifndef DOMPASCH_TREE_REXX_VARIABLE_DOMAIN_H
#define DOMPASCH_TREE_REXX_VARIABLE_DOMAIN_H

class VariableDomain {

private:
    static int _running_var_id;
    static bool _locked;

public:
    static int nextVar();
    
    static bool isLocked();
    static void lock();
    static void unlock();
};

#endif