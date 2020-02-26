
#include "data/arg_iterator.h"
#include "data/htn_instance.h"

std::vector<Signature> ArgIterator::getFullInstantiation(const Signature& sig, HtnInstance& _htn) {

    // "Empty" signature?    
    if (sig._args.empty()) {
        return std::vector<Signature>(1, sig);
    }

    // enumerate all arg combinations for variable args
    // Get all constants of the respective type(s)
    assert(_htn._signature_sorts_table.count(sig._name_id));
    std::vector<int> sorts = _htn._signature_sorts_table[sig._name_id];
    assert(sorts.size() == sig._args.size() || fail("Sorts table of predicate " + Names::to_string(sig) + " has an invalid size\n"));
    
    //log("SORTS %s ", Names::to_string(sig._name_id).c_str());
    //for (int s : sorts) log("%s ", Names::to_string(s).c_str());
    //log("\n");
    
    std::vector<std::vector<int>> constantsPerArg;

    for (int pos = 0; pos < sorts.size(); pos++) {
        int arg = sig._args[pos];
        
        if (_htn._var_ids.count(arg)) {
            // free argument

            int sort = sorts[pos];
            assert(_htn._constants_by_sort.count(sort));

            // Scan through all eligible arguments, filtering out q constants
            std::vector<int> eligibleConstants;
            for (int arg : _htn._constants_by_sort[sort]) {
                if (_htn._q_constants.count(arg)) continue;
                eligibleConstants.push_back(arg);
            }

            // Empty instantiation if there is not a single eligible constant at some pos
            //log("OF_SORT %s : %i constants\n", _htn._name_back_table[sort].c_str(), eligibleConstants.size());
            if (eligibleConstants.empty()) return std::vector<Signature>();

            constantsPerArg.push_back(eligibleConstants);
        } else {
            // constant
            constantsPerArg.push_back(std::vector<int>(1, arg));
        }
    }

    std::vector<Signature> instantiation = instantiate(sig, constantsPerArg);
    
    return instantiation;
}

std::vector<Signature> ArgIterator::instantiate(const Signature& sig, const std::vector<std::vector<int>>& constantsPerArg) {

    std::vector<Signature> instantiation;

    // Check validity
    assert(constantsPerArg.size() > 0);
    int numChoices = 1;
    for (auto vec : constantsPerArg) {
        numChoices *= vec.size();
    }
    if (numChoices == 0) return instantiation;
    instantiation.reserve(numChoices);

    // Iterate over all possible assignments
    std::vector<int> counter(constantsPerArg.size(), 0);
    int numInstantiations = 0;
    std::vector<int> newArgs(counter.size());
    while (true) {
        // Assemble the assignment
        for (int argPos = 0; argPos < counter.size(); argPos++) {

            assert(argPos < constantsPerArg.size());
            assert(counter[argPos] < constantsPerArg[argPos].size());

            newArgs[argPos] = constantsPerArg[argPos][counter[argPos]];
        }
        instantiation.push_back(sig.substitute(Substitution::get(sig._args, newArgs)));            

        // Increment exponential counter
        int x = 0;
        while (x < counter.size()) {
            if (counter[x]+1 == constantsPerArg[x].size()) {
                // max value reached
                counter[x] = 0;
                if (x+1 == counter.size()) break;
            } else {
                // increment
                counter[x]++;
                break;
            }
            x++;
        }
        numInstantiations++;

        // Counter finished?
        if (counter[x] == 0 && x+1 == counter.size()) break;
    }

    assert(numChoices == numInstantiations || 
        fail(std::to_string(numChoices) + " != " + std::to_string(numInstantiations)) + "\n");
    
    return instantiation;
}