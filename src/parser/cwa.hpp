#ifndef __CWA
#define __CWA

#include <vector>
#include <string>
#include "parsetree.hpp"

using namespace std;

struct ground_literal{
	string predicate;
	bool positive;
	vector<string> args;
};

bool operator< (const ground_literal& lhs, const ground_literal& rhs);

void flatten_goal();
void compute_cwa();

extern vector<ground_literal> init;
extern vector<pair<ground_literal,int>> init_functions;
extern vector<ground_literal> goal;
extern general_formula* goal_formula;

#endif
