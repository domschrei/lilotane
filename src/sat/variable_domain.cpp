
#include "variable_domain.h"

#include "util/log.h"

int VariableDomain::_running_var_id = 1;
bool VariableDomain::_locked = false;
bool VariableDomain::_print_variables = false;

void VariableDomain::init(const Parameters& params) {
    _print_variables = params.isSet("pvn");
}

int VariableDomain::nextVar() {
    return _running_var_id++;
}
int VariableDomain::getMaxVar() {
    return _running_var_id-1;
}

void VariableDomain::printVar(int var, const char* desc) {
    if (_print_variables) {
        log("VARMAP %i %s\n", var, desc);
    }
}

void VariableDomain::lock() {
    _locked = true;
}
void VariableDomain::unlock() {
    _locked = false;
}

bool VariableDomain::isLocked() {
    return _locked;
}