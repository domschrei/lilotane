
#include <assert.h>
#include <iostream>

#include "data/effector_table.h"
#include "util/names.h"
#include "data/htn_instance.h"

std::vector<Signature> EffectorTable::getPossibleFactChanges(const USignature& sig) {

    int nameId = sig._name_id;
    
    // Substitution mapping
    std::vector<int> placeholderArgs;
    USignature normSig = _htn->getNormalizedLifted(sig, placeholderArgs);
    HashMap<int, int> sFromPlaceholder = Substitution::get(placeholderArgs, sig._args);

    if (!_fact_changes.count(nameId)) {
        // Compute fact changes for origSig
        
        HashSet<Signature, SignatureHasher> facts;

        _traversal.traverse(normSig.substitute(Substitution::get(normSig._args, placeholderArgs)), 
        [&](const USignature& nodeSig, int depth) {
            // If visited node is an action: add effects
            if (_htn->_actions.count(nodeSig._name_id)) {
                Action a = _htn->_actions[nodeSig._name_id];
                HtnOp op = a.substitute(Substitution::get(a.getArguments(), nodeSig._args));
                for (const Signature& eff : op.getEffects()) {
                    facts.insert(eff);
                }
            }
        });

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
        for (int arg : sigRes._usig._args) assert(arg > 0);
        
        for (const Signature& sigGround : ArgIterator::getFullInstantiation(sigRes, *_htn)) {
            out.push_back(sigGround);
        }
    }
    //log("\n");
    return out;
}

