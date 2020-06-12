#include "parsetree.hpp"
#include "cwa.hpp"
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

set<string> general_formula::occuringUnQuantifiedVariables(){
	set<string> ret;

	if (this->type == EMPTY) return ret;

	if (this->type == AND || this->type == OR){
		for (general_formula* sub : this->subformulae){
			set<string> subres = sub->occuringUnQuantifiedVariables();
			ret.insert(subres.begin(), subres.end());
		}
		return ret;
	}
	
	if (this->type == FORALL || this->type == EXISTS) {
		set<string> subres = this->subformulae[0]->occuringUnQuantifiedVariables();

		for (auto [var,_] : this->qvariables.vars){
			subres.erase(var);
		}

		return subres;
	}

	if (this->type == ATOM || this->type == NOTATOM){
		ret.insert(this->arguments.vars.begin(), this->arguments.vars.end());
		return ret;
	}
	
	if (this->type == EQUAL || this->type == NOTEQUAL || 
			this->type == OFSORT || this->type == NOTOFSORT){
		ret.insert(this->arg1);
		ret.insert(this->arg2);
		return ret;
	}

	// things that I don't want to support ...
	if (this->type == WHEN) assert(false);
	if (this->type == VALUE) assert(false);
	if (this->type == COST_CHANGE) assert(false);
	if (this->type == COST) assert(false);
	if (this->type == OFSORT) assert(false);
	if (this->type == NOTOFSORT) assert(false);

	assert(false);
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

bool general_formula::isDisjunctive(){
	if (this->type == EMPTY) return false;
	else if (this->type == EQUAL) return false;
	else if (this->type == NOTEQUAL) return false;
	else if (this->type == ATOM) return false;
	else if (this->type == NOTATOM) return false;
	else if (this->type == VALUE) return false;
	else if (this->type == COST_CHANGE) return false;
	else if (this->type == COST) return false;
	// or and when are disjunctive	
	else if (this->type == OR) return true;
	else if (this->type == WHEN) return true;

	// else AND, FORALL, EXISTS
	
	for(auto sub : this->subformulae) if (sub->isDisjunctive()) return true;
	
	return false;
}

int global_exists_variable_counter = 0;

// hard expansion of formulae. This can grow up to exponentially, but is currently the only thing we can do about disjunctions.
// this will also handle forall and exists quantors by expansion
// sorts must have been parsed and expanded prior to this call
vector<pair<pair<vector<variant<literal,conditional_effect>>,vector<literal> >, additional_variables> > general_formula::expand(bool compileConditionalEffects){
	vector<pair<pair<vector<variant<literal,conditional_effect>>,vector<literal> >, additional_variables> > ret;

	if (this->type == EMPTY || (this->subformulae.size() == 0 &&
				(this->type == AND || this->type == OR || this->type == FORALL || this->type == EXISTS))){
		vector<variant<literal,conditional_effect>> empty1;
		vector<literal> empty2;
		additional_variables none;
		ret.push_back(make_pair(make_pair(empty1,empty2),none));
		return ret;
	}

	// generate a big conjunction for forall and expand it
	if (this->type == FORALL){
		general_formula and_formula;
		and_formula.type = AND;
		auto [var_replace,avs] = forallVariableReplacement();

		for (auto r : var_replace)
			and_formula.subformulae.push_back(this->subformulae[0]->copyReplace(r));

		ret = and_formula.expand(compileConditionalEffects);
		// add variables
		for (unsigned int i = 0; i < ret.size(); i++)
				ret[i].second.insert(avs.begin(), avs.end());
	}


	// add additional variables for every quantified variable. We have to do this for every possible instance of the precondition below	
	if (this->type == EXISTS){
		map<string,string> var_replace = existsVariableReplacement();
		this->subformulae[0] = this->subformulae[0]->copyReplace(var_replace);

		vector<pair<pair<vector<variant<literal,conditional_effect>>, vector<literal> >, additional_variables> > cur = 
			this->subformulae[0]->expand(compileConditionalEffects);
		for(pair<string,string> var : this->qvariables.vars){
			vector<pair<pair<vector<variant<literal,conditional_effect>>, vector<literal> >, additional_variables> > prev = cur;
			cur.clear();
			string newName = var.first + "_" + to_string(++global_exists_variable_counter);

			
			for(auto old : prev){
				pair<pair<vector<variant<literal,conditional_effect>>,vector<literal> >,  additional_variables>	combined = old;
				combined.second.insert(make_pair(var_replace[var.first],var.second));
				cur.push_back(combined);
			}
		}
		return cur;
	}



	vector<vector<pair<pair<vector<variant<literal,conditional_effect>>, vector<literal> >, additional_variables> > > subresults;
	for(auto sub : this->subformulae) subresults.push_back(sub->expand(compileConditionalEffects));	
	
	// just add all disjuncts to set of literals
	if (this->type == OR) for(auto subres : subresults) for (auto res: subres) ret.push_back(res);

	//
	if (this->type == AND){
		vector<pair<pair<vector<variant<literal,conditional_effect>>,vector<literal> >, additional_variables> > cur = subresults[0];
		for(unsigned int i = 1; i < subresults.size(); i++){
			vector<pair<pair<vector<variant<literal,conditional_effect>>, vector<literal> >, additional_variables> > prev = cur;
			cur.clear();
			for(auto next : subresults[i]) for(auto old : prev){
				pair<pair<vector<variant<literal,conditional_effect>>, vector<literal> >, additional_variables> combined = old;
				for(variant<literal,conditional_effect> l : next.first.first) combined.first.first.push_back(l);
				for(literal l : next.first.second) combined.first.second.push_back(l);
				for(auto v : next.second) combined.second.insert(v);
				cur.push_back(combined);
			}
		}
		ret = cur;
	}


	if (this->type == ATOM || this->type == NOTATOM || this->type == COST) {
		vector<variant<literal,conditional_effect>> ls;
		literal l = this->atomLiteral();
		ls.push_back(l);

		additional_variables vars = this->arguments.newVar;
		vector<literal> empty;
		ret.push_back(make_pair(make_pair(ls,empty),vars));	
	}

	if (this->type == VALUE){
		vector<variant<literal,conditional_effect>> ls;
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
		assert(holds_alternative<literal>(subresults[0][0].first.first[0]));
		assert(get<literal>(subresults[0][0].first.first[0]).predicate == metric_target);
		assert(get<literal>(subresults[0][0].first.first[0]).arguments.size() == 0);

		assert(subresults[1].size() == 1);
		assert(subresults[1][0].first.first.size() == 1);
		get<literal>(subresults[1][0].first.first[0]).isCostChangeExpression = true;
		ret.push_back(subresults[1][0]);
	}

	// add dummy literal for equal and not equal constraints
	if (this->type == EQUAL || this->type == NOTEQUAL || this->type == OFSORT || this->type == NOTOFSORT){
		vector<variant<literal,conditional_effect>> ls;
		literal l = this->equalsLiteral();
		ls.push_back(l);

		additional_variables vars; // no new vars. Never
		vector<literal> empty;
		ret.push_back(make_pair(make_pair(ls,empty),vars));	
		
	}

	if (this->type == WHEN) {
		// condition might be a disjunction ..
		// effect might have multiple expansions (i.e. be disjunctive) we here assume that this means angelic non-determinism
		for (auto expanded_condition : subresults[0]) for (auto expanded_effect : subresults[1]){
			vector<variant<literal,conditional_effect>> eff = expanded_effect.first.first;
			vector<literal> cond = expanded_effect.first.second;
			
			for (variant<literal,conditional_effect> x : expanded_condition.first.first)
				if (holds_alternative<literal>(x)){
					cond.push_back(get<literal>(x));
				} else assert(false); // conditional effect inside condition
			
			
			additional_variables avs = expanded_effect.second;
			avs.insert(expanded_condition.second.begin(), expanded_condition.second.end());

			if (compileConditionalEffects)
				ret.push_back(make_pair(make_pair(eff,cond),avs));
			else {
				// we have to build the conditional effects
				vector<variant<literal,conditional_effect>> newEff;
				for (variant<literal,conditional_effect> & e : eff){
					if (holds_alternative<literal>(e)){
						newEff.push_back(conditional_effect(cond,get<literal>(e)));
					} else {
						// nested conditional effect
						vector<literal> innercond = get<conditional_effect>(e).condition;
						literal innerE = get<conditional_effect>(e).effect;
						innercond.insert(innercond.end(), cond.begin(),cond.end());
						
						newEff.push_back(conditional_effect(innercond,innerE));
					}
				}
				vector<literal> empty;
				ret.push_back(make_pair(make_pair(newEff,empty),avs));
			}
		}
			
			
		if (compileConditionalEffects){
			// remove conditional effects by compiling them into multiple actions ...
			general_formula cond = *(this->subformulae[0]);
			cond.negate();
			for (auto expanded_condition : cond.expand(false)){ // condition cannot contain conditional effect!
				expanded_condition.first.second.clear();
				for (variant<literal,conditional_effect> x : expanded_condition.first.first)
					if (holds_alternative<literal>(x)){
						expanded_condition.first.second.push_back(get<literal>(x));
					} else assert(false); // conditional effect inside condition
				expanded_condition.first.first.clear();
				ret.push_back(expanded_condition);
			}
		}
	}
	return ret;
}

literal general_formula::equalsLiteral(){
	assert(this->type == EQUAL || this->type == NOTEQUAL || 
			this->type == OFSORT || this->type == NOTOFSORT);
	
	literal l;
	l.positive = this->type == EQUAL || this->type == OFSORT;
	if (this->type == EQUAL || this->type == NOTEQUAL)
		l.predicate = dummy_equal_literal;
	else
		l.predicate = dummy_ofsort_literal; 
	l.arguments.push_back(this->arg1);
	l.arguments.push_back(this->arg2);
	return l;
}


literal general_formula::atomLiteral(){
	assert(this->type == ATOM || this->type == NOTATOM);
	
	literal l;
	l.positive = this->type == ATOM;
	l.isConstantCostExpression = false;
	l.isCostChangeExpression = false;
	l.predicate = this->predicate;
	l.arguments = this->arguments.vars;

	return l;
}



pair<vector<map<string,string> >, additional_variables> general_formula::forallVariableReplacement(){
	assert(this->type == FORALL);

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
	
	return make_pair(var_replace, avs);
}

map<string,string> general_formula::existsVariableReplacement(){
	map<string,string> var_replace;
	for(pair<string,string> var : this->qvariables.vars){
		string newName = var.first + "_" + to_string(++global_exists_variable_counter);
		var_replace[var.first] = newName;
	}
	return var_replace;
}


void compile_goal_into_action(){
	if (goal_formula == NULL) return;
	if (goal_formula->isEmpty()) return;
	
	parsed_task t;
	t.name = "goal_action";
	t.arguments = new var_declaration();
	t.prec = goal_formula;
	t.eff = new general_formula(); t.eff->type = EMPTY;
	parsed_primitive.push_back(t);

	// add as last task into top method
	assert(parsed_methods["__top"].size() == 1);
	parsed_method m = parsed_methods["__top"][0];
	parsed_methods["__top"].clear();
	sub_task * g = new sub_task();	
	g->id = "__goal_id";
	g->task = t.name;
	g->arguments = new var_and_const();
	
	// ordering
	for (sub_task* st : m.tn->tasks){
		pair<string,string>* o = new pair<string,string>();
		o->first = st->id;
		o->second = g->id;
		m.tn->ordering.push_back(o);
	}
	
	m.tn->tasks.push_back(g);

	
	// add the method back in
	parsed_methods["__top"].push_back(m);


	// clear the goal formula
	goal_formula = NULL;
}

