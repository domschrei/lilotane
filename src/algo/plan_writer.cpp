
// PandaPIparser
#include "plan.hpp"
#include "verify.hpp"

#include "algo/plan_writer.h"

void PlanWriter::outputPlan(Plan& _plan) {

    // Create stringstream which is being fed the plan
    std::stringstream stream;

    // Print plan into stream

    // -- primitive part
    stream << "==>\n";
    FlatHashSet<int> actionIds;
    FlatHashSet<int> idsToRemove;

    FlatHashSet<int> primitivizationIds;
    std::vector<PlanItem> decompsToInsert;
    size_t decompsToInsertIdx = 0;
    size_t length = 0;
    
    for (PlanItem& item : std::get<0>(_plan)) {

        if (item.id < 0) continue;
        
        if (_htn.toString(item.abstractTask._name_id).rfind("__LLT_SECOND") != std::string::npos) {
            // Second part of a split action: discard
            idsToRemove.insert(item.id);
            continue;
        }
        
        if (_htn.toString(item.abstractTask._name_id).rfind("__SURROGATE") != std::string::npos) {
            // Primitivized reduction: Replace with actual action, remember represented method to include in decomposition

            [[maybe_unused]] const auto& [parentId, childId] = _htn.getReductionAndActionFromPrimitivization(item.abstractTask._name_id);
            const Reduction& parentRed = _htn.toReduction(parentId, item.abstractTask._args);
            primitivizationIds.insert(item.id);
            
            PlanItem parent;
            parent.abstractTask = parentRed.getTaskSignature();  
            parent.id = item.id-1;
            parent.reduction = parentRed.getSignature();
            parent.subtaskIds = std::vector<int>(1, item.id);
            decompsToInsert.push_back(parent);

            const USignature& childSig = parentRed.getSubtasks()[0];
            item.abstractTask = childSig;
        }

        actionIds.insert(item.id);

        // Do not write blank actions or the virtual goal action
        if (item.abstractTask == _htn.getBlankActionSig()) continue;
        if (item.abstractTask._name_id == _htn.nameId("<goal_action>")) continue;

        stream << item.id << " " << Names::to_string_nobrackets(_htn.cutNonoriginalTaskArguments(item.abstractTask)) << "\n";
        length++;
    }
    // -- decomposition part
    bool root = true;
    for (size_t itemIdx = 0; itemIdx < _plan.second.size() || decompsToInsertIdx < decompsToInsert.size(); itemIdx++) {

        // Pick next plan item to print
        PlanItem item;
        if (decompsToInsertIdx < decompsToInsert.size() && (itemIdx >= _plan.second.size() || decompsToInsert[decompsToInsertIdx].id < _plan.second[itemIdx].id)) {
            // Pick plan item from primitivized decompositions
            item = decompsToInsert[decompsToInsertIdx];
            decompsToInsertIdx++;
            itemIdx--;
        } else {
            // Pick plan item from "normal" plan list
            item = _plan.second[itemIdx];
        }
        if (item.id < 0) continue;

        std::string subtaskIdStr = "";
        for (int subtaskId : item.subtaskIds) {
            if (item.id+1 != subtaskId && primitivizationIds.count(subtaskId)) subtaskId--;
            if (!idsToRemove.count(subtaskId)) subtaskIdStr += " " + std::to_string(subtaskId);
        }
        
        if (root) {
            stream << "root " << subtaskIdStr << "\n";
            root = false;
            continue;
        } else if (item.id <= 0 || actionIds.count(item.id)) continue;
        
        stream << item.id << " " << Names::to_string_nobrackets(_htn.cutNonoriginalTaskArguments(item.abstractTask)) << " -> " 
            << Names::to_string_nobrackets(item.reduction) << subtaskIdStr << "\n";
    }
    stream << "<==\n";

    // Feed plan into parser to convert it into a plan to the original problem
    // (w.r.t. previous compilations the parser did)
    std::ostringstream outstream;
    convert_plan(stream, outstream);
    std::string planStr = outstream.str();

    if (_params.isNonzero("vp")) {
        // Verify plan (by copying converted plan stream and putting it back into panda)
        std::stringstream verifyStream;
        verifyStream << planStr << std::endl;
        bool ok = verify_plan(verifyStream, /*useOrderingInfo=*/true, /*lenientMode=*/false, /*debugMode=*/0);
        if (!ok) {
            Log::e("ERROR: Plan declared invalid by pandaPIparser! Exiting.\n");
            exit(1);
        }
    }
    
    // Print plan
    Log::log_notime(Log::V0_ESSENTIAL, planStr.c_str());
    Log::log_notime(Log::V0_ESSENTIAL, "<==\n");
    
    Log::i("End of solution plan. (counted length of %i)\n", length);
}
