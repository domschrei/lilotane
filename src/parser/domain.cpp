#include "domain.hpp"
#include "parsetree.hpp"
#include "util.hpp"
#include "cwa.hpp"
#include <iostream>
#include <cassert>

using namespace std;

void flatten_tasks(bool compileConditionalEffects){
	for(parsed_task a : parsed_primitive){
		// expand the effects first, as they will contain only one element
		vector<pair<pair<vector<variant<literal,conditional_effect>>,vector<literal> >, additional_variables> > elist = a.eff->expand(compileConditionalEffects);

		vector<pair<pair<vector<variant<literal,conditional_effect>>, vector<literal> >, additional_variables> > plist = a.prec->expand(false); // precondition cannot contain conditional effects
		int i = 0;
		for(auto e : elist) for(auto p : plist){
			assert(p.first.second.size() == 0); // precondition cannot contain conditional effects
			task t; i++;
			t.name = a.name;
			// sort out the constraints
			for(variant<literal,conditional_effect> pl : p.first.first){
				if (!holds_alternative<literal>(pl)) assert(false); // precondition cannot have conditional effects in it
				if (get<literal>(pl).predicate == dummy_equal_literal)
					t.constraints.push_back(get<literal>(pl));
				else
					t.prec.push_back(get<literal>(pl));
			}

			// add preconditions for conditional effects
			for(literal ep : e.first.second){
				assert(ep.predicate != dummy_equal_literal);
				t.prec.push_back(ep);
			}

			// set effects
			for (auto eff : e.first.first){
				if (holds_alternative<literal>(eff)){
					if (get<literal>(eff).isCostChangeExpression)
						t.costExpression.push_back(get<literal>(eff));
					else
						t.eff.push_back(get<literal>(eff));
				} else {
					if (get<conditional_effect>(eff).effect.isCostChangeExpression)
						assert(false); // TODO state dependent action costs. This is very complicated. Ask Robert Mattmüller how to do it.
					// conditional effect
					t.ceff.push_back(get<conditional_effect>(eff));
				}
			}

			// add declared vars
			t.vars = a.arguments->vars;
			t.number_of_original_vars = t.vars.size(); // the parsed variables were the original ones
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
				t.name += "|instance_" + to_string(i);
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
					at.number_of_original_vars = at.vars.size();
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
		at.number_of_original_vars = at.vars.size();
		// abstract tasks cannot have additional variables (e.g. for constants): these cannot be declared in the input
		abstract_tasks.push_back(at);
	}
	for(task t : primitive_tasks) task_name_map[t.name] = t;
	for(task t : abstract_tasks) task_name_map[t.name] = t;
}

void parsed_method_to_data_structures(bool compileConditionalEffects){
	int i = 0;
	for (auto e : parsed_methods) for (parsed_method pm : e.second) {
		method m; i++;
		m.name = pm.name;
		m.vars = pm.vars->vars;
		set<string> mVarSet; for (auto v : m.vars) mVarSet.insert(v.first);

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

		// compute flattening of method precondition
		vector<pair<pair<vector<variant<literal,conditional_effect>>, vector<literal> >, additional_variables> > precs = pm.prec->expand(false); // precondition cannot contain conditional effect
		vector<pair<pair<vector<variant<literal,conditional_effect>>, vector<literal> >, additional_variables> > effs = pm.eff->expand(compileConditionalEffects); // precondition cannot contain conditional effect

		bool addPreconditionTask = false;
		task preconditionTask;
		map<string,string> mprec_additional_vars;

		// put the task directly into the method, if it has just one expansion
		if (precs.size() == 1 && effs.size() == 1){
			auto prec = precs[0];
			auto eff = effs[0];
			// variables from precondition
			for (pair<string, string> av : prec.second){
				mprec_additional_vars[av.first] = av.first + "_" + to_string(i++);
				m.vars.push_back(make_pair(mprec_additional_vars[av.first],av.second));
			}
			// variables from effect
			for (pair<string, string> av : eff.second){
				mprec_additional_vars[av.first] = av.first + "_" + to_string(i++);
				m.vars.push_back(make_pair(mprec_additional_vars[av.first],av.second));
			}

			// add a subtask for the method precondition
			vector<literal> mPrec;
			vector<variant<literal,conditional_effect>> & mEff = eff.first.first;
			// if we don't compile method effects, add the required precondition
			mPrec.insert(mPrec.end(), eff.first.second.begin(), eff.first.second.end());

			for (variant<literal,conditional_effect> l : prec.first.first){
				if (holds_alternative<conditional_effect>(l))
					assert(false); // precondition

				if (get<literal>(l).predicate == dummy_equal_literal || get<literal>(l).predicate == dummy_ofsort_literal)
					m.constraints.push_back(get<literal>(l));
				else mPrec.push_back(get<literal>(l));
			}


			map<string,string> sorts_of_vars; for(auto pp : m.vars) sorts_of_vars[pp.first] = pp.second;

			// if we actually have method precondition, then create a task carrying the precondition
			if (mPrec.size() || mEff.size()){
				addPreconditionTask = true;
				preconditionTask.name = method_precondition_action_name + m.name;
				preconditionTask.prec = mPrec;
				for (auto effElem : mEff){
					if (holds_alternative<literal>(effElem))
						preconditionTask.eff.push_back(get<literal>(effElem));
					else
						preconditionTask.ceff.push_back(get<conditional_effect>(effElem));
				}

				set<string> args;
				for (literal l : preconditionTask.prec) for (string a : l.arguments) args.insert(a);
				for (literal l : preconditionTask.eff) for (string a : l.arguments) args.insert(a);
				for (conditional_effect ceff : preconditionTask.ceff) {
					for (literal l : ceff.condition) for (string a : l.arguments) args.insert(a);
					for (string a : ceff.effect.arguments) args.insert(a);
				}
				// get types of vars
				for (string v : args) {
					string accessV = v;
					if (mprec_additional_vars.count(v)) accessV = mprec_additional_vars[v];
					assert(sorts_of_vars.count(accessV));
					preconditionTask.vars.push_back(make_pair(v,sorts_of_vars[accessV]));
				}
				preconditionTask.number_of_original_vars = preconditionTask.vars.size(); // does not really matter as this action will get removed by the output formatter
				// add t as a new primitive task
				preconditionTask.check_integrity();
				primitive_tasks.push_back(preconditionTask);
				task_name_map[preconditionTask.name] = preconditionTask;
			}
		} else {
			// at least two expansions, so we need an abstract task
			addPreconditionTask = true;
			preconditionTask.name = method_precondition_action_name + m.name;

			// that AT must have all variables as parameters that are also parameters of the method
			set<string> args;
			for (auto prec : precs)	for (variant<literal,conditional_effect> l : prec.first.first) {
				if (holds_alternative<conditional_effect>(l))
					assert(false); // precondition
				for (string a : get<literal>(l).arguments) if (mVarSet.count(a)) args.insert(a);
			}
			// add variables from effects
			for (auto eff : effs){
				// additional preconditions for conditional effects
				for (literal l : eff.first.second)
					for (string a : l.arguments)
						if (mVarSet.count(a)) args.insert(a);

				for (variant<literal,conditional_effect> elem : eff.first.first) {
					if (holds_alternative<conditional_effect>(elem)){
						conditional_effect ceff = get<conditional_effect>(elem);
						for (literal l : ceff.condition)
							for (string a : l.arguments)
								if (mVarSet.count(a)) args.insert(a);
						for (string a : ceff.effect.arguments) if (mVarSet.count(a)) args.insert(a);

					} else {
						for (string a : get<literal>(elem).arguments) if (mVarSet.count(a)) args.insert(a);
					}
				}
			}


			map<string,string> sorts_of_vars;
			for(auto pp : m.vars) sorts_of_vars[pp.first] = pp.second;

			// get types of vars
			for (string v : args) preconditionTask.vars.push_back(make_pair(v,sorts_of_vars[v]));
			preconditionTask.number_of_original_vars = preconditionTask.vars.size(); // does not really matter as this action will get removed by the output formatter
			// add t as a new abstract task
			preconditionTask.check_integrity();
			abstract_tasks.push_back(preconditionTask);
			assert(task_name_map.count(preconditionTask.name) == 0);
			task_name_map[preconditionTask.name] = preconditionTask;


			// create a method per expansion
			int precInstance = 0;
			for (auto prec : precs) for (auto eff : effs){
				method precM; precInstance++;
				precM.name = "_expansion_" + m.name + "_instance_" + to_string(precInstance);
				precM.vars = preconditionTask.vars;
				precM.at = preconditionTask.name;
				precM.atargs.clear();
				for (auto v : precM.vars) precM.atargs.push_back(v.first);

				mprec_additional_vars.clear();
				// variables from precondition
				for (pair<string, string> av : prec.second){
					mprec_additional_vars[av.first] = av.first + "_" + to_string(i++);
					precM.vars.push_back(make_pair(mprec_additional_vars[av.first],av.second));
				}
				// variables from effect
				for (pair<string, string> av : eff.second){
					mprec_additional_vars[av.first] = av.first + "_" + to_string(i++);
					precM.vars.push_back(make_pair(mprec_additional_vars[av.first],av.second));
				}


				// add a subtask for the method precondition
				vector<literal> mPrec;
				vector<variant<literal,conditional_effect>> & mEff = eff.first.first;

				// if we don't compile method effects, add the required precondition
				mPrec.insert(mPrec.end(), eff.first.second.begin(), eff.first.second.end());

				for (variant<literal,conditional_effect> l : prec.first.first){
					if (holds_alternative<conditional_effect>(l))
						assert(false); // method precondition
					if (get<literal>(l).predicate == dummy_equal_literal || get<literal>(l).predicate == dummy_ofsort_literal)
						precM.constraints.push_back(get<literal>(l));
					else mPrec.push_back(get<literal>(l));
				}


				map<string,string> sorts_of_vars; for(auto pp : precM.vars) sorts_of_vars[pp.first] = pp.second;

				// if we actually have method precondition, then create a task carrying the precondition
				if (mPrec.size() || mEff.size()){
					task primitivePrec;
					primitivePrec.name = method_precondition_action_name + m.name + "_instance_" + to_string(precInstance);
					primitivePrec.prec = mPrec;
					for (auto effElem : mEff){
						if (holds_alternative<literal>(effElem))
							primitivePrec.eff.push_back(get<literal>(effElem));
						else
							primitivePrec.ceff.push_back(get<conditional_effect>(effElem));
					}

					// determine parameter variables of the new primitive
					set<string> args;
					for (literal l : primitivePrec.prec) for (string a : l.arguments) args.insert(a);
					for (literal l : primitivePrec.eff) for (string a : l.arguments) args.insert(a);
					for (conditional_effect ceff : primitivePrec.ceff) {
						for (literal l : ceff.condition) for (string a : l.arguments) args.insert(a);
						for (string a : ceff.effect.arguments) args.insert(a);
					}

					// get types of vars
					for (string v : args) {
						string accessV = v;
						if (mprec_additional_vars.count(v)) accessV = mprec_additional_vars[v];
						assert(sorts_of_vars.count(accessV));
						primitivePrec.vars.push_back(make_pair(v,sorts_of_vars[accessV]));
					}
					primitivePrec.number_of_original_vars = primitivePrec.vars.size(); // does not really matter as this action will get removed by the output formatter
					// add t as a new primitive task
					primitivePrec.check_integrity();
					primitive_tasks.push_back(primitivePrec);
					assert(task_name_map.count(primitivePrec.name) == 0);
					task_name_map[primitivePrec.name] = primitivePrec;

					plan_step ps;
					ps.task = primitivePrec.name;
					ps.id = "__m-prec-id";
					for(auto v : primitivePrec.vars) {
						string arg = v.first;
						if (mprec_additional_vars.count(arg)) arg = mprec_additional_vars[arg];
						ps.args.push_back(arg);
					}
					// add plan step to method
					precM.ps.push_back(ps);
				}
				precM.check_integrity();
				methods.push_back(precM);
			}

			mprec_additional_vars.clear(); // used for inner methods
		}

		if (addPreconditionTask){
			plan_step ps;
			ps.task = preconditionTask.name;
			ps.id = "__m-prec-id";
			for(auto v : preconditionTask.vars) {
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
		vector<pair<pair<vector<variant<literal,conditional_effect>>, vector<literal> >, additional_variables> > exconstraints = pm.tn->constraint->expand(false); // constraints cannot contain conditional effects
		assert(exconstraints.size() == 1);
		assert(exconstraints[0].second.size() == 0); // no additional vars due to constraints
		assert(exconstraints[0].first.second.size() == 0); // no conditional effects
		for(variant<literal,conditional_effect> l : exconstraints[0].first.first) 
			if (holds_alternative<literal>(l))
				m.constraints.push_back(get<literal>(l));
			else
				assert(false); // constraints cannot contain conditional effects


		m.check_integrity();
		methods.push_back(m);
	}
}

void reduce_constraints(){
	int ns_count = 0;
	vector<method> oldm = methods;
	methods.clear();

	for (method m : oldm){
		// remove sort constraints
		vector<literal> oldC = m.constraints;
		m.constraints.clear();
		map<string,string> sorts_of_vars; for(auto pp : m.vars) sorts_of_vars[pp.first] = pp.second;
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
		methods.push_back(m);
	}

	oldm = methods;
	methods.clear();


	for (method m : oldm){
		method nm = m;
		vector<literal> oldC = nm.constraints;
		nm.constraints.clear();

		bool removeMethod = false;
		map<string,string> vSort; for (auto x : m.vars) vSort[x.first] = x.second;
		for (literal l : oldC){
			if (l.arguments[1][0] == '?' && l.arguments[0][0] != '?'){
				// ensure that the constant is always the second
				swap(l.arguments[0],l.arguments[1]);
			}

			if (l.arguments[1][0] != '?' && l.arguments[0][0] != '?'){
				// comparing two constants ... well this is a bit stupid, but heck
				if (l.positive == (l.arguments[0][0] != l.arguments[1][0])){
					// constraint cannot be satisfied
					removeMethod = true;
				}
				continue;
			}


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
		if (!removeMethod) methods.push_back(nm);
	}

	vector<task> oldt = primitive_tasks;
	primitive_tasks.clear();
	for (task t : oldt){
		task nt = t;
		vector<literal> oldC = t.constraints;
		nt.constraints.clear();

		bool removeTask = false;
		map<string,string> vSort; for (auto x : t.vars) vSort[x.first] = x.second;
		for (literal l : oldC){
			if (l.arguments[1][0] == '?' && l.arguments[0][0] != '?'){
				// ensure that the constant is always the second
				swap(l.arguments[0],l.arguments[1]);
			}

			if (l.arguments[1][0] != '?' && l.arguments[0][0] != '?'){
				// comparing two constants ... well this is a bit stupid, but heck
				if (l.positive == (l.arguments[0][0] != l.arguments[1][0])){
					// constraint cannot be satisfied
					removeTask  = true;
				}
				continue;
			}
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
		if (!removeTask) primitive_tasks.push_back(nt);
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
	// conditions of conditional effects also occur
	for (task t : primitive_tasks) 
		for (conditional_effect ceff : t.ceff)
			for (literal l : ceff.condition)
				occuring_preds.insert(l.predicate);

	for (ground_literal gl : goal) occuring_preds.insert(gl.predicate);

	vector<predicate_definition> old = predicate_definitions;
	predicate_definitions.clear();


	// find predicates that do not occur in preconditions
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

conditional_effect::conditional_effect(vector<literal> cond, literal eff){
	condition = cond;
	effect = eff;
}


