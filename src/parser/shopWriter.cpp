#include "shopWriter.hpp"
#include "cwa.hpp"
#include <iostream>
#include <algorithm>
#include <cassert>
#include <variant>
#include <strings.h> // for ffs
#include <bitset>


const string shop_type_predicate_prefix = "(type_";

bool shop_1_compatability_mode = false;

string sanitise(string in){
	if (in == "call") in = "_call";
	
	if (!shop_1_compatability_mode) return in;
	
	replace(in.begin(),in.end(),'_','-');
	// replace leading minus by training
	if (in[0] == '-') return "x" + in;
	return in;
}

void write_literal_list_SHOP(ostream & dout, vector<literal> & literals){
	bool first = true;
	for (literal & l : literals){
		if (!first) dout << " "; else first=false;
		dout << "(";
		if (!l.positive) dout << "not (";
		dout << sanitise(l.predicate);
		for (string arg : l.arguments) dout << " " << sanitise(arg);
		if (!l.positive) dout << ")";
		dout << ")";
	}
}


struct shop_order {
	bool isParallel; // if not it is sequential
	vector<variant<string,shop_order*>> elements;
};


bool shop_order_matrix[1024][1024];

shop_order* extract_shop_order_dfs(vector<string> & ids, vector<int> bitmask);


shop_order* try_extract_shop_order(vector<string> & ids, vector<int> bitmask, set<int> bitmask_set, set<int> split, bool & success){
	success = false;

	// check
	bool anyOrder = false;
	bool allAfter = true;

	for (size_t ax = 0; ax < ids.size(); ax++){
		if (! bitmask_set.count(ax)) continue;
		if (! split.count(ax)) continue;
		for (size_t bx = 0; bx < ids.size(); bx++){
			if (! bitmask_set.count(bx)) continue;
			if (  split.count(bx)) continue;

			if (shop_order_matrix[ax][bx] || shop_order_matrix[bx][ax]) anyOrder = true;
			if (!shop_order_matrix[ax][bx]) allAfter = false;
		}
	}

	if (anyOrder && (!allAfter)) return 0;
	vector<int> first_set,second_set;
	for (int & b : bitmask)
		if (split.count(b)) first_set.push_back(b);
		else second_set.push_back(b);

	shop_order* first = extract_shop_order_dfs(ids, first_set);
	shop_order* second = extract_shop_order_dfs(ids, second_set);

	shop_order* ret = new shop_order;
	if (first) ret->elements.push_back(first);
	else ret->elements.push_back(ids[*first_set.begin()]);
	
	if (second) ret->elements.push_back(second);
	else ret->elements.push_back(ids[*second_set.begin()]);


	if (!anyOrder) ret->isParallel = true;
	else ret->isParallel = false;

	success = true;
	return ret;
}


shop_order* extract_shop_order_dfs(vector<string> & ids, vector<int> bitmask){
	//cout << "try for:";
	//for (int b : bitmask) cout << " " << b;
	//cout << endl;

	if (bitmask.size() == 1) return 0;
	set<int> bitmask_set;
	for (int &b : bitmask) bitmask_set.insert(b);
	
	for (size_t e = 0; e < bitmask.size(); e++){
		set<int> split; split.insert(bitmask[e]);
		bool succ;
		//cout << "try split on: " << e << endl;
		shop_order* ord = try_extract_shop_order(ids,bitmask,bitmask_set,split,succ);
		if (succ) return ord;
	}

	// try to find a subset to split on
	for (int i = 1; i < (1 << bitmask.size()); i++){
		set<int> split;
		for (int b = 0; b < 32; b++) if (i & (1 << b)) split.insert(bitmask[b]);
		bool succ;
		shop_order* ord = try_extract_shop_order(ids,bitmask,bitmask_set,split,succ);
		if (succ) return ord;
	}

	assert(false);
	return NULL;
}


shop_order* extract_shop_order(vector<pair<string,string>> ordering, vector<string> ids){
	if (!ids.size()) return NULL;
	if (ids.size() == 1) {
		shop_order* ret = new shop_order;
		ret->isParallel = false;
		ret->elements.push_back(ids[0]);
		return ret;
	}
	
	map<string,int> ids_to_int;
	for (size_t i = 0; i < ids.size(); i++) ids_to_int[ids[i]] = i;
	for (size_t i = 0; i < ids.size(); i++)
		for (size_t j = 0; j < ids.size(); j++)
			shop_order_matrix[i][j] = false;

	for (pair<string,string> & o : ordering)
		shop_order_matrix[ids_to_int[o.first]][ids_to_int[o.second]] = true;
	
	for (size_t k = 0; k < ids.size(); k++)
		for (size_t i = 0; i < ids.size(); i++)
			for (size_t j = 0; j < ids.size(); j++)
				if (shop_order_matrix[i][k] && shop_order_matrix[k][j])
					shop_order_matrix[i][j] = true;

	vector<int> bitmask;
	for (size_t i = 0; i < ids.size(); i++) bitmask.push_back(i);

	return extract_shop_order_dfs(ids,bitmask);
}

shop_order* simplify_shop_order(shop_order* ord){
	vector<variant<string,shop_order*>> sub;
	for (variant<string,shop_order*> s : ord->elements)
		if (holds_alternative<string>(s))
			sub.push_back(s);
		else
			sub.push_back(simplify_shop_order(get<shop_order*>(s)));

	ord->elements.clear();
	for (variant<string,shop_order*> s : sub)
		if (holds_alternative<string>(s))
			ord->elements.push_back(s);
		else if (ord->isParallel != get<shop_order*>(s)->isParallel)
			ord->elements.push_back(s);
		else {
			// expand
			for (variant<string,shop_order*> ss : get<shop_order*>(s)->elements)
				ord->elements.push_back(ss);
		}
	return ord;
}


void write_shop_order(ostream & dout, shop_order* order, map<string,plan_step> & idmap, set<string> names_of_primitives){
	if (!order) return;
	dout << "(";
	if (order->isParallel) dout << ":unordered ";

	bool first = true;
	for (variant<string,shop_order*> elem : order->elements){
		if (first) first = false; else dout << " ";
		if (holds_alternative<string>(elem)){
			plan_step ps = idmap[get<string>(elem)];
			dout << "(";
			if (names_of_primitives.count(ps.task)) dout << "!";
		   	dout << sanitise(ps.task);
			for (string & arg : ps.args) dout << " " << sanitise(arg);
			dout << ")";
		} else
			write_shop_order(dout, get<shop_order*>(elem),idmap, names_of_primitives);
	}
	
	dout << ")";
}


void write_instance_as_SHOP(ostream & dout, ostream & pout){
	dout << "(defdomain domain (" << endl;

	set<string> names_of_primitives; // this is needed for writing the !'s in methods
	// output all actions, in shop they are named operators
	for (task & prim : primitive_tasks){
		if (prim.name.rfind(method_precondition_action_name, 0) == 0) continue; // don't output method precondition action ... they will be part of the output
		names_of_primitives.insert(prim.name);
		dout << "  (:operator (!" << sanitise(prim.name);
		// arguments
		for (pair<string,string> var : prim.vars)
			dout << " " << sanitise(var.first);
		dout << ")" << endl;
		
		// precondition
		dout << "    ;; preconditions" << endl;
		dout << "    (" << endl;
		dout << "      ";
		for (size_t i = 0; i < prim.vars.size(); i++){
			if (i) dout << " ";
			dout << sanitise(shop_type_predicate_prefix) << sanitise(prim.vars[i].second) << " " << sanitise(prim.vars[i].first) << ")";
		}
		dout << endl << "      ";
		write_literal_list_SHOP(dout, prim.prec);
		// write constraints
		for (literal & constraint : prim.constraints){
			dout << " (";
			if (!constraint.positive) dout << "not (";
			dout << "call ";
			if (shop_1_compatability_mode) dout << "equal"; else dout << "=";
			for (string arg : constraint.arguments) dout << " " << sanitise(arg);
			if (!constraint.positive) dout << ")";
			dout << ")";
		}
		dout << endl << "    )" << endl;

		vector<literal> add, del;
		for (literal & l : prim.eff)
			if (l.positive) add.push_back(l); else {
				del.push_back(l);
				del[del.size()-1].positive = true;
			}


		// delete effects
		dout << "    ;; delete effects" << endl;
		dout << "    (";
		write_literal_list_SHOP(dout, del);
		dout << ")" << endl;

		// add effects
		dout << "    ;; add effects" << endl;
		dout << "    (";
		write_literal_list_SHOP(dout, add);
		dout << ")" << endl;

		// costs
		if (prim.costExpression.size()){
			int value = 0;
			for (literal & cexpr : prim.costExpression){
				if (!cexpr.isConstantCostExpression){
					cerr << "Primitive action \"" << prim.name << "\" does not have constant costs" << endl;
					return;
				}

				value += cexpr.costValue;
			}
			dout << "    " << value << endl;
		}

		dout << "  )" << endl;
	}


	for (method & m : methods){
		dout << "  (:method (" << sanitise(m.at);
		for (string & atarg : m.atargs)
			dout << " " << sanitise(atarg);
		dout << ")" << endl;
		// method name
		dout << "    " << sanitise(m.name) << endl;

		// find the corresponding at
		task at; bool found = false;
		for (task & a : abstract_tasks)
			if (a.name == m.at){
				at = a;
				found = true;
				break;
			}

		if (! found) {
			cerr << "method " << m.name << " decomposes unknown task " << m.at << endl;
			_Exit(1);
		}


		// method precondition
		dout << "    ("  << endl << "      ";
		// typing constraints of the AT
		for (size_t i = 0; i < at.vars.size(); i++){
			if (i) dout << " ";
			dout << sanitise(shop_type_predicate_prefix) << sanitise(at.vars[i].second) << " " << sanitise(m.atargs[i]) << ")";
		}
		dout << endl << "      ";
		// typing of the method
		bool first = true;
		for (pair<string,string> & v : m.vars){
			if (first) first = false; else dout << " ";
			dout << sanitise(shop_type_predicate_prefix) << sanitise(v.second) << " " << sanitise(v.first) << ")";
		}
		dout << endl << "      ";
		// method precondition in the input
		for (plan_step & ps : m.ps){
			if (ps.task.rfind(method_precondition_action_name, 0)) continue;
			found = false;
			for (task & p : primitive_tasks)
				if (p.name == ps.task){
					found = true;
					assert(p.eff.size() == 0);
					write_literal_list_SHOP(dout,p.prec);
					break;
				}
			assert(found);
		}

		dout << endl << "    )" << endl;

		// subtasks
		
		vector<string> ids;
		map<string,plan_step> idmap;
		for (plan_step & ps : m.ps){
			if (ps.task.rfind(method_precondition_action_name, 0) == 0) continue;
			ids.push_back(ps.id);
			idmap[ps.id] = ps;
		}

		vector<pair<string,string>> filtered_ordering;
		for (pair<string,string> & ord : m.ordering)
			if (idmap.count(ord.first) && idmap.count(ord.second))
				filtered_ordering.push_back(ord);
		// else the ordering pertains to the method precondition. At this point, there can be only one so just ignoring these constraints is ok -- they cannot introduce transitive constraints, that are relevant
		
		if (ids.size()){	
			shop_order* order = extract_shop_order(filtered_ordering,ids);
			order = simplify_shop_order(order);
			dout << "    ";
			write_shop_order(dout,order,idmap,names_of_primitives);
			dout << endl;	
		} else {
			// empty method
			dout << "    ()" << endl;
		}
		


		dout << "  )" << endl;
	}
	
	
	dout << "))" << endl;


	//-------------------------------------------
	// write the problem instance
	pout << "(defproblem problem domain " << endl;
	pout << "  (" << endl;
	// initial state
	for (ground_literal & gl : init){
		if (!gl.positive) continue;
		pout << "    (" << sanitise(gl.predicate);
		for (string & arg : gl.args)
			pout << " " << sanitise(arg);
		pout << ")" << endl;
	}
	
	// type assertions
	for (auto & entry : sorts){
		for (const string & constant : entry.second){
			pout << "    " << sanitise(shop_type_predicate_prefix) << sanitise(entry.first) << " " << sanitise(constant) << ")" << endl;
		}	
	}

	pout << "  )" << endl;
	pout << "  ((" << sanitise("__top") << "))" << endl;
	pout << ")" << endl;

}

