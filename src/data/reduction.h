
#ifndef DOMPASCH_TREE_REXX_REDUCTION_H
#define DOMPASCH_TREE_REXX_REDUCTION_H

#include <vector>
#include <unordered_set>

#include "data/htn_op.h"
#include "data/signature.h"
#include "data/bound_condition.h"

class Reduction : public HtnOp {

private:

    // Coding of the methods' AT's name.
    int _task_name_id;
    // The method's AT's arguments.
    std::vector<int> _task_args;

    // The ordered list of subtasks.
    // TODO replace with BoundSubtask structure ... ?
    std::vector<Signature> _subtasks;

public:
    Reduction() : HtnOp() {}
    Reduction(HtnOp& op) : HtnOp(op) {}
    Reduction(int nameId, std::vector<int> args, Signature task) : 
            HtnOp(nameId, args), _task_name_id(task._name_id), _task_args(task._args) {
    }

    Reduction substituteRed(std::unordered_map<int, int> s) {
        HtnOp op = HtnOp::substitute(s);
        Reduction r(op);
        r._task_name_id = _task_name_id;
        r._task_args.resize(_task_args.size());
        for (int i = 0; i < _task_args.size(); i++) {
            r._task_args[i] = s[_task_args[i]];
        }
        r._subtasks.resize(_subtasks.size());
        for (int i = 0; i < _subtasks.size(); i++) {
            r._subtasks[i] = _subtasks[i].substitute(s);
        }
        return r;
    }

    void addSubtask(Signature subtask) {
        _subtasks.push_back(subtask);
    }

    Signature getTaskSignature() {
        return Signature(_task_name_id, _task_args);
    }
    const std::vector<int>& getTaskArguments() {
        return _task_args;
    }

    std::vector<Signature>& getSubtasks() {
        return _subtasks;
    }
};

#endif