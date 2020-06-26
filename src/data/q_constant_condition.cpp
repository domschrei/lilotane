
#include "q_constant_condition.h"
#include "util/names.h"

bool QConstantDatabase::isRegistered(int qconst) {
    return _registered_q_constants.count(qconst);
}

void QConstantDatabase::registerQConstant(int qconst) {
    _registered_q_constants.insert(qconst);
}

void QConstantDatabase::add(const std::vector<int>& reference, char conjunction, const ValueSet& values) {
    
    QConstantCondition* cond = new QConstantCondition(reference, conjunction, values);
    
    // Check if this very condition is already contained
    bool contained = false;
    for (const int& q : reference) {
        if (_conditions_per_qconst.count(q)) {
            if (_conditions_per_qconst[q].count(cond)) {
                contained = true;
                break;
            }
        }
    }
    if (contained) {
        delete cond;
        return;
    }

    // Simplify condition w.r.t. present conditions
    forEachFitCondition(reference, [&](QConstantCondition* subcond) {
        if (subcond->referencesSubsetOf(cond->reference)) {

            // Find and remove all tuples which evaluate to FALSE in that subset condition
            std::vector<std::vector<int>> tuplesToRemove;
            for (const auto& tuple : cond->values) {
                //log("QCONSTCOND_SIMP is %s:%s simplified by %s ?\n", TOSTR(cond->reference), TOSTR(tuple), TOSTR(subcond->reference));
                if (!subcond->test(cond->reference, tuple)) {
                    //log("QCONSTCOND_SIMP remove %s from %s\n", TOSTR(tuple), TOSTR(cond->reference));
                    tuplesToRemove.push_back(tuple);
                }
            }
            for (const auto& tuple : tuplesToRemove) cond->values.erase(tuple);

        }
        return true;
    });

    std::string out = "QCONSTCOND_CREATE ";
    for (int arg : reference) out += Names::to_string(arg) + ",";
    out += " : ";
    out += conjunction == QConstantCondition::CONJUNCTION_OR ? "OR " : "NOR ";
    for (const auto& tuple : cond->values) {
        out += "(";
        for (int arg : tuple) out += Names::to_string(arg) + ",";
        out += ") ";
    }
    out += "\n";
    log(out.c_str());

    _conditions.push_back(cond);
    for (const auto& c : reference) {
        _conditions_per_qconst[c];
        _conditions_per_qconst[c].insert(cond);
    }
}

bool QConstantDatabase::test(const std::vector<int>& refQConsts, const std::vector<int>& vals) {
    
    bool holds = true;
    forEachFitCondition(refQConsts, [&](QConstantCondition* cond) {

        // Check if the reference is fitting
        bool validReference = true;
        std::vector<int> v; v.reserve(cond->reference.size());
        for (int ref : cond->reference) {
            int i = 0; for (; i < refQConsts.size(); i++) if (refQConsts[i] == ref) break;
            if (i == refQConsts.size()) {
                validReference = false;
                break;
            } else {
                v.push_back(vals[i]);
            }
        }
        
        // Check the condition, if fitting
        if (validReference && !cond->test(v)) {
            //log("QCONSTCOND %s / %s not valid\n", TOSTR(cond->reference), TOSTR(v));
            holds = false;
            return false; // break
        } 
        return true; // continue
    });

    /*
    std::string out = "QCONSTCOND [ ";
    for (int arg : refQConsts) out += Names::to_string(arg) + ",";
    out += " / ";
    for (int arg : vals) out += Names::to_string(arg) + ",";
    out += " ] => %s\n";
    log(out.c_str(), holds ? "TRUE" : "FALSE");
    */

    return holds;
}

void QConstantDatabase::forEachFitCondition(const std::vector<int>& qConsts, std::function<bool(QConstantCondition*)> onVisit) {

    // Remember checked conditions
    NodeHashSet<QConstantCondition*, QConstCondHasher, QConstCondEquals> checked;

    // For each condition referencing one of the q constants:
    for (const auto& qconst : qConsts) {
        if (_conditions_per_qconst.count(qconst)) {
            for (const auto& cond : _conditions_per_qconst.at(qconst)) {
                if (checked.count(cond)) continue;
                else checked.insert(cond);

                bool proceed = onVisit(cond);
                if (!proceed) return;
            }
        }
    }
}