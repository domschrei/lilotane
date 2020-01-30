%{
	#include <cstdio>
	#include <iostream>
	#include <vector>
	#include <cassert>
	#include <string.h>
	#include <algorithm>
	#include "parsetree.hpp"
	#include "domain.hpp"
	#include "cwa.hpp"
	
	using namespace std;
	
	// Declare stuff from Flex that Bison needs to know about:
	extern int yylex();
	extern int yyparse();
	extern FILE *yyin;
	char* current_parser_file_name;
	
	void yyerror(const char *s);
%}

%locations
%define parse.error verbose
%define parse.lac full

%locations

%union {
	bool bval;
	int ival;
	float fval;
	char *sval;
	std::vector<std::string>* vstring;
	var_declaration* vardecl;
	predicate_definition* preddecl;
	general_formula* formula;
	std::vector<predicate_definition*>* preddecllist;
	std::vector<general_formula*>* formulae;
	var_and_const* varandconst;
	sub_task* subtask;
	std::vector<sub_task*>* subtasks;
	std::pair<bool,std::vector<sub_task*>*>* osubtasks;
	parsed_task_network* tasknetwork;
	std::pair<string,string>* spair;
	std::vector<std::pair<string,string>*>* spairlist;
}

%token KEY_TYPES KEY_DEFINE KEY_DOMAIN KEY_PROBLEM KEY_REQUIREMENTS KEY_PREDICATES KEY_FUNCTIONS
%token KEY_TASK KEY_CONSTANTS KEY_ACTION KEY_PARAMETERS KEY_PRECONDITION KEY_EFFECT KEY_METHOD
%token KEY_GOAL KEY_INIT KEY_OBJECTS KEY_HTN KEY_TIHTN KEY_MIMIZE KEY_METRIC   
%token KEY_AND KEY_OR KEY_NOT KEY_IMPLY KEY_FORALL KEY_EXISTS KEY_WHEN KEY_INCREASE KEY_TYPEOF
%token KEY_CAUSAL_LINKS KEY_CONSTRAINTS KEY_ORDER KEY_ORDER_TASKS KEY_TASKS 
%token <sval> NAME REQUIRE_NAME VAR_NAME 
%token <fval> FLOAT
%token <ival> INT

%type <sval> var_or_const
%type <sval> typed_function_list_continuation 
%type <bval> task_or_action
%type <vstring> NAME-list NAME-list-non-empty 
%type <vstring> VAR_NAME-list VAR_NAME-list-non-empty
%type <vardecl> parameters-option typed_var_list typed_var typed_vars
%type <preddecl> atomic_predicate_def 
%type <preddecllist> atomic_function_def-list
%type <varandconst> var_or_const-list
%type <formulae> gd-list
%type <formula> atomic_formula
%type <formula> gd
%type <formula> gd_empty 
%type <formula> gd_conjuction 
%type <formula> gd_disjuction 
%type <formula> gd_negation 
%type <formula> gd_implication 
%type <formula> gd_existential 
%type <formula> gd_universal 
%type <formula> gd_equality_constraint 
%type <formula> precondition_option
%type <formula> effect_option
%type <formulae> effect-list
%type <formula> effect
%type <formula> eff_empty
%type <formula> eff_conjunction
%type <formula> eff_universal
%type <formula> eff_conditional
%type <formula> literal
%type <formula> p_effect
%type <formula> neg_atomic_formula 
%type <formula> f_head 
%type <formula> f_exp


%type <formula> constraint_def 
%type <formulae> constraint_def-list
%type <formula> constraints_option

%type <osubtasks> subtasks_option
%type <subtasks> subtask_defs
%type <subtasks> subtask_def-list
%type <subtask> subtask_def
%type <tasknetwork> tasknetwork_def

%type <spairlist> ordering_def-list 
%type <spairlist> ordering_defs 
%type <spairlist> ordering_option 
%type <spair> ordering_def 



%%
document: domain | problem


domain: '(' KEY_DEFINE '(' KEY_DOMAIN domain_symbol ')'
        domain_defs 
		')' 
domain_defs:	domain_defs require_def |
				domain_defs type_def |
				domain_defs const_def |
				domain_defs predicates_def |
				domain_defs functions_def |
				domain_defs task_def |
				domain_defs method_def |

problem: '(' KEY_DEFINE '(' KEY_PROBLEM NAME ')'
              '(' KEY_DOMAIN NAME ')'
			problem_defs
		')'

problem_defs: problem_defs require_def |
              problem_defs p_object_declaration |
              problem_defs p_htn | 
              problem_defs p_init | 
              problem_defs p_goal | 
              problem_defs p_constraint | // I think this is only for global LTL constraints
			  problem_defs p_metric |

p_object_declaration : '(' KEY_OBJECTS constant_declaration_list')';
p_init : '(' KEY_INIT init_el ')';
init_el : init_el literal {
		assert($2->type == ATOM);
		map<string,string> access;
		// for each constant a new sort with a uniq name has been created. We access it here and retrieve its only element, the constant in questions
		for(auto x : $2->arguments.newVar) access[x.first] = *sorts[x.second].begin();  
		ground_literal l;
		l.positive = true;
		l.predicate = $2->predicate;
		for(string v : $2->arguments.vars) l.args.push_back(access[v]);
		init.push_back(l);
	} |
	init_el '(' '=' literal INT ')' {
		assert($4->type == ATOM);
		map<string,string> access;
		// for each constant a new sort with a uniq name has been created. We access it here and retrieve its only element, the constant in questions
		for(auto x : $4->arguments.newVar) access[x.first] = *sorts[x.second].begin();
		ground_literal l;
		l.positive = true;
		l.predicate = $4->predicate;
		for(string v : $4->arguments.vars) l.args.push_back(access[v]);
		init_functions.push_back(std::make_pair(l,$5));
	} |

p_goal : '(' KEY_GOAL gd ')' {goal_formula = $3;}

htn_type: KEY_HTN | KEY_TIHTN {assert(false); /*we don't support ti-htn yet*/}
parameters-option: KEY_PARAMETERS '(' typed_var_list ')' {$$ = $3;} | {$$ = new var_declaration(); }
p_htn : '(' htn_type
        parameters-option
        tasknetwork_def
		')' {
		parsed_method m;
		m.name = "__top_method";		
		string atName("__top"); // later for insertion into map
		m.vars = $3;
		m.prec = new general_formula(); m.prec->type = EMPTY;
		m.eff = new general_formula(); m.eff->type = EMPTY;
		m.tn = $4;
		parsed_methods[atName].push_back(m);

		parsed_task	top;
		top.name = "__top";
		top.arguments = new var_declaration();
		top.prec = new general_formula(); top.prec->type = EMPTY;
		top.eff = new general_formula(); top.eff->type = EMPTY;
		parsed_abstract.push_back(top);
}

p_constraint : '(' KEY_CONSTRAINTS gd ')'

p_metric : '(' KEY_METRIC KEY_MIMIZE metric_f_exp ')' 
metric_f_exp : NAME { metric_target = $1; }
metric_f_exp : '(' NAME ')' { metric_target = $2; }

/////////////////////////////////////////////////////////////////////////////////////////////////////
// @PDDL
domain_symbol : NAME

/////////////////////////////////////////////////////////////////////////////////////////////////////
// Requirement Statement
// @PDDL
require_def : '(' KEY_REQUIREMENTS require_defs ')'
require_defs : require_defs REQUIRE_NAME {string r($2); if (r == ":typeof-predicate") has_typeof_predicate = true; } | 



/////////////////////////////////////////////////////////////////////////////////////////////////////
// Type Definition
// @PDDL
type_def : '(' KEY_TYPES type_def_list ')' { /*reverse list after all types have been parsed*/ reverse(sort_definitions.begin(), sort_definitions.end()); };
type_def_list : NAME-list {	sort_definition s; s.has_parent_sort = false; s.declared_sorts = *($1); delete $1;
			  				if (s.declared_sorts.size()) {
								sort_definitions.push_back(s);
								// touch constant map to ensure a consistent access
								for (string & ss : s.declared_sorts) sorts[ss].size();
							}
				}
			  | NAME-list-non-empty '-' NAME type_def_list {
							sort_definition s; s.has_parent_sort = true; s.parent_sort = $3; free($3);
							s.declared_sorts = *($1); delete $1;
			  				sort_definitions.push_back(s);
							// touch constant map to ensure a consistent access
							for (string & ss : s.declared_sorts) sorts[ss].size();
							sorts[s.parent_sort].size();
							}



/////////////////////////////////////////////////////////////////////////////////////////////////////
// Constant Definition
const_def : '(' KEY_CONSTANTS constant_declaration_list ')'
// this is used in both the domain and the initial state
constant_declaration_list : constant_declaration_list constant_declarations |
constant_declarations : NAME-list-non-empty '-' NAME {
						string type($3);
						for(unsigned int i = 0; i < $1->size(); i++)
							sorts[type].insert((*($1))[i]);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// Predicate Definition
// @PDDL
predicates_def : '(' KEY_PREDICATES atomic_predicate_def-list ')'

atomic_predicate_def-list : atomic_predicate_def-list atomic_predicate_def {predicate_definitions.push_back(*($2)); delete $2;} | 
atomic_predicate_def : '(' NAME typed_var_list ')' {
		$$ = new predicate_definition();
		$$->name = $2;
		for (unsigned int i = 0; i < $3->vars.size(); i++) $$->argument_sorts.push_back($3->vars[i].second);
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// Functions Definition
// @PDDL
functions_def : '(' KEY_FUNCTIONS typed_atomic_function_def-list ')'

typed_atomic_function_def-list : atomic_function_def-list typed_function_list_continuation {
	char * type_of_functions = $2;
	for (predicate_definition* p : *$1){
		parsed_functions.push_back(std::make_pair(*p,type_of_functions));
		delete p;
	}
	delete $1;
}
typed_function_list_continuation : '-' NAME typed_atomic_function_def-list { $$ = $2; } | { $$ = strdup(numeric_funtion_type.c_str()); }

atomic_function_def-list : atomic_function_def-list atomic_predicate_def { $$->push_back($2); } | { $$ = new std::vector<predicate_definition*>();}


/////////////////////////////////////////////////////////////////////////////////////////////////////
// Task Definition
// @HDDL
// @LABEL Abstract tasks are defined similar to actions. To use preconditions and effects in the definition,
//         please add the requirement definition :htn-abstract-actions

task_or_action: KEY_TASK {$$=true;} | KEY_ACTION {$$=false;}

task_def : '(' task_or_action NAME
			parameters-option
			precondition_option
			effect_option ')'{
				// found a new task, add it to list
				parsed_task t;
				t.name = $3;
				t.arguments = $4;
				t.prec = $5; 
				t.eff = $6;

				if ($2) parsed_abstract.push_back(t); else parsed_primitive.push_back(t);
}

precondition_option: KEY_PRECONDITION gd {$$ = $2;} | {$$ = new general_formula(); $$->type = EMPTY;}
effect_option: KEY_EFFECT effect {$$ = $2;} | {$$ = new general_formula(); $$->type = EMPTY;}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// Method Definition
// @HDDL
// @LABEL In a pure HTN setting, methods consist of the definition of the abstract task they may decompose as well as the
//         resulting task network. The parameters of a method are supposed to include all parameters of the abstract task
//         that it decomposes as well as those of the tasks in its network of subtasks. By setting the :htn-method-pre-eff
//         requirement, one might use method preconditions and effects similar to the ones used in SHOP.
method_def :
   '(' KEY_METHOD NAME
      parameters-option
      KEY_TASK '(' NAME var_or_const-list ')'
      precondition_option
	  effect_option
      tasknetwork_def
	')'{
		parsed_method m;
		m.name = $3;		
		string atName($7); // later for insertion into map
		m.atArguments = $8->vars; 
		m.newVarForAT = $8->newVar;
		m.vars = $4;
		m.prec = $10;
		m.eff = $11;
		m.tn = $12;

		parsed_methods[atName].push_back(m);
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// Task Definition
// @HDDL
// @LABEL  The following definition of a task network is used in method definitions as well as in the problem definition
//         to define the intial task network. It contains the definition of a number of tasks as well sets of ordering
//         constraints, variable constraints between any method parameters. Please use the requirement :htn-causal-links
//         to include causal links into the model. When the keys :ordered-subtasks or :ordered-tasks are used, the
//         network is regarded to be totally ordered. In the other cases, ordering relations may be defined in the
//         respective section. To do so, the task definition includes an id for every task that can be referenced here.
//         They are also used to define causal links. Two dedicated ids "init" and "goal" can be used in causal link
//         definition to reference the initial state and the goal definition.
tasknetwork_def :
	subtasks_option
	ordering_option
	constraints_option	
	causal_links_option {
	$$ = new parsed_task_network();
	$$->tasks = *($1->second);
	$$->ordering = *($2);
	if ($1->first){
		if ($$->ordering.size()) assert(false); // given ordering but said that this is a total order
		for(unsigned int i = 1; i < $$->tasks.size(); i++){
			pair<string,string>* o = new pair<string,string>();
			o->first = $$->tasks[i-1]->id;
			o->second = $$->tasks[i]->id;
			$$->ordering.push_back(o);
		}
	}
	$$->constraint = $3;

	// TODO causal links?????
} 

subtasks_option: 	  KEY_TASKS subtask_defs {$$ = new pair<bool,vector<sub_task*>*>(); $$->first = false; $$->second = $2; }
			   		| KEY_ORDER_TASKS subtask_defs {$$ = new pair<bool,vector<sub_task*>*>(); $$->first = true; $$->second = $2; }
					| {$$ = new pair<bool,vector<sub_task*>*>();
					   $$->first = true; $$->second = new vector<sub_task*>();}

ordering_option: KEY_ORDER ordering_defs {$$ = $2;}| {$$ = new vector<pair<string,string>*>();}
constraints_option: KEY_CONSTRAINTS constraint_def {$$ = $2;} | {$$ = new general_formula(); $$->type = EMPTY;}
causal_links_option: KEY_CAUSAL_LINKS causallink_defs |   

 /////////////////////////////////////////////////////////////////////////////////////////////////////
// Subtasks
// @HDDL
// @LABEL The subtask definition may contain one or more subtasks. The tasks consist of a task symbol as well as a
//         list of parameters. In case of a method's subnetwork, these parameters have to be included in the method's
//         parameters, in case of the initial task network, they have to be defined as constants in s0 or in a dedicated
//         parameter list (see definition of the initial task network). The tasks may start with an id that can
//         be used to define ordering constraints and causal links.
subtask_defs : '(' ')' {$$ = new vector<sub_task*>();}
			  | subtask_def {$$ = new vector<sub_task*>(); $$->push_back($1);}
			  | '(' KEY_AND subtask_def-list ')' {$$ = $3;}
subtask_def-list : subtask_def-list subtask_def {$$ = $1; $$->push_back($2);}
				 | {$$ = new vector<sub_task*>();}
subtask_def :	'(' NAME var_or_const-list ')' {$$ = new sub_task(); $$->id = "__t_id_" + to_string(task_id_counter); task_id_counter++; $$->task = $2; $$->arguments = $3; }
			  | '(' NAME '(' NAME var_or_const-list ')' ')' {$$ = new sub_task(); $$->id = $2; $$->task = $4; $$->arguments = $5; }

/////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Ordering
//
// @HDDL
// @LABEL The ordering constraints are defined via the task ids. They have to induce a partial order.
ordering_defs : '(' ')'{$$ = new vector<pair<string,string>*>();}
			   | ordering_def {$$ = new vector<pair<string,string>*>(); $$->push_back($1);}
			   | '(' KEY_AND ordering_def-list ')' {$$ = $3;}
ordering_def-list: ordering_def-list ordering_def {$$ = $1; $$->push_back($2);}
				 | {$$ = new vector<pair<string,string>*>();}
ordering_def : '(' NAME '<' NAME ')' {$$ = new pair<string,string>(); $$->first = $2; $$->second = $4;}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// Variable Constraits
// @HDDL
// @LABEL The variable constraints enable to codesignate or non-codesignate variables, or to enforce (or forbid) a
//         variable to have a certain type.
// @EXAMPLE (= ?v1 ?v2)), (not (= ?v3 ?v4)), (sort ?v - type), (not (sort ?v - type))
constraint_def-list: constraint_def-list constraint_def {$$ = $1; $$->push_back($2);}
				    | {$$ = new vector<general_formula*>();}
constraint_def : '(' ')' {$$ = new general_formula(); $$->type = EMPTY;}
				| '(' KEY_AND constraint_def-list ')' {$$ = new general_formula(); $$->type=AND; $$->subformulae = *($3);}
				| '(' '=' var_or_const var_or_const ')' {$$ = new general_formula(); $$->type = EQUAL; $$->arg1 = $3; $$->arg2 = $4;}
				| '(' KEY_NOT '(' '=' var_or_const var_or_const ')' ')' {$$ = new general_formula(); $$->type = NOTEQUAL; $$->arg1 = $5; $$->arg2 = $6;}
                | '(' KEY_TYPEOF typed_var ')' {$$ = new general_formula(); $$->type = OFSORT; $$->arg1 = $3->vars[0].first; $$->arg2 = $3->vars[0].second; }
                | '(' KEY_NOT '(' KEY_TYPEOF typed_var ')' ')'  {$$ = new general_formula(); $$->type = NOTOFSORT; $$->arg1 = $5->vars[0].first; $$->arg2 = $5->vars[0].second; }

/////////////////////////////////////////////////////////////////////////////////////////////////////
// Causal Links
// @HDDL
// @LABEL Causal links in the model enable the predefinition on which action supports a certain precondition. They
//         reference the tasks by the ids that are also used in the definition of ordering constraints.
causallink_defs : '(' ')' | causallink_def | '(' KEY_AND causallink_def-list ')'
causallink_def-list: causallink_def-list causallink_def |
causallink_def : '(' NAME literal NAME ')'


/////////////////////////////////////////////////////////////////////////////////////////////////////
// Goal Description
// @LABEL gd means "goal description". It is used to define goals and preconditions. The PDDL 2.1 definition has been extended by the LTL defintions given by Gerevini andLon "Plan Constraints and Preferences in PDDL3"
gd :  gd_empty {$$ = $1;}
	| atomic_formula {$$ = $1;}
	| gd_negation {$$ = $1;}
	| gd_implication {$$ = $1;}
	| gd_conjuction {$$ = $1;}
	| gd_disjuction {$$ = $1;}
	| gd_existential {$$ = $1;}
	| gd_universal {$$ = $1;}
	| gd_equality_constraint {$$ = $1;}

gd-list : gd-list gd {$$ = $1; $$->push_back($2);} 
		| {$$ = new vector<general_formula*>();}

gd_empty : '(' ')' {$$ = new general_formula(); $$->type=EMPTY;}
gd_conjuction : '(' KEY_AND gd-list ')' {$$ = new general_formula(); $$->type=AND; $$->subformulae = *($3);}
gd_disjuction : '(' KEY_OR gd-list ')' {$$ = new general_formula(); $$->type=OR; $$->subformulae = *($3);}
gd_negation : '(' KEY_NOT gd ')' {$$ = $3; $$->negate();}
gd_implication : '(' KEY_IMPLY gd gd ')' {$$ = new general_formula(); $$->type=OR; $3->negate(); $$->subformulae.push_back($3); $$->subformulae.push_back($4);}
gd_existential : '(' KEY_EXISTS '(' typed_var_list ')' gd ')' {$$ = new general_formula(); $$->type = EXISTS; $$->subformulae.push_back($6); $$->qvariables = *($4);} 
gd_universal : '(' KEY_FORALL '(' typed_var_list ')' gd ')' {$$ = new general_formula(); $$->type = FORALL; $$->subformulae.push_back($6); $$->qvariables = *($4);} 
gd_equality_constraint : '(' '=' var_or_const var_or_const ')' {$$ = new general_formula(); $$->type = EQUAL; $$->arg1 = $3; $$->arg2 = $4;}

var_or_const-list :   var_or_const-list NAME {
						$$ = $1;
						string c($2); string s = "sort_for_" + c; string v = "?var_for_" + c;
						sorts[s].insert(c);
						$$->vars.push_back(v);
						$$->newVar.insert(make_pair(v,s));
					}
					| var_or_const-list VAR_NAME {$$ = $1; string s($2); $$->vars.push_back(s);}
					| {$$ = new var_and_const();}

var_or_const : NAME {$$=$1;}| VAR_NAME {$$=$1;}
atomic_formula : '('NAME var_or_const-list')' {$$ = new general_formula(); $$->type=ATOM;
			   								   $$->predicate = $2; $$->arguments = *($3);
											  }



/////////////////////////////////////////////////////////////////////////////////////////////////////
// Effects
// @LABEL In contrast to earlier versions of this grammar, nested conditional effects are now permitted.
//         This is not allowed in PDDL 2.1

effect-list: effect-list effect {$$ = $1; $$->push_back($2);} 
		| {$$ = new vector<general_formula*>();}

effect : eff_empty {$$ = $1;}
	   | eff_conjunction {$$ = $1;}
	   | eff_universal {$$ = $1;}
	   | eff_conditional {$$ = $1;}
	   | literal {$$ = $1;}
	   | p_effect {$$ = $1;}

eff_empty : '(' ')' {$$ = new general_formula(); $$->type=EMPTY;}
eff_conjunction : '(' KEY_AND effect-list ')' {$$ = new general_formula(); $$->type=AND; $$->subformulae = *($3);}
eff_universal : '(' KEY_FORALL '(' typed_var_list ')' effect ')'{$$ = new general_formula(); $$->type = FORALL; $$->subformulae.push_back($6); $$->qvariables = *($4);}
eff_conditional : '(' KEY_WHEN gd effect ')' {$$ = new general_formula(); $$->type=WHEN; $$->subformulae.push_back($3); $$->subformulae.push_back($4);}


literal : neg_atomic_formula {$$ = $1;} | atomic_formula {$$ = $1;}
neg_atomic_formula : '(' KEY_NOT atomic_formula ')' {$$ = $3; $$->negate();}


// these rules are just here to be able to parse action consts in the future
p_effect : '(' assign_op f_head f_exp ')' {$$ = new general_formula(); $$->type=COST_CHANGE; $$->subformulae.push_back($3); $$->subformulae.push_back($4); }
assign_op : KEY_INCREASE
f_head : NAME { $$ = new general_formula(); $$->type = COST; $$->predicate = $1; }
f_head : '(' NAME var_or_const-list ')' { $$ = new general_formula(); $$->type = COST; $$->predicate = $2; $$->arguments = *($3); }
f_exp : INT { $$ = new general_formula(); $$->type = VALUE; $$->value = $1; }
f_exp : f_head { $$ = $1; }

/////////////////////////////////////////////////////////////////////////////////////////////////////
// elementary list of names
NAME-list-non-empty: NAME-list NAME {string s($2); free($2); $$->push_back(s);}
NAME-list: NAME-list NAME {string s($2); free($2); $$->push_back(s);}
			|  {$$ = new vector<string>();} 


/////////////////////////////////////////////////////////////////////////////////////////////////////
// elementary list of variable names
VAR_NAME-list-non-empty: VAR_NAME-list VAR_NAME {string s($2); free($2); $$->push_back(s);}
VAR_NAME-list: VAR_NAME-list VAR_NAME {string s($2); free($2); $$->push_back(s);}
			|  {$$ = new vector<string>();} 

/////////////////////////////////////////////////////////////////////////////////////////////////////
// lists of variables
typed_vars : VAR_NAME-list-non-empty '-' NAME {
		   	$$ = new var_declaration;
			string t($3);
			for (unsigned int i = 0; i < $1->size(); i++)
				$$->vars.push_back(make_pair((*($1))[i],t));
			}
typed_var : VAR_NAME '-' NAME { $$ = new var_declaration; string v($1); string t($3); $$->vars.push_back(make_pair(v,t));}
typed_var_list : typed_var_list typed_vars {
			   		$$ = $1;
					for (unsigned int i = 0; i < $2->vars.size(); i++) $$->vars.push_back($2->vars[i]);
					delete $2;
				}
			   | {$$ = new var_declaration;} 

%%
void run_parser_on_file(FILE* f, char* filename){
	current_parser_file_name = filename;
	yyin = f;
	yyparse();
}

void yyerror(const char *s) {
  cout << "\x1b[31mParse error\x1b[0m in file " << current_parser_file_name << " in line \x1b[1m" << yylloc.first_line << "\x1b[0m" << endl;
  if (strlen(s) >= 14 && (strncmp("syntax error, ",s,14) == 0)){
    s += 14;
  }
  cout << s << endl;
  // might as well halt now:
  exit(-1);
}
