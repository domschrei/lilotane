
#ifndef PANDA_PI_PARSER_DOMPASCH_MAIN_H
#define PANDA_PI_PARSER_DOMPASCH_MAIN_H

#include <vector>
#include <map>
#include <cstring>

#include "cwa.hpp"
#include "domain.hpp"
#include "hddl.hpp"
#include "hpdlWriter.hpp"
#include "output.hpp"
#include "parametersplitting.hpp"
#include "parsetree.hpp"
#include "plan.hpp"
#include "shopWriter.hpp"
#include "sortexpansion.hpp"
#include "typeof.hpp"
#include "util.hpp"
#include "verify.hpp"

using namespace std;

// declare parser function manually
void run_parser_on_file(FILE* f, char* filename);

struct ParsedProblem {

    ParsedProblem(
        bool& has_typeof_predicate, vector<sort_definition>& sort_definitions, 
        vector<predicate_definition>& predicate_definitions, vector<parsed_task>& parsed_primitive, 
        vector<parsed_task>& parsed_abstract, map<string,vector<parsed_method> >& parsed_methods, 
        vector<pair<predicate_definition,string>>& parsed_functions, 
        string& metric_target, map<string,set<string> >& sorts, vector<method>& methods, vector<task>& primitive_tasks, 
        vector<task>& abstract_tasks, map<string, task>& task_name_map    
    ) : has_typeof_predicate(has_typeof_predicate), sort_definitions(sort_definitions), predicate_definitions(predicate_definitions), parsed_primitive(parsed_primitive),
        parsed_abstract(parsed_abstract), parsed_methods(parsed_methods), parsed_functions(parsed_functions), metric_target(metric_target), sorts(sorts), methods(methods),
        primitive_tasks(primitive_tasks), abstract_tasks(abstract_tasks), task_name_map(task_name_map) {}

    bool& has_typeof_predicate;
    vector<sort_definition>& sort_definitions;
    vector<predicate_definition>& predicate_definitions;
    vector<parsed_task>& parsed_primitive;
    vector<parsed_task>& parsed_abstract;
    map<string,vector<parsed_method> >& parsed_methods;
    vector<pair<predicate_definition,string>>& parsed_functions;
    string& metric_target;

    map<string,set<string> >& sorts;
    vector<method>& methods;
    vector<task>& primitive_tasks;
    vector<task>& abstract_tasks;

    map<string, task>& task_name_map;
};

int run_pandaPIparser(int argc, char** argv);
ParsedProblem& get_parsed_problem();

#endif