
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

#ifndef TREEREXX_VERSION
#define TREEREXX_VERSION "(dbg)"
#endif

#ifndef IPASIRSOLVER
#define IPASIRSOLVER "(unknown)"
#endif

void handleAbort(int sig) {
    void *array[20];
    size_t size;

    // get void*'s for all entries on the stack
    size = backtrace(array, 20);

    // print out all the frames
    log("Error: signal %d. Backtrace:\n", sig);
    char** bt = backtrace_symbols(array, size);
    for (int i = 0; i < size; i++) {
        log("- %s\n", bt[i]);
    }

    exit(sig);
}

int run(Parameters& params) {

    ParsedProblem& p = HtnInstance::parse(params.getDomainFilename(), params.getProblemFilename());

    log("%i methods, %i abstract tasks, %i primitive tasks\n", 
        p.methods.size(), p.abstract_tasks.size(), p.primitive_tasks.size());

    Planner planner(params, p);
    return planner.findPlan();
}

int main(int argc, char** argv) {

    signal(SIGSEGV, handleAbort);
    signal(SIGABRT, handleAbort);

    log("\n");
    log("Welcome to  t r e e r e x x ,  a SAT-based planner for totally-ordered HTN problems\n");
    log("- Version %s\n", TREEREXX_VERSION);
    log("- Developed and designed by Dominik Schreiber <dominik.schreiber@kit.edu> 2020\n");
    log("- Partially based on works by D. Schreiber, D. Pellier, H. Fiorino, and T. Balyo, 2018-2019\n");
    log("- Using pandaPIparser, the parser of the pandaPI planning system,\n");
    log("    by G. Behnke, D. Höller, P. Bercher et al.\n");
    log("- Using SAT solver %s\n", IPASIRSOLVER);
    log("- Freely usable, modifiable and redistributable via GPLv3 licence\n");
    log("\n");

    Parameters params;
    params.init(argc, argv);

    int result = run(params);

    if (result == 0) log("Exiting happily.\n");
    return result;
}