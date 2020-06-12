#include "hpdlWriter.hpp"
#include "parsetree.hpp"
#include "domain.hpp" // for sorts of constants
#include "cwa.hpp"
#include "orderingDecomposition.hpp"
#include <iostream>
#include <algorithm>
#include <cassert>
#include <variant>
#include <bitset>
#include <functional>

string get_hpdl_sort_name(string original_sort_name){
	// all sorts have lower case names
	transform(original_sort_name.begin(), original_sort_name.end(), original_sort_name.begin(), ::tolower);	

	// "object" denotes in HPDL the root sort of the type hierarchy. HDDL does not have a dedicated root, so we change the same of any sort "object"
	if (original_sort_name == "object")
		return "object__compiled";

	return original_sort_name;
}


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
			return varOrConst + " - " + get_hpdl_sort_name(method2TaskSort[varOrConst]);
		// write declaration
		for (auto varDecl :  m.vars->vars)
			if (varDecl.first == varOrConst){
				declared.insert(varOrConst);
		        return varOrConst + " - " + get_hpdl_sort_name(varDecl.second);
			}

		cout << "FAIL !! for " << varOrConst << endl;
		exit(1);
		return varOrConst;
    };
}

// ----------------------------------------
// Returns a predicate of the type (= ?a ?b) when ?a has been substituted for ?b
// but both are equivalent
function<string(string)> get_variable_substitution(map<string,string> method2task){
    return [method2task](string varOrConst) mutable {		
		// check if this variable is bound to a AT argument
		if (method2task.find(varOrConst) != method2task.end()) {			
			// If is not the same variable
			if (varOrConst != method2task[varOrConst]) {				
				return "(= " + varOrConst + " " + method2task[varOrConst] + ")";
			}
		}

		varOrConst.clear();
		return varOrConst;
    };
}
// ----------------------------------------


void write_HPDL_parameters(ostream & out, parsed_task & task){
	out << "    :parameters (";
	bool first = true;
	for (pair<string,string> var : task.arguments->vars){
		if (! first) out << " ";
	    first = false;	
		out << var.first << " - " << get_hpdl_sort_name(var.second);
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
				out << varDecl.first << " - " << get_hpdl_sort_name(varDecl.second);
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
		out << "(type_member_" << get_hpdl_sort_name(f->arg2) << " " << var(f->arg1) << ")";
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

void add_consts_to_set(additional_variables additionalVars, set<string> & const_set){
	for(pair<string,string> varDecl : additionalVars){
		// determine const of this sort
		assert(sorts[varDecl.second].size() == 1);
		const_set.insert(*(sorts[varDecl.second].begin()));
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


// writes the ordering of subtask in the format of HPDL
void write_hpdl_order_decomposition(ostream & dout, order_decomposition* order, map<string,sub_task*> & sub_tasks_for_id, function<string(string)> variable_declaration, int depth){
	if (!order && depth == 1) dout << "()" << endl;
	if (!order) return;

	if (depth != 1) write_HPDL_indent(dout,2+depth);
	if (order->isParallel) dout << "["; else dout << "(";
	dout << endl;

	for (variant<string,order_decomposition*> elem : order->elements){
		if (holds_alternative<string>(elem)){
			sub_task* ps = sub_tasks_for_id[get<string>(elem)];
			write_HPDL_indent(dout,3+depth);
			dout << "(" << ps->task;
			for (string & var : ps->arguments->vars) dout << " " << variable_declaration(var);
			dout << ")" << endl;
		} else
			write_hpdl_order_decomposition(dout, get<order_decomposition*>(elem), sub_tasks_for_id, variable_declaration, depth+1);
	}
	
	write_HPDL_indent(dout,2+depth);
	if (order->isParallel) dout << "]"; else dout << ")";
	dout << endl;
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

		dout << "   ";
		for (string sort : sort_def.declared_sorts){
			string output_sort = get_hpdl_sort_name(sort);
			dout << " " << output_sort;
			sorts_lhs.insert(output_sort);
		}

		if (sort_def.has_parent_sort){
			string output_sort = get_hpdl_sort_name(sort_def.parent_sort);
			dout << " - " << output_sort;
			sorts_rhs.insert(output_sort);
		} else {
			lastSorts = true;
		}

		dout << endl;
	}
	// output all sorts on the RHS, which are not on an LHS
	bool anyOutputSorts = false;
	for (string r : sorts_rhs) if (!sorts_lhs.count(r)) dout << " " << r, anyOutputSorts = true;
	if (anyOutputSorts) dout << " - object"; // output that they are children of the root-type
	dout << "  )" << endl;

	dout << endl;

	// determine which constants need to be declared in the domain
	set<string> constants_in_domain;
	for (parsed_task & at : parsed_abstract){
		if (at.name == "__top") continue; // the __top task will be written into the problem file
		for (parsed_method & method : parsed_methods[at.name]){
			// determine which variables are actually constants
			add_consts_to_set(method.newVarForAT,constants_in_domain);
			for (sub_task* st : method.tn->tasks)
				add_consts_to_set(st->arguments->newVar,constants_in_domain);
		}
	}
	// constants in primitices
	for (parsed_task prim : parsed_primitive){
		add_consts_to_set(prim.prec->variables_for_constants(),constants_in_domain);
		add_consts_to_set(prim.eff->variables_for_constants(),constants_in_domain);
	}


	pout << "(define (problem prob) (:domain dom)" << endl;

	dout << "  (:constants" << endl;
	pout << "  (:objects" << endl;
	const int MAX_OBJECTS_PER_LINE = 100;
	for (auto & s_entry : sorts){
		// don't write sorts that are artificial
		if (s_entry.first.rfind("sort_for", 0) == 0) continue;

		set<string> dconst;
		set<string> pconst;
		
		for(string constant : s_entry.second) 
			if (constants_in_domain.count(constant))
				dconst.insert(constant);
			else
				pconst.insert(constant);
			
		
		if (dconst.size()){
			dout << "   ";
			int counter = 0;
			for(string constant : dconst) {
				counter += 1;
				if (counter >= MAX_OBJECTS_PER_LINE) {
					counter = 0;
					dout << " - " << get_hpdl_sort_name(s_entry.first) << endl;
					dout << "   ";
				}
				dout << " " << constant;
			}
			if (counter) dout << " - " << get_hpdl_sort_name(s_entry.first) << endl;
		}
		if (pconst.size()){
			pout << "   ";
			int counter = 0;
			for(string constant : pconst) {
				counter += 1;
				if (counter >= MAX_OBJECTS_PER_LINE) {
					counter = 0;
					pout << " - " << get_hpdl_sort_name(s_entry.first) << endl;
					pout << "   ";
				}
				pout << " " << constant;
			}
			if (counter) pout << " - " << get_hpdl_sort_name(s_entry.first) << endl;
		}
	}
	pout << "  )" << endl << endl;
	dout << "  )" << endl << endl;

	// write the rest of the problem s.t. we can insert the content of the top method at the correct position
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
			pout << "    (type_member_" << get_hpdl_sort_name(s) << " " << constant << ")" << endl;
		}
	}
	pout << "  )" << endl << endl;
	pout << "  (:tasks-goal" << endl;





	///////////////////////////////////////////////////// Writing the main part of the domain
	if (sorts.size() > 0 || predicate_definitions.size() > 0) {
		dout << "  (:predicates" << endl;
		for(auto [s,elems] : sorts){
			(void) elems; // get rid of unused variable
			if (s.rfind("sort_for", 0) == 0) continue;
			dout << "    (type_member_" << get_hpdl_sort_name(s) << " ?var - object)" << endl;
		}
		for (predicate_definition pred_def : predicate_definitions){
			dout << "    (" << pred_def.name;
			for(unsigned int i = 0; i < pred_def.argument_sorts.size(); i++)
				dout << " ?var" << i << " - " << get_hpdl_sort_name(pred_def.argument_sorts[i]);
			dout << ")" << endl;
		}
		dout << "  )" << endl;

		dout << endl << endl;
	}

	// Creating a new task as a wrapper_compound for each primitive
	for (parsed_task prim : parsed_primitive) {
		dout << "  (:task ";
		dout << prim.name << endl;

		// Parameters -------------------------
		dout << "    :parameters (";
		bool first = true;
		for (pair<string,string> var : prim.arguments->vars){
			if (! first) dout << " ";
			first = false;	
			dout << var.first << " - object";
		}
		dout << ")" << endl;

		dout << "    (:method method1" << endl;

		// Precondition ------------------------
		dout << "      :precondition (";
		if (prim.arguments->vars.size() > 0) dout << "and";
		dout << endl;
		for (pair<string,string> & arg : prim.arguments->vars) {
			dout << "        (type_member_" << get_hpdl_sort_name(arg.second) << " " << arg.first << ")" << endl;
		}
		dout << "      )" << endl;
		
		// subtasks --------------------------
		dout << "      :tasks (" << endl;

		dout << "        (" << prim.name << "_primitive";
		for (pair<string,string> v : prim.arguments->vars) {
			dout << " " << v.first << " - " << get_hpdl_sort_name(v.second);
		}
		dout << ")" << endl;
		dout << "      )" << endl;
		dout << "    )" << endl;
		dout << "  )" << endl << endl;
	}

	dout << "; ************************************************************" << endl;
	dout << "; ************************************************************" << endl;

	// write abstract tasks
	for (parsed_task & at : parsed_abstract){
		bool top_task = at.name == "__top";

		if (!top_task){
			dout << "  (:task ";
			if (at.name[0] == '_') dout << "t";
			dout << at.name << endl;
			write_HPDL_parameters(dout,at);
		}
			
		set<string> atArgs;
		for (unsigned int i = 0; i < at.arguments->vars.size(); i++)
			atArgs.insert(at.arguments->vars[i].first);
		
		// HPDL puts methods into the abstract tasks, so output them
		for (parsed_method & method : parsed_methods[at.name]){
			// the top task will have just one!
			if (!top_task){
				dout << "    (:method ";
				if (method.name[0] == '_') dout << "t";
				dout << method.name << endl;
			}

			// determine which variables are actually constants
			map<string,string> varsForConst;
			add_var_for_const_to_map(method.newVarForAT,varsForConst);
			for (sub_task* st : method.tn->tasks)
				add_var_for_const_to_map(st->arguments->newVar,varsForConst);
			
			// the method might contain variables that have the same name as variables of the AT, we first have to rename them

			// ----------------------------------------
			set<string> varSubstituted;
			// ----------------------------------------
			map<string,string> method2Task;
			map<string,string> method2TaskSort;
			
			for (auto & varDecl : method.vars->vars)
				if (atArgs.count(varDecl.first)) {
					// ----------------------------------------
					varSubstituted.insert(varDecl.first);
					// ----------------------------------------

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
			}

			// the top tasks method does not have a precondition
			if (!top_task){
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
			}
			
		
			set<string> state_declared_variables;
			bool declareVariables = true;
			auto variable_declaration = variable_declaration_closure(method2Task,method2TaskSort,varsForConst,method,&declareVariables,state_declared_variables);
			// bind all variables
			if (!top_task) for (pair<string,string> & varDecl : method.vars->vars){
				if (varsForConst.count(varDecl.first)) continue;
				write_HPDL_indent(dout,4);
				dout << "(type_member_" << get_hpdl_sort_name(varDecl.second) << " " << variable_declaration(varDecl.first) << ")" << endl;
			}

			declareVariables = false;
			if (!top_task){
			   	write_HPDL_general_formula_outer_and(dout,method.prec,variable_declaration,4);
				write_HPDL_general_formula_outer_and(dout,method.tn->constraint,variable_declaration,4);
				// constraints!
				for (pair<string,string> v : variableConstantToCheck){
					write_HPDL_indent(dout,4);
					dout << "(= " << v.first << " " << v.second << ")" << endl;
				}
				for (pair<string,string> v : variableTypesToCheck){
					write_HPDL_indent(dout,4);
					dout << "(type_member_" << get_hpdl_sort_name(v.second) << " " << v.first << ")" << endl;
				}
				
								
				// ----------------------------------------
				auto variable_substitution = get_variable_substitution(method2Task);
				for (string var : varSubstituted) {
					string sub = variable_substitution(var);
					if (!sub.empty()) {
						write_HPDL_indent(dout,4);
						dout << variable_substitution(var) << endl;
					}
				}
				// ----------------------------------------


				dout << "      )" << endl;
			}


			// the orderings in the task network are pointers, so de-ref them
			vector<pair<string,string>> task_network_ordering;
			for (pair<string,string>* ord : method.tn->ordering)
				task_network_ordering.push_back(*ord);

			vector<string> subtask_ids;
			map<string,sub_task*> subtasks_for_id; // create a map to find tasks quicker
			for (sub_task* task : method.tn->tasks){
				subtask_ids.push_back(task->id);
				subtasks_for_id[task->id] = task;
			}

			(top_task?pout:dout) << "      :tasks ";
			// compute the order decomposition
			if (subtask_ids.size()){
				order_decomposition* order = extract_order_decomposition(task_network_ordering, subtask_ids);
				order = simplify_order_decomposition(order);

				// writesubtasks
				declareVariables = false;
				write_hpdl_order_decomposition((top_task?pout:dout),order,subtasks_for_id,variable_declaration, 1);
			} else dout << "()" << endl;
			if (!top_task) dout << "    )" << endl;
		}


		if (!top_task) dout << "  )" << endl << endl;
	}

	for (parsed_task prim : parsed_primitive){
		map<string,string> varsForConst;
		add_var_for_const_to_map(prim.prec->variables_for_constants(),varsForConst);
		add_var_for_const_to_map(prim.eff->variables_for_constants(),varsForConst);
		auto simple_variable_output = variable_output_closure(varsForConst);

		// Adding prefix "_primitive" to each primitive
		dout << "  (:action " << prim.name << "_primitive" << endl;
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



	// problem is done. Close its brackets
	pout << "  )" << endl;
	pout << ")" << endl;
}
