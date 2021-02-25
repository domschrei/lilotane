
#include "binary_amo.h"

#include "variable_domain.h"
#include "util/log.h"

BinaryAtMostOne::BinaryAtMostOne(const std::vector<int>& states, size_t numStates) : _states(states), _num_states(numStates) {

    // Set up helper variables for a binary number representation
    _num_repr_states = 1;
    while (_num_repr_states < _num_states) {
        int var = VariableDomain::nextVar();
        Log::d("VARMAP %i (__amo_%i-%i_%i)\n", var, states[0], states[states.size()-1], _bin_num_vars.size());
        _bin_num_vars.push_back(var);
        _num_repr_states *= 2;
    }
    /*
    Log::d("BAMO vars:%lu binvars:%lu states:%lu reprstates:%i forbstates:[%i,%i)\n", 
            _states.size(), _bin_num_vars.size(), _num_states, _num_repr_states, _num_states, _num_repr_states);
    assert(!_bin_num_vars.empty());
    */
}

std::vector<std::vector<int>> BinaryAtMostOne::encode() {
    std::vector<std::vector<int>> cls;
    
    if (_num_states <= 1) return cls;

    // For each possible state with a variable representing it
    for (size_t state = 0; state < _states.size(); state++) {
        // Encode direction "=>"
        assert(!_bin_num_vars.empty());
        auto digitVars = getClause(state, false);
        for (int digitVar : digitVars) {
            std::vector<int> ifStateThenDigit(2);
            ifStateThenDigit[0] = -_states[state];
            ifStateThenDigit[1] = -digitVar;
            cls.push_back(std::move(ifStateThenDigit));
        }
        // Encode direction "<="
        auto& ifDigitsThenState = digitVars;
        ifDigitsThenState.push_back(_states[state]);
        cls.push_back(std::move(ifDigitsThenState));
    }

    // Forbid all representable but invalid states
    // by forming blocks of digit combinations that must be false
    int blockSize = _bin_num_vars.size();
    size_t firstForbiddenState = _num_repr_states;
    while (blockSize >= 0) {
        int diff = firstForbiddenState - _num_states;
        if (diff == 0) break;
        int expBlockSize = 1 << blockSize;
        if (diff >= expBlockSize) {
            firstForbiddenState -= expBlockSize;
            Log::d("BAMO forbid block [%i,%i)\n", firstForbiddenState, firstForbiddenState+expBlockSize);
            auto clause = getClause(firstForbiddenState, false);
            std::vector<int> constraint;
            for (size_t i = 0; i < _bin_num_vars.size() - blockSize; i++) {
                constraint.push_back(clause[clause.size()-i-1]);
            }
            cls.push_back(constraint);
        }
        blockSize--;
    }
    assert(firstForbiddenState == _num_states);

    /*
    Log::d("BAMO =>\n");    
    for (auto c : cls) {
        Log::d("BAMO ");
        for (auto lit : c) Log::log_notime(Log::V4_DEBUG, "%i ", lit);
        Log::log_notime(Log::V4_DEBUG, "\n");
    }
    Log::d("<= BAMO\n");*/

    return cls;
}

std::vector<int> BinaryAtMostOne::getClause(int state, bool sign) const {
    assert(!_bin_num_vars.empty());
    std::vector<int> cls(_bin_num_vars.size());
    for (size_t i = 0; i < _bin_num_vars.size(); i++) {
        bool mod = (state & 0x1);
        cls[i] = (!sign ^ mod ? 1 : -1) * _bin_num_vars[i];
        state >>= 1;
    }
    return cls;
}