
#ifndef DOMPASCH_TREE_REXX_EFFECTOR_GRAPH_H
#define DOMPASCH_TREE_REXX_EFFECTOR_GRAPH_H


#include <unordered_map>
#include <functional>

#include "data/action.h"
#include "data/reduction.h"
#include "data/signature.h"
#include "parser/main.h"

class EffectorTable {

private:
    ParsedProblem& _problem;

    // Maps an action ID to its action object.
    std::unordered_map<int, Action>& _actions;
    // Maps a reduction ID to its reduction object.
    std::unordered_map<int, Reduction>& _reductions;

    std::unordered_map<int, std::vector<int>>& _task_id_to_reduction_ids;

    // Maps an (action|reduction) name 
    // to the set of (partially lifted) fact signatures
    // that might be added to the state due to this operator. 
    std::unordered_map<int, std::vector<Signature>> _fact_changes; 

public:
    EffectorTable(ParsedProblem& p, std::unordered_map<int, Action>& actions, std::unordered_map<int, Reduction>& reductions, 
            std::unordered_map<int, std::vector<int>>& taskToReductions) : 
            _problem(p), _actions(actions), _reductions(reductions), _task_id_to_reduction_ids(taskToReductions) {}

    // Maps a (action|reduction) signature of any grounding state
    // to the corresponding set of (partially lifted) fact signatures
    // that might be added to the state due to this operator. 
    std::vector<Signature> getPossibleFactChanges(Signature sig);

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