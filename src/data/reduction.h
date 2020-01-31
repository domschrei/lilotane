
#ifndef DOMPASCH_TREE_REXX_REDUCTION_H
#define DOMPASCH_TREE_REXX_REDUCTION_H

#include <vector>
#include <unordered_set>

#include "data/signature.h"
#include "data/bound_condition.h"

class Reduction : public HtnOp {

private:

    // Coding of the methods' AT's name.
    int _task_name_id;
    // The method's AT's arguments: 
    // integer x at pos. i references the x-th positional argument of the method. 
    std::vector<int> _task_arg_refs;

    std::vector<Signature> _subtasks;

public:
    Reduction() : HtnOp() {}
    Reduction(int nameId, std::vector<int> args, Signature task) : HtnOp(nameId, args), _task_name_id(task._name_id) {
        for (int arg : task._args) {

            // Where is "arg" in the method's arguments?
            int i = 0; 
            while (_args[i] != arg) i++;
            assert(_args[i] == arg);

            // TODO check types (?)

            _task_arg_refs.push_back(i);
        }
    }

    void addSubtask(Signature subtask) {
        
    }

    Signature getTaskSignature() {
        std::vector<int> args;
        for (int pos : _task_arg_refs) {
            args.push_back(_args[pos]);
        }
        return Signature(_task_name_id, args);
    }
};

#endif