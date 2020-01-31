
#ifndef DOMPASCH_TREE_REXX_PLANNER_H
#define DOMPASCH_TREE_REXX_PLANNER_H

#include <unordered_map>
#include <assert.h>

 
 
 
 
#include "parser/main.h"
#include "data/arg.h"
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

    // Maps a predicate ID to a list of sorts IDs.
    std::unordered_map<int, std::vector<int>> _predicate_sorts_table;
    // Maps a task name ID to a list of sorts IDs.
    std::unordered_map<int, std::vector<int>> _task_sorts_table;

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

    int getNameId(const std::string& name) {
        if (_name_table.count(name) == 0)
            _name_table[name] = _name_table_running_id++;
        return _name_table[name];
    }

    std::vector<int> getArguments(std::vector<std::pair<string, string>>& vars) {
        std::vector<int> args;
        for (auto var : vars) {
            args.push_back(getNameId(var.first));
        }
        return args;
    }
    std::vector<int> getArguments(std::vector<std::string>& vars) {
        std::vector<int> args;
        for (auto var : vars) {
            args.push_back(getNameId(var));
        }
        return args;
    }

    Signature getSignature(task& task) {
        return Signature(getNameId(task.name), getArguments(task.vars));
    }
    Signature getSignature(method& method) {
        return Signature(getNameId(method.name), getArguments(method.vars));
    }
    Signature getSignature(literal& literal) {
        Signature sig = Signature(getNameId(literal.predicate), getArguments(literal.arguments));
        if (!literal.positive) sig.negate();
        return sig;
    }

    void extractPredSorts(predicate_definition& p) {
        std::vector<int> sorts;
        for (auto var : p.argument_sorts) {
            sorts.push_back(getNameId(var));
        }
        int pId = getNameId(p.name);
        assert(_predicate_sorts_table.count(pId) == 0);
        _predicate_sorts_table[pId] = sorts;
    }
    void extractTaskSorts(task& t) {
        std::vector<int> sorts;
        for (auto var : t.vars) {
            int sortId = getNameId(var.second);
            sorts.push_back(sortId);
        }
        int tId = getNameId(t.name);
        assert(_task_sorts_table.count[tId] == 0);
        _task_sorts_table[tId] = sorts;
    }
    // TODO extractMethodSorts

    Reduction getReduction(method& method) {
        int id = getNameId(method.name);
        std::vector<int> args = getArguments(method.vars);
        
        int taskId = getNameId(method.at);
        std::vector<int> taskArgs = getArguments(method.atargs);

        _reductions[id] = Reduction(id, args, Signature(taskId, taskArgs));
        for (literal lit : method.constraints) {
            _reductions[id].addPrecondition(getSignature(lit));
        }
        for (plan_step st : method.ps) {
            _reductions[id].addSubtask(getSignature(st.task, getArguments(st.args)));
        }
        return _reductions[id];
    }
    Action getAction(task& task) {
        int id = getNameId(task.name);
        std::vector<int> args = getArguments(task.vars);
        // TODO
    }
};

#endif