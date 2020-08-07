
#ifndef DOMPASCH_LILOTANE_PANDA_PI_PARSER_INTERFACE_HPP
#define DOMPASCH_LILOTANE_PANDA_PI_PARSER_INTERFACE_HPP

#include <vector>
#include <map>
#include <string>
#include <iostream>

#include "parsetree.hpp"
#include "domain.hpp"
#include "cwa.hpp"

// Parsed domain data structures
struct ParsedProblem {
    bool has_typeof_predicate = false;
    std::vector<sort_definition> sort_definitions;
    std::vector<predicate_definition> predicate_definitions;
    std::vector<parsed_task> parsed_primitive;
    std::vector<parsed_task> parsed_abstract;
    std::map<std::string,std::vector<parsed_method> > parsed_methods;
    std::vector<std::pair<predicate_definition,std::string>> parsed_functions;
    std::string metric_target = dummy_function_type;
    std::map<std::string,std::set<std::string> > sorts;
    std::vector<method> methods;
    std::vector<task> primitive_tasks;
    std::vector<task> abstract_tasks;
    std::map<std::string, task> task_name_map;
    std::vector<ground_literal> init;
    std::vector<ground_literal> goal;
};

// "main method"
int run_pandaPIparser(int argc, char** argv, ParsedProblem& pp);

// preprocessor macro to write into the given ParsedProblem struct before exiting
#define COPY_INTO_PARSED_PROBLEM(pp) \
pp.has_typeof_predicate = has_typeof_predicate;\
pp.sort_definitions = sort_definitions;\
pp.predicate_definitions = predicate_definitions;\
pp.parsed_primitive = parsed_primitive;\
pp.parsed_abstract = parsed_abstract;\
pp.parsed_methods = parsed_methods;\
pp.parsed_functions = parsed_functions;\
pp.metric_target = metric_target;\
pp.sorts = sorts;\
pp.methods = methods;\
pp.primitive_tasks = primitive_tasks;\
pp.abstract_tasks = abstract_tasks;\
pp.task_name_map = task_name_map;\
pp.init = init;\
pp.goal = goal;

#endif
