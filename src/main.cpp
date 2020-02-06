
#include <iostream>
#include <assert.h>

#include "data/htn_instance.h"
#include "planner/planner.h"

int main(int argc, char** argv) {
    
    std::string domainFile = argv[1];
    std::string problemFile = argv[2];
    ParsedProblem& p = HtnInstance::parse(domainFile, problemFile);

    printf("%i methods, %i abstract tasks, %i primitive tasks\n", 
        p.methods.size(), p.abstract_tasks.size(), p.primitive_tasks.size());
    
    printf("initial abstract task: %s\n", p.task_name_map["__top"].name.c_str());
    std::vector<parsed_method>& initMethods = p.parsed_methods["__top"];
    for (auto m : initMethods) {
        printf("  init method : %i tasks in tn\n", m.tn->tasks.size());
    }

    Planner planner(p);
    planner.findPlan();
}