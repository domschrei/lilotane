#include <iostream>
#include <cassert>
#include <algorithm>

#include "properties.hpp"
#include "parsetree.hpp"

using namespace std;

vector<string> liftedTopSort;

void liftedPropertyTopSortDFS(string cur, map<string,vector<string>> & adj, map<string, int> & colour){
	assert (colour[cur] != 1);
	if (colour[cur]) return;

	colour[cur] = 1;
	for (string & nei : adj[cur]) liftedPropertyTopSortDFS(nei,adj,colour);
	colour[cur] = 2;

	liftedTopSort.push_back(cur);
}

void liftedPropertyTopSort(parsed_task_network* tn){
	liftedTopSort.clear();
	map<string,vector<string>> adj;
	for (pair<string,string> * nei : tn->ordering)
		adj[nei->first].push_back(nei->second);

	map<string,int> colour;

	for (sub_task* t : tn->tasks)
		if (!colour[t->id]) liftedPropertyTopSortDFS(t->id, adj, colour);

	reverse(liftedTopSort.begin(), liftedTopSort.end());
}

bool recursionFindingDFS(string cur, map<string,int> & colour){
	if (colour[cur] == 1) return true;
	if (colour[cur]) return false;

	colour[cur] = 1;
	for (parsed_method & m : parsed_methods[cur])
		for (sub_task* sub : m.tn->tasks)
			if (recursionFindingDFS(sub->task,colour)) return true;
	colour[cur] = 2;

	return false;
}

void printProperties(){
	// determine lifted instance properties and print them
	
	// 1. Total order
	bool totalOrder = true;
	for (auto & [_,ms] : parsed_methods)
		for (auto & m : ms){
			if (m.tn->tasks.size() < 2) continue;
			// do topsort
			liftedPropertyTopSort(m.tn);

			// check whether it is a total order
			for (size_t i = 1; i < liftedTopSort.size(); i++){
				bool orderEnforced = false;
				for (pair<string,string> * nei : m.tn->ordering)
					if (nei->first == liftedTopSort[i-1] && nei->second == liftedTopSort[i]){
						orderEnforced = true;
						break;
					}

				if (! orderEnforced){
					totalOrder = false;
					break;
				}
			}
			if (! totalOrder) break;
		}


	cout << "Instance is totally-ordered: ";
	if (totalOrder) cout << "yes" << endl; else cout << "no" << endl;


	// 2. recursion
	map<string,int> colour;
	bool hasLiftedRecursion = recursionFindingDFS("__top",colour);
	
	cout << "Instance is acyclic:         ";
	if (!hasLiftedRecursion) cout << "yes" << endl; else cout << "no" << endl;


}

