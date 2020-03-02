#include <cstdio>
#include <iostream>
#include <vector>
#include <map>
#include <cassert>
#include "output.hpp"
#include "parsetree.hpp"
#include "hddl.hpp"
#include "domain.hpp"
#include "sortexpansion.hpp"
#include "cwa.hpp"
#include "util.hpp"

using namespace std;

void verbose_output(int verbosity){
	cout << "number of sorts: " << sorts.size() << endl;
	if (verbosity > 0) for(auto s : sorts){
		cout << "\t" << color(COLOR_RED,s.first) << ":";
		for (string e : s.second) cout << " " << e;
		cout << endl;
	}
	
	cout << "number of predicates: " << predicate_definitions.size() << endl;
	if (verbosity > 0) for (auto def : predicate_definitions){
		cout << "\t" << color(COLOR_RED,def.name);
		for (string arg : def.argument_sorts) cout << " " << arg;
		cout << endl;
	}

	cout << "number of primitives: " << primitive_tasks.size() << endl;
   	if (verbosity > 0) for(task t : primitive_tasks){
		cout << "\t" << color(COLOR_RED, t.name) << endl;
		if (verbosity == 1) continue;
		cout << "\t\tvars:" << endl;
		for(auto v : t.vars) cout << "\t\t     " << v.first << " - " << v.second << endl;
		if (verbosity == 2) continue;
		cout << "\t\tprec:" << endl;
		for(literal l : t.prec){
			cout << "\t\t     " << (l.positive?"+":"-") << " " << color(COLOR_BLUE,l.predicate);
			for(string v : l.arguments) cout << " " << v;
			cout << endl;
		}
		cout << "\t\teff:" << endl;
		for(literal l : t.eff){
			cout << "\t\t     " << (l.positive?"+":"-") << " " << color(COLOR_GREEN,l.predicate);
			for(string v : l.arguments) cout << " " << v;
			cout << endl;
		}
		cout << "\t\tconstraints:" << endl;
		for(literal l : t.constraints){
			cout << "\t\t     " << (l.positive?"+":"-") << " " << color(COLOR_CYAN,l.predicate);
			for(string v : l.arguments) cout << " " << v;
			cout << endl;
		}
	}	

	cout << "number of abstracts: " << abstract_tasks.size() << endl;
   	if (verbosity > 0) for(task t : abstract_tasks){
		cout << "\t" << color(COLOR_RED, t.name) << endl;
		if (verbosity == 1) continue;
		cout << "\t\tvars:" << endl;
		for(auto v : t.vars) cout << "\t\t     " << v.first << " - " << v.second << endl;
	}

	cout << "number of methods: " << methods.size() << endl;
	if (verbosity > 0) for (method m : methods) {
		cout << "\t" << color(COLOR_RED, m.name) << endl;
		cout << "\t\tabstract task: " << color(COLOR_BLUE, m.at);
	    for (string v : m.atargs) cout << " " << v;
		cout << endl;	
		if (verbosity == 1) continue;
		cout << "\t\tvars:" << endl;
		for(auto v : m.vars) cout << "\t\t     " << v.first << " - " << v.second << endl;
		if (verbosity == 2) continue;
		cout << "\t\tsubtasks:" << endl;
		for(plan_step ps : m.ps){
			cout << "\t\t     " << ps.id << " " << color(COLOR_GREEN, ps.task);
			for (string v : ps.args) cout << " " << v;
			cout << endl;
		}
		if (verbosity == 3) continue;
		cout << "\t\tordering:" << endl;
		for(auto o : m.ordering)
			cout << "\t\t     " << color(COLOR_YELLOW,o.first) << " < " << color(COLOR_YELLOW,o.second) << endl;
		cout << "\t\tconstraints:" << endl;
		for(literal l : m.constraints){
			cout << "\t\t     " << (l.positive?"+":"-") << " " << color(COLOR_CYAN,l.predicate);
			for(string v : l.arguments) cout << " " << v;
			cout << endl;
		}
	}

	cout << "Initial state: " << init.size() << endl;
	if (verbosity > 0) for(ground_literal l : init){
		cout << "\t" << (l.positive?"+":"-")<< color(COLOR_BLUE,l.predicate);
		for(string c : l.args) cout << " " << c;
		cout << endl;
	}
	
	cout << "Goal state: " << goal.size() << endl;
	if (verbosity > 0) for(ground_literal l : goal){
		cout << "\t" << (l.positive?"+":"-")<< color(COLOR_BLUE,l.predicate);
		for(string c : l.args) cout << " " << c;
		cout << endl;
	}
}

void simple_hddl_output(ostream & dout){
	// prep indices
	map<string,int> constants;
	vector<string> constants_out;
	for (auto x : sorts) for (string s : x.second) {
		if (constants.count(s) == 0) constants[s] = constants.size(), constants_out.push_back(s);
	}

	map <string,int> sort_id;
	vector<pair<string,set<string>>> sort_out;
	for (auto x : sorts) if (!sort_id.count(x.first))
		sort_id[x.first] = sort_id.size(), sort_out.push_back(x);

	set<string> neg_pred;
	for (task t : primitive_tasks) for (literal l : t.prec) if (!l.positive) neg_pred.insert(l.predicate);
	for (task t : primitive_tasks) for (conditional_effect ceff : t.ceff) for (literal l : ceff.condition) if (!l.positive) neg_pred.insert(l.predicate);
	for (auto l : goal) if (!l.positive) neg_pred.insert(l.predicate);

	map<string,int> predicates;
	vector<pair<string,predicate_definition>> predicate_out;
	for (auto p : predicate_definitions){
		predicates["+" + p.name] = predicates.size();
		predicate_out.push_back(make_pair("+" + p.name, p));

		if (neg_pred.count(p.name)){
			predicates["-" + p.name] = predicates.size();
			predicate_out.push_back(make_pair("-" + p.name, p));
		}
	}

	map<string,int> function_declarations;
	vector<predicate_definition> functions_out;
	for (auto p : parsed_functions){
		if (p.second != numeric_funtion_type){
			cerr << "the parser currently supports only numeric (type \"number\") functions." << endl;
			exit(1);
		}

		if (p.first.name == metric_target) continue; // don't output the metric target, we don't need it
		function_declarations[p.first.name] = function_declarations.size();
		functions_out.push_back(p.first);
	}

	// determine whether the instance actually has action costs. If not, we insert in the output that every action has cost 1
	bool instance_has_action_costs = metric_target != dummy_function_type;


	bool instance_is_classical = true;
	for (task t : abstract_tasks)
		if (t.name == "__top") instance_is_classical = false;

	// if we are in a classical domain remove everything HTNy
	if (instance_is_classical){
		abstract_tasks.clear();
		methods.clear();
	}
	

	map<string,int> task_id;
	vector<pair<task,bool>> task_out;
	for (task t : primitive_tasks){
		assert(task_id.count(t.name) == 0);
		task_id[t.name] = task_id.size();
		task_out.push_back(make_pair(t,true));
	}
	for (task t : abstract_tasks){
		assert(task_id.count(t.name) == 0);
		task_id[t.name] = task_id.size();
		task_out.push_back(make_pair(t,false));
	}

	// write domain to std out
	dout << "#number_constants_number_sorts" << endl;
	dout << constants.size() << " " << sorts.size() << endl;
	dout << "#constants" << endl;
	for (string c : constants_out) dout << c << endl;
	dout << "#end_constants" << endl;
	dout << "#sorts_each_with_number_of_members_and_members" << endl;
	for(auto s : sort_out) {
		dout << s.first << " " << s.second.size();
		for (auto c : s.second) dout << " " << constants[c];
		dout << endl;	
	}
	dout << "#end_sorts" << endl;
	dout << "#number_of_predicates" << endl;
	dout << predicate_out.size() << endl;
	dout << "#predicates_each_with_number_of_arguments_and_argument_sorts" << endl;
	for(auto p : predicate_out){
		dout << p.first << " " << p.second.argument_sorts.size();
		for(string s : p.second.argument_sorts) assert(sort_id.count(s)), dout << " " << sort_id[s];
		dout << endl;
	}
	dout << "#end_predicates" << endl;
	dout << "#number_of_functions" << endl;
	dout << function_declarations.size() << endl;
	dout << "#function_declarations_with_number_of_arguments_and_argument_sorts" << endl;
	for(auto f : functions_out){
		dout << f.name << " " << f.argument_sorts.size();
		for(string s : f.argument_sorts) assert(sort_id.count(s)), dout << " " << sort_id[s];
		dout << endl;
	}
	
	dout << "#number_primitive_tasks_and_number_abstract_tasks" << endl;
	dout << primitive_tasks.size() << " " << abstract_tasks.size() << endl;

	for (auto tt : task_out){
		task t = tt.first;
		dout << "#begin_task_name_number_of_original_variables_and_number_of_variables" << endl;
		dout << t.name << " " << t.number_of_original_vars << " " << t.vars.size() << endl;
		dout << "#sorts_of_variables" << endl;
		map<string,int> v_id;
		for (auto v : t.vars) assert(sort_id.count(v.second)), dout << sort_id[v.second] << " ", v_id[v.first] = v_id.size();
		dout << endl;
		dout << "#end_variables" << endl;

		if (tt.second){
			dout << "#number_of_cost_statements" << endl;
			if (instance_has_action_costs)
				dout << t.costExpression.size() << endl;
			else
				dout << 1 << endl;
			dout << "#begin_cost_statements" << endl;
			if (instance_has_action_costs){
				for (auto c : t.costExpression){
					if (c.isConstantCostExpression)
						dout << "const " << c.costValue << endl;
					else {
						dout << "var " << function_declarations[c.predicate];
						for (string v : c.arguments) dout << " " << v_id[v];
						dout << endl;
					}
				}
			} else
				dout << "const 1" << endl;
			dout << "#end_cost_statements" << endl;

			dout << "#preconditions_each_predicate_and_argument_variables" << endl;
			dout << t.prec.size() << endl;
			for (literal l : t.prec){
				string p = (l.positive ? "+" : "-") + l.predicate;
				dout << predicates[p];
				for (string v : l.arguments) dout << " " << v_id[v];
				dout << endl;
			}
	
			// determine number of add and delete effects
			int add = 0, del = 0;
			for (literal l : t.eff){
				if (neg_pred.count(l.predicate)) add++,del++;
				else if (l.positive) add++;
				else del++;
			}

			// count conditional add and delete effects
			int cadd = 0, cdel = 0;
			for (conditional_effect ceff : t.ceff) {
				literal l = ceff.effect;
				if (neg_pred.count(l.predicate)) cadd++,cdel++;
				else if (l.positive) cadd++;
				else cdel++;
			}

			dout << "#add_each_predicate_and_argument_variables" << endl;
			dout << add << endl;
			for (literal l : t.eff){
				if (!neg_pred.count(l.predicate) && !l.positive) continue;
				string p = (l.positive ? "+" : "-") + l.predicate;
				dout << predicates[p];
				for (string v : l.arguments) dout << " " << v_id[v];
				dout << endl;
			}

			dout << "#conditional_add_each_with_conditions_and_effect" << endl;
			dout << cadd << endl;
			for (conditional_effect ceff : t.ceff) {
				// if this is a delete effect and the "-" predicates is not necessary
				if (!neg_pred.count(ceff.effect.predicate) && !ceff.effect.positive) continue;
				// number of conditions
				dout << ceff.condition.size();
				for (literal l : ceff.condition){
					string p = (l.positive ? "+" : "-") + l.predicate;
					dout << "  "  << predicates[p]; // two spaces for better human readability
					for (string v : l.arguments) dout << " " << v_id[v];
				}

				// effect
				string p = (ceff.effect.positive ? "+" : "-") + ceff.effect.predicate;
				dout << "  "  << predicates[p]; // two spaces for better human readability
				for (string v : ceff.effect.arguments) dout << " " << v_id[v];

				dout << endl;
			}

			
			dout << "#del_each_predicate_and_argument_variables" << endl;
			dout << del << endl;
			for (literal l : t.eff){
				if (!neg_pred.count(l.predicate) && l.positive) continue;
				string p = (l.positive ? "-" : "+") + l.predicate;
				dout << predicates[p];
				for (string v : l.arguments) dout << " " << v_id[v];
				dout << endl;
			}

			dout << "#conditional_del_each_with_conditions_and_effect" << endl;
			dout << cdel << endl;
			for (conditional_effect ceff : t.ceff) {
				// if this is an add effect and the "+" predicates is not necessary
				if (!neg_pred.count(ceff.effect.predicate) && ceff.effect.positive) continue;
				// number of conditions
				dout << ceff.condition.size();
				for (literal l : ceff.condition){
					string p = (l.positive ? "+" : "-") + l.predicate;
					dout << "  "  << predicates[p]; // two spaces for better human readability
					for (string v : l.arguments) dout << " " << v_id[v];
				}

				// effect
				string p = (ceff.effect.positive ? "+" : "-") + ceff.effect.predicate;
				dout << "  "  << predicates[p]; // two spaces for better human readability
				for (string v : ceff.effect.arguments) dout << " " << v_id[v];

				dout << endl;
			}

	
			dout << "#variable_constraints_first_number_then_individual_constraints" << endl;
			dout << t.constraints.size() << endl;
			for (literal l : t.constraints){
				if (!l.positive) dout << "!";
				dout << "= " << v_id[l.arguments[0]] << " " << v_id[l.arguments[1]] << endl;
				assert(l.arguments[0][0] == '?'); // cannot be a constant
				assert(l.arguments[1][0] == '?'); // cannot be a constant
			}
		}
		dout << "#end_of_task" << endl;
	}
	
	dout << "#number_of_methods" << endl;
	dout << methods.size() << endl;

	for (method m : methods){
		dout << "#begin_method_name_abstract_task_number_of_variables" << endl;
		dout << m.name << " " << task_id[m.at] << " " << m.vars.size() << endl;
		dout << "#variable_sorts" << endl;
		map<string,int> v_id;
		for (auto v : m.vars) assert(sort_id.count(v.second)), dout << sort_id[v.second] << " ", v_id[v.first] = v_id.size();
		dout << endl;
		dout << "#parameter_of_abstract_task" << endl;
		for (string v : m.atargs) dout << v_id[v] << " ";
		dout << endl;
		dout << "#number_of_subtasks" << endl;
		dout << m.ps.size() << endl;
		dout << "#subtasks_each_with_task_id_and_parameter_variables" << endl;
		map<string,int> ps_id;
		for (plan_step ps : m.ps){
			ps_id[ps.id] = ps_id.size();
			dout << task_id[ps.task];
			for (string v : ps.args) dout << " " << v_id[v];
			dout << endl;
		}
		dout << "#number_of_ordering_constraints_and_ordering" << endl;
		dout << m.ordering.size() << endl;
		for (auto o : m.ordering) dout << ps_id[o.first] << " " << ps_id[o.second] << endl;

		dout << "#variable_constraints" << endl;
		dout << m.constraints.size() << endl;
		for (literal l : m.constraints){
			if (!l.positive) dout << "!";
			dout << "= " << v_id[l.arguments[0]] << " " << v_id[l.arguments[1]] << endl;
			assert(l.arguments[1][0] == '?'); // cannot be a constant
		}
		dout << "#end_of_method" << endl;
	}

	dout << "#init_and_goal_facts" << endl;
	dout << init.size() << " " << goal.size() << endl;
	for (auto gl : init){
		string pn = (gl.positive ? "+" : "-") + gl.predicate;
		assert(predicates.count(pn) != 0);
		dout << predicates[pn];
		for (string c : gl.args) dout << " " << constants[c];
		dout << endl;
	}
	dout << "#end_init" << endl;
	for (auto gl : goal){
		string pn = (gl.positive ? "+" : "-") + gl.predicate;
		assert(predicates.count(pn) != 0);
		dout << predicates[pn];
		for (string c : gl.args) dout << " " << constants[c];
		dout << endl;
	}
	dout << "#end_goal" << endl;
	dout << "#init_function_facts" << endl;
	vector<string> function_lines;
	for (auto f : init_functions){
		if (f.first.predicate == metric_target){
			cerr << "Ignoring initialisation of metric target \"" << metric_target << "\"" << endl;
			continue;
		}
		string line = to_string(function_declarations[f.first.predicate]);
		for (auto c : f.first.args) line += " " + to_string(constants[c]);
		line += " " + to_string(f.second);
		function_lines.push_back(line);
	}
	dout << function_lines.size() << endl;
	for (string l : function_lines)
		dout << l << endl;

	dout << "#initial_task" << endl;
	if (instance_is_classical) dout << "-1" << endl;
	else dout << task_id["__top"] << endl;
}


void hddl_output(ostream & dout, ostream & pout){
	set<string> neg_pred;
	for (task t : primitive_tasks) for (literal l : t.prec) if (!l.positive) neg_pred.insert(l.predicate);
	for (task t : primitive_tasks) for (conditional_effect ceff : t.ceff) for (literal l : ceff.condition) if (!l.positive) neg_pred.insert(l.predicate);
	for (auto l : goal) if (!l.positive) neg_pred.insert(l.predicate);



	dout << "(define (domain d)" << endl;
	dout << "  (:requirements :typing)" << endl;
	
	dout << endl;

	// sorts
	dout << "  (:types" << endl;
	for (auto x : sorts) dout << "    " << x.first << endl;
	dout << "  )" << endl;
	
	dout << endl;

	// predicate definitions
	dout << "  (:predicates" << endl;
	for (auto p : predicate_definitions) for (int negative = 0; negative < 2; negative++){
		if (negative && !neg_pred.count(p.name)) continue;
		dout << "    (";
		if (negative) dout << "not-";
		dout << p.name;

		// arguments
		for (size_t arg = 0; arg < p.argument_sorts.size(); arg++)
			dout << " ?var" << arg << " - " << p.argument_sorts[arg]; 
		
		dout << ")" << endl;
	}
	dout << "  )" << endl;

	dout << endl;

	bool hasActionCosts = metric_target != dummy_function_type;

	// functions (for cost expressions)
	if (parsed_functions.size() && hasActionCosts){
		dout << "  (:functions" << endl;
		for(auto f : parsed_functions){
			dout << "    (" << f.first.name;
			for (size_t arg = 0; arg < f.first.argument_sorts.size(); arg++)
				dout << " ?var" << arg << " - " << f.first.argument_sorts[arg]; 
			dout << ") - " << f.second << endl;
		}
		dout << "  )" << endl;
	}
	dout << endl;

	// abstract tasks
	for (task t : abstract_tasks){
		dout << "  (:task ";
		if (t.name[0] == '_') dout << "t";
		dout << t.name;
		dout << " :parameters (";
		bool first = true;
		for (auto v : t.vars){
			if (!first) dout << " ";
			first = false;
			dout << v.first << " - " << v.second;
		}
		dout << "))" << endl;
	}
	
	dout << endl;

	// decomposition methods
	for (method m : methods){
		dout << "  (:method ";
		if (m.name[0] == '_') dout << "m";
	   	dout << m.name << endl;
		dout << "    :parameters (";
		bool first = true;
		for (auto v : m.vars){
			if (!first) dout << " ";
			first = false;
			dout << v.first << " - " << v.second;
		}
		dout << ")" << endl;

		// AT
		dout << "    :task (";
		if (m.at[0] == '_') dout << "t";
		dout << m.at;
		for (string v : m.atargs) dout << " " << v;
		dout << ")" << endl;

		// constraints
		if (m.constraints.size()){
			dout << "    :precondition (and" << endl;
			
			for (literal l : m.constraints){
				dout << "      ";
				if (!l.positive) dout << "(not ";
				dout << "(= " << l.arguments[0] << " " << l.arguments[1] << ")";
				if (!l.positive) dout << ")";
				dout << endl;
			}
			
			dout << "    )" << endl;
		}

		// subtasks
		dout << "    :subtasks (and" << endl;
		for (plan_step ps : m.ps){
			dout << "      (x" << ps.id << " (";
			if (ps.task[0] == '_') dout << "t";
			dout << ps.task;
			for (string v : ps.args) dout << " " << v;
			dout << "))" << endl;
		}
		dout << "    )" << endl;

		
		if (m.ordering.size()){
			// ordering of subtasks
			dout << "    :ordering (and" << endl;
			for (auto o : m.ordering)
				dout << "      (x" << o.first << " < x" << o.second << ")" << endl;
			dout << "    )" << endl;
		}

		dout << "  )" << endl << endl;
	}


	// actions
	for (task t : primitive_tasks){
		dout << "  (:action ";
		if (t.name[0] == '_') dout << "t";
		dout << t.name << endl;
		dout << "    :parameters (";
		bool first = true;
		for (auto v : t.vars){
			if (!first) dout << " ";
			first = false;
			dout << v.first << " - " << v.second;
		}
		dout << ")" << endl;
		
	
		if (t.prec.size() || t.constraints.size()){
			// precondition
			dout << "    :precondition (and" << endl;
			
			for (literal l : t.constraints){
				dout << "      ";
				if (!l.positive) dout << "(not ";
				dout << "(= " << l.arguments[0] << " " << l.arguments[1] << ")";
				if (!l.positive) dout << ")";
				dout << endl;
			}
			
			for (literal l : t.prec){
				string p = (l.positive ? "" : "not-") + l.predicate;
				dout << "      (" << p;
				for (string v : l.arguments) dout << " " << v;
				dout << ")" << endl;
			}
			dout << "    )" << endl;
		}

		
		if (t.eff.size() || t.ceff.size() || 
				(hasActionCosts && t.costExpression.size())){
			// effect
			dout << "    :effect (and" << endl;

			for (literal l : t.eff){
				for (int positive = 0; positive < 2; positive ++){
					if (neg_pred.count(l.predicate) || (l.positive == positive)){
						dout << "      (";
						if (!positive) dout << "not (";
						dout << ((l.positive == positive) ? "" : "not-") << l.predicate;
						for (string v : l.arguments) dout << " " << v;
						if (!positive) dout << ")";
						dout << ")" << endl;
					}
				}
			}

			for (conditional_effect ceff : t.ceff) {
				for (int positive = 0; positive < 2; positive ++){
					if (neg_pred.count(ceff.effect.predicate) || (ceff.effect.positive == positive)){

						dout << "      (when (and";
					
						for (literal l : ceff.condition){
							dout << " (" << (l.positive ? "" : "not-") << l.predicate;
							for (string v : l.arguments) dout << " " << v;
							dout << ")";
						}

						dout << ") (";

						// actual effect
						if (!positive) dout << "not (";
						dout << ((ceff.effect.positive == positive) ? "" : "not-") << ceff.effect.predicate;
						for (string v : ceff.effect.arguments) dout << " " << v;
						if (!positive) dout << ")";

						dout << "))" << endl;
					}
				}
			}
			
			if (hasActionCosts){
				for (auto c : t.costExpression){
					dout << "      (increase (" << metric_target << ") ";
					if (c.isConstantCostExpression)
						 dout << c.costValue << ")" << endl;
					else {
						dout << "(" << c.predicate;
						for (string v : c.arguments) dout << " " << v;
						dout << ")";
					}
					dout << ")" << endl;
				}
			}
	
			dout << "    )" << endl;
		}
	
		dout << "  )" << endl << endl;
	}

	dout << ")" << endl;


	pout << "(define" << endl;
	pout << "  (problem p)" << endl;
	pout << "  (:domain d)" << endl;

	pout << "  (:objects" << endl;
		for (auto x : sorts) for (string s : x.second)
			pout << "    " << s << " - " << x.first << endl;
	pout << "  )" << endl;


	pout << "  (:htn" << endl;
	pout << "    :parameters ()" << endl;
	pout << "    :subtasks (and (t__top))" << endl;
	pout << "  )" << endl;

	pout << "  (:init" << endl;
	for (auto gl : init){
		pout << "    (" << (gl.positive ? "" : "not-") + gl.predicate;
		for (string c : gl.args) pout << " " << c;
		pout << ")" << endl;
	}

	// metric 
	if (hasActionCosts)
		for (auto f : init_functions){
			pout << "    (= (" << f.first.predicate;
			for (auto c : f.first.args) pout << " " + c;
			pout << ") " << f.second << ")" << endl;
		}

	pout << "  )" << endl;

	if (goal.size()){
		pout << "  (:goal (and" << endl;
		for (auto gl : goal){
			pout << "    (" << (gl.positive ? "" : "not-") + gl.predicate;
			for (string c : gl.args) pout << " " << c;
			pout << endl;
		}
		pout << "  )" << endl;
	}

	if (hasActionCosts)
		pout << "  (:metric minimize (" << metric_target << "))" << endl;


	pout << ")" << endl;


















}
