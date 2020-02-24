#include "orderingDecomposition.hpp"
#include <set>
#include <map>
#include <cassert>

bool order_decomposition_matrix[1024][1024];

order_decomposition* extract_order_decomposition_dfs(vector<string> & ids, vector<int> bitmask);


order_decomposition* try_extract_order_decomposition(vector<string> & ids, vector<int> bitmask, set<int> bitmask_set, set<int> split, bool & success){
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

			if (order_decomposition_matrix[ax][bx] || order_decomposition_matrix[bx][ax]) anyOrder = true;
			if (!order_decomposition_matrix[ax][bx]) allAfter = false;
		}
	}

	if (anyOrder && (!allAfter)) return 0;
	vector<int> first_set,second_set;
	for (int & b : bitmask)
		if (split.count(b)) first_set.push_back(b);
		else second_set.push_back(b);

	order_decomposition* first = extract_order_decomposition_dfs(ids, first_set);
	order_decomposition* second = extract_order_decomposition_dfs(ids, second_set);

	order_decomposition* ret = new order_decomposition;
	if (first) ret->elements.push_back(first);
	else ret->elements.push_back(ids[*first_set.begin()]);
	
	if (second) ret->elements.push_back(second);
	else ret->elements.push_back(ids[*second_set.begin()]);


	if (!anyOrder) ret->isParallel = true;
	else ret->isParallel = false;

	success = true;
	return ret;
}


order_decomposition* extract_order_decomposition_dfs(vector<string> & ids, vector<int> bitmask){
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
		order_decomposition* ord = try_extract_order_decomposition(ids,bitmask,bitmask_set,split,succ);
		if (succ) return ord;
	}

	// try to find a subset to split on
	for (int i = 1; i < (1 << bitmask.size()); i++){
		set<int> split;
		for (int b = 0; b < 32; b++) if (i & (1 << b)) split.insert(bitmask[b]);
		bool succ;
		order_decomposition* ord = try_extract_order_decomposition(ids,bitmask,bitmask_set,split,succ);
		if (succ) return ord;
	}

	assert(false);
	return NULL;
}


order_decomposition* extract_order_decomposition(vector<pair<string,string>> ordering, vector<string> ids){
	if (!ids.size()) return NULL;
	if (ids.size() == 1) {
		order_decomposition* ret = new order_decomposition;
		ret->isParallel = false;
		ret->elements.push_back(ids[0]);
		return ret;
	}
	
	map<string,int> ids_to_int;
	for (size_t i = 0; i < ids.size(); i++) ids_to_int[ids[i]] = i;
	for (size_t i = 0; i < ids.size(); i++)
		for (size_t j = 0; j < ids.size(); j++)
			order_decomposition_matrix[i][j] = false;

	for (pair<string,string> & o : ordering)
		order_decomposition_matrix[ids_to_int[o.first]][ids_to_int[o.second]] = true;
	
	for (size_t k = 0; k < ids.size(); k++)
		for (size_t i = 0; i < ids.size(); i++)
			for (size_t j = 0; j < ids.size(); j++)
				if (order_decomposition_matrix[i][k] && order_decomposition_matrix[k][j])
					order_decomposition_matrix[i][j] = true;

	vector<int> bitmask;
	for (size_t i = 0; i < ids.size(); i++) bitmask.push_back(i);

	return extract_order_decomposition_dfs(ids,bitmask);
}

order_decomposition* simplify_order_decomposition(order_decomposition* ord){
	vector<variant<string,order_decomposition*>> sub;
	for (variant<string,order_decomposition*> s : ord->elements)
		if (holds_alternative<string>(s))
			sub.push_back(s);
		else
			sub.push_back(simplify_order_decomposition(get<order_decomposition*>(s)));

	ord->elements.clear();
	for (variant<string,order_decomposition*> s : sub)
		if (holds_alternative<string>(s))
			ord->elements.push_back(s);
		else if (ord->isParallel != get<order_decomposition*>(s)->isParallel)
			ord->elements.push_back(s);
		else {
			// expand
			for (variant<string,order_decomposition*> ss : get<order_decomposition*>(s)->elements)
				ord->elements.push_back(ss);
		}
	return ord;
}

