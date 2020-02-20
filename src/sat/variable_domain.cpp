
#include "variable_domain.h"

int VariableDomain::_running_var_id = 1;
bool VariableDomain::_locked = false;

void VariableDomain::lock() {
    _locked = true;
}
void VariableDomain::unlock() {
    _locked = false;
}

bool VariableDomain::isLocked() {
    return _locked;
}

int VariableDomain::nextVar() {
    return _running_var_id++;
}
int VariableDomain::getMaxVar() {
    return _running_var_id-1;
}