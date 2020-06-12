#include <cstdio>
#include <iostream>
#include <vector>
#include <map>
#include <cassert>
#include <algorithm>
#include <unordered_set>
#include "hddlWriter.hpp"
#include "parsetree.hpp"
#include "hddl.hpp"
#include "domain.hpp"
#include "sortexpansion.hpp"
#include "cwa.hpp"
#include "util.hpp"

using namespace std;




bool replacement_type_dfs(int cur, int end, set<int> & visited, vector<set<int>> & parents){
	if (cur == end) return true;
	if (!parents[cur].size()) return false;
	
	if (visited.count(cur)) return true;
	visited.insert(cur);

	for (int p : parents[cur])
		if (!replacement_type_dfs(p,end,visited,parents)) return false;

	return true;
}


pair<int,set<int>> get_replacement_type(int type_to_replace, vector<set<int>> & parents){
	// try all possible replacement sorts
	set<int> best_visited;
	int best_replacement = -1;
	for (size_t s = 0; s < parents.size(); s++) if (int(s) != type_to_replace){
		set<int> visited;
		if (replacement_type_dfs(type_to_replace, s, visited, parents))
			if (best_replacement == -1 || best_visited.size() > visited.size()){
				best_replacement = s;
				best_visited = visited;
			}
	}

	return make_pair(best_replacement, best_visited);
}




tuple<vector<string>,
	  vector<int>,
	  map<string,string>,
	  map<int,int> > compute_local_type_hierarchy(){
	// find subset relations between sorts
	
	// [i][j] = true means that j is a subset of i
	vector<string> sortsInOrder;
	for(auto & [s,_] : sorts) sortsInOrder.push_back(s);

	vector<vector<bool>> subset (sortsInOrder.size());
	
	for (size_t s1I = 0; s1I < sortsInOrder.size(); s1I++){
		string s1 = sortsInOrder[s1I];

		if (!sorts[s1].size()) {
			for (size_t s2 = 0; s2 < sortsInOrder.size(); s2++) subset[s1I].push_back(false);
			continue;
		}

		for (size_t s2I = 0; s2I < sortsInOrder.size(); s2I++){
			string s2 = sortsInOrder[s2I];
			if (s1I != s2I && sorts[s2].size()){
				if (includes(sorts[s1].begin(), sorts[s1].end(),
							sorts[s2].begin(), sorts[s2].end())){
					// here we know that s2 is a subset of s1
					subset[s1I].push_back(true);
				} else
					subset[s1I].push_back(false);
			} else subset[s1I].push_back(false);
		}
	}

	// transitive reduction
	for (size_t s1 = 0; s1 < sortsInOrder.size(); s1++)
		for (size_t s2 = 0; s2 < sortsInOrder.size(); s2++)
			for (size_t s3 = 0; s3 < sortsInOrder.size(); s3++)
				if (subset[s2][s1] && subset[s1][s3])
					subset[s2][s3] = false;
	
	
	/*for (size_t s1 = 0; s1 < sortsInOrder.size(); s1++){
		for (size_t s2 = 0; s2 < sortsInOrder.size(); s2++){
			if (subset[s1][s2]){
				cout << sortsInOrder[s2];
				cout << " is a subset of ";
				cout << sortsInOrder[s1];
				cout << endl;
			
			}
		}
	}*/

	// determine parents
	vector<set<int>> parents(sortsInOrder.size());
	vector<int> parent (sortsInOrder.size());
	for (size_t s1 = 0; s1 < sortsInOrder.size(); s1++)
		parent[s1] = -1;
	
	for (size_t s1 = 0; s1 < sortsInOrder.size(); s1++)
		for (size_t s2 = 0; s2 < sortsInOrder.size(); s2++)
			if (subset[s1][s2]){
				parents[s2].insert(s1);
				if (parent[s2] != -1){
					if (parent[s2] >= 0) 
						cout << "Type hierarchy is not a tree ...I can't write this in standard conform HDDL, so ..." << endl;
					parent[s2] = -2;
				} else {
					parent[s2] = s1;
				}
			}


	/*for (size_t s1 = 0; s1 < sortsInOrder.size(); s1++){
		cout << sortsInOrder[s1];
   		if (parent[s1] == -2) {	
			cout << " - {";
			for (int s : parents[s1])
				cout << sortsInOrder[s] << " ";
			cout << "}";

		} else if (parent[s1] != -1)
			cout << " - " << sortsInOrder[parent[s1]];
		cout << endl; 
	}*/

	// sorts with multiple parents need to be replaced by parent sorts ...
	map<int,int> replacedSorts;
	
	for (size_t s = 0; s < sortsInOrder.size(); s++) if (parent[s] == -2){
		//cout << "Sort " << sortsInOrder[s] << " has multiple parents and must be replaced." << endl;
		auto [replacement, all_covered] = get_replacement_type(s,parents);
		assert(replacement != -1); // there is always the object type
		
		//cout << "Replacement sort is " << sortsInOrder[replacement] << endl;
		//cout << "All to be replaced:";
		//for (int ss : all_covered) cout << " " << sortsInOrder[ss]; cout << endl;

		for (int covered : all_covered){
			replacedSorts[covered] = replacement;
			parent[covered] = -2;
		}
	}

	// update parent relation
	for (size_t s = 0; s < sortsInOrder.size(); s++) if (parent[s] >= 0){
		if (replacedSorts.count(parent[s]))
			parent[s] = replacedSorts[parent[s]];
	}

	// who is whose direct subset, with handling the replaced sorts
	vector<unordered_set<int>> directsubset (sortsInOrder.size());
	for (size_t s1 = 0; s1 < sortsInOrder.size(); s1++)
		for (size_t s2 = 0; s2 < sortsInOrder.size(); s2++) if (parent[s2] >= 0)
			if (subset[s1][s2]){
				if (replacedSorts.count(s1))
					directsubset[replacedSorts[s1]].insert(s2);
				else
					directsubset[s1].insert(s2);
			}

	/*for (size_t s1 = 0; s1 < sortsInOrder.size(); s1++) if (parent[s1] != -2){
		cout << "Direct subsorts of " << sortsInOrder[s1] << ":";
		for (int s : directsubset[s1]) cout << " " << sortsInOrder[s];
		cout << endl;
	}*/

	// assign elements to sorts
	vector<set<string>> directElements (sortsInOrder.size());
	
	for (size_t s1 = 0; s1 < sortsInOrder.size(); s1++) if (parent[s1] != -2){
		for (string elem : sorts[sortsInOrder[s1]]){
			bool in_sub_sort = false;
			for (int s2 : directsubset[s1]){
				if (sorts[sortsInOrder[s2]].count(elem)){
					in_sub_sort = true;
					break;
				}
			}
			if (in_sub_sort) continue;
			directElements[s1].insert(elem);
		}
	}
	
	/*for (size_t s1 = 0; s1 < sortsInOrder.size(); s1++){
		cout << "Sort Elements: " << sortsInOrder[s1] << ":";
		for (string e : directElements[s1]) cout << " " << e;
		cout << endl;
	}*/

	map<string,string> sortOfElement;
	
	for (size_t s1 = 0; s1 < sortsInOrder.size(); s1++)
		for (string elem : directElements[s1])
			sortOfElement[elem] = sortsInOrder[s1];

	
	return make_tuple(sortsInOrder, parent, sortOfElement, replacedSorts);
}



















void hddl_output(ostream & dout, ostream & pout, bool internalHDDLOutput){

	auto sanitise = [&](string s){
		if (internalHDDLOutput) {
			if (s[0] == '_') return "t" + s;
			return s;
		}
		string result = "";
		for (size_t i = 0; i < s.size(); i++){
			char c = s[i];

			if (c == '_' && !i) result += "US";

			if (c == '<') result += "LA_";
			else if (c == '>') result += "RA_";
			else if (c == '[') result += "LB_";
			else if (c == ']') result += "RB_";
			else if (c == '|') result += "BAR_";
			else if (c == ';') result += "SEM_";
			else if (c == ',') result += "COM_";
			else if (c == '+') result += "PLUS_";
			else if (c == '-') result += "MINUS_";
			else result += c;
		}
		return result;
	};


	set<string> neg_pred;
	for (task t : primitive_tasks) for (literal l : t.prec) if (!l.positive) neg_pred.insert(l.predicate);
	for (task t : primitive_tasks) for (conditional_effect ceff : t.ceff) for (literal l : ceff.condition) if (!l.positive) neg_pred.insert(l.predicate);
	for (auto l : goal) if (!l.positive) neg_pred.insert(l.predicate);


	dout << "(define (domain d)" << endl;
	dout << "  (:requirements :typing :hierarchy";
	if (!internalHDDLOutput) dout << " :negative-preconditions";
	dout << ")" << endl;
	
	dout << endl;

	
	
	dout << "  (:types" << endl;
	
	// add artificial root type ...
	set<string> allConstants;
	for (auto [_,cs] : sorts)
		allConstants.insert(cs.begin(), cs.end());
	
	bool hasRootSort = false;
	for (auto [_,cs] : sorts) if (cs.size() == allConstants.size()) hasRootSort = true;
	if (! hasRootSort) sorts["master_sort"] = allConstants;
	
	
	auto [sortsInOrder, parent, sortOfElement, replacedSorts] = compute_local_type_hierarchy();
	
	if (internalHDDLOutput){
		// if we do internal output, remove it immediately
		sorts.erase("master_sort");
		// sorts
		for (auto x : sorts) dout << "    " << sanitise(x.first) << endl;
	} else {
		// compute and output an appropriate type hierarchy
		vector<int> sortsWithoutParents;
		unordered_set<int> onRightHandSide;
		for (size_t s = 0; s < sortsInOrder.size(); s++){
			if (parent[s] >= 0){
				dout << "    " << sanitise(sortsInOrder[s]) << " - " << sanitise(sortsInOrder[parent[s]]) << endl;
				onRightHandSide.insert(parent[s]);
			} else if (parent[s] == -1)
				sortsWithoutParents.push_back(s);
		}

		for (int s : sortsWithoutParents) if (!onRightHandSide.count(s))
			dout << "    " << sanitise(sortsInOrder[s]) << endl;
		
	}
	dout << "  )" << endl;
	
	dout << endl;

	// predicate definitions
	dout << "  (:predicates" << endl;
	map<string,string> sortReplace;
	map<string,string> typeConstraint;
	for (auto & [s,_] : sorts) sortReplace[s] = s;

	if (!internalHDDLOutput){
		for (auto [originalSort,replacement] : replacedSorts){
			string oldSort = sortsInOrder[originalSort];
			string newSort = sortsInOrder[replacement];
			sortReplace[oldSort] = newSort;
			string predicateName = "p_sort_member_" + sanitise(oldSort);
			typeConstraint[oldSort] = predicateName;
			dout << "    (" << predicateName << " ?x - " << sanitise(newSort) << ")" << endl;
		}
	}

	for (auto p : predicate_definitions) for (int negative = 0; negative < 2; negative++){
		if (negative && !internalHDDLOutput) continue; // compile only for internal output
		if (negative && !neg_pred.count(p.name)) continue;
		dout << "    (";
		if (negative) dout << "not_";
		dout << sanitise(p.name);

		// arguments
		for (size_t arg = 0; arg < p.argument_sorts.size(); arg++)
			dout << " ?var" << arg << " - " << sanitise(sortReplace[p.argument_sorts[arg]]);
		
		dout << ")" << endl;
	}

	dout << "  )" << endl;
	dout << endl;

	bool hasActionCosts = metric_target != dummy_function_type;

	// functions (for cost expressions)
	if (parsed_functions.size() && hasActionCosts){
		dout << "  (:functions" << endl;
		for(auto f : parsed_functions){
			dout << "    (" << sanitise(f.first.name);
			for (size_t arg = 0; arg < f.first.argument_sorts.size(); arg++)
				dout << " ?var" << arg << " - " << sanitise(f.first.argument_sorts[arg]);
			dout << ") - " << f.second << endl;
		}
		dout << "  )" << endl;
	}
	dout << endl;

	// abstract tasks
	for (task t : abstract_tasks){
		dout << "  (:task ";
		dout << sanitise(t.name);
		dout << " :parameters (";
		bool first = true;
		for (auto v : t.vars){
			if (!first) dout << " ";
			first = false;
			dout << sanitise(v.first) << " - " << sanitise(sortReplace[v.second]);
		}
		dout << "))" << endl;
	}
	
	dout << endl;

	// decomposition methods
	for (method m : methods){
		dout << "  (:method ";
	   	dout << sanitise(m.name) << endl;
		dout << "    :parameters (";
		bool first = true;
		vector<pair<string,string>> variablesToConstrain;
		for (auto v : m.vars){
			if (!first) dout << " ";
			first = false;
			dout << sanitise(v.first) << " - " << sanitise(sortReplace[v.second]);
			if (typeConstraint.count(v.second))
				variablesToConstrain.push_back(make_pair(v.first, typeConstraint[v.second]));
		}
		dout << ")" << endl;

		// AT
		dout << "    :task (";
		dout << sanitise(m.at);
		for (string v : m.atargs) dout << " " << sanitise(v);
		dout << ")" << endl;

		// constraints
		if (m.constraints.size() || variablesToConstrain.size()){
			dout << "    :precondition (and" << endl;

			for (auto & [v,pred] : variablesToConstrain)
				dout << "      (" << sanitise(pred) << " " << sanitise(v) << ")" << endl;
			
			for (literal l : m.constraints){
				dout << "      ";
				if (!l.positive) dout << "(not ";
				dout << "(= " << sanitise(l.arguments[0]) << " " << sanitise(l.arguments[1]) << ")";
				if (!l.positive) dout << ")";
				dout << endl;
			}
			
			dout << "    )" << endl;
		}

		// subtasks
		dout << "    :subtasks (and" << endl;
		for (plan_step ps : m.ps){
			dout << "      (x" << sanitise(ps.id) << " (";
			dout << sanitise(ps.task);
			for (string v : ps.args) dout << " " << sanitise(v);
			dout << "))" << endl;
		}
		dout << "    )" << endl;

		
		if (m.ordering.size()){
			// ordering of subtasks
			dout << "    :ordering (and" << endl;
			for (auto o : m.ordering)
				dout << "      (x" << sanitise(o.first) << " < x" << sanitise(o.second) << ")" << endl;
			dout << "    )" << endl;
		}

		dout << "  )" << endl << endl;
	}


	// actions
	for (task t : primitive_tasks){
		dout << "  (:action ";
		dout << sanitise(t.name) << endl;
		dout << "    :parameters (";
		bool first = true;
		vector<pair<string,string>> variablesToConstrain;
		for (auto v : t.vars){
			if (!first) dout << " ";
			first = false;
			dout << sanitise(v.first) << " - " << sanitise(sortReplace[v.second]);
			if (typeConstraint.count(v.second))
				variablesToConstrain.push_back(make_pair(v.first, typeConstraint[v.second]));
		}
		dout << ")" << endl;
		
	
		if (variablesToConstrain.size() || t.prec.size() || t.constraints.size()){
			// precondition
			dout << "    :precondition (and" << endl;
			
			for (auto & [v,pred] : variablesToConstrain)
				dout << "      (" << sanitise(pred) << " " << sanitise(v) << ")" << endl;
			
			for (literal l : t.constraints){
				dout << "      ";
				if (!l.positive) dout << "(not ";
				dout << "(= " << sanitise(l.arguments[0]) << " " << sanitise(l.arguments[1]) << ")";
				if (!l.positive) dout << ")";
				dout << endl;
			}
			
			for (literal l : t.prec){
				string p;
				if (internalHDDLOutput)
					p = (l.positive ? "" : "not_")  + sanitise(l.predicate);
				else
					p = (l.positive ? "" : "not (") + sanitise(l.predicate);

				dout << "      (" << p;
				for (string v : l.arguments) dout << " " << sanitise(v);
				if (!internalHDDLOutput && !l.positive) dout << ")";
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
					if ((neg_pred.count(l.predicate) && internalHDDLOutput) || (l.positive == positive)){
						dout << "      (";
						if (!positive) dout << "not (";
						dout << ((l.positive == positive) ? "" : "not_") << sanitise(l.predicate);
						for (string v : l.arguments) dout << " " << sanitise(v);
						if (!positive) dout << ")";
						dout << ")" << endl;
					}
				}
			}

			for (conditional_effect ceff : t.ceff) {
				for (int positive = 0; positive < 2; positive ++){
					if ((neg_pred.count(ceff.effect.predicate) && internalHDDLOutput) || (ceff.effect.positive == positive)){

						dout << "      (when (and";
					
						for (literal l : ceff.condition){
							dout << " (";
							if (!l.positive){
								dout << "not";
								if (internalHDDLOutput) dout << "_";
								else dout << " (";
							}
							dout << sanitise(l.predicate);
							for (string v : l.arguments) dout << " " << sanitise(v);
							if (!l.positive && !internalHDDLOutput) dout << ")";
							dout << ")";
						}

						dout << ") (";

						// actual effect
						if (!positive) dout << "not (";
						dout << ((ceff.effect.positive == positive) ? "" : "not_") << sanitise(ceff.effect.predicate);
						for (string v : ceff.effect.arguments) dout << " " << sanitise(v);
						if (!positive) dout << ")";

						dout << "))" << endl;
					}
				}
			}
			
			if (hasActionCosts){
				for (auto c : t.costExpression){
					dout << "      (increase (" << sanitise(metric_target) << ") ";
					if (c.isConstantCostExpression)
						 dout << c.costValue << ")" << endl;
					else {
						dout << "(" << sanitise(c.predicate);
						for (string v : c.arguments) dout << " " << sanitise(v);
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
	if (internalHDDLOutput){
		for (auto x : sorts) for (string s : x.second)
			pout << "    " << sanitise(s) << " - " << sanitise(x.first) << endl;
	} else {
		for (auto [c,s] : sortOfElement)
			pout << "    " << sanitise(c) << " - " << sanitise(s) << endl;
	}
	pout << "  )" << endl;


	bool instance_is_classical = true;
	for (task t : abstract_tasks)
		if (t.name == "__top") instance_is_classical = false;

	if (! instance_is_classical){
		pout << "  (:htn" << endl;
		pout << "    :parameters ()" << endl;
		pout << "    :subtasks (and (";
		if (internalHDDLOutput) pout << "t";
		else pout << "US";
		pout << "__top))" << endl;
		pout << "  )" << endl;
	}

	pout << "  (:init" << endl;
	for (auto gl : init){
		if (!gl.positive && !internalHDDLOutput) continue; // don't output negatives in normal mode
		pout << "    (";
	   	if (!gl.positive){
		   pout << "not";
		   if (internalHDDLOutput) pout << "_";
		   else pout << " (";
		}
		pout << sanitise(gl.predicate);
		for (string c : gl.args) pout << " " << sanitise(c);
		if (!gl.positive && !internalHDDLOutput) pout << ")";
		pout << ")" << endl;
	}
	for (auto [oldType,predicate] : typeConstraint){
		for (string c : sorts[oldType])
			pout << "    (" << sanitise(predicate) << " " << sanitise(c) << ")" << endl;
	}

	// metric 
	if (hasActionCosts)
		for (auto f : init_functions){
			pout << "    (= (" << sanitise(f.first.predicate);
			for (auto c : f.first.args) pout << " " << sanitise(c);
			pout << ") " << f.second << ")" << endl;
		}

	pout << "  )" << endl;

	if (goal.size()){
		pout << "  (:goal (and" << endl;
		for (auto gl : goal){
			pout << "    (";
			if (!gl.positive) {
				pout << "not";
				if (internalHDDLOutput) pout << "_";
				else pout << " (";
			}
			pout << sanitise(gl.predicate);
			for (string c : gl.args) pout << " " << sanitise(c);
			if (!gl.positive && !internalHDDLOutput) pout << ")";
			pout << ")" << endl;
		}
		pout << "  ))" << endl;
	}

	if (hasActionCosts)
		pout << "  (:metric minimize (" << sanitise(metric_target) << "))" << endl;


	pout << ")" << endl;
}
