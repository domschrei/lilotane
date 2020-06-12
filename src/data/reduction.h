
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
    std::vector<USignature> _subtasks;

public:
    Reduction() : HtnOp() {}
    Reduction(HtnOp& op) : HtnOp(op) {}
    Reduction(const Reduction& r) : HtnOp(r._id, r._args), _task_name_id(r._task_name_id), _task_args(r._task_args), _subtasks(r._subtasks) {
        for (auto pre : r.getPreconditions()) addPrecondition(pre);
        for (auto eff : r.getEffects()) addEffect(eff);
    }
    Reduction(int nameId, const std::vector<int>& args, const USignature& task) : 
            HtnOp(nameId, args), _task_name_id(task._name_id), _task_args(task._args) {
    }

    void orderSubtasks(const std::map<int, std::vector<int>>& orderingNodelist) {

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
                    if (!orderingNodelist.count(n)) continue;
                    for (int m : orderingNodelist.at(n)) {
                        if (visitedStates[m] < 2)
                            nodeStack.push_back(m);
                    }
                }
            }
        }

        // Reorder subtasks
        std::vector<USignature> newSubtasks;
        newSubtasks.resize(_subtasks.size());
        for (int i = 0; i < _subtasks.size(); i++) {
            newSubtasks[i] = _subtasks[sortedNodes[i]];
        }
        _subtasks = newSubtasks;
    }

    Reduction substituteRed(const HashMap<int, int>& s) const {
        HtnOp op = HtnOp::substitute(s);
        Reduction r(op);
        
        r._task_name_id = _task_name_id;
        
        r._task_args.resize(_task_args.size());
        for (int i = 0; i < _task_args.size(); i++) {
            if (s.count(_task_args[i])) r._task_args[i] = s.at(_task_args[i]);
            else r._task_args[i] = _task_args[i];
        }
        
        r._subtasks.resize(_subtasks.size());
        for (int i = 0; i < _subtasks.size(); i++) {
            r._subtasks[i] = _subtasks[i].substitute(s);
        }

        return r;
    }

    void addSubtask(USignature subtask) {
        _subtasks.push_back(subtask);
    }
    void setSubtasks(const std::vector<USignature>& subtasks) {
        _subtasks = subtasks;
    }

    USignature getTaskSignature() const {
        return USignature(_task_name_id, _task_args);
    }
    const std::vector<int>& getTaskArguments() const {
        return _task_args;
    }
    const std::vector<USignature>& getSubtasks() const {
        return _subtasks;
    }

    Reduction& operator=(const Reduction& other) {
        _id = other._id;
        _args = other._args;
        _preconditions = other._preconditions;
        _effects = other._effects;
        _task_name_id = other._task_name_id;
        _task_args = other._task_args;
        _subtasks = other._subtasks;
        return *this;
    }
};

#endif