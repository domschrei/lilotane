
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

    // Assigns and manages unique IDs to action and reduction signatures.
    CodeTable _task_table;

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
    Signature getSignature(parsed_task& task) {
        int nameId = getNameId(task.name);
        std::vector<int> args;
        // TODO fill args
        return Signature(nameId, args);
    }

    Reduction addReduction(parsed_method m) {
        int nameId = getNameId(m.name);
        std::vector<int> args; 
        // TODO fill args
        Signature s(nameId, args);
        int redId = _reduction_table(s);
        Reduction r(nameId, args);
        // TODO add additional members of r: expansion, ...
        _reductions[redId] = r;
    }
    Action addAction(parsed_task task) {
        int nameId = getNameId(task.name);
        std::vector<int> args; 
        // TODO fill args
        Signature s(nameId, args);
        int aId = _action_table(s);
        Action a(nameId, args);
        // TODO add additional members of a: prec, eff, ...
        _actions[aId] = a;
    }
};

#endif