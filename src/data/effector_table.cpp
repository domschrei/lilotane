
#include <assert.h>

#include "data/effector_table.h"

std::vector<Signature> EffectorTable::getPossibleFactChanges(Signature sig) {

    int nameId = sig._name_id;
    
    // Get original signature of this operator (fully lifted)
    Signature origSig;
    if (_reductions.count(nameId) == 0) {
        // Action
        origSig = _reductions[nameId].getSignature();
    } else {
        // Reduction
        origSig = _actions[nameId].getSignature();
    }

    // Substitution mapping from original signature
    // to queried signature
    std::unordered_map<int, int> s;
    assert(sig._args.size() == origSig._args.size());
    for (int i = 0; i < sig._args.size(); i++) {
        s[origSig._args[i]] = sig._args[i];     
    }

    if (_fact_changes.count(nameId) > 0) {
        // Compute fact changes for origSig
        
        std::unordered_set<Signature, SignatureHasher> seenSignatures;
        std::vector<Signature> frontier;
        frontier.push_back(origSig);

        // Traverse graph of signatures with sub-reduction relationships
        while (!frontier.empty()) {
            Signature sig = frontier.back();
            frontier.pop_back();

            // Already saw this very signature?
            if (seenSignatures.count(sig) > 0) continue;
            
            // Add to seen signatures
            seenSignatures.insert(sig);

            // Expand node, add children to frontier
            for (Signature child : _child_generator(sig))
                frontier.push_back(child);
        }

        // Convert result to vector
        std::vector<Signature> result;
        result.reserve(seenSignatures.size());
        for (Signature sig : seenSignatures) {
            result.push_back(sig);
        }
        _fact_changes[nameId] = result;
    }

    // Get fact changes, substitute arguments
    std::vector<Signature>& sigs = _fact_changes[nameId];
    std::vector<Signature> out;
    for (Signature sig : sigs) {
        out.push_back(sig.substitute(s));
    }
    return out;
}