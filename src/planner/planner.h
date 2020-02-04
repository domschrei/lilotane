
#ifndef DOMPASCH_TREE_REXX_PLANNER_H
#define DOMPASCH_TREE_REXX_PLANNER_H

#include <unordered_map>
#include <assert.h> 
 
#include "parser/main.h"
#include "data/code_table.h"
#include "data/layer.h"
#include "data/action.h"
#include "data/reduction.h"

class Planner {

private:
    // The raw parsed problem.
    ParsedProblem& _p;

    // Maps a string to its name ID within the problem.
    std::unordered_map<std::string, int> _name_table;
    // Running ID to assign to new strings of the problem.
    int _name_table_running_id = 1;
    std::unordered_set<int> _var_ids;

    // Maps a predicate ID to a list of sorts IDs.
    std::unordered_map<int, std::vector<int>> _predicate_sorts_table;
    // Maps a task name ID to a list of sorts IDs.
    std::unordered_map<int, std::vector<int>> _task_sorts_table;
    std::unordered_map<int, std::vector<int>> _method_sorts_table;

    std::unordered_map<int, std::vector<int>> _constants_by_sort;

    // Maps an action ID to its action object.
    std::unordered_map<int, Action> _actions;

    // Maps a reduction ID to its reduction object.
    std::unordered_map<int, Reduction> _reductions;

    // Assigns and manages unique IDs to atom signatures.
    CodeTable _atom_table;

    // Assigns and manages unique IDs to action and reduction signatures.
    CodeTable _task_table;

    // Assigns and manages unique IDs to reduction signatures.
    CodeTable _reduction_table;

    std::unordered_map<int, std::vector<int>> _task_id_to_reduction_ids;

    std::vector<Layer> _layers;

public:
    Planner(ParsedProblem& problem) : _p(problem) {}
    void findPlan();

private:

    int getNameId(const std::string& name) {
        if (_name_table.count(name) == 0) {
            _name_table[name] = _name_table_running_id++;
            if (name[0] == '?') {
                // variable
                _var_ids.insert(_name_table[name]);
            }
        }
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
        int pId = getNameId(p.name);
        std::vector<int> sorts;
        for (auto var : p.argument_sorts) {
            sorts.push_back(getNameId(var));
        }
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
        assert(_task_sorts_table.count(tId) == 0);
        _task_sorts_table[tId] = sorts;
    }
    void extractMethodSorts(method& m) {
        std::vector<int> sorts;
        for (auto var : m.vars) {
            int sortId = getNameId(var.second);
            sorts.push_back(sortId);
        }
        int mId = getNameId(m.name);
        assert(_method_sorts_table.count(mId) == 0);
        _method_sorts_table[mId] = sorts;
    }
    void extractConstants() {
        for (auto sortPair : _p.sorts) {
            int nameId = getNameId(sortPair.first);
            _constants_by_sort[nameId] = std::vector<int>();
            std::vector<int>& constants = _constants_by_sort[nameId];
            for (std::string c : sortPair.second) {
                constants.push_back(getNameId(c));
            }
        }
    }

    Reduction& getReduction(method& method) {
        int id = getNameId(method.name);
        std::vector<int> args = getArguments(method.vars);
        
        int taskId = getNameId(method.at);
        std::vector<int> taskArgs = getArguments(method.atargs);
        _task_id_to_reduction_ids[taskId];
        _task_id_to_reduction_ids[taskId].push_back(id);

        assert(_reductions.count(id) == 0);
        _reductions[id] = Reduction(id, args, Signature(taskId, taskArgs));
        for (literal lit : method.constraints) {
            Signature sig = getSignature(lit);
            _reductions[id].addPrecondition(sig);
        }
        for (plan_step st : method.ps) {
            Signature sig(getNameId(st.task), getArguments(st.args));
            _reductions[id].addSubtask(sig);
        }
        return _reductions[id];
    }
    Action& getAction(task& task) {
        int id = getNameId(task.name);
        std::vector<int> args = getArguments(task.vars);
        assert(_actions.count(id) == 0);
        _actions[id] = Action(id, args);
        for (auto p : task.prec) {
            Signature sig = getSignature(p);
            _actions[id].addPrecondition(sig);
        }
        for (auto p : task.eff) {
            Signature sig = getSignature(p);
            _actions[id].addEffect(sig);
        }
        return _actions[id];
    }
    int getFact(Signature& sig) {
        return _atom_table(sig);
    }


    std::vector<Signature> getPossibleChildren(Signature& actionOrReduction) {
        std::vector<Signature> result;

        int nameId = actionOrReduction._name_id;
        if (_actions.count(nameId) == 0) {
            // Reduction
            assert(_reductions.count(nameId) > 0);
            Reduction& r = _reductions[nameId];
            std::vector<Signature> subtasks = r.getSubtasks();

            // For each of the reduction's subtasks:
            for (Signature sig : subtasks) {
                // Find all possible (sub-)reductions of this subtask
                std::vector<int> subredIds = _task_id_to_reduction_ids[nameId];
                for (int subredId : subredIds) {
                    Reduction& subred = _reductions[subredId];
                    // Substitute original subred. arguments
                    // with the subtask's arguments
                    std::unordered_map<int, int> s;
                    std::vector<int> origArgs = subred.getTaskArguments();
                    assert(origArgs.size() == sig._args.size());
                    for (int i = 0; i < origArgs.size(); i++) {
                        s[origArgs[i]] = sig._args[i];
                    }
                    result.push_back(subred.substitute(s).getSignature());
                }
            }
        }

        return result;
    }
};

#endif