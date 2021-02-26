
#ifndef DOMPASCH_LILOTANE_SUBSTITUTION_CONSTRAINT_H
#define DOMPASCH_LILOTANE_SUBSTITUTION_CONSTRAINT_H

#include "data/htn_instance.h"
#include "sat/literal_tree.h"
#include "util/log.h"

typedef LiteralTree<IntPair, IntPairHasher> IntPairTree;

class SubstitutionConstraint {

public:
    enum Polarity {UNDECIDED, ANY_VALID, NO_INVALID};

private:
    std::vector<int> _involved_q_consts;
    IntPairTree _valid_substitutions;
    IntPairTree _invalid_substitutions;
    Polarity _polarity = UNDECIDED;

public:
    SubstitutionConstraint(const std::vector<int>& involvedQConsts) : _involved_q_consts(involvedQConsts) {}
    SubstitutionConstraint(std::vector<int>&& involvedQConsts) : _involved_q_consts(std::move(involvedQConsts)) {}
    SubstitutionConstraint(const SubstitutionConstraint& other) : 
        _involved_q_consts(other._involved_q_consts), 
        _valid_substitutions(other._valid_substitutions),
        _invalid_substitutions(other._invalid_substitutions),
        _polarity(other._polarity) {}
    SubstitutionConstraint(SubstitutionConstraint&& other) : 
        _involved_q_consts(other._involved_q_consts), 
        _valid_substitutions(std::move(other._valid_substitutions)),
        _invalid_substitutions(std::move(other._invalid_substitutions)),
        _polarity(other._polarity) {}

    SubstitutionConstraint& operator=(const SubstitutionConstraint& other) {
        _involved_q_consts = other._involved_q_consts;
        _valid_substitutions = other._valid_substitutions;
        _invalid_substitutions = other._invalid_substitutions;
        _polarity = other._polarity;
        return *this;
    }

    void addValid(const std::vector<IntPair>& vals) {
        _valid_substitutions.insert(vals);
    }

    void addInvalid(const std::vector<IntPair>& vals) {
        _invalid_substitutions.insert(vals);
    }

    void fixPolarity(Polarity polarity = UNDECIDED) {
        size_t negSize = _invalid_substitutions.getSizeOfNegationEncoding();
        size_t posSize = _valid_substitutions.getSizeOfEncoding();
        if (polarity == ANY_VALID || (polarity == UNDECIDED && negSize > posSize)) {
            // More invalids
            _invalid_substitutions = IntPairTree();
            _polarity = ANY_VALID;
        } else {
            // More valids
            _valid_substitutions = IntPairTree();
            _polarity = NO_INVALID;
        }
    }

    bool involvesSupersetOf(const std::vector<int>& involvedQConsts) const {
        // Every q-constant in the query must also be in the involved q-constants
        // (in the same order), otherwise no meaningful check can be done
        size_t j = 0;
        for (size_t i = 0; i < involvedQConsts.size(); i++) {
            while (j < _involved_q_consts.size() && _involved_q_consts[j] != involvedQConsts[i])
                j++;
            if (j == _involved_q_consts.size()) return false;
        }
        return true;
    }

    bool isValid(const std::vector<IntPair>& sub, bool sameReference) const {
        if (_polarity == ANY_VALID) {
            // Same involved q-constants: Can perform exact (in)validity check
            return sameReference ? _valid_substitutions.contains(sub) : _valid_substitutions.subsumes(sub);
        } else {
            return sameReference ? !_invalid_substitutions.contains(sub) : !_invalid_substitutions.hasPathSubsumedBy(sub);
        }
    }

    bool canMerge(const SubstitutionConstraint& other) const {
        if (_polarity != other._polarity) return false;
        if (_polarity == UNDECIDED) return false;
        // Must have same _involved_q_consts
        return _involved_q_consts == other._involved_q_consts;
    }

    void merge(SubstitutionConstraint&& other) {
        if (_polarity == ANY_VALID) {
            // intersect paths of both literal trees
            _valid_substitutions.intersect(std::move(other._valid_substitutions));
        }
        if (_polarity == NO_INVALID) {
            // unite paths of both literal trees
            _invalid_substitutions.merge(std::move(other._invalid_substitutions));
        }
    }

    std::vector<std::vector<IntPair>> getEncoding(Polarity p = UNDECIDED) const {
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

    static std::vector<IntPair> decodingToPath(const std::vector<int>& qArgs, const std::vector<int>& decArgs, const std::vector<int>& sortedIndices) {
        
        // Write argument substitutions into the result in correct order
        std::vector<IntPair> path;
        path.reserve(sortedIndices.size());
        for (size_t x = 0; x < sortedIndices.size(); x++) {
            size_t argIdx = sortedIndices[x];
            path.emplace_back(IntPair(qArgs[argIdx], decArgs[argIdx]));
        }
        return path;
    }
};

#endif