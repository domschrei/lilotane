
#include "data/arg_iterator.h"
#include "data/htn_instance.h"

// TODO expensive copying
std::vector<Signature> ArgIterator::getFullInstantiation(const Signature& sig, HtnInstance& _htn) {
    std::vector<USignature> inst = getFullInstantiation(sig._usig, _htn);
    std::vector<Signature> result;
    for (const auto& s : inst) {
        result.push_back(s.toSignature(sig._negated));
    }
    return result;
}

std::vector<USignature> ArgIterator::getFullInstantiation(const USignature& sig, HtnInstance& _htn) {

    // "Empty" signature?    
    if (sig._args.empty()) {
        return std::vector<USignature>(1, sig);
    }

    // enumerate all arg combinations for variable args
    // Get all constants of the respective type(s)
    std::vector<int> sorts = _htn.getSorts(sig._name_id);
    assert(sorts.size() == sig._args.size() || Log::e("Sorts table of predicate %s has an invalid size\n", TOSTR(sig)));
    
    //log("SORTS %s ", TOSTR(sig._name_id));
    //for (int s : sorts) log("%s ", TOSTR(s));
    //log("\n");
    
    std::vector<std::vector<int>> constantsPerArg;

    for (size_t pos = 0; pos < sorts.size(); pos++) {
        int arg = sig._args[pos];

        if (_htn.isVariable(arg)) {
            // free argument

            int sort = sorts[pos];

            // Scan through all eligible arguments, filtering out q constants
            std::vector<int> eligibleConstants;
            for (int arg : _htn.getConstantsOfSort(sort)) {
                if (_htn.isQConstant(arg)) continue;
                eligibleConstants.push_back(arg);
            }

            // Empty instantiation if there is not a single eligible constant at some pos
            //log("OF_SORT %s : %i constants\n", _htn._name_back_table[sort].c_str(), eligibleConstants.size());
            if (eligibleConstants.empty()) return std::vector<USignature>();

            constantsPerArg.push_back(eligibleConstants);
        } else {
            // constant
            constantsPerArg.push_back(std::vector<int>(1, arg));
        }
    }

    return instantiate(sig, constantsPerArg);
}

std::vector<USignature> ArgIterator::instantiate(const USignature& sig, const std::vector<std::vector<int>>& constantsPerArg) {

    std::vector<USignature> instantiation;

    // Check validity
    assert(constantsPerArg.size() > 0);
    int numChoices = 1;
    for (const auto& vec : constantsPerArg) {
        numChoices *= vec.size();
    }
    if (numChoices == 0) return instantiation;
    instantiation.reserve(numChoices);

    // Iterate over all possible assignments
    std::vector<size_t> counter(constantsPerArg.size(), 0);
    int numInstantiations = 0;
    std::vector<int> newArgs(counter.size());
    while (true) {
        // Assemble the next assignment
        for (size_t argPos = 0; argPos < counter.size(); argPos++) {

            assert(argPos < constantsPerArg.size());
            assert(counter[argPos] < constantsPerArg[argPos].size());

            newArgs[argPos] = constantsPerArg[argPos][counter[argPos]];
        }
        // There may be multiple possible substitutions
        for (const Substitution& s : Substitution::getAll(sig._args, newArgs)) {
            instantiation.push_back(sig.substitute(s));            
        }

        // Increment exponential counter
        size_t x = 0;
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

    assert(numChoices <= numInstantiations || 
        Log::e("Erroneous instantiation: %s > %s\n", numChoices, numInstantiations));
    
    return instantiation;
}