#ifndef __PARSETREE
#define __PARSETREE

#include <vector>
#include <map>
#include <set>
#include <string>
#include <variant>
#include "domain.hpp"
using namespace std;

extern int task_id_counter;

struct sort_definition{
	vector<string> declared_sorts;
	bool has_parent_sort;
	string parent_sort;
};

struct var_declaration{
	vector<pair<string, string> > vars;
};

struct predicate_definition{
	string name;
	vector<string> argument_sorts;
};

typedef set<pair<string, string> > additional_variables ;
struct var_and_const{
	vector<string> vars;
	additional_variables newVar; // varname & sort
};

struct function_expression{
	bool isOnlyValue;
	int value;
	string name;
	var_and_const arguments;
};

enum formula_type {EMPTY, AND, OR, FORALL, EXISTS, ATOM, NOTATOM,  // formulae
				   EQUAL, NOTEQUAL, OFSORT, NOTOFSORT,
				   WHEN,   // conditional effect
				   VALUE, COST, COST_CHANGE // cost statement
				  };

class general_formula{
	public:
		formula_type type;
		vector<general_formula*> subformulae;
		string predicate;
		var_and_const arguments;
		var_declaration qvariables;
		function_expression *value_increased, *increase_amount;

		string arg1;
		string arg2;
		int value;

		void negate();
		bool isEmpty();
		// first: effect, second: additional precondition for that effect
		// if it is an uncompiled conditional effect, the additional prec will be empty
		vector<pair<pair<vector<variant<literal,conditional_effect>>,vector<literal> >, additional_variables> > expand(bool compileConditionalEffects);
		bool isDisjunctive();
		additional_variables variables_for_constants();
		
		literal equalsLiteral();
		literal atomLiteral();
		pair<vector<map<string,string> >, additional_variables> forallVariableReplacement();
		map<string,string> existsVariableReplacement();

		set<string> occuringUnQuantifiedVariables();

		general_formula* copyReplace(map<string,string>& replace);
};


struct parsed_task{
	string name;
	var_declaration* arguments;
	general_formula* prec;
	general_formula* eff;
};


struct sub_task{
	string id;
	string task;
	var_and_const* arguments;
};

struct parsed_task_network{
	vector<sub_task*> tasks;
	vector<pair<string,string>*> ordering;
	general_formula* constraint;
};

struct parsed_method{
	string name;
	vector<string> atArguments;
	var_declaration* vars; 	
	additional_variables newVarForAT; // varname & sort
	general_formula* prec;
	general_formula* eff;
	parsed_task_network* tn;
};

// global places to put data structures
extern bool has_typeof_predicate;
extern vector<sort_definition> sort_definitions;
extern vector<predicate_definition> predicate_definitions;
extern vector<parsed_task> parsed_primitive;
extern vector<parsed_task> parsed_abstract;
extern map<string,vector<parsed_method> > parsed_methods;
extern vector<pair<predicate_definition,string>> parsed_functions;
extern string metric_target;


string sort_for_const(string c);
void compile_goal_into_action();

#endif
