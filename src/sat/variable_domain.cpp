
#include "variable_domain.h"

#include "util/log.h"
#include "util/names.h"

int VariableDomain::_running_var_id = 1;
bool VariableDomain::_locked = false;
bool VariableDomain::_print_variables = false;

void VariableDomain::init(const Parameters& params) {
    _print_variables = params.isNonzero("pvn");
}

int VariableDomain::nextVar() {
    return _running_var_id++;
}
int VariableDomain::getMaxVar() {
    return _running_var_id-1;
}

void VariableDomain::printVar(int var, int layerIdx, int pos, const USignature& sig) {
    if (_print_variables) {
        Log::d("VARMAP %i %s\n", var, varName(layerIdx, pos, sig).c_str());
    }
}
std::string VariableDomain::varName(int layerIdx, int pos, const USignature& sig) {
    std::string out = Names::to_string(sig) + "@(" + std::to_string(layerIdx) + "," + std::to_string(pos) + ")";
    return out;
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