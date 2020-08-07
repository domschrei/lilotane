
#include <iostream>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <cstdlib>
#include <sys/wait.h>
#include <exception>
#include <execinfo.h>
#include <signal.h>

#include "data/htn_instance.h"
#include "planner/planner.h"
#include "util/timer.h"

#ifndef LILOTANE_VERSION
#define LILOTANE_VERSION "(dbg)"
#endif

#ifndef IPASIRSOLVER
#define IPASIRSOLVER "(unknown)"
#endif

void run(Parameters& params) {

    ParsedProblem p;
    HtnInstance::parse(params.getDomainFilename(), params.getProblemFilename(), p);

    Log::i("%i methods, %i abstract tasks, %i primitive tasks\n", 
        p.methods.size(), p.abstract_tasks.size(), p.primitive_tasks.size());

    Planner planner(params, p);
    int result = planner.findPlan();

    if (result == 0) {
        // Exit directly -- avoid to clean up :)
        Log::i("Exiting happily.\n");
        exit(result);
    }
    return;
}

int main(int argc, char** argv) {

    Timer::init();

    Parameters params;
    params.init(argc, argv);

    int verbosity = params.getIntParam("v");
    Log::init(verbosity, /*coloredOutput=*/params.isNonzero("co"));

    if (verbosity >= Log::V2_INFORMATION) {
        Log::e("\n");
        Log::e("Hello from  t r e e r e x x  %s\n", LILOTANE_VERSION);
        Log::e("by Dominik Schreiber <dominik.schreiber@kit.edu> 2020\n");
        Log::e("using SAT solver %s\n", IPASIRSOLVER);
        Log::e("\n");
    }

    run(params);
    return 1; // something went wrong if run() returns
}