#include <cstdio>
#include <iostream>
#include <vector>
#include <map>
#include "parsetree.hpp"
#include "domain.hpp"

using namespace std;

map<string,set<string> > sort_adj;

map<string, int> sort_visi;

void expansion_dfs(string sort){
	if (sort_visi[sort] == 1) {
		cout << "Sort hierarchy contains a cycle ... " << endl;
		exit(3);
	}
	
	sort_visi[sort] = 1;

	for(string subsort : sort_adj[sort]){
		if (sort_visi[sort] != 2) expansion_dfs(subsort); // if not black
		// add constants to myself
		for (string subelem : sorts[subsort]) sorts[sort].insert(subelem);
	}
	sort_visi[sort] = 2;
}

void expand_sorts(){
	for (auto def : sort_definitions){
		if (def.has_parent_sort) sorts[def.parent_sort].size();
		for (string subsort : def.declared_sorts){
			sorts[subsort].size(); // touch to ensure it is contained in the map
			if (def.has_parent_sort) sort_adj[def.parent_sort].insert(subsort);
		}
	}
	
	for(auto s : sorts) expansion_dfs(s.first);
}
