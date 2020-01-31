
#include <iostream>
#include <assert.h>

#include "planner/planner.h"
#include "parser/main.h"

ParsedProblem& parse(std::string domainFile, std::string problemFile) {

    const char* firstArg = "pandaPIparser";
    const char* domainStr = domainFile.c_str();
    const char* problemStr = problemFile.c_str();

    char* args[3];
    args[0] = (char*)firstArg;
    args[1] = (char*)domainStr;
    args[2] = (char*)problemStr;
    
    int result = run_pandaPIparser(3, args);
    return get_parsed_problem();
}

int main(int argc, char** argv) {
    
    std::string domainFile = argv[1];
    std::string problemFile = argv[2];
    ParsedProblem& p = parse(domainFile, problemFile);

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