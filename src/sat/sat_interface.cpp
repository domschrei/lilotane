
#include "sat_interface.h"

int callbackShouldTerminate(void* data) {
    SatInterface::BranchData* branchData = (SatInterface::BranchData*) data;
    return branchData->interface->shouldBranchTerminate(*branchData) ? 1 : 0;
}

void callbackFinished(int result, void* solver, void* data) {
    SatInterface::BranchData* branchData = (SatInterface::BranchData*) data;
    branchData->interface->branchDone(result, solver, *branchData);
}
