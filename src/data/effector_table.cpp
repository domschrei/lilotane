
#include <assert.h>
#include <iostream>

#include "data/effector_table.h"
#include "util/names.h"
#include "data/htn_instance.h"

std::vector<Signature> EffectorTable::getPossibleFactChanges(const Signature& sig) {

    int nameId = sig._name_id;
    
    // Get original signature of this operator (fully lifted)
    Signature origSig;
    if (_htn->_reductions.count(nameId)) {
        // Reduction
        origSig = _htn->_reductions[nameId].getSignature();
    } else {
        // Action
        origSig = _htn->_actions[nameId].getSignature();
    }

    // Substitution mapping
    std::vector<int> placeholderArgs;
    for (int i = 0; i < sig._args.size(); i++) {
        placeholderArgs.push_back(-i-1);
    }
    HashMap<int, int> sFromPlaceholder = Substitution::get(placeholderArgs, sig._args);

    if (!_fact_changes.count(nameId)) {
        // Compute fact changes for origSig
        
        std::unordered_set<Signature, SignatureHasher> seenSignatures;
        std::unordered_set<Signature, SignatureHasher> facts;
        std::vector<Signature> frontier;

        // For each possible placeholder substitution
        frontier.push_back(origSig.substitute(Substitution::get(origSig._args, placeholderArgs)));

        // Traverse graph of signatures with sub-reduction relationships
        while (!frontier.empty()) {
            Signature nodeSig = frontier.back();
            frontier.pop_back();
            //log("%s\n", Names::to_string(nodeSig).c_str());

            // Normalize node signature arguments to compare to seen signatures
            substitution_t s;
            for (int argPos = 0; argPos < nodeSig._args.size(); argPos++) {
                int arg = nodeSig._args[argPos];
                if (arg > 0 && _htn->_var_ids.count(arg)) {
                    // Variable
                    if (!s.count(arg)) s[arg] = _htn->getNameId(std::string("??_") + std::to_string(argPos));
                }
            }
            Signature normNodeSig = nodeSig.substitute(s);

            // Already saw this signature?
            if (seenSignatures.count(normNodeSig)) continue;
            
            // If it is an action: add effects
            if (_htn->_actions.count(nodeSig._name_id)) {
                Action a = _htn->_actions[nodeSig._name_id];
                HtnOp op = a.substitute(Substitution::get(a.getArguments(), nodeSig._args));
                for (const Signature& eff : op.getEffects()) {
                    facts.insert(eff);
                }
            }

            // Add to seen signatures
            seenSignatures.insert(normNodeSig);

            // Expand node, add children to frontier
            for (const Signature& child : getPossibleChildren(nodeSig)) {
                // Arguments need to be renamed on recursive domains to generate all possible effects
                substitution_t s;
                for (int arg : child._args) {
                    if (arg > 0) s[arg] = _htn->getNameId(_htn->_name_back_table[arg] + "_");
                }
                frontier.push_back(child.substitute(s));
                //log("-> %s\n", Names::to_string(child).c_str());
            }
        }

        // Convert result to vector
        std::vector<Signature>& result = _fact_changes[nameId];
        result.reserve(facts.size());
        for (const Signature& sig : facts) {
            result.push_back(sig);
        }
    }

    // Get fact changes, substitute arguments
    const std::vector<Signature>& sigs = _fact_changes[nameId];
    std::vector<Signature> out;
    
    //log("   fact changes of %s : ", Names::to_string(sig).c_str());
    for (const Signature& fact : sigs) {
        //log("%s ", Names::to_string(fact).c_str());
        Signature sigRes = fact.substitute(sFromPlaceholder);
        for (int arg : sigRes._args) assert(arg > 0);
        
        for (const Signature& sigGround : ArgIterator::getFullInstantiation(sigRes, *_htn)) {
            out.push_back(sigGround);
        }
    }
    //log("\n");
    return out;
}

std::vector<Signature> EffectorTable::getPossibleChildren(const Signature& actionOrReduction) {
    std::vector<Signature> result;

    int nameId = actionOrReduction._name_id;
    if (!_htn->_reductions.count(nameId)) return result;

    // Reduction
    Reduction r = _htn->_reductions[nameId];
    r = r.substituteRed(Substitution::get(r.getArguments(), actionOrReduction._args));

    const std::vector<Signature>& subtasks = r.getSubtasks();

    // For each of the reduction's subtasks:
    for (const Signature& sig : subtasks) {
        // Find all possible (sub-)reductions of this subtask
        int taskNameId = sig._name_id;
        if (_htn->_actions.count(taskNameId)) {
            // Action
            Action& subaction = _htn->_actions[taskNameId];
            const std::vector<int>& origArgs = subaction.getArguments();
            Signature substSig = sig.substitute(Substitution::get(origArgs, sig._args));
            result.push_back(substSig);
        } else {
            // Reduction
            std::vector<int> subredIds = _htn->_task_id_to_reduction_ids[taskNameId];
            for (int subredId : subredIds) {
                const Reduction& subred = _htn->_reductions[subredId];
                // Substitute original subred. arguments
                // with the subtask's arguments
                const std::vector<int>& origArgs = subred.getTaskArguments();
                // When substituting task args of a reduction, there may be multiple possibilities
                std::vector<substitution_t> ss = Substitution::getAll(origArgs, sig._args);
                for (const substitution_t& s : ss) {
                    Signature substSig = subred.getSignature().substitute(s);
                    result.push_back(substSig);
                }
            }
        }
    }

    return result;
}