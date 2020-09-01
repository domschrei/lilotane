
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
#include "util/signal_manager.h"

#ifndef LILOTANE_VERSION
#define LILOTANE_VERSION "(dbg)"
#endif

#ifndef IPASIRSOLVER
#define IPASIRSOLVER "(unknown)"
#endif

void handleSignal(int signum) {
    SignalManager::signalExit();
}

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
    
    signal(SIGTERM, handleSignal);
    signal(SIGINT, handleSignal);

    Timer::init();

    Parameters params;
    params.init(argc, argv);

    int verbosity = params.getIntParam("v");
    Log::init(verbosity, /*coloredOutput=*/params.isNonzero("co"));

    if (verbosity >= Log::V2_INFORMATION) {
        Log::i("\n");
        Log::i("Hello from  ");
        Log::log_notime(Log::V0_ESSENTIAL, "L i l o t a n e");
        Log::log_notime(Log::V2_INFORMATION, "  version %s\n", LILOTANE_VERSION);
        Log::i("by Dominik Schreiber <dominik.schreiber@kit.edu> 2020\n");
        Log::i("using SAT solver %s\n", IPASIRSOLVER);
        Log::i("\n");
    }

    run(params);
    return 1; // something went wrong if run() returns
}