
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





















    
    Argument getArgument(std::pair<std::string, std::string>& var) {
        return Argument(getNameId(var.first), getNameId(var.second));
    }
    std::vector<Argument> getArguments(var_declaration* decl) {
        std::vector<Argument> args;
        for (auto pair : decl->vars) {
            //printf("<%s,%s>\n", pair.first.c_str(), pair.second.c_str());
            args.push_back(getArgument(pair));
        }
        return args;
    }
    
    Signature getSignature(parsed_task& task) {
        return getSignature(task.name, task.arguments);
    }
    Signature getSignature(std::string& name, var_declaration* decl) {
        int nameId = getNameId(name);
        std::vector<Argument> args = getArguments(decl);
        return Signature(nameId, args);
    }
    std::vector<Signature> getSignatures(general_formula& formula) {
        assert(formula.type == EMPTY || formula.type == ATOM 
                || formula.type == NOTATOM || formula.type == AND 
                || formula.type == EQUAL || formula.type == NOTEQUAL);
        
        std::vector<Signature> sigs;
        if (formula.type == EMPTY) return sigs;

        if (formula.type == ATOM || formula.type == NOTATOM) {
            int nameId = getNameId(formula.predicate);
            if (_predicate_sorts_table.count(nameId) == 0) {
                // Not a valid predicate
                printf("pred %s not found -- skipping\n", formula.predicate.c_str());
                return sigs;
            }
            std::vector<int>& sorts = _predicate_sorts_table[nameId];
            //printf("pred %s : id=%i, %i sorts\n", formula.predicate.c_str(), nameId, sorts.size());
            std::vector<Argument> args;
            assert(formula.arguments.vars.size() == sorts.size());
            for (int i = 0; i < formula.arguments.vars.size(); i++) {
                args.push_back(Argument(getNameId(formula.arguments.vars[i]), sorts[i]));
            }
            assert(formula.arguments.newVar.empty());
            sigs.push_back(Signature(nameId, args));
            if (formula.type == NOTATOM) sigs.back().negate();
            return sigs;
        }

        if (formula.type == AND) {
            for (auto f : formula.subformulae) {
                for (Signature sig : getSignatures(*f)) sigs.push_back(sig);
            }
            return sigs;
        }

        // TODO add equal/notequal
        assert(false);
    }





    std::vector<int>& addPredicateSorts(predicate_definition& pred) {
        int nameId = getNameId(pred.name);
        std::vector<int> sortIds;
        std::string sorts = " ";
        for (std::string sortName : pred.argument_sorts) {
            sortIds.push_back(getNameId(sortName));
            sorts += sortName + " ";
        }
        printf("%s : id=%i sorts=[%s]\n", pred.name.c_str(), nameId, sorts.c_str());
        _predicate_sorts_table[nameId] = sortIds;
        return _predicate_sorts_table[nameId];
    }
    std::vector<int>& addTaskSorts(parsed_task& task) {
        int nameId = getNameId(task.name);
        std::vector<Argument> args = getArguments(task.arguments);
        std::vector<int> sortIds;
        for (Argument arg : args) {
            sortIds.push_back(arg._sort);
        }
        _task_sorts_table[nameId] = sortIds;
        return _task_sorts_table[nameId];
    }

    Reduction& addReduction(const std::string& abstractTaskName, parsed_method& m) {
        int nameId = getNameId(m.name);

        std::vector<Argument> reductionArgs = getArguments(m.vars);
        assert(m.newVarForAT.empty());
        
        int taskNameId = getNameId(abstractTaskName);
        std::vector<Argument> taskArgs;
        std::vector<int>& taskSorts = _task_sorts_table[taskNameId];
        assert(taskSorts.size() == m.atArguments.size());
        for (int i = 0; i < m.atArguments.size(); i++) {
            // Use name of the AT-argument inside the method
            Argument arg(getNameId(m.atArguments[i]), taskSorts[i]);
            taskArgs.push_back(arg);
        }
        Signature task(taskNameId, taskArgs);

        Signature s = getSignature(m.name, m.vars);
        int redId = _reduction_table(s);

        _reductions[redId] = Reduction(nameId, reductionArgs, task);
        for (Signature sig : getSignatures(*m.prec)) {
            _reductions[redId].addPrecondition(sig);
        }
        assert(m.eff->type == EMPTY);
        
        // add expansion TODO sort the tasks according to the ordering !
        for (sub_task* subtask : m.tn->tasks) {
            int subtaskNameId = getNameId(subtask->task);
            std::vector<Argument> args;
            for (int i = 0; i < subtask->arguments->vars.size(); i++) {
                int argNameId = getNameId(subtask->arguments->vars[i]);
                int sort = _task_sorts_table[subtaskNameId][i];
                args.push_back(Argument(argNameId, sort));
            }
            if (!subtask->arguments->newVar.empty()) {
                for (pair<string, string> p : subtask->arguments->newVar) {
                    printf("method %s : subtask %s : <%s,%s>\n", m.name.c_str(), subtask->task.c_str(), p.first.c_str(), p.second.c_str());
                }
                //exit(1);
            }
            _reductions[redId].addSubtask(Signature(subtaskNameId, args));
        }

        return _reductions[redId];
    }

    Action& addAction(parsed_task task) {
        int nameId = getNameId(task.name);
        std::vector<Argument> args = getArguments(task.arguments);
        Signature s = Signature(nameId, args);
        int aId = _task_table(s);
        _actions[aId] = Action(nameId, args);
        for (Signature sig : getSignatures(*task.prec)) {
            _actions[aId].addPrecondition(sig);
        }
        for (Signature sig : getSignatures(*task.eff)) {
            _actions[aId].addEffect(sig);
        }

        return _actions[aId];
    }
};

#endif