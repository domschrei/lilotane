#include "domain.hpp"
#include "parsetree.hpp"
#include "util.hpp"
#include "cwa.hpp"
#include <iostream>
#include <cassert>
#include <algorithm>
#include <functional>

using namespace std;


string GUARD_PREDICATE="multi_stage_execution_guard";

void add_to_method_as_last(method & m, plan_step ps){
	for (auto & ops : m.ps)
		m.ordering.push_back(make_pair(ops.id, ps.id));

	m.ps.push_back(ps);
}


void addPrimitiveTask(task & t){
	primitive_tasks.push_back(t);
	task_name_map[t.name] = t;
}

void addAbstractTask(task & t){
	abstract_tasks.push_back(t);
	task_name_map[t.name] = t;
}



pair<task,bool> flatten_primitive_task(parsed_task & a,
							bool compileConditionalEffects,
							bool linearConditionalEffectExpansion,
							bool encodeDisjunctivePreconditionsInMethods
							){
	// first check whether this primitive as a disjunctive precondition
	bool disjunctivePreconditionForHTN = encodeDisjunctivePreconditionsInMethods && a.prec->isDisjunctive();
	
	// expand effects and preconditions (if necessary and possible)
	vector<pair<pair<vector<variant<literal,conditional_effect>>,vector<literal> >, additional_variables> > elist = a.eff->expand(compileConditionalEffects);
	vector<pair<pair<vector<variant<literal,conditional_effect>>, vector<literal> >, additional_variables> > plist;
	if (!disjunctivePreconditionForHTN)	plist = a.prec->expand(false); // precondition cannot contain conditional effects
	else {
		pair<pair<vector<variant<literal,conditional_effect>>, vector<literal> >, additional_variables> _temp;
		plist.push_back(_temp);
	}
	
	// determine whether any expansion has a conditional effect
	bool expansionHasConditionalEffect = false;
	if (linearConditionalEffectExpansion)
		for (auto e : elist)
			for (auto eff : e.first.first)
				if (holds_alternative<conditional_effect>(eff))
					expansionHasConditionalEffect = true;
	
	int i = 0;
	task mainTask;
	bool mainTaskIsPrimitive;
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

		// add preconditions for conditional effects (if exponentially compiled)
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
			for (auto ceff : t.ceff) {
				for (auto arg : ceff.effect.arguments) contained |= v.first == arg;
				for (auto cond : ceff.condition)
					for (auto arg : cond.arguments) contained |= v.first == arg;
			}

			if (!contained) continue;
			t.vars.push_back(v);
		}

		if (plist.size() > 1 || elist.size() > 1 || expansionHasConditionalEffect || disjunctivePreconditionForHTN) {
			// HELPER FUNCTIONS
			auto create_predicate_and_literal = [&](string prefix, task ce_at){
				// build three predicates, one for telling that something has to be checked still
				predicate_definition argument_predicate;
				argument_predicate.name = prefix + ce_at.name;
				literal argument_literal;
				argument_literal.positive = true;
				argument_literal.predicate = argument_predicate.name;
				for(auto v : t.vars){
					argument_predicate.argument_sorts.push_back(v.second);
					argument_literal.arguments.push_back(v.first);
				}
				predicate_definitions.push_back(argument_predicate);
				return make_pair(argument_predicate, argument_literal);
			};
			
			auto create_task = [&](string prefix, vector<pair<string,string>> vars){
				task tt;
				tt.name = prefix + t.name;
				tt.vars = vars;
				tt.number_of_original_vars = vars.size();
				tt.constraints.clear();
				tt.costExpression.clear();
				return tt;
			};

			auto create_method = [&](task at, string prefix){
				method m_ce;
				m_ce.name = prefix + at.name;
				m_ce.at = at.name;
				m_ce.vars = at.vars;
				for(auto v : at.vars) m_ce.atargs.push_back(v.first);
				return m_ce;
			};

			auto create_singleton_method = [&](task at, task sub, string prefix){
				method m_ce = create_method(at, prefix);
				plan_step ps;
				ps.task = sub.name;
				ps.id = "id0";
				set<string> existingVars; for (auto [var,_] : m_ce.vars) existingVars.insert(var);
				for(auto v : sub.vars) {
					if (existingVars.count(v.first) == 0){
						m_ce.vars.push_back(v);
						existingVars.insert(v.first);
					}
					ps.args.push_back(v.first);
				}
				m_ce.ps.push_back(ps);
				methods.push_back(m_ce);
			};

			auto create_plan_step = [&](task tt, string prefix){
				plan_step p;
				p.task = tt.name;
				p.id = prefix ;
				p.args.clear();
				for(auto v : tt.vars) p.args.push_back(v.first);
				return p;
			};
		
			////////////////// ACTUAL WORK STARTS
			if (plist.size() > 1 || elist.size() > 1)
				t.name += "|instance_" + to_string(i);
			if (expansionHasConditionalEffect)
				t.name += "|ce_base_action";
			if (disjunctivePreconditionForHTN)
				t.name += "|disjunctive_prec";

			// we have to create a new decomposition method at this point
			method m;
			m.name = "_method_for_multiple_expansions_of_" + t.name; // must start with an underscore s.t. this method is applied by the solution compiler
			m.at = a.name;
			for(auto v : a.arguments->vars) m.atargs.push_back(v.first);
			m.vars = t.vars;

			// add the starting task as the first one to the method
			m.ps.push_back(create_plan_step(t,"id_main"));

			
			if (disjunctivePreconditionForHTN){
				// the precondition is disjunctive, so we have to check them using the HTN structure
				
				int globalFCounter = 0;
				function<void (task &, general_formula*, var_declaration)> generate_formula_HTN;
				generate_formula_HTN = [&](task & current_task, general_formula * f, var_declaration current_vars) -> void {
					// do different things based on formula type
					int fcounter = globalFCounter++;

					if (f->type == EMPTY) {
						method m = create_method(current_task, "__formula_empty_" + to_string(fcounter));
						m.check_integrity();
						methods.push_back(m);
					} else if (f->type == ATOM || f->type == NOTATOM ||
							f->type == EQUAL || f->type == NOTEQUAL || 
							f->type == OFSORT || f->type == NOTOFSORT) {
						string typ = "eq";
						if (f->type == NOTEQUAL) typ = "neq";
						if (f->type == ATOM) typ = "atom_" + f->predicate;
						if (f->type == NOTATOM) typ = "not_atom_" + f->predicate;
					
						task check = create_task("__formula_" + typ + "_" + to_string(fcounter), current_vars.vars);
						
						literal l = (f->type == ATOM || f->type == NOTATOM)?
								f->atomLiteral():
								f->equalsLiteral();

						check.prec.push_back(l);
						addPrimitiveTask(check);

						create_singleton_method(current_task, check, "__formula_" + typ + "_" + to_string(fcounter));

					} else if (f->type == OR) {
						int subCounter = 0;
						for (general_formula * sub : f->subformulae){
							// determine variables relevant for sub formula
							var_declaration subVars;
							set<string> occuringVariables = sub->occuringUnQuantifiedVariables();

							for (pair<string,string> var_decl : current_vars.vars)
								if (occuringVariables.count(var_decl.first))
									subVars.vars.push_back(var_decl);
								
							task subTask = create_task("__formula_or_" + to_string(fcounter) + "_" + to_string(subCounter) + "_", subVars.vars);
							addAbstractTask(subTask);
							
							// create the method
							create_singleton_method(current_task,subTask, "__formula_or_" +
									to_string(fcounter) + "_" + to_string(subCounter) + "_");
						
							generate_formula_HTN(subTask, sub, subVars);

							subCounter++;
						}
						
					} else if (f->type == WHEN || f->type == VALUE || f->type == COST_CHANGE || f->type == COST) {
						assert(false); // not allowed
					} else if (f->type == FORALL){
						auto [var_replace, additional_vars] = f->forallVariableReplacement();
						map<string,string> newVarTypes;
						for (auto [var,type] : additional_vars)
							newVarTypes[var] = type;

						
						// we get new variables from the quantification, but some might already be there ...
						set<string> existing_variables;
						for (auto & [var,_] : current_vars.vars) existing_variables.insert(var);

						// all of the forall instantiations go into one method
						method m = create_method(current_task, "__formula_forall_" + to_string(fcounter));
						
						int sub = 0;
						for (map<string,string> replacement : var_replace){
							var_declaration sub_vars = current_vars;
							for (auto & [qvar,newVar] : replacement)
								if (existing_variables.count(newVar))
									assert(false);
								else {
									string type = newVarTypes[newVar];
									sub_vars.vars.push_back(make_pair(newVar,type));
									m.vars.push_back(make_pair(newVar,type));
								}


							task subTask = create_task("__formula_forall_" + to_string(fcounter) + "_" + to_string(sub) + "_", sub_vars.vars);
							addAbstractTask(subTask);
							add_to_method_as_last(m,create_plan_step(subTask,"id"+to_string(sub++)));

							// generate an HTN for the subtask
							generate_formula_HTN(subTask, f->subformulae[0]->copyReplace(replacement), sub_vars);
						}

						m.check_integrity();
						methods.push_back(m);
					} else if (f->type == AND){
						method m = create_method(current_task, "__formula_and_" + to_string(fcounter));

						int subCounter = 0;
						for (general_formula * sub : f->subformulae){
							var_declaration subVars;
							set<string> occuringVariables = sub->occuringUnQuantifiedVariables();
							for (pair<string,string> var_decl : current_vars.vars)
								if (occuringVariables.count(var_decl.first))
									subVars.vars.push_back(var_decl);
							
							
							task subTask = create_task("__formula_and_" + to_string(fcounter) + "_" + to_string(subCounter) + "_", subVars.vars);
							addAbstractTask(subTask);
							add_to_method_as_last(m,create_plan_step(subTask,"id"+to_string(subCounter++)));

							// generate an HTN for the subtask
							generate_formula_HTN(subTask, sub, subVars);
						}

						m.check_integrity();
						methods.push_back(m);
					} else if (f->type == EXISTS){
						map<string,string> var_replace = f->existsVariableReplacement();

						// we get new variables from the quantification, but some might already be there ...
						set<string> existing_variables;
						for (auto & [var,_] : current_vars.vars) existing_variables.insert(var);
						
						var_declaration sub_vars = current_vars;
						for (auto & [qvar,type] : f->qvariables.vars){
							string newVar = var_replace[qvar];
							if (existing_variables.count(newVar))
								assert(false);
							else {
								sub_vars.vars.push_back(make_pair(newVar,type));
								m.vars.push_back(make_pair(newVar,type));
							}
						}

						task subTask = create_task("__formula_exists_" + to_string(fcounter) + "_", sub_vars.vars);
						addAbstractTask(subTask);

						create_singleton_method(current_task, subTask, "__formula_exists_" + to_string(fcounter) + "_");
						generate_formula_HTN(subTask, f->subformulae[0]->copyReplace(var_replace), sub_vars);
					}
				};

				// determine initially needed variables
				var_declaration initialVariables;
				set<string> occuringVariables = a.prec->occuringUnQuantifiedVariables();

				for (pair<string,string> var_decl : t.vars)
					if (occuringVariables.count(var_decl.first))
						initialVariables.vars.push_back(var_decl);
				
				for (pair<string,string> vars_for_consts : a.prec->variables_for_constants()){
					initialVariables.vars.push_back(vars_for_consts);
					m.vars.push_back(vars_for_consts);
				}

				// create a carrier task
				task formula_carrier_root = create_task("__formula-root", initialVariables.vars);
				formula_carrier_root.check_integrity();
				addAbstractTask(formula_carrier_root);
				add_to_method_as_last(m,create_plan_step(formula_carrier_root,"id_prec_root"));

				generate_formula_HTN(formula_carrier_root, a.prec, initialVariables);
			}




			if (linearConditionalEffectExpansion || disjunctivePreconditionForHTN){
				// construct action applying the conditional effect

				literal guard_literal;
				guard_literal.positive = false;
				guard_literal.predicate = GUARD_PREDICATE;
				guard_literal.arguments.clear();

				t.prec.push_back(guard_literal);

				vector<pair<bool,plan_step>> steps_with_effects;
				
				// effects of the main task need to be performed *AFTER* splitting ...
				vector<literal> add_effects, del_effects;
				for (literal & el : t.eff)
					if (el.positive) add_effects.push_back(el);
					else 			 del_effects.push_back(el);
				t.eff.clear();
				
				auto [main_arguments, main_literal] = create_predicate_and_literal("doing_action_", t);
												t.eff.push_back(main_literal);
				guard_literal.positive = true;	t.eff.push_back(guard_literal);

				int j = 0;
				for (conditional_effect & ceff : t.ceff){
					/////////// PHASE 1 check conditions
					task ce_at = create_task("__ce_check_" + to_string(j) + "_", t.vars);
					ce_at.check_integrity();
					addAbstractTask(ce_at);						
					
					plan_step ce_at_ps = create_plan_step(ce_at, "id_ce_prec_" + to_string(j));
					add_to_method_as_last(m,ce_at_ps);

					auto [apply_predicate, apply_literal] = create_predicate_and_literal("do_apply_", ce_at);
					auto [not_apply_predicate, not_apply_literal] = create_predicate_and_literal("not_apply_", ce_at);
				
					// add methods for this task, first the one that actually apply the CE
					task ce_yes = create_task("__ce_yes_" + to_string(j) + "_", t.vars);
					ce_yes.prec = ceff.condition;
					// additional preconditions
					main_literal.positive = true;   	ce_yes.prec.push_back(main_literal);
					apply_literal.positive = true;		ce_yes.eff.push_back(apply_literal);
					ce_yes.check_integrity();
					addPrimitiveTask(ce_yes);
	
					create_singleton_method(ce_at,ce_yes,"_method_for_ce_yes_");
				
					// for every condition of the CE add one possible negation
					int noCount = 0;
					for (literal precL : ceff.condition){
						task ce_no = create_task("__ce_no_" + to_string(j) + "_cond_" + to_string(noCount) + "_", t.vars);
						
						precL.positive = !precL.positive;		ce_no.prec.push_back(precL);
						main_literal.positive = true;			ce_no.prec.push_back(main_literal);
						not_apply_literal.positive = true;		ce_no.eff.push_back(not_apply_literal);
						// additional preconditions
						ce_no.check_integrity();
						addPrimitiveTask(ce_no);
					
						create_singleton_method(ce_at,ce_no,"_method_for_ce_no_" + to_string(noCount++));
					}


					/////////// PHASE 2 apply the effect
					task ce_apply = create_task("__ce_apply_if_applicable_" + to_string(j) + "_", t.vars);
					ce_apply.check_integrity();
					addAbstractTask(ce_apply);						
					
					plan_step ce_apply_ps = create_plan_step(ce_apply, "id_ce_eff_" + to_string(j));
					steps_with_effects.push_back(make_pair(ceff.effect.positive, ce_apply_ps));

					// a task that applies the effect if necessary
					task ce_do = create_task("__ce_apply_" + to_string(j) + "_", t.vars);
														ce_do.eff.push_back(ceff.effect);
					apply_literal.positive = true;		ce_do.prec.push_back(apply_literal);
					apply_literal.positive = false;		ce_do.eff.push_back(apply_literal);
					ce_do.check_integrity();
					addPrimitiveTask(ce_do);
					create_singleton_method(ce_apply,ce_do,"_method_for_ce_do_apply_");

					// a task that does not applies the effect if necessary
					task ce_not_do = create_task("__ce_not_apply_" + to_string(j) + "_", t.vars);
					not_apply_literal.positive = true;	ce_not_do.prec.push_back(not_apply_literal);
					not_apply_literal.positive = false;	ce_not_do.eff.push_back(not_apply_literal);
					ce_not_do.check_integrity();
					addPrimitiveTask(ce_not_do);
					create_singleton_method(ce_apply,ce_not_do,"_method_for_ce_not_do_apply_");


					// increment
					j++;
				}

				// remove conditional effects from main task
				t.ceff.clear();
				
				// apply delete effects
				if (del_effects.size()){
					task main_deletes = create_task("_main_delete_", t.vars);
					main_literal.positive = true;	main_deletes.prec.push_back(main_literal);
													main_deletes.eff = del_effects;
					
					main_deletes.check_integrity();
					addPrimitiveTask(main_deletes);
					plan_step main_deletes_ps = create_plan_step(main_deletes, "id_main_del");
					add_to_method_as_last(m,main_deletes_ps);
				}
				
				// first apply the deleting then the adding effects! else we break basic STRIPS rules
				sort(steps_with_effects.begin(), steps_with_effects.end());
				for (auto & [_,ps] : steps_with_effects) add_to_method_as_last(m,ps);
			
				// lastly, we need an action that ends the CE
				task main_add = create_task("_main_adds_", t.vars);
				main_literal.positive = true;	main_add.prec.push_back(main_literal);
												main_add.eff = add_effects;
				guard_literal.positive = false;	main_add.eff.push_back(guard_literal);
				main_literal.positive = false;	main_add.eff.push_back(main_literal);
				
				main_add.check_integrity();
				addPrimitiveTask(main_add);	
				
				plan_step main_add_ps = create_plan_step(main_add, "id_main_add");
				add_to_method_as_last(m,main_add_ps);
			}

			// for this to be ok, we have to create an abstract task in the first round
			if (i == 1){
				task at;
				at.name = a.name;
				at.vars = a.arguments->vars;
				at.number_of_original_vars = at.vars.size();
				at.check_integrity();
				addAbstractTask(at);
				mainTask = at;
				mainTaskIsPrimitive = false;
			}
			// add to list, moved to if to enable integrity checking
			t.check_integrity();
			addPrimitiveTask(t);	
			
			m.check_integrity();
			methods.push_back(m);
		} else {
			// add to list, moved to else to enable integrity checking in if
			t.check_integrity();
			addPrimitiveTask(t);	
			
			mainTask = t;
			mainTaskIsPrimitive = true;
		}

	}
	return make_pair(mainTask,mainTaskIsPrimitive);
}


void flatten_tasks(bool compileConditionalEffects,
				   bool linearConditionalEffectExpansion,
				   bool encodeDisjunctivePreconditionsInMethods){

	if (linearConditionalEffectExpansion || encodeDisjunctivePreconditionsInMethods){
		predicate_definition guardPredicate;
		guardPredicate.name = GUARD_PREDICATE;
		guardPredicate.argument_sorts.clear();

		predicate_definitions.push_back(guardPredicate);
	}

	// flatten the primitives ...
	for(parsed_task & a : parsed_primitive)
		flatten_primitive_task(a, compileConditionalEffects, linearConditionalEffectExpansion, encodeDisjunctivePreconditionsInMethods);
	
	for(parsed_task & a : parsed_abstract){
		task at;
		at.name = a.name;
		at.vars = a.arguments->vars;
		at.number_of_original_vars = at.vars.size();
		// abstract tasks cannot have additional variables (e.g. for constants): these cannot be declared in the input
		at.check_integrity();
		addAbstractTask(at);
	}
}

void parsed_method_to_data_structures(bool compileConditionalEffects,
									  bool linearConditionalEffectExpansion,
									  bool encodeDisjunctivePreconditionsInMethods){
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


		parsed_task mPrec_task;
		mPrec_task.name = method_precondition_action_name + m.name;
		mPrec_task.prec = pm.prec;
		mPrec_task.eff = pm.eff;
		mPrec_task.arguments = new var_declaration();

		set<string> mPrecVars = pm.prec->occuringUnQuantifiedVariables();
		set<string> mEffVars = pm.eff->occuringUnQuantifiedVariables();

		for (pair<string,string> var : m.vars)
			if (mPrecVars.count(var.first) || mEffVars.count(var.first))
				mPrec_task.arguments->vars.push_back(var);
		
		auto [mPrec,isPrimitive] = flatten_primitive_task(mPrec_task, compileConditionalEffects, linearConditionalEffectExpansion, encodeDisjunctivePreconditionsInMethods);
		for (size_t newVar = mPrec_task.arguments->vars.size(); newVar < mPrec.vars.size(); newVar++)
			m.vars.push_back(mPrec.vars[newVar]);

		// edge case: the precondition might only have contained constraints
		if (isPrimitive && mPrec.prec.size() == 0 && mPrec.eff.size() == 0 && mPrec.ceff.size() == 0){
			// has only constraints
			for (literal l : mPrec.constraints)
				m.constraints.push_back(l);
			
			// remove the task
			primitive_tasks.pop_back();
			task_name_map.erase(mPrec.name);
		} else {
			// here we actually add the task as a plan step
			plan_step ps;
			ps.id = "mprec";
			ps.task = mPrec.name;
			for (auto [v,_] : mPrec.vars)
				ps.args.push_back(v);

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
		m.check_integrity();
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
		m.check_integrity();
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
		if (!removeMethod) {
			nm.check_integrity();
			methods.push_back(nm);
		}
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

	assert(number_of_original_vars <= int(vars.size()));

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
		assert(task_name_map.count(ps.task));
		task t = task_name_map[ps.task];
		if (ps.args.size() != t.vars.size()){
			cerr << "Method " << this->name << " has the subtask (" << ps.id << ") " << ps.task << ". The task is declared with " << t.vars.size() << " parameters, but " << ps.args.size() << " are given in the method." << endl;
			assert(false);
		}

		for (string v : ps.args) {
			bool found = false;
			for (auto vd : this->vars) found |= vd.first == v;
			if (!found){
				cerr << "Method " << this->name << " has the subtask (" << ps.id << ") " << ps.task << ". It has a parameter " << v << " which is not declared in the method." << endl;
			
			}
			assert(found);
		}
	}
}

conditional_effect::conditional_effect(vector<literal> cond, literal eff){
	condition = cond;
	effect = eff;
}


