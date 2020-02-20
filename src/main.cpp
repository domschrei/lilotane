
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

    printf("%i methods, %i abstract tasks, %i primitive tasks\n", 
        p.methods.size(), p.abstract_tasks.size(), p.primitive_tasks.size());
    
    printf("initial abstract task: %s\n", p.task_name_map["__top"].name.c_str());
    std::vector<parsed_method>& initMethods = p.parsed_methods["__top"];
    for (auto m : initMethods) {
        printf("  init method : %i tasks in tn\n", m.tn->tasks.size());
    }

    Planner planner(params, p);
    planner.findPlan();
}

int main(int argc, char** argv) {

    std::string ipasirsolver = "(UNKNOWN)";

    printf("\n");
    printf("Welcome to  t r e e r e x x ,  a SAT-based planner for totally-ordered HTN problems\n");
    printf("- Version %s\n", TREEREXX_VERSION);
    printf("- Developed and designed by Dominik Schreiber <dominik.schreiber@kit.edu> 2020\n");
    printf("- Partially based on works by D. Schreiber, D. Pellier, H. Fiorino, and T. Balyo, 2018-2019\n");
    printf("- Using pandaPIparser, the parser of the pandaPI planning system,\n");
    printf("    by G. Behnke, D. Höller, P. Bercher et al.\n");
    printf("- Using SAT solver %s\n", IPASIRSOLVER);
    printf("- Freely usable, modifiable and redistributable via GPLv3 licence\n");
    printf("\n");

    Parameters params;
    params.init(argc, argv);

    run(params);

    printf("Exiting happily.\n");
}