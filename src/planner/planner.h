
#ifndef DOMPASCH_TREE_REXX_PLANNER_H
#define DOMPASCH_TREE_REXX_PLANNER_H

#include <unordered_map>
#include <assert.h> 
 
#include "parser/main.h"
#include "data/code_table.h"
#include "data/layer.h"
#include "data/action.h"
#include "data/reduction.h"
#include "data/signature.h"
#include "util/names.h"

#include "data/instantiator.h"
#include "data/effector_table.h"


class Planner {

private:
    // The raw parsed problem.
    ParsedProblem& _p;

    // Maps a string to its name ID within the problem.
    std::unordered_map<std::string, int> _name_table;
    std::unordered_map<int, std::string> _name_back_table;
    // Running ID to assign to new strings of the problem.
    int _name_table_running_id = 1;
    std::unordered_set<int> _var_ids;

    // Maps a predicate ID to a list of sorts IDs.
    std::unordered_map<int, std::vector<int>> _predicate_sorts_table;
    // Maps a task name ID to a list of sorts IDs.
    std::unordered_map<int, std::vector<int>> _task_sorts_table;
    std::unordered_map<int, std::vector<int>> _method_sorts_table;

    std::unordered_map<int, std::vector<int>> _constants_by_sort;

    // Maps an action name ID to its action object.
    std::unordered_map<int, Action> _actions;

    // Maps a reduction name ID to its reduction object.
    std::unordered_map<int, Reduction> _reductions;

    std::unordered_map<Signature, Action, SignatureHasher> _actions_by_sig;
    std::unordered_map<Signature, Reduction, SignatureHasher> _reductions_by_sig;

    // Assigns and manages unique IDs to atom signatures.
    //CodeTable _atom_table;

    // Assigns and manages unique IDs to action and reduction signatures.
    //CodeTable _action_table;

    // Assigns and manages unique IDs to reduction signatures.
    //CodeTable _reduction_table;

    std::unordered_map<int, std::vector<int>> _task_id_to_reduction_ids;

    std::vector<Layer> _layers;
    Instantiator* _instantiator;
    EffectorTable* _effector_table;


public:
    Planner(ParsedProblem& problem) : _p(problem) {}
    void findPlan();

private:

    void addToLayer(Signature& task, Layer& layer, int pos, std::unordered_map<int, SigSet>& state, std::unordered_map<int, SigSet>& newState);

    int getNameId(const std::string& name) {
        if (_name_table.count(name) == 0) {
            _name_table[name] = _name_table_running_id++;
            _name_back_table[_name_table_running_id-1] = name;
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

        // TODO add method preconditions from its first "virtual subtask", if applicable
        assert(method.constraints.empty());
        /*
        for (literal lit : method.constraints) {
            Signature sig = getSignature(lit);
            _reductions[id].addPrecondition(sig);
            printf("  %s : precondition %s\n", to_string(_reductions[id].getSignature()).c_str(), to_string(sig).c_str());
        }
        */
        for (plan_step st : method.ps) {

            if (st.task.rfind("__method_precondition_", 0) == 0) {

                // Actually a method precondition which was compiled out
                
                // Find primitive task belonging to this method precondition
                task precTask;
                for (task t : primitive_tasks) {
                    if (t.name == st.task) {
                        precTask = t;
                        break;
                    }
                }
                // Add its preconditions to the method's preconditions
                for (auto p : precTask.prec) {
                    Signature sig = getSignature(p);
                    _reductions[id].addPrecondition(sig);
                }
                // (Do not add the task to the method's subtasks)

            } else {
                
                // Actual subtask
                Signature sig(getNameId(st.task), getArguments(st.args));
                _reductions[id].addSubtask(sig);
            }
        }
        printf(" %s : %i preconditions, %i subtasks\n", Names::to_string(_reductions[id].getSignature()).c_str(), 
                    _reductions[id].getPreconditions().size(), 
                    _reductions[id].getSubtasks().size());
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

    /*
    int getFact(Signature& sig) {
        return _atom_table(sig);
    }
    int getAction(Signature& sig) {
        return _action_table(sig);
    }
    int getReduction(Signature& sig) {
        return _reduction_table(sig);
    }
    */

};

#endif