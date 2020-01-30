#ifndef __PLAN
#define __PLAN

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include "parsetree.hpp"


struct instantiated_plan_step{
	std::string name;
	std::vector<std::string> arguments;
	bool declaredPrimitive;
};


struct parsed_plan{
	std::map<int,instantiated_plan_step> tasks;
	std::vector<int> primitive_plan;
	std::map<int,int> pos_in_primitive_plan;
	std::map<int,std::string> appliedMethod;
	std::map<int,std::vector<int>> subtasksForTask;
	std::vector<int> root_tasks;
};

parsed_plan parse_plan(istream & plan, int debugMode);

void convert_plan(istream & plan, ostream & pout);

#endif
