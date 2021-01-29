
#ifndef DOMSCHREI_LILOTANE_BINARY_AMO_H
#define DOMSCHREI_LILOTANE_BINARY_AMO_H

#include <vector>

class BinaryAtMostOne {

private:
    std::vector<int> _states;
    size_t _num_states;
    size_t _num_repr_states;
    std::vector<int> _bin_num_vars;

public:
    BinaryAtMostOne(const std::vector<int>& states, size_t numStates);
    std::vector<std::vector<int>> encode();

private:
    std::vector<int> getClause(int state, bool sign) const;

};

#endif