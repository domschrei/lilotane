
#include "data/signature.h"

namespace Substitution {

    substitution_t get(std::vector<int> src, std::vector<int> dest) {
        substitution_t s;
        assert(src.size() == dest.size());
        for (int i = 0; i < src.size(); i++) {
            if (src[i] != dest[i]) s[src[i]] = dest[i];
        }
        return s;
    }
}