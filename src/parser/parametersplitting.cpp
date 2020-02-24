#include "domain.hpp"
#include "parametersplitting.hpp"
#include "util.hpp"
#include <iostream>
using namespace std;

void split_independent_parameters(){
	vector<method> old = methods;
	methods.clear();
	int i = 0;
	for(method m : old){
		// find variables that occur only in one of the plan steps
		map<string,set<string>> variables_ps_id;
		for(plan_step ps : m.ps) for(string v : ps.args) variables_ps_id[v].insert(ps.id);
		// don't consider variables that have at most one instantiation
		for(auto v : m.vars) if (sorts[v.second].size() < 2) variables_ps_id.erase(v.first); 
		// don't consider variables that are part of constraints
		for (literal l : m.constraints) variables_ps_id.erase(l.arguments[0]), variables_ps_id.erase(l.arguments[1]);
		// can't split on arguments of abstract task
		for (string v : m.atargs) variables_ps_id.erase(v);

		map<string,set<string>> ps_only_variables;
		for (auto vps : variables_ps_id) if (vps.second.size() == 1) ps_only_variables[*vps.second.begin()].insert(vps.first);
		
		if (ps_only_variables.size() == 0 || m.ps.size() == 1) { methods.push_back(m); continue; } // nothing to do here
		
		map<string,string> variables_to_remove;
		for (auto x : ps_only_variables) for (string v : x.second) variables_to_remove[v].size(); // insert into map

		method base;
		base.name = m.name;
		base.at = m.at;
		base.atargs = m.atargs;
		base.constraints = m.constraints;
		base.ordering = m.ordering;
		// only take variables that are not to be removed
		map<string,string> sorts_of_remaining;
		for(auto v : m.vars)
			if (variables_to_remove.count(v.first))
				variables_to_remove[v.first] = v.second;
			else
				base.vars.push_back(v), sorts_of_remaining[v.first] = v.second;
		
		for (plan_step ops : m.ps) if (!ps_only_variables.count(ops.id)) base.ps.push_back(ops); else {
			plan_step nps;
			// create new abstract task
			task at;
			at.name = ops.task + "_splitted_" + to_string(++i);
			// create a new method for the splitted task
			method sm;
			sm.name = "_splitting_method_" + at.name; // must start with an underscore s.t. this method will be removed by the solution compiler
			sm.at = at.name;
			set<string> smVars; // for duplicate check
			
			// variables
			for (string v : ops.args){	
				if (!variables_to_remove.count(v)){	// if we don't remove it, it is an argument of the abstract task
					at.vars.push_back(make_pair(v,sorts_of_remaining[v]));
					nps.args.push_back(v);
					if (!smVars.count(v)){
						smVars.insert(v);
						sm.vars.push_back(make_pair(v,sorts_of_remaining[v]));
					}
				} else {
					if (!smVars.count(v)){
						smVars.insert(v);
						sm.vars.push_back(make_pair(v,variables_to_remove[v]));
					}
				}
			}
			at.number_of_original_vars = at.vars.size(); // does not matter as it will get pruned
			abstract_tasks.push_back(at);
			task_name_map[at.name] = at;
			
			nps.task = at.name;
			nps.id = ops.id;
			base.ps.push_back(nps);

			
			sm.atargs = nps.args;
			sm.ps.push_back(ops);
			sm.check_integrity();
			methods.push_back(sm);
		}

		base.check_integrity();
		methods.push_back(base);
	}
}

