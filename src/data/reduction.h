
#ifndef DOMPASCH_TREE_REXX_REDUCTION_H
#define DOMPASCH_TREE_REXX_REDUCTION_H

#include <vector>
#include <unordered_set>
#include <map>

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
    std::vector<Signature> _subtasks;

public:
    Reduction() : HtnOp() {}
    Reduction(HtnOp& op) : HtnOp(op) {}
    Reduction(const Reduction& r) : HtnOp(r._id, r._args), _task_name_id(r._task_name_id), _task_args(r._task_args), _subtasks(r._subtasks) {}
    Reduction(int nameId, std::vector<int> args, Signature task) : 
            HtnOp(nameId, args), _task_name_id(task._name_id), _task_args(task._args) {
    }

    void orderSubtasks(std::map<int, std::vector<int>> orderingNodelist) {

        // Initialize "visited" state for each node
        std::map<int, int> visitedStates;
        for (auto pair : orderingNodelist) {
            visitedStates[pair.first] = 0;
        }

        // Topological ordering via multiple DFS
        std::vector<int> sortedNodes;
        while (true) {

            // Pick an unvisited node
            int node = -1;
            for (auto pair : orderingNodelist) {
                if (visitedStates[pair.first] == 0) {
                    node = pair.first;
                    break;
                }
            }
            if (node == -1) break; // no node left: done

            // Traverse ordering graph
            std::vector<int> nodeStack;
            nodeStack.push_back(node);
            while (!nodeStack.empty()) {

                int n = nodeStack.back();

                if (visitedStates[n] == 2) {
                    // Closed node: pop
                    nodeStack.pop_back();

                } else if (visitedStates[n] == 1) {
                    // Open node: close, pop
                    nodeStack.pop_back();
                    visitedStates[n] = 2;
                    sortedNodes.insert(sortedNodes.begin(), n);

                } else {
                    // Unvisited node: open, visit children
                    visitedStates[n] = 1;
                    for (int m : orderingNodelist[n]) {
                        if (visitedStates[m] < 2)
                            nodeStack.push_back(m);
                    }
                }
            }
        }

        // Reorder subtasks
        std::vector<Signature> newSubtasks;
        newSubtasks.resize(_subtasks.size());
        for (int i = 0; i < _subtasks.size(); i++) {
            newSubtasks[i] = _subtasks[sortedNodes[i]];
        }
        _subtasks = newSubtasks;
    }

    Reduction substituteRed(std::unordered_map<int, int> s) {
        HtnOp op = HtnOp::substitute(s);
        Reduction r(op);
        r._task_name_id = _task_name_id;
        r._task_args.resize(_task_args.size());
        for (int i = 0; i < _task_args.size(); i++) {
            if (s.count(_task_args[i])) r._task_args[i] = s[_task_args[i]];
            else r._task_args[i] = _task_args[i];
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