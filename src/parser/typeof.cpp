#include "typeof.hpp"
#include "domain.hpp"
#include "cwa.hpp"
#include "parsetree.hpp"
#include <set>

void create_typeof(){
	// create a sort containing all objects
	for (auto s : sorts) for (string e : s.second) sorts["__object"].insert(e);
	
	// add instances to init
	for (auto s : sorts) for (string e : s.second){
		ground_literal l;
		l.predicate = "typeOf";
		l.positive = true;
		l.args.push_back(e);
		l.args.push_back(s.first);
		init.push_back(l);
	}
	
	set<string> allSorts;
	for(auto s : sorts) allSorts.insert(s.first);
	sorts["Type"] = allSorts;

	predicate_definition typePred;
	typePred.name = "typeOf";
	typePred.argument_sorts.push_back("__object");
	typePred.argument_sorts.push_back("Type");
	predicate_definitions.push_back(typePred);

	
}
