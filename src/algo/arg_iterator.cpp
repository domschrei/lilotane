
#include "algo/arg_iterator.h"
#include "data/htn_instance.h"

ArgIterator ArgIterator::getFullInstantiation(const USignature& sig, HtnInstance& _htn) {
    std::vector<std::vector<int>> constantsPerArg;

    // "Empty" signature?    
    if (sig._args.empty()) {
        return ArgIterator(sig._name_id, std::move(constantsPerArg));
    }

    // enumerate all arg combinations for variable args
    // Get all constants of the respective type(s)
    std::vector<int> sorts = _htn.getSorts(sig._name_id);
    assert(sorts.size() == sig._args.size() || Log::e("Sorts table of predicate %s has an invalid size\n", TOSTR(sig)));

    for (size_t pos = 0; pos < sorts.size(); pos++) {
        int arg = sig._args[pos];
        
        if (arg > 0 && _htn.isVariable(arg)) {
            // free argument and no placeholder

            int sort = sorts[pos];

            // Scan through all eligible arguments, filtering out q constants
            std::vector<int> eligibleConstants;
            for (int arg : _htn.getConstantsOfSort(sort)) {
                if (_htn.isQConstant(arg)) continue;
                eligibleConstants.push_back(arg);
            }

            // Empty instantiation if there is not a single eligible constant at some pos
            //log("OF_SORT %s : %i constants\n", _htn._name_back_table[sort].c_str(), eligibleConstants.size());
            if (eligibleConstants.empty()) {
                constantsPerArg.clear();
                return ArgIterator(sig._name_id, std::move(constantsPerArg));
            }

            constantsPerArg.push_back(eligibleConstants);
        } else {
            // constant
            constantsPerArg.push_back(std::vector<int>(1, arg));
        }
    }

    return ArgIterator(sig._name_id, std::move(constantsPerArg));
}
