#include "parsetree.hpp"
#include <iostream>
#include <cassert>

int task_id_counter = 0;

void general_formula::negate(){
	if (this->type == EMPTY) return;
	else if (this->type == AND) this->type = OR;
	else if (this->type == OR) this->type = AND;
	else if (this->type == FORALL) this->type = EXISTS;
	else if (this->type == EXISTS) this->type = FORALL;
	else if (this->type == EQUAL) this->type = NOTEQUAL;
	else if (this->type == NOTEQUAL) this->type = EQUAL;
	else if (this->type == ATOM) this->type = NOTATOM;
	else if (this->type == NOTATOM) this->type = ATOM;
	else if (this->type == WHEN) assert(false); // conditional effect cannot be negated 
	else if (this->type == VALUE) assert(false); // conditional effect cannot be negated 
	else if (this->type == COST_CHANGE) assert(false); // conditional effect cannot be negated 
	else if (this->type == COST) assert(false); // conditional effect cannot be negated 

	for(auto sub : this->subformulae) sub->negate();
}


bool general_formula::isEmpty(){
	if (this->type == EMPTY) return true;

	if (this->type == ATOM) return false;
	if (this->type == NOTATOM) return false;
	if (this->type == EQUAL) return false;
	if (this->type == NOTEQUAL) return false;
	
	if (this->type == VALUE) return false;
	if (this->type == COST) return false;
	if (this->type == COST_CHANGE) return false;


	for(auto sub : this->subformulae) if (!sub->isEmpty()) return false;

	return true;
}


additional_variables general_formula::variables_for_constants(){
	additional_variables ret;
	ret.insert(this->arguments.newVar.begin(),this->arguments.newVar.end());
	for (general_formula* sf : this->subformulae){
		additional_variables sfr = sf->variables_for_constants();
		ret.insert(sfr.begin(),sfr.end());
	}
	return ret;
}

string sort_for_const(string c){
	string s = "sort_for_" + c;
	sorts[s].insert(c);
	return s;
}

general_formula* general_formula::copyReplace(map<string,string> & replace){
	general_formula* ret = new general_formula();
	ret->type = this->type;
	ret->qvariables = this->qvariables;
	ret->arg1 = this->arg1;
	if (replace.count(ret->arg1)) ret->arg1 = replace[ret->arg1];
	ret->arg2 = this->arg2;
	if (replace.count(ret->arg2)) ret->arg2 = replace[ret->arg2];
	ret->value = this->value;

	ret->predicate = this->predicate;
	for (auto sub : this->subformulae) ret->subformulae.push_back(sub->copyReplace(replace));
	
	ret->arguments = this->arguments;
	for(unsigned int i = 0; i < ret->arguments.vars.size(); i++)
		if (replace.count(ret->arguments.vars[i]))
				ret->arguments.vars[i] = replace[ret->arguments.vars[i]];
	
	return ret;
}

// hard expansion of formulae. This can grow up to exponentially, but is currently the only thing we can do about disjunctions.
// this will also handle forall and exists quantors by expansion
// sorts must have been parsed and expanded prior to this call
vector<pair<pair<vector<literal>,vector<literal> >, additional_variables> > general_formula::expand(){
	vector<pair<pair<vector<literal>,vector<literal> >, additional_variables> > ret;

	if (this->type == EMPTY || (this->subformulae.size() == 0 &&
				(this->type == AND || this->type == OR || this->type == FORALL || this->type == EXISTS))){
		vector<literal> empty; additional_variables none;
		ret.push_back(make_pair(make_pair(empty,empty),none));
		return ret;
	}

	// generate a big conjunction for forall and expand it
	if (this->type == FORALL){
		general_formula and_formula;
		and_formula.type = AND;
		additional_variables avs;
		vector<map<string,string> > var_replace;
		map<string,string> empty;
		var_replace.push_back(empty);
		int counter = 0;
		for(pair<string,string> var : this->qvariables.vars) {
			vector<map<string,string> > old_var_replace = var_replace;
			var_replace.clear();

			for(string c : sorts[var.second]){
				string newSort = sort_for_const(c);
				string newVar = var.first + "_" + to_string(counter); counter++;
				avs.insert(make_pair(newVar,newSort));
				for (map<string,string> old : old_var_replace){
					old[var.first] = newVar; // add new variable binding
					var_replace.push_back(old);
				}
			}
		}
		for (auto r : var_replace)
			and_formula.subformulae.push_back(this->subformulae[0]->copyReplace(r));

		ret = and_formula.expand();
		// add variables
		for (unsigned int i = 0; i < ret.size(); i++)
				ret[i].second.insert(avs.begin(), avs.end());
	}




	vector<vector<pair<pair<vector<literal>, vector<literal> >, additional_variables> > > subresults;
	for(auto sub : this->subformulae) subresults.push_back(sub->expand());	
	
	// just add all disjuncts to set of literals
	if (this->type == OR) for(auto subres : subresults) for (auto res: subres) ret.push_back(res);

	//
	if (this->type == AND){
		vector<pair<pair<vector<literal>,vector<literal> >, additional_variables> > cur = subresults[0];
		for(unsigned int i = 1; i < subresults.size(); i++){
			vector<pair<pair<vector<literal>, vector<literal> >, additional_variables> > prev = cur;
			cur.clear();
			for(auto next : subresults[i]) for(auto old : prev){
				pair<pair<vector<literal>, vector<literal> >, additional_variables> combined = old;
				for(literal l : next.first.first) combined.first.first.push_back(l);
				for(literal l : next.first.second) combined.first.second.push_back(l);
				for(auto v : next.second) combined.second.insert(v);
				cur.push_back(combined);
			}
		}
		ret = cur;
	}

	// add additional variables for every quantified variable. We have to do this for every possible instance of the precondition below	
	if (this->type == EXISTS){
		vector<pair<pair<vector<literal>, vector<literal> >, additional_variables> > cur = subresults[0];	
		for(pair<string,string> var : this->qvariables.vars){
			vector<pair<pair<vector<literal>, vector<literal> >, additional_variables> > prev = cur;
			cur.clear();
			for(auto old : prev){
				pair<pair<vector<literal>,vector<literal> >,  additional_variables>	combined = old;
				combined.second.insert(var);
				cur.push_back(combined);
			}
		}
		ret = cur;
		// we cannot generate more possible expansions.
		assert(ret.size() == subresults[0].size());
	}


	if (this->type == ATOM || this->type == NOTATOM || this->type == COST) {
		vector<literal> ls;
		literal l;
		l.positive = this->type == ATOM;
		l.isConstantCostExpression = false;
		l.isCostChangeExpression = false;
		l.predicate = this->predicate;
		l.arguments = this->arguments.vars;
		ls.push_back(l);

		additional_variables vars = this->arguments.newVar;
		vector<literal> empty;
		ret.push_back(make_pair(make_pair(ls,empty),vars));	
	}

	if (this->type == VALUE){
		vector<literal> ls;
		literal l;
		l.positive = this->type == ATOM;
		l.isConstantCostExpression = true;
		l.costValue = this->value;
		ls.push_back(l);

		additional_variables vars;
		vector<literal> empty;
		ret.push_back(make_pair(make_pair(ls,empty),vars));	
	}
	
	if (this->type == COST_CHANGE){
		assert(subresults.size() == 2);
		assert(subresults[0].size() == 1);
		assert(subresults[0][0].first.first.size() == 1);
		assert(subresults[0][0].first.first[0].predicate == metric_target);
		assert(subresults[0][0].first.first[0].arguments.size() == 0);

		assert(subresults[1].size() == 1);
		assert(subresults[1][0].first.first.size() == 1);
		subresults[1][0].first.first[0].isCostChangeExpression = true;
		ret.push_back(subresults[1][0]);
	}

	// add dummy literal for equal and not equal constraints
	if (this->type == EQUAL || this->type == NOTEQUAL || this->type == OFSORT || this->type == NOTOFSORT){
		vector<literal> ls;
		literal l;
		l.positive = this->type == EQUAL || this->type == OFSORT;
		if (this->type == EQUAL || this->type == NOTEQUAL)
			l.predicate = dummy_equal_literal;
		else
			l.predicate = dummy_ofsort_literal; 
		l.arguments.push_back(this->arg1);
		l.arguments.push_back(this->arg2);
		ls.push_back(l);

		additional_variables vars; // no new vars. Never
		vector<literal> empty;
		ret.push_back(make_pair(make_pair(ls,empty),vars));	
		
	}

	if (this->type == WHEN) {
		for (auto expanded_condition : subresults[0]) for (auto expanded_effect : subresults[1]){
			vector<literal> eff = expanded_effect.first.first;
			vector<literal> cond = expanded_effect.first.second;
			cond.insert(cond.end(), expanded_condition.first.first.begin(), expanded_condition.first.first.end());
			additional_variables avs = expanded_effect.second;
			avs.insert(expanded_condition.second.begin(), expanded_condition.second.end());
			ret.push_back(make_pair(make_pair(eff,cond),avs));
		}

		general_formula cond = *(this->subformulae[0]);
		cond.negate();
		for (auto expanded_condition : cond.expand()){
			expanded_condition.first.second = expanded_condition.first.first;
			expanded_condition.first.first.clear();
			ret.push_back(expanded_condition);
		}
	}

	return ret;
}



