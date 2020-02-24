
#include <iostream>
#include <assert.h>

#include "data/htn_instance.h"
#include "planner/planner.h"

#ifndef TREEREXX_VERSION
#define TREEREXX_VERSION "(dbg)"
#endif

#ifndef IPASIRSOLVER
#define IPASIRSOLVER "(unknown)"
#endif

void run(Parameters& params) {

    ParsedProblem& p = HtnInstance::parse(params.getDomainFilename(), params.getProblemFilename());

    log("%i methods, %i abstract tasks, %i primitive tasks\n", 
        p.methods.size(), p.abstract_tasks.size(), p.primitive_tasks.size());
    
    log("initial abstract task: %s\n", p.task_name_map["__top"].name.c_str());
    std::vector<parsed_method>& initMethods = p.parsed_methods["__top"];
    for (auto m : initMethods) {
        log("  init method : %i tasks in tn\n", m.tn->tasks.size());
    }

    Planner planner(params, p);
    planner.findPlan();
}

int main(int argc, char** argv) {

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

    run(params);

    log("Exiting happily.\n");
}