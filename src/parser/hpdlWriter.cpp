#include "hpdlWriter.hpp"
#include "parsetree.hpp"
#include "domain.hpp" // for sorts of constants
#include "cwa.hpp"
#include <iostream>
#include <algorithm>
#include <cassert>
#include <variant>
#include <bitset>
#include <functional>


function<string(string)> variable_output_closure(map<string,string> var2const){
    return [var2const](string varOrConst) mutable { 
		// a variable that is not a parameter results from a constant being compiled away
		if (var2const.count(varOrConst)) return var2const[varOrConst];
        
		return varOrConst;
    };
}

function<string(string)> variable_declaration_closure(map<string,string> method2task, map<string,string> method2TaskSort, map<string,string> var2const, parsed_method & m, bool * declareVar, set<string> & declared){
    return [method2task,method2TaskSort,var2const,declared,m,declareVar](string varOrConst) mutable {
		if (varOrConst[0] != '?') return varOrConst;
		// check if this variable is bound to a AT argument
		if (method2task.find(varOrConst) != method2task.end())
		   varOrConst = method2task[varOrConst];
		// a variable that is not a parameter results from a constant being compiled away
		if (var2const.count(varOrConst)) return var2const[varOrConst];
		
		//if (declared.count(varOrConst)) return varOrConst;
		if (! *declareVar) return varOrConst;

		if (method2TaskSort.count(varOrConst))
			return varOrConst + " - " + method2TaskSort[varOrConst];
		// write declaration
		for (auto varDecl :  m.vars->vars)
			if (varDecl.first == varOrConst){
				declared.insert(varOrConst);
		        return varOrConst + " - " + varDecl.second;
			}

		cout << "FAIL !! for " << varOrConst << endl;
		exit(1);
		return varOrConst;
    };
}

void write_HPDL_parameters(ostream & out, parsed_task & task){
	out << "    :parameters (";
	bool first = true;
	for (pair<string,string> var : task.arguments->vars){
		if (! first) out << " ";
	    first = false;	
		out << var.first << " - " << var.second;
	}
	out << ")" << endl;
}


inline void write_HPDL_indent(ostream & out, int indent){
	for (int i = 0; i < indent; i++) out << "  ";
}

void write_HPDL_general_formula(ostream & out, general_formula * f, function<string(string)> & var, int indent){
	if (!f) return;
	if (f->type == EMPTY) return;
	
	write_HPDL_indent(out,indent);

	if (f->type == ATOM || f->type == NOTATOM){
		if (f->type == NOTATOM) out << "(not ";
		out << "(" << f->predicate;
		for (string & v : f->arguments.vars) out << " " << var(v);
		if (f->type == NOTATOM) out << ")";
		out << ")" << endl;
		return;
	}
	
	if (f->type == AND || f->type == OR ||
		f->type == FORALL || f->type == EXISTS ||
		f->type == WHEN){
		if (f->type == AND) out << "(and" << endl;
		if (f->type == OR)  out << "(or"  << endl;
		if (f->type == WHEN)  out << "(when"  << endl;
		if (f->type == FORALL) out << "(forall";
		if (f->type == EXISTS)  out << "(exists";
		
		if (f->type == FORALL || f->type == EXISTS){
			out << " (";
			int first = 0;
			for(pair<string,string> varDecl : f->qvariables.vars){
				if (first++) out << " ";
				out << varDecl.first << " - " << varDecl.second;
			}
			out << ")" << endl;
		}
		
		
		// write subformulae
		for (general_formula* s : f->subformulae) write_HPDL_general_formula(out,s,var,indent+1);

		write_HPDL_indent(out,indent);
		out << ")" << endl;
		return;
	}

	if (f->type == EQUAL || f->type == NOTEQUAL){
		if (f->type == NOTEQUAL) out << "(not ";
		out << "(= " << var(f->arg1) << " " << var(f->arg2) << ")";
		if (f->type == NOTEQUAL) out << ")";
		out << endl;
		return;
	}

	if (f->type == OFSORT || f->type == NOTOFSORT){
		if (f->type == NOTOFSORT) out << "(not ";
		out << "(type_member_" << f->arg2 << " " << var(f->arg1) << ")";
		if (f->type == NOTOFSORT) out << ")";
		out << endl;
		return;
	}

	// something occurred inside the formula that we cannot handle
	cout << "formula of type " << f->type << " occurred, which we cannot handle." << endl;
	exit(1);
}

void write_HPDL_general_formula_outer_and(ostream & out, general_formula * f, function<string(string)> & var, int indent=1){
	if (!f) return;
	if (f->type == EMPTY) return;

	if (f->type == AND){
		for (general_formula* s : f->subformulae) write_HPDL_general_formula(out,s,var, indent);
	} else {
		write_HPDL_general_formula(out,f,var,indent);
	}
}

void add_var_for_const_to_map(additional_variables additionalVars, map<string,string> & var2const){
	for(pair<string,string> varDecl : additionalVars){
		// determine const of this sort
		assert(sorts[varDecl.second].size() == 1);
		var2const[varDecl.first] = *(sorts[varDecl.second].begin());
	}
}

vector<sub_task*> get_tasks_in_total_order(vector<sub_task*> tasks, vector<pair<string,string>*> & ordering){
	// we can do this inefficiently, as methods are usually small
	vector<sub_task*> ordered_subtasks;

	map<string,int> pre; // number of predecessors
	for (pair<string,string>* p : ordering) pre[p->second]++;
	
	for (unsigned int r = 0; r < tasks.size(); r++){
		int found = -1;
		for (unsigned int i = 0; i < tasks.size(); i++){
			if (pre[tasks[i]->id] == -1) continue; // already added
			if (pre[tasks[i]->id]) continue; // still has predecessors
			if (found != -1){
				cout << "Domain contains non-totally-ordered method" << endl;
				exit(1);
			}
			found = i;
		}

		ordered_subtasks.push_back(tasks[found]);
		for (pair<string,string>*  p : ordering)
			if (p->first == tasks[found]->id)
				pre[p->second]--;
		pre[tasks[found]->id] = -1;
	}

	assert(ordered_subtasks.size() == tasks.size());
	return ordered_subtasks;
}

void write_instance_as_HPDL(ostream & dout, ostream & pout){
	dout << "(define (domain dom)" << endl;
	dout << "  (:requirements " << endl;
	dout << "    :typing" << endl;
	dout << "    :htn-expansion" << endl;
	dout << "    :negative-preconditions" << endl;
	dout << "    :conditional-effects" << endl;
	dout << "    :universal-preconditions" << endl;
	dout << "    :disjuntive-preconditions" << endl;
	dout << "    :equality" << endl;
	dout << "    :existential-preconditions" << endl;
	dout << "  )" << endl;
	dout << "  (:types " << endl;

	// the one declaring only elementary types will be the last one
	set<string> sorts_rhs;
	set<string> sorts_lhs;
	bool lastSorts = false;
	for (sort_definition sort_def : sort_definitions){
		assert(!lastSorts); // only one sort definition without parents

		string _s = *(sort_def.declared_sorts.begin()); transform(_s.begin(), _s.end(), _s.begin(), ::tolower);

		if (sort_def.declared_sorts.size() == 1 && _s == "object"){
			sorts_rhs.insert(sort_def.parent_sort);
			continue; // ignore the rest of this definition
		}

		_s = sort_def.parent_sort; transform(_s.begin(), _s.end(), _s.begin(), ::tolower);

		if (sort_def.has_parent_sort && _s == "object"){
			for (string sort : sort_def.declared_sorts)
				sorts_rhs.insert(sort);
			continue;
		}

		dout << "   ";
		for (string sort : sort_def.declared_sorts){
			_s = sort; transform(_s.begin(), _s.end(), _s.begin(), ::tolower);
			if (_s != "object") // it is a build-in in HPDL 
				dout << " " << sort, sorts_lhs.insert(sort);
		}

		if (sort_def.has_parent_sort)
			dout << " - " << sort_def.parent_sort, sorts_rhs.insert(sort_def.parent_sort);
		else {
			lastSorts = true;
		}

		dout << endl;
	}
	// output all sorts on the RHS, which are not on an LHS
	for (string r : sorts_rhs) if (!sorts_lhs.count(r)) dout << " " << r;
	dout << "  )" << endl;

	dout << endl;
	dout << "  (:constants" << endl;
	for (auto & s_entry : sorts){
		if (s_entry.first.rfind("sort_for", 0) == 0) continue;
		if (!s_entry.second.size()) continue;
			
		dout << "   ";
		for(string constant : s_entry.second) dout << " " << constant;
		dout << " - " << s_entry.first << endl;
	}
	dout << "  )" << endl << endl;


	dout << "  (:predicates" << endl;
	for(auto [s,elems] : sorts){
		(void) elems; // get rid of unused variable
		if (s.rfind("sort_for", 0) == 0) continue;
		dout << "    (type_member_" << s << " ?var - " << s << ")" << endl;
	}
	for (predicate_definition pred_def : predicate_definitions){
		dout << "    (" << pred_def.name;
		for(unsigned int i = 0; i < pred_def.argument_sorts.size(); i++)
			dout << " ?var" << i << " - " << pred_def.argument_sorts[i];
		dout << ")" << endl;
	}
	dout << "  )" << endl;

	dout << endl << endl;

	//set<string> sortsWithDeclaredPredicates;

	// write abstract tasks
	for (parsed_task & at : parsed_abstract){
		dout << "  (:task ";
		if (at.name[0] == '_') dout << "t";
		dout << at.name << endl;
		write_HPDL_parameters(dout,at);
			
		set<string> atArgs;
		for (unsigned int i = 0; i < at.arguments->vars.size(); i++)
			atArgs.insert(at.arguments->vars[i].first);
		
		// HPDL puts methods into the abstract tasks, so output them
		for (parsed_method & method : parsed_methods[at.name]){
			dout << "    (:method ";
			if (method.name[0] == '_') dout << "t";
			dout << method.name << endl;

			// determine which variables are actually constants
			map<string,string> varsForConst;
			add_var_for_const_to_map(method.newVarForAT,varsForConst);
			for (sub_task* st : method.tn->tasks)
				add_var_for_const_to_map(st->arguments->newVar,varsForConst);
			
			// the method might contain variables that have the same name as variables of the AT, we first have to rename them

			map<string,string> method2Task;
			map<string,string> method2TaskSort;
			
			for (auto & varDecl : method.vars->vars)
				if (atArgs.count(varDecl.first)) {
					method2Task[varDecl.first] = varDecl.first + "_in_method";
					method2TaskSort[method2Task[varDecl.first]] = varDecl.second;
				}


			vector<pair<string,string>> variableTypesToCheck;
			vector<pair<string,string>> variableConstantToCheck;
			for (unsigned int i = 0; i < method.atArguments.size(); i++){
				string methodArg = method.atArguments[i];
				string atArg = at.arguments->vars[i].first;
				method2Task[methodArg] = atArg;

				// the new variable may have another type than the old one, in this case we have to write a constraint into the precondition
				string sortOfAT = at.arguments->vars[i].second;
				method2TaskSort[atArg] = sortOfAT;
				
				if (varsForConst.count(methodArg)){
					// we are dealing with an artificial parameter, i.e. a constant.
					variableConstantToCheck.push_back(make_pair(atArg,varsForConst[methodArg]));
					continue;
				}
				
				// iterate over variables in method to find the correct one
				string sortOfParam = "";
				for (unsigned int j = 0; j < method.vars->vars.size(); j++)
					if (method.vars->vars[j].first == methodArg)
						sortOfParam = method.vars->vars[j].second;
				assert(sortOfParam.size()); // must be found
				if (sortOfAT == sortOfParam) continue;
				variableTypesToCheck.push_back(make_pair(atArg,sortOfParam));
				//sortsWithDeclaredPredicates.insert(sortOfParam);
			}



			// preconditions
			dout << "      :precondition (";
			bool variableToBind = false;
			for (pair<string,string> & varDecl : method.vars->vars){
				if (varsForConst.count(varDecl.first)) continue;
				variableToBind = true; break;
			}
			
			if ((method.prec != NULL && method.prec->type != EMPTY) ||
				(method.tn->constraint != NULL && method.tn->constraint->type != EMPTY) ||
			  	variableToBind || variableConstantToCheck.size() ||
				variableTypesToCheck.size() ) dout << "and";
			dout << endl;
			
		
			set<string> state_declared_variables;
			bool declareVariables = true;
			auto variable_declaration = variable_declaration_closure(method2Task,method2TaskSort,varsForConst,method,&declareVariables,state_declared_variables);
			// bind all variables
			for (pair<string,string> & varDecl : method.vars->vars){
				if (varsForConst.count(varDecl.first)) continue;
				write_HPDL_indent(dout,4);
				//sortsWithDeclaredPredicates.insert(varDecl.second);
				dout << "(type_member_" << varDecl.second << " " << variable_declaration(varDecl.first) << ")" << endl;
			}

			declareVariables = false;
			write_HPDL_general_formula_outer_and(dout,method.prec,variable_declaration,4);
			write_HPDL_general_formula_outer_and(dout,method.tn->constraint,variable_declaration,4);
			for (pair<string,string> v : variableConstantToCheck){
				write_HPDL_indent(dout,4);
				dout << "(= " << v.first << " " << v.second << ")" << endl;
			}
			for (pair<string,string> v : variableTypesToCheck){
				write_HPDL_indent(dout,4);
				dout << "(type_member_" << v.second << " " << v.first << ")" << endl;
			}
			// constraints!
			dout << "      )" << endl;

			// subtasks
			declareVariables = false;
			dout << "      :tasks (" << endl;
			for (sub_task* t : get_tasks_in_total_order(method.tn->tasks,method.tn->ordering)){
				dout << "        (" << t->task;
				for (string & var : t->arguments->vars)
					dout << " " << variable_declaration(var);
				dout << ")" << endl;
			}
			dout << "      )" << endl;

			dout << "    )" << endl;
		}


		dout << "  )" << endl << endl;
	}

	for (parsed_task prim : parsed_primitive){
		map<string,string> varsForConst;
		auto simple_variable_output = variable_output_closure(varsForConst);
		add_var_for_const_to_map(prim.prec->variables_for_constants(),varsForConst);
		add_var_for_const_to_map(prim.eff->variables_for_constants(),varsForConst);

		dout << "  (:action " << prim.name << endl;
		write_HPDL_parameters(dout,prim);
		// preconditions
		dout << "    :precondition (";
		if (prim.prec != NULL && prim.prec->type != EMPTY) dout << "and";
		dout << endl;
		write_HPDL_general_formula_outer_and(dout,prim.prec,simple_variable_output ,3);
		dout << "    )" << endl;
		// effects
		dout << "    :effect (";
		if (prim.prec != NULL && prim.eff->type != EMPTY) dout << "and";
		dout << endl;
		write_HPDL_general_formula_outer_and(dout,prim.eff,simple_variable_output, 3);
		dout << "    )" << endl;


		dout << "  )" << endl << endl;
	}
	
	dout << ")" << endl;



	pout << "(define (problem prob) (:domain dom)" << endl;

	pout << "  (:init" << endl;
	for (ground_literal & lit : init){
		if (!lit.positive) continue;
		pout << "    (" << lit.predicate;
		for (string & arg : lit.args)
			pout << " " << arg;
		pout << ")" << endl;
	}
	// expand sorts before writing the sort membership information to keep them correct
	expand_sorts(); // add constants to all sorts
	for(auto [s, elems] : sorts){
		(void) elems; // get rid of unused variable
		if (s.rfind("sort_for", 0) == 0) continue;
		for (string constant : sorts[s]){
			pout << "    (type_member_" << s << " " << constant << ")" << endl;
		}
	}
	pout << "  )" << endl << endl;

	pout << "  (:tasks-goal" << endl << "    :tasks (" << endl;
	pout << "      (t__top)" << endl;
	pout << "    )" << endl;
	pout << "  )" << endl;
	pout << ")" << endl;
}
