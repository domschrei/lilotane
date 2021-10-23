
#include "retroactive_pruning.h"

void RetroactivePruning::prune(const USignature& op, int layerIdx, int pos) {

    NodeHashSet<PositionedUSig, PositionedUSigHasher> opsToRemove;
    NodeHashSet<PositionedUSig, PositionedUSigHasher> openOps;
    openOps.emplace(layerIdx, pos, op);
    NodeHashMap<PositionedUSig, USigSet, PositionedUSigHasher> removedExpansionsOfParents;

    // Traverse the hierarchy upwards, removing expansions/predecessors
    // and marking all "root" operations whose induces subtrees should be pruned 

    while (!openOps.empty()) {
        PositionedUSig psig = *openOps.begin();
        openOps.erase(psig);
        Log::d("PRUNE_UP %s\n", TOSTR(psig));

        if (psig.layer == 0) {
            // Top of the hierarchy hit
            opsToRemove.insert(psig);
            continue;
        }

        Position& position = _layers[psig.layer]->at(psig.pos);
        assert(position.hasAction(psig.usig) || position.hasReduction(psig.usig));
        int oldPos = 0;
        Layer& oldLayer = *_layers.at(psig.layer-1);
        while (oldPos+1 < (int)oldLayer.size() && oldLayer.getSuccessorPos(oldPos+1) <= psig.pos) 
            oldPos++;

        bool pruneSomeParent = false;
        assert(position.getPredecessors().count(psig.usig) || Log::e("%s has no predecessors!\n", TOSTR(psig)));
        for (const auto& parent : position.getPredecessors().at(psig.usig)) {
            PositionedUSig parentPSig(psig.layer-1, oldPos, parent);
            //assert(oldLayer.at(oldPos).hasAction(parent) || oldLayer.at(oldPos).hasReduction(parent) || Log::e("%s\n", TOSTR(parentPSig)));
            const auto& siblings = position.getExpansions().at(parent);

            // Mark op for removal from expansion of the parent
            assert(siblings.count(psig.usig));
            removedExpansionsOfParents[parentPSig].insert(psig.usig);

            if (removedExpansionsOfParents[parentPSig].size() == siblings.size()) {
                // Siblings become empty -> prune parent as well
                openOps.insert(std::move(parentPSig));
                pruneSomeParent = true;
            }
        }

        // No parent pruned? -> This op is a root of a subtree to be pruned
        if (!pruneSomeParent) opsToRemove.insert(psig);
    }

    // Traverse the hierarchy downwards, pruning all children who became impossible

    while (!opsToRemove.empty()) {
        PositionedUSig psig = *opsToRemove.begin();
        opsToRemove.erase(psig);
        Position& position = _layers[psig.layer]->at(psig.pos);
        Log::d("PRUNE_DOWN %s\n", TOSTR(psig));
        assert(position.hasAction(psig.usig) || position.hasReduction(psig.usig));

        // Go down one layer and mark all children for removal which have only one predecessor left
        if (psig.layer+1 < _layers.size()) {
            int belowPosIdx = _layers.at(psig.layer)->getSuccessorPos(psig.pos);
            while (belowPosIdx < (int)_layers.at(psig.layer)->getSuccessorPos(psig.pos+1)) {

                Position& below = _layers.at(psig.layer+1)->at(belowPosIdx);
                if (below.getExpansions().count(psig.usig)) for (auto& child : below.getExpansions().at(psig.usig)) {
                    assert(below.getPredecessors().at(child).count(psig.usig));
                    if (psig.layer+1 == (size_t)layerIdx && belowPosIdx == pos && child == op) {
                        // Arrived back at original op to prune
                        opsToRemove.emplace(layerIdx, pos, op);
                    } else if (below.getPredecessors().at(child).size() == 1) {
                        // Child has this op as its only predecessor -> prune
                        opsToRemove.emplace(psig.layer+1, belowPosIdx, child);
                    } else {
                        Log::d("PRUNE %i pred left for %s@(%i,%i): %s\n", below.getPredecessors().at(child).size()-1, TOSTR(child), psig.layer+1, belowPosIdx);
                        below.getPredecessors().at(child).erase(psig.usig);
                    }
                } else Log::d("PRUNE No expansions for %s @ (%i,%i)\n", TOSTR(psig), psig.layer+1, belowPosIdx);

                belowPosIdx++;
            }
        }

        // Remove the operation's occurrence itself,
        // together with its expansions and predecessors
        int opVar = position.getVariableOrZero(VarType::OP, psig.usig);
        if (opVar != 0) _enc.addUnitConstraint(-opVar);
        position.removeActionOccurrence(psig.usig);
        position.removeReductionOccurrence(psig.usig);
        _num_retroactively_pruned_ops++;
    }

    _num_retroactive_prunings++;
}
