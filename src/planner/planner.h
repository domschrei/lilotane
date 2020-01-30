
#ifndef DOMPASCH_TREE_REXX_PLANNER_H
#define DOMPASCH_TREE_REXX_PLANNER_H

#include <unordered_map>

#include "parser/main.h"
#include "data/code_table.h"
#include "data/layer.h"
#include "data/action.h"
#include "data/reduction.h"

class Planner {

private:
    // The raw parsed problem.
    ParsedProblem& _problem;

    // Maps a string to its name ID within the problem.
    std::unordered_map<std::string, int> _name_table;
    // Running ID to assign to new strings of the problem.
    int _name_table_running_id = 1;

    // Assigns and manages unique IDs to atom signatures.
    CodeTable _atom_table;

    // Assigns and manages unique IDs to action signatures.
    CodeTable _action_table;
    // Maps an action ID to its action object.
    std::unordered_map<int, Action> _actions;

    // Assigns and manages unique IDs to reduction signatures.
    CodeTable _reduction_table;
    // Maps a reduction ID to its reduction object.
    std::unordered_map<int, Reduction> _reductions;

    std::vector<Layer> _layers;

public:
    Planner(ParsedProblem& problem) : _problem(problem) {}
    void findPlan();

private:

    int getNameId(std::string& name) {
        if (_name_table.count(name) == 0)
            _name_table[name] = _name_table_running_id++;
        return _name_table[name];
    }

};

#endif