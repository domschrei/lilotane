#include "domain.hpp"
#include "parsetree.hpp"
#include "util.hpp"
#include "cwa.hpp"
#include <iostream>
#include <cassert>

using namespace std;

void flatten_tasks(){
	for(parsed_task a : parsed_primitive){
		// expand the effects first, as they will contain only one element
		vector<pair<pair<vector<literal>,vector<literal> >, additional_variables> > elist = a.eff->expand();
		
		vector<pair<pair<vector<literal>, vector<literal> >, additional_variables> > plist = a.prec->expand();
		int i = 0;
		for(auto e : elist) for(auto p : plist){
			assert(p.first.second.size() == 0); // precondition cannot contain conditional effects
			task t; i++;
			t.name = a.name;
			// sort out the constraints
			for(literal pl : p.first.first)
				if (pl.predicate == dummy_equal_literal)
					t.constraints.push_back(pl);
				else
					t.prec.push_back(pl);

			// add preconditions for conditional effects
			for(literal ep : e.first.second){
				assert(ep.predicate != dummy_equal_literal);
				t.prec.push_back(ep);
			}

			// set effects
			for (auto eff : e.first.first){
				if (eff.isCostChangeExpression)
					t.costExpression.push_back(eff);
				else
					t.eff.push_back(eff);
			}

			// add declared vars
			t.vars = a.arguments->vars;
			// gather the additional variables
			additional_variables addVars = p.second;
			for (auto elem : e.second) addVars.insert(elem);
			for (auto v : addVars) {
				// check whether this variable actually occurs anywhere
				bool contained = false;
				for (auto pre : t.prec) for (auto arg : pre.arguments) contained |= v.first == arg;
				for (auto eff : t.eff) for (auto arg : eff.arguments) contained |= v.first == arg;

				if (!contained) continue;
				t.vars.push_back(v);
			}
			
			if (plist.size() > 1 || elist.size() > 1) {
				t.name += "_instance_" + to_string(i);
				// we have to create a new decomposition method at this point
				method m;
				m.name = "_method_for_multiple_expansions_of_" + t.name; // must start with an underscore s.t. this method is applied by the solution compiler
				m.at = a.name;
				for(auto v : a.arguments->vars) m.atargs.push_back(v.first);
				m.vars = t.vars;
				plan_step ps;
				ps.task = t.name;
				ps.id = "id0";
				for(auto v : m.vars) ps.args.push_back(v.first);
				m.ps.push_back(ps);

				methods.push_back(m);
				// for this to be ok, we have to create an abstract task in the first round
				if (i == 1){
					task at;
					at.name = a.name;
					at.vars = a.arguments->vars;
					abstract_tasks.push_back(at);
				}
			}
			
			// add to list
			t.check_integrity();
			primitive_tasks.push_back(t);	
		}
	}
	for(parsed_task a : parsed_abstract){
		task at;
		at.name = a.name;
		at.vars = a.arguments->vars;
		abstract_tasks.push_back(at);
	}
	for(task t : primitive_tasks) task_name_map[t.name] = t;
	for(task t : abstract_tasks) task_name_map[t.name] = t;
}

void parsed_method_to_data_structures(){
	int i = 0;
	for (auto e : parsed_methods) for (parsed_method pm : e.second) {
		// compute flattening of method precondition
		vector<pair<pair<vector<literal>, vector<literal> >, additional_variables> > precs = pm.prec->expand();
		for (auto prec : precs){
			method m; i++;
			m.name = pm.name;
			// method names may not be changed for verification
			//if (precs.size() > 1) m.name += "_" + to_string(i);
			// collect all the variable
			m.vars = pm.vars->vars;

			
			// add variables needed due to constants in the arguments of the abstract task
			map<string,string> at_arg_additional_vars;
			for (pair<string, string> av : pm.newVarForAT){
				at_arg_additional_vars[av.first] = av.first + "_" + to_string(i++);
				m.vars.push_back(make_pair(at_arg_additional_vars[av.first],av.second));
			}
			// compute arguments of the abstract task
			m.at = e.first;
			m.atargs.clear();
			for (string arg : pm.atArguments){
				if (at_arg_additional_vars.count(arg))
					m.atargs.push_back(at_arg_additional_vars[arg]);
				else
					m.atargs.push_back(arg);
			}
			
			// variables from precondition
			map<string,string> mprec_additional_vars;
			for (pair<string, string> av : prec.second){
				mprec_additional_vars[av.first] = av.first + "_" + to_string(i++);
				m.vars.push_back(make_pair(mprec_additional_vars[av.first],av.second));
			}

			// subtasks
			for(sub_task* st : pm.tn->tasks){
				plan_step ps;
				ps.id = st->id;
				ps.task = st->task;
				map<string,string> arg_replace;
				// add new variables for artificial variables of the subtask
				for (auto av : st->arguments->newVar){
					arg_replace[av.first] = av.first + "_" + to_string(i++);
					m.vars.push_back(make_pair(arg_replace[av.first],av.second));
				}
				for (string v : st->arguments->vars)
					if (arg_replace.count(v))
						ps.args.push_back(arg_replace[v]);
					else
						ps.args.push_back(v);

				// we might have added more parameters to these tasks to account for constants in them. We have to add them here
				task psTask = task_name_map[ps.task];
				if (psTask.name != ps.task){
					cerr << "There is no declaration of the subtask " << ps.task << " in the input" << endl;
				}
				assert(psTask.name == ps.task); // ensure that we have found one
				for (unsigned int j = st->arguments->vars.size(); j < psTask.vars.size(); j++){
					string v = psTask.vars[j].first + "_method_" + m.name + "_instance_" + to_string(i++);
					m.vars.push_back(make_pair(v,psTask.vars[j].second)); // add var to set of vars
					ps.args.push_back(v);
				}

				m.ps.push_back(ps);
			}

			// add a subtask for the method precondition
			vector<literal> mPrec;
			for (literal l : prec.first.first)
				if (l.predicate == dummy_equal_literal || l.predicate == dummy_ofsort_literal)
					m.constraints.push_back(l);
				else mPrec.push_back(l);

			map<string,string> sorts_of_vars; for(auto pp : m.vars) sorts_of_vars[pp.first] = pp.second;

			// actually have method precondition
			if (mPrec.size()){
				task t;
				t.name = method_precondition_action_name + m.name;
				t.prec = mPrec;
				set<string> args;
				for (literal l : t.prec) for (string a : l.arguments) args.insert(a);
				// get types of vars
				for (string v : args) {
					string accessV = v;
					if (mprec_additional_vars.count(v)) accessV = mprec_additional_vars[v];
					t.vars.push_back(make_pair(v,sorts_of_vars[accessV]));
				}
				// add t as a new primitive task
				t.check_integrity();
				primitive_tasks.push_back(t);
				task_name_map[t.name] = t;
				
				plan_step ps;
				ps.task = t.name;
				ps.id = "__m-prec-id";
				for(auto v : t.vars) {
					string arg = v.first;
					if (mprec_additional_vars.count(arg)) arg = mprec_additional_vars[arg];
					ps.args.push_back(arg);
				}

				// precondition ps precedes all other tasks
				for (plan_step other_ps : m.ps)
					m.ordering.push_back(make_pair(ps.id,other_ps.id));
				m.ps.push_back(ps);
			}

			// ordering
			for(auto o : pm.tn->ordering) m.ordering.push_back(*o);

			// constraints
			vector<pair<pair<vector<literal>, vector<literal> >, additional_variables> > exconstraints = pm.tn->constraint->expand();
			assert(exconstraints.size() == 1);
			assert(exconstraints[0].second.size() == 0); // no additional vars due to constraints
			assert(exconstraints[0].first.second.size() == 0); // no conditional effects
			for(literal l : exconstraints[0].first.first) m.constraints.push_back(l);

			// remove sort constraints
			vector<literal> oldC = m.constraints;
			m.constraints.clear();
			for(literal l : oldC) if (l.predicate == dummy_equal_literal) m.constraints.push_back(l); else {
				// determine the now forbidden variables
				set<string> currentVals = sorts[sorts_of_vars[l.arguments[0]]];
				set<string> sortElems = sorts[l.arguments[1]];
				vector<string> forbidden;
				if (l.positive) {
					for (string s : currentVals) if (sortElems.count(s) == 0) forbidden.push_back(s);
				} else
					for (string s : sortElems) forbidden.push_back(s);
				for(string f : forbidden){
					literal nl;
					nl.positive = false;
					nl.predicate = dummy_equal_literal;
					nl.arguments.push_back(l.arguments[0]);
					nl.arguments.push_back(f);
					m.constraints.push_back(nl);
				}
			}

			m.check_integrity();
			methods.push_back(m);
		}
	}
}

void reduce_constraints(){
	int ns_count = 0;
	vector<method> oldm = methods;
	methods.clear();
	for (method m : oldm){
		method nm = m;
		vector<literal> oldC = nm.constraints;
		nm.constraints.clear();

		map<string,string> vSort; for (auto x : m.vars) vSort[x.first] = x.second;
		for (literal l : oldC){
			if (l.arguments[1][0] == '?'){
				nm.constraints.push_back(l);
				continue;
			}

			string v = l.arguments[0];
			string c = l.arguments[1];

			// v != c and c is not in the sort of v
			if (!l.positive && !sorts[vSort[v]].count(c)) continue;
			// recompute sort
			set<string> vals = sorts[vSort[v]];
			if (l.positive) {
				if (vals.count(c)) {
					vals.clear();
					vals.insert(c);
				} else vals.clear();
			} else vals.erase(c);
			if (vals != sorts[vSort[v]]){
				string ns = vSort[v] + "_constraint_propagated_" + to_string(++ns_count);
				vSort[v] = ns;
				sorts[ns] = vals;
			}
		}
		// rebuild args
		vector<pair<string,string>>	nvar;
		for (auto x : nm.vars) nvar.push_back(make_pair(x.first,vSort[x.first]));
		nm.vars = nvar;
		methods.push_back(nm);
	}
	
	vector<task> oldt = primitive_tasks;
	primitive_tasks.clear();
	for (task t : oldt){
		task nt = t;
		vector<literal> oldC = t.constraints;
		nt.constraints.clear();

		map<string,string> vSort; for (auto x : t.vars) vSort[x.first] = x.second;
		for (literal l : oldC){
			if (l.arguments[1][0] == '?'){
				nt.constraints.push_back(l);
				continue;
			}

			string v = l.arguments[0];
			string c = l.arguments[1];

			// v != c and c is not in the sort of v
			if (!l.positive && !sorts[vSort[v]].count(c)) continue;
			// recompute sort
			set<string> vals = sorts[vSort[v]];
			if (l.positive) {
				if (vals.count(c)) {
					vals.clear();
					vals.insert(c);
				} else vals.clear();
			} else vals.erase(c);
			if (vals != sorts[vSort[v]]){
				string ns = vSort[v] + "_constraint_propagated_" + to_string(++ns_count);
				vSort[v] = ns;
				sorts[ns] = vals;
			}
		}
		// rebuild args
		vector<pair<string,string>>	nvar;
		for (auto x : nt.vars) nvar.push_back(make_pair(x.first,vSort[x.first]));
		nt.vars = nvar;
		primitive_tasks.push_back(nt);
	}
}

void clean_up_sorts(){
	// reduce the number of sorts
	map<set<string>,string> elems_to_sort;
	
	vector<task> oldt = primitive_tasks;
	primitive_tasks.clear();
	for (task t : oldt){
		task nt = t;
		vector<pair<string,string>>	nvar;
		for (auto x : nt.vars) {
			set<string> elems = sorts[x.second];
			if (!elems_to_sort.count(elems))
				elems_to_sort[elems] = x.second;
			nvar.push_back(make_pair(x.first,elems_to_sort[elems]));
		}
		nt.vars = nvar;
		nt.check_integrity();
		primitive_tasks.push_back(nt);
	}

	vector<task> olda = abstract_tasks;
	abstract_tasks.clear();
	for (task t : olda){
		task nt = t;
		vector<pair<string,string>>	nvar;
		for (auto x : nt.vars) {
			set<string> elems = sorts[x.second];
			if (!elems_to_sort.count(elems))
				elems_to_sort[elems] = x.second;
			nvar.push_back(make_pair(x.first,elems_to_sort[elems]));
		}
		nt.vars = nvar;
		nt.check_integrity();
		abstract_tasks.push_back(nt);
	}
	
	vector<method> oldm = methods;
	methods.clear();
	for (method m : oldm){
		method nm = m;
		vector<pair<string,string>>	nvar;
		for (auto x : nm.vars) {
			set<string> elems = sorts[x.second];
			if (!elems_to_sort.count(elems))
				elems_to_sort[elems] = x.second;
			nvar.push_back(make_pair(x.first,elems_to_sort[elems]));
		}
		nm.vars = nvar;
		nm.check_integrity();
		methods.push_back(nm);
	}

	vector<predicate_definition> opred = predicate_definitions;
	predicate_definitions.clear();
	for (auto p : opred){
		predicate_definition np = p;
		vector<string>	nvar;
		for (auto x : np.argument_sorts) {
			set<string> elems = sorts[x];
			if (!elems_to_sort.count(elems))
				elems_to_sort[elems] = x;
			nvar.push_back(elems_to_sort[elems]);
		}
		np.argument_sorts = nvar;
		predicate_definitions.push_back(np);
	}

	sorts.clear();
	for (auto x : elems_to_sort)
		sorts[x.second] = x.first;
}

void remove_unnecessary_predicates(){
	set<string> occuring_preds;
	for (task t : primitive_tasks) for (literal l : t.prec) occuring_preds.insert(l.predicate);
	for (ground_literal gl : goal) occuring_preds.insert(gl.predicate);

	vector<predicate_definition> old = predicate_definitions;
	predicate_definitions.clear();


	// find predicates that occur only in effects
	set<string> removed_predicates;
	for (predicate_definition p : old){
		if (occuring_preds.count(p.name)){
			predicate_definitions.push_back(p);
		} else
			removed_predicates.insert(p.name);
	}

	// remove these from primitive tasks and the goal
	vector<task> oldt = primitive_tasks;
	primitive_tasks.clear();
	for (task t : oldt){
		task nt = t;
		vector<literal> np;
		// filter effects
		for (literal l: nt.eff)
			if (!removed_predicates.count(l.predicate))
				np.push_back(l);
		nt.eff = np;
		nt.check_integrity();
		primitive_tasks.push_back(nt);
	}

	// remove useless predicates from init
	vector<ground_literal> ni;
	for (ground_literal l : init)
		if (!removed_predicates.count(l.predicate))
			ni.push_back(l);
	init = ni;
	
	// remove useless predicates from goal
	vector<ground_literal> ng;
	for (ground_literal l : goal)
		if (!removed_predicates.count(l.predicate))
			ng.push_back(l);
	goal = ng;
}

void task::check_integrity(){
	for(auto v : this->vars)
		assert(v.second.size() != 0); // variables must have a sort
	
	for(literal l : this->prec) {
		bool hasPred = false;
		for(auto p : predicate_definitions) if (p.name == l.predicate) hasPred = true;
		if (! hasPred){
			cerr << "Task " << this->name << " has the predicate \"" << l.predicate << "\" in its precondition, which is not declared." << endl;
			assert(hasPred);
		}
		
		for(string v : l.arguments) {
			bool hasVar = false;
			for(auto mv : this->vars) if (mv.first == v) hasVar = true;
			if (! hasVar){
				cerr << "Task " << this->name << " has the predicate \"" << l.predicate << "\" in its precondition, which has the argument \"" << v << "\", which is unknown." << endl;
				assert(hasVar);
			}
		}
	}

	for(literal l : this->eff) {
		bool hasPred = false;
		for(auto p : predicate_definitions) if (p.name == l.predicate) hasPred = true;
		if (! hasPred){
			cerr << "Task " << this->name << " has the predicate \"" << l.predicate << "\" in its effects, which is not declared." << endl;
			assert(hasPred);
		}
		
		for(string v : l.arguments) {
			bool hasVar = false;
			for(auto mv : this->vars) if (mv.first == v) hasVar = true;
			if (! hasVar){
				cerr << "Task " << this->name << " has the predicate \"" << l.predicate << "\" in its effects, which has the argument \"" << v << "\", which is unknown." << endl;
				assert(hasVar);
			}
		}
	}

}

void method::check_integrity(){
	set<string> varnames;
	for (auto v : vars) varnames.insert(v.first);
	assert(varnames.size() == vars.size());
	
	for (plan_step ps : this->ps){
		task t = task_name_map[ps.task];
		assert(ps.args.size() == t.vars.size());

		for (string v : ps.args) {
			bool found = false;
			for (auto vd : this->vars) found |= vd.first == v;
			assert(found);
		}
	}
}
