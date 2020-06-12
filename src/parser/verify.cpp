#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include "verify.hpp"
#include "parsetree.hpp"
#include "util.hpp"
#include "cwa.hpp"
#include "plan.hpp"

using namespace std;


inline void print_n_spaces(int n){
	for (int i = 0; i < n; i++) cout << " ";
}


string get_const(string varOrConst, map<string,string> & variableAssignment, additional_variables additionalVars = additional_variables()){
	auto it = variableAssignment.find(varOrConst);
	if (it != variableAssignment.end()) return it->second;
	// try implicit variables
	for (auto implicit_declaration : additionalVars)
		if (implicit_declaration.first == varOrConst){
			string sort = implicit_declaration.second;
			if (sorts[sort].size() != 1){
				cout << color(COLOR_RED,"Sort " + sort + " used as the sort for an implicit parameter, but this sort contains more than one constant.") << endl;
				exit(2);
			}
			return *(sorts[sort].begin());
		}
	return varOrConst; // a constant
}

bool doesFormulaContainVariable(general_formula * f, string variable, int debugMode, int level){
	if (f->type == EMPTY) return false;
	if (f->type == EQUAL || f->type == NOTEQUAL) {
		return (f->arg1 == variable) || (f->arg2 == variable);
	}
	if (f->type == OFSORT || f->type == NOTOFSORT) {
		return f->arg1 == variable;
	}

	if (f->type == AND || f->type == OR){
		for (auto sub : f->subformulae)
			if (doesFormulaContainVariable(sub,variable,debugMode,level+1)) return true;
		return false;
	}

	if (f->type == ATOM || f->type == NOTATOM){
		for (string v : f->arguments.vars){
			if (v == variable) return true;
		}
		return false;
	}

	if (f->type == FORALL || f->type == EXISTS){
		for (pair<string,string> qvar : f->qvariables.vars)
			if (qvar.first == variable){
				print_n_spaces(1+2*level+1);
				cout << color(COLOR_RED,"Quantifiying over variable that is a method parameter: ") << variable << endl;
				exit(2);
			}
		return doesFormulaContainVariable(f->subformulae[0], variable, debugMode, level+1);
	}

	if (f->type == WHEN){
		return doesFormulaContainVariable(f->subformulae[0],variable,debugMode,level+1) || doesFormulaContainVariable(f->subformulae[1],variable,debugMode,level+1);
	}

	if (f->type == VALUE || f->type == COST || f->type == COST_CHANGE){
		print_n_spaces(1+2*level+1);
		cout << color(COLOR_RED,"Found action cost in method ...") << endl;
		exit(2);
	}

	print_n_spaces(1+2*level+1);
	cout << color(COLOR_RED,"Unknown formula type .." + to_string(f->type)) << endl;
	exit(2);
	return false;	
}




// FORWARD DECLARATION -- the recursion of the next two function is intertwined
bool evaluateFormulaOnState(general_formula * f, set<ground_literal> & state, map<string,string> & variable_assignment, bool ignore_state, int debugMode, int level);

bool evaluateFormulaOnStateQuantified(formula_type type, var_declaration & qvariables, size_t done, int & instance_counter, general_formula * f, set<ground_literal> & state, map<string,string> & variable_assignment, bool ignore_state, int debugMode, int level){

	// if we have assigned all variables evaluate the sub expression
	if (qvariables.vars.size() == done){
		instance_counter++;
		if (debugMode == 2) {
			print_n_spaces(1+2*level);
			cout << color(COLOR_PURPLE,"Checking instantiation #" + to_string(instance_counter)) << endl;
		}

		bool sub_result = evaluateFormulaOnState(f,state,variable_assignment,ignore_state, debugMode, level+1);

		if (debugMode == 2) {
			print_n_spaces(1+2*level);
			cout << color(COLOR_CYAN,"Instantiation #" + to_string(instance_counter)) << " result: ";
			if (sub_result) cout << color(COLOR_GREEN,"true");
			else cout << color(COLOR_RED,"false");
			cout << endl;
		}
		
		return sub_result;
	}

	pair<string,string> var = qvariables.vars[done];

	// assign value to variable done!
	if (debugMode == 2){
		print_n_spaces(1+2*level+1);
		cout << "Var " << done << " |sort| = " << sorts[var.second].size() << endl;
	}

	for(string constant : sorts[var.second]){
		if (debugMode == 2) {
			print_n_spaces(1+2*level+1);
			cout << color(COLOR_CYAN,"Assigning") << " " << var.first << "=" << constant << endl;
		}
		variable_assignment[var.first] = constant; // add new variable binding

		bool result = evaluateFormulaOnStateQuantified(type,qvariables,done+1,instance_counter,f,state,variable_assignment,ignore_state,debugMode,level+1);

		if (debugMode == 2) {
			print_n_spaces(1+2*level+1);
			cout << color(COLOR_PURPLE,"Recursion gave result: ");
			if (result) cout << color(COLOR_GREEN,"true");
			else cout << color(COLOR_RED,"false");
			cout << endl;
		}

		if (type == FORALL && !result) {
			if (debugMode == 2) {
				print_n_spaces(1+2*level+1);
				cout << color(COLOR_RED,"Formula is now false. Aborting evaluation.") << endl;
			}
			variable_assignment.erase(var.first);
	   		return false;
		}
		if (type == EXISTS && result) {
			if (debugMode == 2) {
				print_n_spaces(1+2*level+1);
				cout << color(COLOR_GREEN,"Formula is now true. Aborting evaluation.") << endl;
			}
			variable_assignment.erase(var.first);
			return true;
		}
	}

	if (debugMode == 2) {
		print_n_spaces(1+2*level+1);
		cout << color(COLOR_PURPLE,"Checked every instantiation.") << " Formula is now: ";
		if (type == FORALL) cout << color(COLOR_GREEN,"true");
		else cout << color(COLOR_RED,"false");
		cout << endl;
	}

	variable_assignment.erase(var.first);
	return type == FORALL;
}

bool evaluateFormulaOnState(general_formula * f, set<ground_literal> & state, map<string,string> & variable_assignment, bool ignore_state, int debugMode, int level){
	if (f->type == EMPTY) {
		if (debugMode == 2) {
			print_n_spaces(1+2*level);
			cout << color(COLOR_GREEN,"EMPTY") << endl;
		}
		return true;
	}
	if (f->type == EQUAL || f->type == NOTEQUAL) {
		string a = get_const(f->arg1,variable_assignment);
		string b = get_const(f->arg2,variable_assignment);
		
		bool result = (a == b) == (f->type == EQUAL);

		if (debugMode == 2) {
			print_n_spaces(1+2*level);
			if (f->type == EQUAL) cout << color(COLOR_PURPLE,"EQUAL");
			else                  cout << color(COLOR_PURPLE,"NOT EQUAL");
			cout << " " << a << " " << b << " ";
			if (result) cout << color(COLOR_GREEN,"true");
			else cout << color(COLOR_RED,"false");
			cout << endl;
		}
		
		return result;
	}
	if (f->type == OFSORT || f->type == NOTOFSORT) {
		string a = get_const(f->arg1,variable_assignment);
		string sortname = f->arg2;
		
		bool result = sorts[sortname].count(a) == (f->type == OFSORT);
		
		if (debugMode == 2) {
			print_n_spaces(1+2*level);
			if (f->type == OFSORT) cout << color(COLOR_PURPLE,"OFSORT") << endl;
			else                   cout << color(COLOR_PURPLE,"NOT OF SORT") << endl;
			cout << a << " " << sortname << " ";
			if (result) cout << color(COLOR_GREEN,"true");
			else cout << color(COLOR_RED,"false");
			cout << endl;
		}

		return result;
	}

	if (f->type == AND || f->type == OR){
		if (debugMode == 2) {
			print_n_spaces(1+2*level);
			if (f->type == AND) cout << color(COLOR_PURPLE,"AND") << endl;
			else                cout << color(COLOR_PURPLE,"OR") << endl;
		}
		bool ok = f->type == AND;
		int i = 0;
		for (auto sub : f->subformulae){
			if (debugMode == 2) {
				print_n_spaces(1+2*level+1);
				cout << color(COLOR_PURPLE,"Checking sub-formula #" + to_string(++i)) << endl;
			}
			bool sub_result = evaluateFormulaOnState(sub,state,variable_assignment,ignore_state, debugMode, level+1);
			if (f->type == AND) ok &= sub_result;
			else ok |= sub_result;
			
			if (debugMode == 2) {
				print_n_spaces(1+2*level+1);
				cout << color(COLOR_CYAN,"Sub-formula #" + to_string(i)) << " result: ";
				if (sub_result) cout << color(COLOR_GREEN,"true");
				else cout << color(COLOR_RED,"false");
				cout << " new overall: ";
				if (ok) cout << color(COLOR_GREEN,"true");
				else cout << color(COLOR_RED,"false");
				cout << endl;
			}

			if (f->type == AND && !ok) {
				if (debugMode == 2) {
					print_n_spaces(1+2*level+1);
					cout << color(COLOR_RED,"Formula is now false. Aborting evaluation") << endl;
				}
			   	break;
			}
			if (f->type == OR && ok) {
				if (debugMode == 2) {
					print_n_spaces(1+2*level+1);
					cout << color(COLOR_GREEN,"Formula is now true. Aborting evaluation") << endl;
				}
				break;
			}
		}
		return ok;
	}

	if (f->type == ATOM || f->type == NOTATOM){
		if (ignore_state) return true;
		// construct atom

		if (debugMode == 2) {
			print_n_spaces(1+2*level);
			if (f->type == ATOM) cout << color(COLOR_PURPLE,"ATOM") << endl;
			else                 cout << color(COLOR_PURPLE,"NOTATOM") << endl;
			print_n_spaces(1+2*level+1);
			cout << "atom: " << f->predicate;
			for (string v : f->arguments.vars) cout << " " << v;
			cout << endl;
		}

		ground_literal atom;
		atom.positive = true; // even for not atom, because the state is CWA
		atom.predicate = f->predicate;
		for (string v : f->arguments.vars){
			string value = get_const(v,variable_assignment, f->arguments.newVar);
			if (debugMode == 2){
				print_n_spaces(1+2*level+1);
				cout << "Variable " << v << " evaluates to " << value << endl;
			}
			atom.args.push_back(value);
		}

		/*print_n_spaces(1+2*level+1);
		cout << "  " << (atom.positive?"+":"-") << atom.predicate;
		for (string arg : atom.args) cout << " " << arg;
			cout << endl;*/

		int stateCount = state.count(atom);
		if (debugMode == 2){
			print_n_spaces(1+2*level+1);
			cout << "State count for this atom: " << stateCount << endl;
		}

		return stateCount == (f->type == ATOM);
	}

	if (f->type == FORALL || f->type == EXISTS){
		if (debugMode == 2) {
			print_n_spaces(1+2*level);
			if (f->type == FORALL) cout << color(COLOR_PURPLE,"FORALL") << endl;
			else                   cout << color(COLOR_PURPLE,"EXISTS") << endl;
		}
		
		int instance_counter = 0;
		return evaluateFormulaOnStateQuantified(f->type, f->qvariables, 0, instance_counter, f->subformulae[0], state, variable_assignment, ignore_state, debugMode, level+1);
	}

	// things that are not allowed in preconditions
	if (f->type == WHEN){
		print_n_spaces(1+2*level+1);
		cout << color(COLOR_RED,"Found conditional effect in precondition ...") << endl;
		exit(2);
	}

	if (f->type == VALUE || f->type == COST || f->type == COST_CHANGE){
		print_n_spaces(1+2*level+1);
		cout << color(COLOR_RED,"Found action cost in precondition ...") << endl;
		exit(2);
	}

	print_n_spaces(1+2*level+1);
	cout << color(COLOR_RED,"Unknown formula type .." + to_string(f->type)) << endl;
	exit(2);
	return false;	
}

bool evaluateFormulaOnStateWithMissingAssignments(var_declaration * vars, general_formula * f, set<ground_literal> & state, map<string,string> & variable_assignment, bool ignore_state, int debugMode, int level){
	for (pair<string,string> varDecl : vars->vars){
		string sort = varDecl.second;
		if (!variable_assignment.count(varDecl.first)){
			// Domain may contain free variable ...
			if (debugMode){
				print_n_spaces(1 + 2*level);
				cout << color(COLOR_YELLOW,"Unassigned variable ") << varDecl.first << endl;
			}


			// check if the variable occrus in a method precondition, if not it is wholly irrelevant
			bool mprecontains = doesFormulaContainVariable(f,varDecl.first,debugMode,level);
			
			if (debugMode){
				// only needed for debugging
				print_n_spaces(1 + 2*level);
				cout << color(COLOR_YELLOW,"Contained in method precondition: ") << mprecontains << endl;
			}

			// we have already checked that there is at least one object in the sort
			if (!mprecontains) continue;

			for (string varVal : sorts[sort]){
				map<string,string> newVariableAssignment = variable_assignment;
				newVariableAssignment[varDecl.first] = varVal;
				if (debugMode){
					print_n_spaces(1 + 2*level+1);
					cout << color(COLOR_PURPLE,"Assigning variable ") << varDecl.first << " to " << varVal << endl;
				}

				bool recursive = evaluateFormulaOnStateWithMissingAssignments(vars, f, state, newVariableAssignment, ignore_state, debugMode, level+1);

				if (recursive) return true;
			}
			// no satisfying assignement has been found
			return false;
		}
	}
	return evaluateFormulaOnState(f, state, variable_assignment, ignore_state, debugMode, level);
}


// takes both the old and the new state, as conditional effects must be evaluated on the old state...
void executeFormulaOnState(general_formula * f, set<ground_literal> & old_state, set<ground_literal> & add, set<ground_literal> & del, map<string,string> & variable_assignment, int debugMode, int level){
	if (f->type == EMPTY) return;
	
	if (f->type == EQUAL || f->type == NOTEQUAL || f->type == OFSORT || f->type == NOTOFSORT) {
		cout << color(COLOR_RED,"Found constraint in action effect ...") << endl;
		exit(2);
	}

	if (f->type == OR){
		cout << color(COLOR_RED,"Disjunctive effect ...") << endl;
		exit(2);
	}

	if (f->type == EXISTS){
		cout << color(COLOR_RED,"Existentially quantified effect ...") << endl;
		exit(2);
	}

	if (f->type == ATOM || f->type == NOTATOM){
		// construct atom
		ground_literal atom;
		atom.positive = true; // even for not atom, because the state is CWA
		atom.predicate = f->predicate;
		for (string v : f->arguments.vars)
			atom.args.push_back(get_const(v,variable_assignment, f->arguments.newVar));

		if (f->type == ATOM) add.insert(atom);
		else del.insert(atom);
		return;
	}


	if (f->type == AND){
		for (auto sub : f->subformulae)
			executeFormulaOnState(sub,old_state, add, del, variable_assignment, debugMode, level+1);
		return;
	}

	if (f->type == FORALL){
		// compute all instantiations of the quantified variables
		vector<map<string,string> > var_replace;
		var_replace.push_back(variable_assignment);
		for(pair<string,string> var : f->qvariables.vars) {
			vector<map<string,string> > old_var_replace = var_replace;
			var_replace.clear();

			for(string constant : sorts[var.second]){
				for (map<string,string> old : old_var_replace){
					old[var.first] = constant; // add new variable binding
					var_replace.push_back(old);
				}
			}
		}
		
		for (auto new_assignments : var_replace)
			executeFormulaOnState(f->subformulae[0],old_state, add, del, new_assignments, debugMode, level+1);
		return;
	}

	if (f->type == WHEN){
		if (evaluateFormulaOnState(f->subformulae[0], old_state, variable_assignment,false, debugMode, level+1))
			executeFormulaOnState(f->subformulae[1], old_state, add, del, variable_assignment, debugMode, level+1);
		
		return;
	}

	if (f->type == VALUE || f->type == COST || f->type == COST_CHANGE){
		// cost expression, just ignore it for now ...
		return;
	}

	print_n_spaces(1+2*level+1);
	cout << color(COLOR_RED,"Unknown formula type .." + to_string(f->type)) << endl;
	exit(2);
}



void executeFormulaOnState(general_formula * f, set<ground_literal> & old_state, set<ground_literal> & new_state, map<string,string> & variable_assignment, int debugMode, int level){
	set<ground_literal> add;
	set<ground_literal> del;

	executeFormulaOnState(f,old_state,add,del,variable_assignment, debugMode, level);

	// add effects take precedence over delete effects
	for (ground_literal gl : del){
		if (debugMode) {
			print_n_spaces(1+2*level);
			cout << color(COLOR_CYAN,"Removing fact from state: ");
			cout << gl.predicate;
			for (string a : gl.args) cout << " " << a;
			cout << endl;
		}
		new_state.erase(gl);
	}
	
	for (ground_literal gl : add){
		if (debugMode) {
			print_n_spaces(1+2*level);
			cout << color(COLOR_CYAN,"Adding fact to state: ");
			cout << gl.predicate;
			for (string a : gl.args) cout << " " << a;
			cout << endl;
		}
		new_state.insert(gl);
	}
}




vector<pair<map<string,int>,map<string,string>>> generateAssignmentsDFS(parsed_method & m, map<int,instantiated_plan_step> & tasks,
							set<string> doneIDs, vector<int> & subtasks, int curpos,
							map<string,string> variableAssignment,
							map<string,int> matching,
							map<string,string> & varDecls,
							bool useOrderInformation,
							int debugMode){
	vector<pair<map<string,int>,map<string,string>>> ret;
	if (doneIDs.size() == subtasks.size()){
		for (pair<string,string> varDecl : varDecls){
			string sort = varDecl.second;
			if (!variableAssignment.count(varDecl.first)){
				// Domain may contain free variable ...
				if (debugMode){
					print_n_spaces(1 + 2*curpos);
					cout << color(COLOR_RED,"Unassigned variable ") << varDecl.first << endl;
				}


				// checking where this variable occurs. It cannot be part of the methods subtasks, so it can only occur in method preconditions and effects 
				bool mconstraintscontains = doesFormulaContainVariable(m.tn->constraint,varDecl.first,debugMode,curpos);
				bool meffcontains = doesFormulaContainVariable(m.eff,varDecl.first,debugMode,curpos);
				if (debugMode){
					// only needed for debugging
					bool mprecontains = doesFormulaContainVariable(m.prec,varDecl.first,debugMode,curpos);
					print_n_spaces(1 + 2*curpos);
					cout << color(COLOR_YELLOW,"Contained in method precondition: ") << mprecontains << endl;
					print_n_spaces(1 + 2*curpos);
					cout << color(COLOR_YELLOW,"Contained in method effect: ") << meffcontains << endl;
					print_n_spaces(1 + 2*curpos);
					cout << color(COLOR_YELLOW,"Contained in method constraints: ") << mconstraintscontains << endl;
				}

				// this variables does not have to be assigned, we can do this later on
				if (!meffcontains && !mconstraintscontains) {
					// if there is no constant for the variable ..
					if (!sorts[sort].size()) return ret;
					continue;
				} else {
					for (string varVal : sorts[sort]){
						map<string,string> newVariableAssignment = variableAssignment;
						newVariableAssignment[varDecl.first] = varVal;
						if (debugMode){
							print_n_spaces(1 + 2*curpos+1);
							cout << color(COLOR_PURPLE,"Assigning variable ") << varDecl.first << " to " << varVal << endl;
						}

						auto recursive = generateAssignmentsDFS(m, tasks, doneIDs,
											   subtasks, curpos + 1,
											   newVariableAssignment, matching,
											   varDecls, useOrderInformation, debugMode);
		
						for (auto r : recursive) ret.push_back(r);
					}
					// expanded all possible values for variable
					return ret;
				}
			}
		}


		if (debugMode) {
			print_n_spaces(1 + 2*curpos);
			cout << color(COLOR_GREEN,"Found compatible linearisation.") << endl;
			print_n_spaces(1 + 2*curpos + 1);
			cout << "Checking constants are in variable type ... " << endl;
		}
		for (pair<string,string> varDecl : varDecls){
			string sort = varDecl.second;
			if (!variableAssignment.count(varDecl.first)) continue; // this variables does not need to be assigned yet.
			string param = variableAssignment[varDecl.first];
			if (!sorts[sort].count(param)){
				if (debugMode){
					print_n_spaces(1 + 2*curpos + 1);
					cout << color(COLOR_RED,"Variable " + varDecl.first + "=" + param + " is not in sort " + sort) << endl;
				}
				return ret; // parameter is not consistent with declared sort
			}
		}
	
		set<ground_literal> empty_state;

		if (debugMode){
			print_n_spaces(1 + 2*curpos + 1);
			cout << "Checking method's constraint formula ... " << endl;
		}
		// check explicit constraints of the task network (==, !=, ofsort, notofsort)
		if (!evaluateFormulaOnState(m.tn->constraint,empty_state,variableAssignment,true, debugMode, curpos+1))
			return ret;

		if (debugMode) {
			print_n_spaces(1 + 2*curpos + 1);
			cout << "Checking variable constraints in method's precondition ... " << endl;
		}
		// check constraints hiding in the method precondition		
		if (!evaluateFormulaOnState(m.prec,empty_state,variableAssignment,true, debugMode, curpos+1))
			return ret;

		if (debugMode) {
			print_n_spaces(1 + 2*curpos + 1);
			cout << color(COLOR_GREEN,"Matching is ok.") << endl;
		}
		// found a full matching
		ret.push_back(make_pair(matching, variableAssignment));
		return ret;
	}
	
	
	// find the remaining sources
	set<string> allIDs;
	map<string,sub_task*> subtasksAccess;
	for (sub_task* st : m.tn->tasks) allIDs.insert(st->id), subtasksAccess[st->id] = st;
	for (string id : doneIDs) allIDs.erase(id);
	
	if (useOrderInformation)
		for (auto & order : m.tn->ordering){
			// this order is completely dealt with
			if (doneIDs.count(order->first)) continue;
			if (doneIDs.count(order->second)) continue;
			allIDs.erase(order->second); // has a predecessor
		}

	// now we try to map source to subtasks[curpos]
	instantiated_plan_step & ps = tasks[subtasks[curpos]];

	if (debugMode){
		print_n_spaces(1 + 2*curpos);
		cout << color(COLOR_PURPLE,"Matching Task",MODE_UNDERLINE) << " " << subtasks[curpos];
		cout << " Curpos=" << curpos << " #sources=" << allIDs.size() << endl;
		print_n_spaces(1 + 2*curpos);
		cout << "Task is: " << ps.name;
		for (string arg : ps.arguments)
			cout << " " << arg;
		cout << endl;
	}

	// allIDs now contains sources
	for (string source : allIDs){
		if (debugMode){
			print_n_spaces(1 + 2*curpos); cout << color(COLOR_PURPLE,"Attempting matching with source " + source) << endl;
		}
		
		// check whether this source is ok

		//1. has to be the same task type as declared
		if (subtasksAccess[source]->task != ps.name) {
			if (debugMode){
				print_n_spaces(1 + 2*curpos + 1); cout << "Source has task " << color(COLOR_YELLOW,subtasksAccess[source]->task) << " but next task in line has " << color(COLOR_YELLOW,ps.name) << endl;
			}
			continue;
		}

		// check that the variable assignment is compatible
		map<string,string> newVariableAssignment = variableAssignment;
		bool assignmentOk = true;
		for (unsigned int i = 0; i < ps.arguments.size(); i++){
			string methodVar = subtasksAccess[source]->arguments->vars[i];
			string taskParam = ps.arguments[i];
			if (newVariableAssignment.count(methodVar)){
				if (newVariableAssignment[methodVar] != taskParam){
					if (debugMode){
						print_n_spaces(1 + 2*curpos + 1);
						cout << "Variable " << methodVar << " has value " << newVariableAssignment[methodVar] << " but must be assigned to " << taskParam << endl;
					}
					assignmentOk = false; 
				}
			}
			string sortOfMethodVar = varDecls[methodVar];
			if (!sorts[sortOfMethodVar].count(taskParam)){
				if (debugMode){
					print_n_spaces(1 + 2*curpos + 1);
					cout << color(COLOR_RED,"Variable " + methodVar + "=" + taskParam + " is not in sort " + sortOfMethodVar) << endl;
				}
				assignmentOk = false;
			}

			newVariableAssignment[methodVar] = taskParam;
			if (debugMode){
				print_n_spaces(1 + 2*curpos + 1);
				cout << color(COLOR_CYAN,"Setting ") << methodVar << " = " << taskParam << endl;
			}
		}
		if (!assignmentOk) continue; // if the assignment is not ok then don't continue on it
		set<string> newDone = doneIDs; newDone.insert(source);
		map<string,int> newMatching = matching;
		newMatching[source] = subtasks[curpos];
		
		auto recursive = generateAssignmentsDFS(m, tasks, newDone,
							   subtasks, curpos + 1,
							   newVariableAssignment, newMatching,
							   varDecls, useOrderInformation, debugMode);

		for (auto r : recursive) ret.push_back(r);
	}
	
	return ret;
}


vector<pair<map<string,int>,map<string,string>>> generateAssignmentsDFS(parsed_method & m, map<int,instantiated_plan_step> & tasks,
							vector<int> & subtasks, int curpos,
							map<string,string> variableAssignment,
							bool useOrderInformation,
							int debugMode){
	// extract variable declaration for early abort checking

	map<string,string> varDecls;
	// check whether the variable assignment is ok
	for (pair<string,string> varDecl : m.vars->vars) varDecls[varDecl.first] = varDecl.second;
	// implicit variables (i.e. constants) declared in subtasks
	for (sub_task* st : m.tn->tasks)
		for (pair<string,string> implicit_variable : st->arguments->newVar)
			varDecls[implicit_variable.first] = implicit_variable.second;
	for (pair<string,string> varDecl : m.newVarForAT) varDecls[varDecl.first] = varDecl.second;

	map<string,int> __matching;
	set<string> done;

	return generateAssignmentsDFS(m,tasks,done,subtasks,curpos,variableAssignment,__matching,varDecls, useOrderInformation, debugMode);	

}

void getRecursive(int source, vector<int> & allSub,map<int,vector<int>> & subtasksForTask){
	if (!subtasksForTask.count(source)) {
		allSub.push_back(source);
		return;
	}
	for (int sub : subtasksForTask[source])
		getRecursive(sub,allSub,subtasksForTask);	
}


bool executeDAG(map<int,int> num_prec, map<int,vector<int>> successors, 
		set<ground_literal> & current_state,
		map<int,parsed_method> & parsedMethodForTask,
		map<int,instantiated_plan_step> & tasks,
		// method instantiation
		map<int,map<string,string>> & method_variable_values,
		// task instantiation
		map<int,map<string,string>> & task_variable_values,
		map<int,parsed_task> & taskIDToParsedTask,
		bool uniqLinearisation,
		int debugMode,
		int level){
	vector<int> sources;
	// try all sources
	bool foundNonSource = false;
	for (auto node : num_prec){
		if (node.second < 0) continue;
		if (node.second > 0) { foundNonSource = true; continue; }
		sources.push_back(node.first);
	}

	if (debugMode){
		print_n_spaces(1+2*level);
		cout << color(COLOR_BLUE,"Executing plan",MODE_BOLD) << " time=" << level << " #sources=" << sources.size() << endl;
	}

	if (!foundNonSource && sources.size() == 0){
		if (debugMode){
			print_n_spaces(1+2*level);
			cout << "Executed the whole plan ... checking whether we reached the goal state." << endl;

			print_n_spaces(1+2*level);
			cout << color(COLOR_BLUE,"The current state is:") << endl;
			for (ground_literal literal : current_state){
				print_n_spaces(1+2*level+1);
				cout << "  " << literal.predicate;
				for (string arg : literal.args)	cout << " " << arg;
				cout << endl;
			}
		}

		map<string,string> no_variables_declared;
		bool goal_reached = goal_formula == NULL || evaluateFormulaOnState(goal_formula,current_state,no_variables_declared,false, debugMode, level+1);
		if (uniqLinearisation && ! goal_reached){
			cout << color(COLOR_RED,"Primitive plan does not reach the goal state .... ") << endl;
			cout << color(COLOR_RED,"The current state is:") << endl;
			for (ground_literal literal : current_state){
				cout << "  " << color(COLOR_RED, literal.predicate);
				for (string arg : literal.args){
					cout << " " << color(COLOR_RED, arg);
				}
				cout << endl;
			}
		}
		return goal_reached;
	}

	if (sources.size() == 0){
		// this should not happen, but you never know
		cout << color(COLOR_RED,"Something went terribly wrong. A DAG has cycles .... ") << endl;
		exit(2);
	}


	// sources that are applicable method preconditions (without method effects) and dummys (everything negative can be done greedily if possible)
	vector<int> branching_sources;
	vector<int> non_applicable_sources;

	for (int source : sources){
		if (debugMode){
			print_n_spaces(1+2*level+1);
			cout << "Source " << source;
		}
		// if the source is a dummy, just progress through it
		bool non_branching_source = source < 0; // more reasons to follow;

		// if it is something (primitive for abstract/method) without an effect, check whether it is applicable
		if (!non_branching_source){
			if (parsedMethodForTask.count(source)){
				if (debugMode) cout << " is the begin of an abstract task." << endl;
				// this is an abstract task, check whether it has a method effect
				parsed_method m = parsedMethodForTask[source];
				// check its precondition
				if (debugMode){
					print_n_spaces(1+2*level+1);
					cout << "Evaluating the method precondition" << endl;
				}
				if (evaluateFormulaOnStateWithMissingAssignments(m.vars,m.prec,current_state,method_variable_values[source],false,debugMode, level+1)){
					if (m.eff->isEmpty()){
						if (debugMode){
							print_n_spaces(1+2*level+1);
							cout << "Method has no effect." << endl;
						}
						non_branching_source = true;
					} else {
						if (debugMode){
							print_n_spaces(1+2*level+1);
							cout << "Method has an effect." << endl;
						}
						branching_sources.push_back(source);
					}
				} else {
					if (debugMode){
						print_n_spaces(1+2*level+1);
						cout << "Method precondition not satisfied" << endl;
					}
					non_applicable_sources.push_back(source);
				}
			} else {
				if (debugMode) cout << " is a primitive action." << endl;
				parsed_task t = taskIDToParsedTask[source];
				// evaluate the tasks preconditions
				if (evaluateFormulaOnState(t.prec,current_state,task_variable_values[source],false,debugMode, level+1)){
					if (t.eff->isEmpty()) {
						if (debugMode){
							print_n_spaces(1+2*level+1);
							cout << "Action has no effect." << endl;
						}
						non_branching_source = true;
					} else {
						if (debugMode){
							print_n_spaces(1+2*level+1);
							cout << "Action has an effect." << endl;
						}
						branching_sources.push_back(source);
					}
				} else {
					if (debugMode){
						print_n_spaces(1+2*level+1);
						cout << "Action precondition not satisfied" << endl;
					}
					non_applicable_sources.push_back(source);
				}
			}
		} else {
			if (debugMode) cout << " is a dummy for the end of a task." << endl;
		}
		
		if (non_branching_source){
			if (debugMode){
				print_n_spaces(1+2*level+1);
				cout << "I can greedily take this source without making any mistake." << endl;
			}
			num_prec[source]--; // set to -1
			for (int succ : successors[source]) num_prec[succ]--;
			return executeDAG(num_prec,successors,current_state, parsedMethodForTask,tasks,method_variable_values,task_variable_values,taskIDToParsedTask,uniqLinearisation,debugMode,level);
		}
	}
	 
	// do this only after everything upon which we don't have to branch has been done
	if (sources.size() > 1) uniqLinearisation = false;
	if (debugMode) {
		print_n_spaces(1+2*level+1);
		cout << color(COLOR_PURPLE,"Performed all sources that can be taken greedily.");
		cout << " " << sources.size() << " sources remain. unique=" << uniqLinearisation << endl;
	}

	// error cases
	if (uniqLinearisation){
		if (branching_sources.size() == 0){
			cout << color(COLOR_RED,"Found no applicable action or method precondition, but had no choice in getting here.") << endl;
			int non_applicable_source = non_applicable_sources[0];
			cout << color(COLOR_RED,"The task with ID=" + to_string(non_applicable_source) + " is not executable in the current state.") << endl;
			cout << color(COLOR_RED,"The current state is:") << endl;
			for (ground_literal literal : current_state){
				cout << "  " << color(COLOR_RED, literal.predicate);
				for (string arg : literal.args){
					cout << " " << color(COLOR_RED, arg);
				}
				cout << endl;
			}
			return false;
		}
	
	}

	
	// try all sources
	for (int source : branching_sources){
		if (debugMode) {
			print_n_spaces(1+2*level+1);
			cout << color(COLOR_PURPLE, "Attempting to progress through source " + to_string(source)) << endl;
			instantiated_plan_step & ps = tasks[source];
			print_n_spaces(1+2*level+1);
			cout << "Task is: " << ps.name;
			for (string arg : ps.arguments)
				cout << " " << arg;
			cout << endl;
		}

		num_prec[source]--; // set to -1
		for (int succ : successors[source]) num_prec[succ]--;

		if (debugMode) {
			print_n_spaces(1+2*level+1);
			cout << "Applying effects ";
		}
		// applicability of the action was checked before, now we only have to apply it
		set<ground_literal> new_state = current_state;
		if (parsedMethodForTask.count(source)){
			if (debugMode) {
				cout << "of the method." << endl;
			}
			// this is an abstract task
			parsed_method m = parsedMethodForTask[source];
			executeFormulaOnState(m.eff,current_state,new_state,method_variable_values[source], debugMode, level+1);
		} else {
			if (debugMode) {
				cout << "of the action." << endl;
			}
			parsed_task t = taskIDToParsedTask[source];
			executeFormulaOnState(t.eff,current_state,new_state,task_variable_values[source], debugMode, level+1);
		}
		if (debugMode){
			print_n_spaces(1+2*level+1);
			cout << color(COLOR_BLUE,"The new state is:") << endl;
			for (ground_literal literal : current_state){
				print_n_spaces(1+2*level+1);
				cout << "  " << literal.predicate;
				for (string arg : literal.args)	cout << " " << arg;
				cout << endl;
			}
		}

		if (branching_sources.size() == 1){
			// if source is not branching, it is not needed any more
			current_state.clear();	
		}

		if (executeDAG(num_prec,successors,new_state, parsedMethodForTask,tasks,method_variable_values,task_variable_values,taskIDToParsedTask,uniqLinearisation,debugMode,level+(uniqLinearisation?0:1))) return true;
				
		for (int succ : successors[source]) num_prec[succ]++;
		num_prec[source]++; // set back to 0
	}

	// no source found that will give success
	return false;
}



// returns whether a linearisation of the derived task network exists that is compatible with the given primitive plan, and if so a graph representing the linearisation (for checking method preconditions).
// in this graph, each task X will be represented by two vertices: X and -X-1 for its start and end (the minus 1 is in case X==0)
pair<pair<bool,bool>,vector<pair<int,int>>> findLinearisation(int currentTask,
		map<int,parsed_method> & parsedMethodForTask,
		map<int,instantiated_plan_step> & tasks,
		map<int,vector<int>> & subtasksForTask,
		map<int,vector<pair<map<string,int>,map<string,string>>>> & matchings,
		map<int,int> pos_in_primitive_plan,
		vector<int> primitive_plan,
		// task instantiation
		map<int,map<string,string>> & task_variable_values,
		map<int,parsed_task> & taskIDToParsedTask,
		// chosen method instantiations
		map<int,map<string,string>> & chosen_method_matchings,
		// chosen non-unique matching positions
		vector<pair<int,int>> & chosen_non_unique_matchings,
		set<vector<pair<int,int>>> & forbidden_matchings,
		bool & backtrackForbidden,
		bool uniqueMatching,
		bool rootTask,
		int debugMode,
		int level){
	if (tasks[currentTask].declaredPrimitive) {
		if (debugMode){
			print_n_spaces(1+2*level);
			cout << color(COLOR_GREEN,"Primitive Task id=" + to_string(currentTask)) << endl;
		}
		vector<pair<int,int>> edge;
		edge.push_back(make_pair(currentTask, -currentTask-1)); // from start to end
		return make_pair(make_pair(true,true),edge); // order is ok
	}

	// update whether I have a unique matching, if not I can't make sensible output
	uniqueMatching &= (matchings[currentTask].size() == 1);
	if (debugMode){
		print_n_spaces(1+2*level);
		cout << color(COLOR_GREEN,"Abstract Task id=" + to_string(currentTask)) << " matching still unique: " << uniqueMatching << " (" << matchings[currentTask].size() << ")"<< endl;
	}
		
	// determine all primitive actions below the stated subtasks of this method application.
	// this is done on the IDs provided by the plan.
	map<int,vector<int>> allSubs;
	for (int st : subtasksForTask[currentTask]){
		vector<int> subs;
		getRecursive(st, subs, subtasksForTask);
		allSubs[st] = subs;
	}
	
	bool foundMatching = false;
	bool allMatchingsAreForbidden = true;

	// now try all orderings
	for (size_t matchingNr = 0; matchingNr < matchings[currentTask].size(); matchingNr++){
		auto & matching = matchings[currentTask][matchingNr];

		if (debugMode){
			print_n_spaces(1+2*level+1);
			cout << color(COLOR_PURPLE,"Attempting matching") << endl;
		}
		
		if (matchings[currentTask].size() > 1) chosen_non_unique_matchings.push_back(make_pair(currentTask,matchingNr));

		if (forbidden_matchings.count(chosen_non_unique_matchings)){
			if (debugMode){
				print_n_spaces(1+2*level+1);
				cout << color(COLOR_RED,"Matching is forbidden due to previous unsuccessful attempt.") << endl;
			}
			if (matchings[currentTask].size() > 1) chosen_non_unique_matchings.pop_back();
			continue;
		}

		allMatchingsAreForbidden = false;


		// check all orderings
		bool badOrdering = false;
		for (auto ordering : parsedMethodForTask[currentTask].tn->ordering){
			vector<int> allBefore = allSubs[matching.first[ordering->first]];
			vector<int> allAfter = allSubs[matching.first[ordering->second]];

			// check whether this ordering is adhered to for all
			for (int before : allBefore) for (int after : allAfter){
				bool primitiveBad = pos_in_primitive_plan[before] > pos_in_primitive_plan[after];
				if (primitiveBad && (uniqueMatching || debugMode)){
					if (debugMode) print_n_spaces(1+2*level+1);
					cout << color(COLOR_RED,"Task with id="+to_string(currentTask)+" has two children: " +
					to_string(matching.first[ordering->first]) + " (" + ordering->first + ") and "  +
					to_string(matching.first[ordering->second]) + " (" + ordering->second + ") who will be decomposed into primitive tasks: " + to_string(before) + " and " + to_string(after) + ". The method enforces " + to_string(before) + " < " + to_string(after) + " but the plan is ordered the other way around.") << endl;
				}

				badOrdering |= primitiveBad;
			}
		}
		if (debugMode && badOrdering){
			print_n_spaces(1+2*level+1);
			cout << color(COLOR_RED,"Bad ordering.") << endl;
		}
		if (badOrdering) {
			if (matchings[currentTask].size() > 1) chosen_non_unique_matchings.pop_back();
			continue;
		}


		// check recursively
		bool subtasksOk = true;
		vector<pair<int,int>> recursiveEdges;
		for (int st : subtasksForTask[currentTask]){
			pair<pair<bool,bool>,vector<pair<int,int>>> linearisation = findLinearisation(st,parsedMethodForTask,tasks,subtasksForTask,matchings,pos_in_primitive_plan,primitive_plan,task_variable_values,taskIDToParsedTask,chosen_method_matchings,chosen_non_unique_matchings,forbidden_matchings, backtrackForbidden, uniqueMatching,false,debugMode,level+1);
			// add all edges to result
			recursiveEdges.insert(recursiveEdges.end(),linearisation.second.begin(), linearisation.second.end());
			subtasksOk &= linearisation.first.first;
		}
		
		// if any subtask is not possible, just try the next one
		if (!subtasksOk) {
			if (matchings[currentTask].size() > 1) chosen_non_unique_matchings.pop_back();
			continue;
		}

		
		chosen_method_matchings[currentTask] = matching.second;
		
		// add edges for the inner ordering
		for (auto ordering : parsedMethodForTask[currentTask].tn->ordering){
			int before = matching.first[ordering->first];
			int after = matching.first[ordering->second];
			
			recursiveEdges.push_back(make_pair(-before-1,after)); // minus is the "end" of this 
		}

		// add edge for myself
		recursiveEdges.push_back(make_pair(currentTask, -currentTask-1)); // from start to end

		for (int st : subtasksForTask[currentTask]){
			// my start is before all by subtasks
			recursiveEdges.push_back(make_pair(currentTask, st));
			// my end is after all by subtasks
			recursiveEdges.push_back(make_pair(-st-1, -currentTask-1));
			
		}

		// if there is just one matching, we can tell the user that it is okay!
		if (uniqueMatching) foundMatching = true;

		// if I am at the root level, I have to check the linearisation now, i.e. we have to test whether it is executable.
		if (rootTask){
			if (debugMode){
				print_n_spaces(1+2*level+1);
				cout << color(COLOR_YELLOW,"Root Task, checking primitive executability ...",MODE_UNDERLINE) << endl;
			}
			// add the total order of the primitive plan
			if (primitive_plan.size())
				for (size_t i = 0; i < primitive_plan.size()-1; i++){
					// edge from the end of i to the beginning if i+1
					recursiveEdges.push_back(make_pair(-primitive_plan[i]-1, primitive_plan[i+1]));
				}

			// build helper structures for quadratic(!!) topsort. Since we have to try potentially every linearisation, this is not soo bad. Except when we are totally-ordered
			map<int,vector<int>> successors;
			map<int,int> num_prec;
			// initialise all num_precs
			for (auto & task : tasks) num_prec[task.first] = 0;

			for (auto edge : recursiveEdges){
				successors[edge.first].push_back(edge.second);
				num_prec[edge.second]++;
			}

			// if this DAG cannot be executed, just continue ...
			set<ground_literal> init_set;
			init_set.insert(init.begin(), init.end());
			if (debugMode){
				print_n_spaces(1+2*level+1);
				cout << "Running exponential top-sort." << endl;
				print_n_spaces(1+2*level+1);
				cout << color(COLOR_BLUE,"The current state is:") << endl;
				for (ground_literal literal : init_set){
					print_n_spaces(1+2*level+2);
					cout << "  " << literal.predicate;
					for (string arg : literal.args)	cout << " " << arg;
						cout << endl;
				}
			}

			if (debugMode == 2){
				print_n_spaces(1+2*level+1);
				cout << "Graph contains the following edges:" << endl;
				for (auto adj : successors){
					print_n_spaces(1+2*level+1);
					cout << adj.first << ":";
					for (int n : adj.second) cout << " " << n;
					cout << endl;
				}
			}

			if (backtrackForbidden || !executeDAG(num_prec, successors, init_set, parsedMethodForTask, tasks, chosen_method_matchings, task_variable_values, taskIDToParsedTask, uniqueMatching,debugMode,level+1)){
				if (debugMode){
					print_n_spaces(1+2*level+1);
					cout << color(COLOR_RED,"Cannot find executable linearisation.") << endl;
				}

				// look what has happened
				if (chosen_method_matchings.size() == 0){
					// this means, we had no choice in getting here
					if (debugMode){
						print_n_spaces(1+2*level+1);
						cout << color(COLOR_RED,"No further matchings are available.") << endl;
					}
					continue;
				} else {
					if (debugMode){
						print_n_spaces(1+2*level-1);
						cout << endl << color(COLOR_RED,"Forbidding deepest matching:");
						for (auto pp : chosen_non_unique_matchings)
							cout << " " << pp.first << "->" << pp.second;
						cout << endl;
					}

					forbidden_matchings.insert(chosen_non_unique_matchings);
					chosen_non_unique_matchings.clear();
					chosen_method_matchings.clear();
				
					backtrackForbidden = false;
					return findLinearisation(currentTask,parsedMethodForTask,tasks,subtasksForTask,matchings,pos_in_primitive_plan,primitive_plan,task_variable_values,taskIDToParsedTask,chosen_method_matchings,chosen_non_unique_matchings,forbidden_matchings,backtrackForbidden, true,true,debugMode,0);
				}
			}
			if (debugMode){
				print_n_spaces(1+2*level+1);
				cout << color(COLOR_GREEN,"Executable linearisation found.") << endl;
			}
		}
		
		if (debugMode){
			print_n_spaces(1+2*level+1);
			cout << color(COLOR_GREEN,"Ordering OK.") << endl;
		}
		return make_pair(make_pair(true,true),recursiveEdges);
	}

	if (allMatchingsAreForbidden && matchings[currentTask].size() > 1){
		
		for (size_t matchingNr = 0; matchingNr < matchings[currentTask].size(); matchingNr++){
			// remove all matching pertaining to the current situation
			chosen_non_unique_matchings.push_back(make_pair(currentTask,matchingNr));
			forbidden_matchings.erase(chosen_non_unique_matchings);
			chosen_non_unique_matchings.pop_back();
		}

		backtrackForbidden = true;
		return make_pair(make_pair(true,true),vector<pair<int,int>>());
	}

	return make_pair(make_pair(foundMatching, false),vector<pair<int,int>>()); // no good matching found
}


bool check_executability_of_primitive_plan(parsed_plan & plan, map<int,parsed_task> taskIDToParsedTask, map<int,map<string,string>> taskVariableValues, int debugMode){
	set<ground_literal> current_state;
	current_state.insert(init.begin(), init.end());

	for (int & primID : plan.primitive_plan){
		parsed_task t = taskIDToParsedTask[primID];
		if (debugMode){
			print_n_spaces(1);
			cout << "Evaluating precondition of task " << primID << endl;
		}

		if (!evaluateFormulaOnState(t.prec,current_state,taskVariableValues[primID],false,debugMode, 1)){
			cout << color(COLOR_RED,"Precondition not satisfied") << endl;
			return false;
		} else if (debugMode){
			print_n_spaces(1);
			cout << "Precondition of task " << primID << " is satisfied " << endl;
		}
		
		// execute action on state
		set<ground_literal> new_state = current_state;
		executeFormulaOnState(t.eff,current_state,new_state,taskVariableValues[primID], debugMode, 1);

		if (debugMode){
			print_n_spaces(1);
			cout << color(COLOR_BLUE,"The new state is:") << endl;
			for (ground_literal literal : new_state){
				print_n_spaces(1);
				cout << "  " << literal.predicate;
				for (string arg : literal.args)	cout << " " << arg;
				cout << endl;
			}
		}
		current_state = new_state;
	}
	
	
	map<string,string> no_variables_declared;
	bool goal_reached = goal_formula == NULL || evaluateFormulaOnState(goal_formula,current_state,no_variables_declared,false, debugMode, 1);

	
	
	if (!goal_reached){
		cout << color(COLOR_RED,"Primitive plan does not reach the goal state .... ") << endl;
		cout << color(COLOR_RED,"The current state is:") << endl;
		for (ground_literal literal : current_state){
			cout << "  " << color(COLOR_RED, literal.predicate);
			for (string arg : literal.args){
				cout << " " << color(COLOR_RED, arg);
			}
			cout << endl;
		}

		return false;
	}

	return true;
}





bool verify_plan(istream & plan, bool useOrderInformation, bool lenientMode, int debugMode){

	parsed_plan pplan = parse_plan(plan,debugMode);

	map<int,instantiated_plan_step> & tasks = pplan.tasks;
	vector<int> & primitive_plan = pplan.primitive_plan;
	map<int,int> & pos_in_primitive_plan = pplan.pos_in_primitive_plan;
	map<int,string> & appliedMethod = pplan.appliedMethod;
	map<int,vector<int>> & subtasksForTask = pplan.subtasksForTask;
	vector<int> & parsed_root_tasks = pplan.root_tasks;
	
	// if the input does not use the __top task, add it so that we can check the initial plan uniformly
	int root_task;
	if (parsed_root_tasks.size() > 1 || (parsed_root_tasks.size() == 1 && parsed_root_tasks[0] != -1 && tasks[parsed_root_tasks[0]].name != "__top")){
		// create a new root task
		instantiated_plan_step top;
		top.name = "__top";
		top.arguments.clear();
		top.declaredPrimitive = false;

		root_task = 0;
		// find ID that is ok
		while (tasks.count(root_task)) root_task++;
		tasks[root_task] = top;
		appliedMethod[root_task] = "__top_method";
		subtasksForTask[root_task] = parsed_root_tasks;
	} else if (parsed_root_tasks.size() == 0){
		root_task = -1;
	} else {
		root_task = parsed_root_tasks[0];
	}

	// now that we successfully got the input, we can start to run the checker
	//=========================================================================================================================
	cout << endl << "Checking the given plan ..." << endl;
	bool overallResult = true;
	//=========================================
	// check whether the subtask ids given by the user actually exist
	bool subtasksExist = true;
	for(auto subtasks : subtasksForTask){
		for (int subtask : subtasks.second){
			if (subtask == subtasks.first){
				cout << color(COLOR_RED,"Task id="+to_string(subtasks.first)+" has itself as a subtask.") << endl;
				subtasksExist = false;
			}

			if (!tasks.count(subtask)){
				cout << color(COLOR_RED,"Task id="+to_string(subtasks.first)+" has the subtask id="+to_string(subtask)+" which does not exist.") << endl;
				subtasksExist = false;
			}
		}
	}
	cout << "IDs of subtasks used in the plan exist: ";
	if (subtasksExist) cout << color(COLOR_GREEN,"true",MODE_BOLD) << endl;
	else cout << color(COLOR_RED,"false",MODE_BOLD) << endl;
	overallResult &= subtasksExist;

	//=========================================
	// check whether the individual tasks exist, and comply with their argument restrictions
	bool wrongTaskDeclarations = false;
	map<int,map<string,string>> taskVariableValues;
	map<int,parsed_task> taskIDToParsedTask;
	for (auto & entry : tasks){
		instantiated_plan_step & ps = entry.second;
		// search for the name of the plan step
		bool foundInPrimitive = true;
		parsed_task domain_task; domain_task.name = "__none_found";
		for (parsed_task prim : parsed_primitive)
			if (prim.name == ps.name)
				domain_task = prim;
		
		for (parsed_task & abstr : parsed_abstract)
			if (abstr.name == ps.name)
				domain_task = abstr, foundInPrimitive = false;

		if (domain_task.name == "__none_found"){
			cout << color(COLOR_RED,"Task with id="+to_string(entry.first)+" and task name \"" + ps.name + "\" is not declared in the domain.") << endl;
			
			if (lenientMode){
				// trying lower casing ...
				string ps_name = ps.name;
				transform(ps_name.begin(), ps_name.end(), ps_name.begin(), [](unsigned char c){ if (c == '-') return int('_'); return tolower(c); });

				for (parsed_task prim : parsed_primitive){
					string lower_c = prim.name;
					transform(lower_c.begin(), lower_c.end(), lower_c.begin(), [](unsigned char c){ if (c == '-') return int('_'); return tolower(c); });
					
					if (lower_c == ps_name){
						domain_task = prim;
						ps.name = prim.name;
					}
				}
		
				for (parsed_task & abstr : parsed_abstract){
					string lower_c = abstr.name;
					transform(lower_c.begin(), lower_c.end(), lower_c.begin(), [](unsigned char c){ if (c == '-') return int('_'); return tolower(c); });
					
					if (lower_c == ps_name)
						domain_task = abstr, ps.name = abstr.name, foundInPrimitive = false;
				}
			}

			
			if (domain_task.name == "__none_found"){
				if (lenientMode) cout << color(COLOR_RED, "Also did not find it as lower case.") << endl;
				wrongTaskDeclarations = true;
				continue;
			} else {
				cout << color(COLOR_GREEN, "Found task " + domain_task.name + " for which the given task is a lower case variant.") << endl;
			}
		}
		
		taskIDToParsedTask[entry.first] = domain_task;
		
		if (foundInPrimitive != ps.declaredPrimitive){
			string inPlanAs = ps.declaredPrimitive ? "primitive" : "abstract";
			string inDomainAs = foundInPrimitive ?   "primitive" : "abstract";
			cout << color(COLOR_RED,"Task with id="+to_string(entry.first)+" is a " + inPlanAs + " in the plan, but is declared as an " + inDomainAs + " in the domain.") << endl;
			wrongTaskDeclarations = true;
			continue;
		}

		if (domain_task.arguments->vars.size() != ps.arguments.size()){
			cout << color(COLOR_RED,"Task with id="+to_string(entry.first)+" has wrong number of arguments. "+ to_string(ps.arguments.size()) + " are given in the plan, but the domain required " + to_string(domain_task.arguments->vars.size()) + " parameters.") << endl;
			wrongTaskDeclarations = true;
			continue;
		}

		// check whether the individual arguments are ok
		for (unsigned int arg = 0; arg < ps.arguments.size(); arg++){
			string param = ps.arguments[arg];
			string argumentSort = domain_task.arguments->vars[arg].second;
			// check that the parameter is part of the variable's sort
			if (sorts[argumentSort].count(param) == 0){
				cout << color(COLOR_RED,"Task with id="+to_string(entry.first)+" has the parameter " + param + " assigned to variable " + domain_task.arguments->vars[arg].first + " of sort " + argumentSort + " - but the parameter is not a member of this sort.") << endl;


				// maybe someone used lower case?
				// try to find a constant for which this one is an lower case version ...
				string alternative_constant = "";
				bool replacement_ok = false;
				
				if (lenientMode){
					string lower_param = param;
					transform(lower_param.begin(), lower_param.end(), lower_param.begin(), [](unsigned char c){ if (c == '-') return int('_'); return tolower(c); });
					
					for (const auto & s : sorts){
						for (const string c : s.second){
							string lower_c = c;
							transform(lower_c.begin(), lower_c.end(), lower_c.begin(), [](unsigned char c){ if (c == '-') return int('_'); return tolower(c); });

							if (lower_c == lower_param){
								cout << color(COLOR_YELLOW, "Found constant " + c + " for which the parameter " +  param + " is a lower case version. I'm using this one.") << endl;
								alternative_constant = c;
								ps.arguments[arg] = c;
								break;
							}
						}
						if (alternative_constant.size()) break; // found constant
					}

					if (alternative_constant.size()){
						if (sorts[argumentSort].count(alternative_constant)){
							cout << color(COLOR_GREEN, "New constant is fine.") << endl;
							replacement_ok = true;
							ps.arguments[arg] = alternative_constant;
							param = alternative_constant; // for taskVariableValues
						} else {
							cout << color(COLOR_RED, "New constant does also not work.") << endl;
						}
					}
				}

				if (!replacement_ok) {
					wrongTaskDeclarations = true;
					continue;
				}
			}
			taskVariableValues[entry.first][domain_task.arguments->vars[arg].first] = param;
		}
	}
	cout << "Tasks declared in plan actually exist and can be instantiated as given: ";
	if (!wrongTaskDeclarations) cout << color(COLOR_GREEN,"true",MODE_BOLD) << endl;
	else cout << color(COLOR_RED,"false",MODE_BOLD) << endl;
	overallResult &= !wrongTaskDeclarations;


	// if this is a primitive plan only, check it separately
	if (root_task == -1){
		cout << color(COLOR_BLUE,"Primitive plan only (only valid if there are no method effects) ...") << endl;
		bool primitive_plan_ok = check_executability_of_primitive_plan(pplan, taskIDToParsedTask, taskVariableValues, debugMode);
		cout << "Primitive plan alone executable: ";
		if (primitive_plan_ok) cout << color(COLOR_GREEN,"true",MODE_BOLD) << endl;
		else cout << color(COLOR_RED,"false",MODE_BOLD) << endl;
		return false;
	}


	
	set<int> causedTasks;
	causedTasks.insert(root_task);
	//=========================================
	bool methodsContainDuplicates = false;
	for (auto & entry : subtasksForTask){
		for (int st : entry.second){
			causedTasks.insert(st);
			int stCount = count(entry.second.begin(), entry.second.end(), st);
			if (stCount != 1){
				cout << color(COLOR_RED,"The method decomposing the task id="+to_string(entry.first)+" contains the subtask id=" + to_string(st) + " " + to_string(stCount) + " times") << endl;
				methodsContainDuplicates = true;
			}
		}
	}
	cout << "Methods don't contain duplicate subtasks: ";
	if (!methodsContainDuplicates) cout << color(COLOR_GREEN,"true",MODE_BOLD) << endl;
	else cout << color(COLOR_RED,"false",MODE_BOLD) << endl;
	overallResult &= !methodsContainDuplicates;

	//=========================================
	// check for orphanded tasks, i.e.\ those that do not occur in methods
	bool orphanedTasks = false;
	for (auto & entry : tasks){
		if (!causedTasks.count(entry.first)){
			cout << color(COLOR_RED,"The task id="+to_string(entry.first)+" is not contained in any method and is not a root task") << endl;
			orphanedTasks = true;
		}
	}
	cout << "Methods don't contain orphaned tasks: ";
	if (!orphanedTasks) cout << color(COLOR_GREEN,"true",MODE_BOLD) << endl;
	else cout << color(COLOR_RED,"false",MODE_BOLD) << endl;
	overallResult &= !orphanedTasks;

	
	///// if the task declarations are wrong, we can't continue after this point
	if (wrongTaskDeclarations) return false;

	//=========================================
	bool wrongMethodApplication = false;
	map<int,vector<pair<map<string,int>,map<string,string>>>> possibleMethodInstantiations;
	map<int,parsed_method> parsedMethodForTask;
	for (auto & entry : appliedMethod){
		int atID = entry.first;
		instantiated_plan_step & at = tasks[atID];
		string taskName = at.name;
		// look for the applied method
		parsed_method m; m.name = "__no_method";
		for (parsed_method & mm : parsed_methods[taskName])
			if (mm.name == entry.second) // if method's name is the name of the method that was given in in the input
				m = mm;

		if (m.name == "__no_method"){
			cout << color(COLOR_RED,"Task with id="+to_string(entry.first)+" is decomposed with method \"" + entry.second + "\", but there is no such method.") << endl;
			
			if (lenientMode) {
				string entry_second = entry.second;
				transform(entry_second.begin(), entry_second.end(), entry_second.begin(), [](unsigned char c){ if (c == '-') return int('_'); return tolower(c); });
				
				for (parsed_method & mm : parsed_methods[taskName]) {
					// translate the model's name to lower case
					string lower_c = mm.name;
					transform(lower_c.begin(), lower_c.end(), lower_c.begin(), [](unsigned char c){ if (c == '-') return int('_'); return tolower(c); });
					
					if (lower_c == entry_second){
						m = mm;
						entry.second = mm.name;
					}
				}
			}

			if (m.name != "__no_method"){
				cout << color(COLOR_GREEN, "Found method " + m.name + " for which the given method is a lower case variant.") << endl;
			} else {
				if (lenientMode) cout << color(COLOR_RED, "Also did not find it as lower case.") << endl;
				wrongMethodApplication = true;
				continue;
			}
		}
		parsedMethodForTask[atID] = m;

		// the __top_method is added by the parser as an artificial top method. If the plan does not contain it, we add it manually to that we can handle everything uniformly

		map<string,string> methodParamers;

		// assert at args to variables
		for (unsigned int i = 0; i < m.atArguments.size(); i++){
			string atParam = at.arguments[i];
			string atVar = m.atArguments[i];
			if (methodParamers.count(atVar)){
				if (methodParamers[atVar] != atParam){
					cout << color(COLOR_RED,"Task with id="+to_string(entry.first)+" is instantiated with \"" + atParam + "\" as its " + to_string(i) + "th parameter, but the variable \"" + atVar + "\" is already assigned to \"" + methodParamers[atVar] + "\".") << endl;
					wrongMethodApplication = true;
				}
			}
			methodParamers[atVar] = atParam;
		}
		
		// generate all compatible assignment of given subtasks to subtasks declared in the method
		if (debugMode) cout << color(COLOR_YELLOW,"Generating Matchings for task with id="+to_string(entry.first),MODE_UNDERLINE) << endl;
		auto matchings = generateAssignmentsDFS(m, tasks, 
							   subtasksForTask[atID], 0,
							   methodParamers, useOrderInformation, debugMode);
		if (debugMode) cout << "Found " << matchings.size() << " matchings for task with id=" << entry.first << endl;

		if (matchings.size() == 0){
			if (entry.first != root_task)
				cout << color(COLOR_RED,"Task with id="+to_string(entry.first)+" has no matchings for its subtasks.") << endl;
			else
				cout << color(COLOR_RED,"Root tasks have no matchings for the initial plan.") << endl;
			wrongMethodApplication = true;
		} else if (matchings.size() > 1){
			cout << color(COLOR_YELLOW,"Task with id="+to_string(entry.first)+" has multiple valid matchings. This might be due to the domain, but it disables detailed output for checking the ordering") << endl;
		}
		
		possibleMethodInstantiations[atID] = matchings;
	}	
		
	cout << "Methods can be instantiated: ";
	if (!wrongMethodApplication) cout << color(COLOR_GREEN,"true",MODE_BOLD) << endl;
	else cout << color(COLOR_RED,"false",MODE_BOLD) << endl;
	overallResult &= !wrongMethodApplication;

	//==============================================
	// check whether the applied methods lead to the ordering enforced by the methods
	bool orderingIsConsistent = true;

	map<int,map<string,string>> chosen_method_matchings;
	vector<pair<int,int>> chosen_non_unique_matchings;
	set<vector<pair<int,int>>> forbidden_matchings;
	bool irrelevant = false;
	if (debugMode){
		cout << color(COLOR_YELLOW,"Check whether primitive plan is a linearisation of the orderings resulting from applied decomposition methods.", MODE_UNDERLINE) << endl;
	}
	pair<pair<bool,bool>,vector<pair<int,int>>> linearisation = findLinearisation(root_task,parsedMethodForTask,tasks,subtasksForTask,possibleMethodInstantiations,pos_in_primitive_plan,primitive_plan,taskVariableValues,taskIDToParsedTask,chosen_method_matchings,chosen_non_unique_matchings,forbidden_matchings,irrelevant,true,true,debugMode,0);
	if (debugMode){
		cout << "Result " << linearisation.first.first << " " << linearisation.first.second << endl;
	}
	
	if (!linearisation.first.first && !forbidden_matchings.size()){
		cout << color(COLOR_RED,"Order of applied methods is under no matching compatible with given primitive plan.") << endl;
		orderingIsConsistent = false;
	}
	
	cout << "Order induced by methods is present in plan: ";
	if (orderingIsConsistent) cout << color(COLOR_GREEN,"true",MODE_BOLD) << endl;
	else cout << color(COLOR_RED,"false",MODE_BOLD) << endl;
	overallResult &= orderingIsConsistent;



	//===============================================
	// executability is tested within finding a valid ordering
	// if the order is unique then there is actually
	bool uniqOrdering = true;
	for (auto & x : possibleMethodInstantiations) uniqOrdering &= x.second.size() == 1;

	cout << "Plan is executable: ";
	if (orderingIsConsistent && linearisation.first.second) cout << color(COLOR_GREEN,"true",MODE_BOLD);
	if (orderingIsConsistent && !linearisation.first.second){
		if (uniqOrdering) cout << color(COLOR_RED,"false",MODE_BOLD);
		else cout << color(COLOR_YELLOW,"unknown",MODE_BOLD);
	}
	if (!orderingIsConsistent) cout << color(COLOR_YELLOW,"unknown",MODE_BOLD);
	cout << endl;

	overallResult &= orderingIsConsistent && linearisation.first.second;
	return overallResult;
}

