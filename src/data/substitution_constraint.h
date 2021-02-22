
#ifndef DOMPASCH_LILOTANE_SUBSTITUTION_CONSTRAINT_H
#define DOMPASCH_LILOTANE_SUBSTITUTION_CONSTRAINT_H

#include "data/htn_instance.h"
#include "sat/literal_tree.h"
#include "util/log.h"

typedef LiteralTree<int, std::hash<int>> IntTree;

class SubstitutionConstraint {

public:
    enum Polarity {UNDECIDED, ANY_VALID, NO_INVALID};

private:
    std::vector<int> _involved_q_consts;
    IntTree _valid_substitutions;
    IntTree _invalid_substitutions;
    Polarity _polarity = UNDECIDED;

public:
    SubstitutionConstraint(const std::vector<int>& involvedQConsts) : _involved_q_consts(involvedQConsts) {}
    SubstitutionConstraint(std::vector<int>&& involvedQConsts) : _involved_q_consts(std::move(involvedQConsts)) {}

    void addValid(const std::vector<int>& vals) {
        _valid_substitutions.insert(vals);
    }

    void addInvalid(const std::vector<int>& vals) {
        _invalid_substitutions.insert(vals);
    }

    void fixPolarity() {
        Log::d("%i valids, %i invalids\n", _valid_substitutions.getSizeOfEncoding(), _invalid_substitutions.getSizeOfNegationEncoding());
        if (_invalid_substitutions.getSizeOfNegationEncoding() > _valid_substitutions.getSizeOfEncoding()) {
            // More invalids
            _invalid_substitutions = IntTree();
            _polarity = ANY_VALID;
        } else {
            // More valids
            _valid_substitutions = IntTree();
            _polarity = NO_INVALID;
        }
    }

    bool isValid(const std::vector<int>& sub) const {
        if (_polarity == ANY_VALID) {
            return _valid_substitutions.contains(sub);
        } else {
            return !_invalid_substitutions.contains(sub);
        }
    }

    std::vector<std::vector<int>> getEncoding(Polarity p = UNDECIDED) const {
        if (p == ANY_VALID) return _valid_substitutions.encode();
        if (p == NO_INVALID) return _invalid_substitutions.encodeNegation();
        if (_polarity == ANY_VALID) return _valid_substitutions.encode();
        return _invalid_substitutions.encodeNegation();
    }

    size_t getEncodedSize() const {
        return _valid_substitutions.getSizeOfEncoding() + _invalid_substitutions.getSizeOfNegationEncoding();
    }

    Polarity getPolarity() const {return _polarity;}

    const std::vector<int>& getInvolvedQConstants() const {return _involved_q_consts;}

    static std::vector<int> getSortedSubstitutedArgIndices(HtnInstance& htn, const std::vector<int>& qargs, const std::vector<int>& sorts) {

        // Collect indices of arguments which will be substituted
        std::vector<int> argIndices;
        for (size_t i = 0; i < qargs.size(); i++) {
            if (htn.isQConstant(qargs[i])) argIndices.push_back(i);
        }

        // Sort argument indices by the potential size of their domain
        std::sort(argIndices.begin(), argIndices.end(), 
                [&](int i, int j) {return htn.getConstantsOfSort(sorts[i]).size() < htn.getConstantsOfSort(sorts[j]).size();});
        return argIndices;
    }

    static std::vector<int> decodingToPath(const std::vector<int>& decArgs, const std::vector<int>& sortedIndices) {
        
        // Write argument substitutions into the result in correct order
        std::vector<int> path;
        path.reserve(sortedIndices.size());
        for (size_t x = 0; x < sortedIndices.size(); x++) {
            size_t argIdx = sortedIndices[x];
            path.push_back(decArgs[argIdx]);
        }
        return path;
    }
};

#endif