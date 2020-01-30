#ifndef __DOMAIN
#define __DOMAIN

#include <vector>
#include <map>
#include <set>
#include <string>
#include "sortexpansion.hpp"

using namespace std;


const string dummy_equal_literal = "__equal";
const string dummy_ofsort_literal = "__ofsort";
const string dummy_function_type = "__none";
const string numeric_funtion_type = "number";
const string method_precondition_action_name = "__method_precondition_";


struct literal{
	bool positive;
	bool isConstantCostExpression;
	bool isCostChangeExpression;
	string predicate;
	vector<string> arguments;
	int costValue;
};


struct task{
	string name;
	vector<pair<string,string>> vars;
	vector<literal> prec;
	vector<literal> eff;
	vector<literal> constraints;
	vector<literal> costExpression;

	void check_integrity();
};

struct plan_step{
	string task;
	string id;
	vector<string> args;
};

struct method{
	string name;
	vector<pair<string,string>> vars;
	string at;
	vector<string> atargs;
	vector<plan_step> ps;
	vector<literal> constraints;
	vector<pair<string,string>> ordering;
	
	void check_integrity();
};


// sort name and set of elements
extern map<string,set<string> > sorts;
extern vector<method> methods;
extern vector<task> primitive_tasks;
extern vector<task> abstract_tasks;
extern map<string, task> task_name_map;

void flatten_tasks();
void parsed_method_to_data_structures();
void reduce_constraints();
void clean_up_sorts();
void remove_unnecessary_predicates();

#endif
