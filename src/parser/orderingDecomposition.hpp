#ifndef __ORDERINGDECOMPOSITION
#define __ORDERINGDECOMPOSITION 


#include <vector>
#include <variant>
#include <string>

using namespace std;

struct order_decomposition {
	bool isParallel; // if not it is sequential
	vector<variant<string,order_decomposition *>> elements;
};

order_decomposition* extract_order_decomposition(vector<pair<string,string>> ordering, vector<string> ids);
order_decomposition* simplify_order_decomposition(order_decomposition* ord);


#endif
