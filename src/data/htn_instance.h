
#ifndef DOMPASCH_TREE_REXX_HTN_INSTANCE_H
#define DOMPASCH_TREE_REXX_HTN_INSTANCE_H

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
#include "data/arg_iterator.h"

struct HtnInstance {

    // The raw parsed problem.
    ParsedProblem& _p;

    // Maps a string to its name ID within the problem.
    std::unordered_map<std::string, int> _name_table;
    std::unordered_map<int, std::string> _name_back_table;
    // Running ID to assign to new strings of the problem.
    int _name_table_running_id = 1;

    // Set of all name IDs that are variables (start with '?').
    std::unordered_set<int> _var_ids;

    // Maps a predicate ID to a list of sorts IDs.
    std::unordered_map<int, std::vector<int>> _predicate_sorts_table;
    // Maps a task name ID to a list of sorts IDs.
    std::unordered_map<int, std::vector<int>> _task_sorts_table;
    // Maps a method name ID to a list of sorts IDs.
    std::unordered_map<int, std::vector<int>> _method_sorts_table;

    // Maps a sort name ID to a list of constant name IDs of that sort.
    std::unordered_map<int, std::vector<int>> _constants_by_sort;

    // Maps an action name ID to its action object.
    std::unordered_map<int, Action> _actions;

    // Maps a reduction name ID to its reduction object.
    std::unordered_map<int, Reduction> _reductions;

    std::unordered_map<Signature, Action, SignatureHasher> _actions_by_sig;
    std::unordered_map<Signature, Reduction, SignatureHasher> _reductions_by_sig;

    std::unordered_map<int, std::vector<int>> _task_id_to_reduction_ids;

    Instantiator* _instantiator;
    EffectorTable* _effector_table;

    Reduction _init_reduction;

    HtnInstance(ParsedProblem& p) : _p(p) {
        Names::init(_name_back_table);
        _instantiator = new Instantiator(*this);
        _effector_table = new EffectorTable(*this);

        for (predicate_definition p : predicate_definitions)
            extractPredSorts(p);
        for (task t : primitive_tasks)
            extractTaskSorts(t);
        for (task t : abstract_tasks)
            extractTaskSorts(t);
        for (method m : methods)
            extractMethodSorts(m);
        
        extractConstants();

        printf("Sorts extracted.\n");
        for (auto sort_pair : _p.sorts) {
            printf("%s : ", sort_pair.first.c_str());
            for (auto c : sort_pair.second) {
                printf("%s ", c.c_str());
            }
            printf("\n");
        }

        for (task t : primitive_tasks)
            getAction(t);
        _init_reduction = getReduction(methods[0]);
        for (int mIdx = 1; mIdx < methods.size(); mIdx++)
            getReduction(methods[mIdx]);
        
        printf("%i operators and %i methods created.\n", _actions.size(), _reductions.size());
    }

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
            int sortId = getNameId(sortPair.first);
            _constants_by_sort[sortId] = std::vector<int>();
            std::vector<int>& constants = _constants_by_sort[sortId];
            for (std::string c : sortPair.second) {
                constants.push_back(getNameId(c));
                //printf("constant %s of sort %s\n", c.c_str(), sortPair.first.c_str());
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

    SigSet getAllFactChanges(Signature& sig) {        
        SigSet result;
        for (Signature effect : _effector_table->getPossibleFactChanges(sig)) {
            std::vector<Signature> instantiation = ArgIterator::getFullInstantiation(effect, _constants_by_sort, _predicate_sorts_table, _var_ids);
            for (Signature i : instantiation) {
                result.insert(i);
            }
        }
        return result;
    }

    Action replaceQConstants(Action& a, int layerIdx, int pos) {
        Signature sig = a.getSignature();
        std::vector<int> qConstPositions = _instantiator->getFreeArgPositions(sig);
        std::unordered_map<int, int> s;
        for (int qConstPos : qConstPositions) {
            int qConst = sig._args[qConstPos];
            std::string qConstName = _name_back_table[qConst];
            assert(qConstName[0] == '?');
            qConstName = "!_" + std::to_string(layerIdx) + "_" + std::to_string(pos) + "_" + qConstName.substr(1);
            int qConstId = getNameId(qConstName);
            s[qConst] = qConstId;
            // Add qConstant to constant tables of its type and each of its
            std::unordered_set<int> containedSorts;
            containedSorts.insert(_predicate_sorts_table[sig._name_id][qConstPos]);
            for (sort_definition sd : _p.sort_definitions) {
                for (int containedSort : containedSorts) {
                    if (sd.has_parent_sort && getNameId(sd.parent_sort) == containedSort) {
                        for (std::string newSort : sd.declared_sorts) {
                            containedSorts.insert(getNameId(newSort));
                        }
                    }
                }
            }
            printf("sorts of %s : ", Names::to_string(qConstId).c_str());
            for (int sort : containedSorts) {
                _constants_by_sort[sort].push_back(qConstId);
                printf("%s ", Names::to_string(sort).c_str());
            } 
            printf("\n");
        }
        HtnOp op = a.substitute(s);
        return Action(op);        
    }
    Reduction replaceQConstants(Reduction& red, int layerIdx, int pos) {
        Signature sig = red.getSignature();
        std::vector<int> qConstPositions = _instantiator->getFreeArgPositions(sig);
        std::unordered_map<int, int> s;
        for (int qConstPos : qConstPositions) {
            int qConst = sig._args[qConstPos];
            std::string qConstName = _name_back_table[qConst];
            assert(qConstName[0] == '?');
            qConstName = "!_" + std::to_string(layerIdx) + "_" + std::to_string(pos) + "_" + qConstName.substr(1);
            s[qConst] = getNameId(qConstName);
        }
        return red.substituteRed(s);
    }
};

#endif