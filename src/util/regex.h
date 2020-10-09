
#ifndef DOMPASCH_LILOTANE_REGEX_H
#define DOMPASCH_LILOTANE_REGEX_H

#include "ctre.hpp"

static constexpr ctll::fixed_string REGEX_SPLITTING_METHOD = ctll::fixed_string{ "_splitting_method_(.*)_splitted_[0-9]+" };

class Regex {
public:
    static void extractCoreNameOfSplittingMethod(std::string& input) {
        while (auto m = ctre::match<REGEX_SPLITTING_METHOD>(input)) {
            input = m.get<1>().to_view();
        }
    }
};

#endif