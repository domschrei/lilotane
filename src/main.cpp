
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
    Log::e("Error: signal %d. Backtrace:\n", sig);
    char** bt = backtrace_symbols(array, size);
    for (int i = 0; i < size; i++) {
        Log::e("- %s\n", bt[i]);
    }

    exit(sig);
}

void run(Parameters& params) {

    ParsedProblem p;
    HtnInstance::parse(params.getDomainFilename(), params.getProblemFilename(), p);

    Log::i("%i methods, %i abstract tasks, %i primitive tasks\n", 
        p.methods.size(), p.abstract_tasks.size(), p.primitive_tasks.size());

    Planner planner(params, p);
    int result = planner.findPlan();

    if (result == 0) Log::i("Exiting happily.\n");
    exit(result);
}

int main(int argc, char** argv) {

    //signal(SIGSEGV, handleAbort);
    //signal(SIGABRT, handleAbort);

    Timer::init();

    Parameters params;
    params.init(argc, argv);

    Log::init(params.getIntParam("v"), params.isSet("co"));

    Log::i("\n");
    Log::i("Welcome to  t r e e r e x x ,  a SAT-based planner for totally ordered HTN problems\n");
    Log::i("- Version %s\n", TREEREXX_VERSION);
    Log::i("- Developed and designed by Dominik Schreiber <dominik.schreiber@kit.edu> 2020\n");
    Log::i("- Partially based on works by D. Schreiber, D. Pellier, H. Fiorino, and T. Balyo, 2018-2019\n");
    Log::i("- Using pandaPIparser, the parser of the pandaPI planning system,\n");
    Log::i("    by G. Behnke, D. Höller, P. Bercher et al.\n");
    Log::i("- Using SAT solver %s\n", IPASIRSOLVER);
    Log::i("- Freely usable, modifiable and redistributable via GPLv3 licence\n");
    Log::i("\n");

    run(params);
}