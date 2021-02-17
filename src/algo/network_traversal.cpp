
#include "network_traversal.h"
#include "data/htn_instance.h"

void NetworkTraversal::traverse(const USignature& opSig, TraverseOrder order, std::function<void(const USignature&, int)> onVisit) {
   
    FlatHashSet<USignature, USignatureHasher> seenSignatures;
    std::vector<USignature> frontier;
    std::vector<int> depths;

    // For each possible placeholder substitution
    frontier.push_back(opSig);
    depths.push_back(1);

    // Traverse graph of signatures with sub-reduction relationships
    while (!frontier.empty()) {
        USignature nodeSig = frontier.back(); frontier.pop_back();
        int depth = depths.back(); depths.pop_back();
        //log("%s\n", TOSTR(nodeSig));

        if (depth < 0) {
            // Post-order traversal: visit and pop
            assert(order == TRAVERSE_POSTORDER);
            onVisit(nodeSig, -depth+1);
            continue;
        }

        // Normalize node signature arguments to compare to seen signatures
        Substitution s;
        for (size_t argPos = 0; argPos < nodeSig._args.size(); argPos++) {
            int arg = nodeSig._args[argPos];
            if (arg > 0 && _htn->isVariable(arg)) {
                // Variable
                if (!s.count(arg)) s[arg] = _htn->nameId(std::string("??_") + std::to_string(argPos));
            }
        }
        USignature normNodeSig = nodeSig.substitute(s);

        // Already saw this signature?
        if (seenSignatures.count(normNodeSig)) continue;
        
        if (order == TRAVERSE_PREORDER) {
            // Visit node (using "original" signature)
            onVisit(nodeSig, depth-1);
        } else {
            // Remember node to be visited after all children have been visited
            frontier.push_back(nodeSig);
            depths.push_back(-depth+1);
        }

        // Add to seen signatures
        seenSignatures.insert(normNodeSig);

        // Expand node, add children to frontier
        for (const USignature& child : getPossibleChildren(nodeSig)) {
            // Arguments need to be renamed on recursive domains
            Substitution s;
            for (int arg : child._args) {
                if (arg > 0) s[arg] = _htn->nameId(_htn->toString(arg) + "_");
            }
            frontier.push_back(child.substitute(s));
            depths.push_back(depth);
            //log("-> %s\n", TOSTR(child));
        }
    }
}

std::vector<USignature> NetworkTraversal::getPossibleChildren(const USignature& opSig) {
    std::vector<USignature> result;

    int nameId = opSig._name_id;
    if (!_htn->isReduction(opSig)) return result;

    // Reduction
    Reduction r = _htn->toReduction(nameId, opSig._args);

    const std::vector<USignature>& subtasks = r.getSubtasks();
    for (size_t i = 0; i < subtasks.size(); i++) getPossibleChildren(subtasks, i, result);    

    return result;
}

void NetworkTraversal::getPossibleChildren(const std::vector<USignature>& subtasks, int offset, std::vector<USignature>& result) {

    // Find all possible (sub-)reductions of this subtask
    const USignature& sig = subtasks[offset];
    int taskNameId = sig._name_id;
    if (_htn->isAction(sig)) {
        // Action
        result.push_back(sig);
    } else {
        // Reduction
        std::vector<int> subredIds = _htn->getReductionIdsOfTaskId(taskNameId);
        for (int subredId : subredIds) {
            const Reduction& subred = _htn->getReductionTemplate(subredId);
            // Substitute original subred. arguments
            // with the subtask's arguments
            const std::vector<int>& origArgs = subred.getTaskArguments();
            // When substituting task args of a reduction, there may be multiple possibilities
            std::vector<Substitution> ss = Substitution::getAll(origArgs, sig._args);
            for (const Substitution& s : ss) {
                result.push_back(subred.getSignature().substitute(s));
            }
        }
    }
}