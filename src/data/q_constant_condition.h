
#ifndef DOMPASCH_TREEREXX_Q_CONSTANT_CONDITION_H
#define DOMPASCH_TREEREXX_Q_CONSTANT_CONDITION_H

#include <vector>
#include <algorithm>
#include <assert.h>

#include "hashmap.h"
#include "util/hash.h"
#include "util/log.h"

struct ValuesHasher {
std::size_t operator()(const std::vector<int>& v) const {
    size_t hash = v.size();
    for (const auto& val : v) {
        hash_combine(hash, val);
    }
    return hash;
}
};

typedef NodeHashSet<std::vector<int>, ValuesHasher> ValueSet;

struct QConstantCondition {

static const char CONJUNCTION_OR = 1;
static const char CONJUNCTION_NOR = 2;

std::vector<int> reference;
char conjunction;
ValueSet values;

QConstantCondition(const std::vector<int>& reference, char conjunction) 
            : reference(reference), conjunction(conjunction) {}
QConstantCondition(const std::vector<int>& reference, char conjunction, const ValueSet& values) 
            : reference(reference), conjunction(conjunction), values(values) {}
QConstantCondition(const QConstantCondition& other) : reference(other.reference), conjunction(other.conjunction), values(other.values) {}

void add(const std::vector<int>& tuple) {
    values.insert(tuple);
}

bool test(const std::vector<int>& args) const {
    bool partial = false;
    for (int a : args) if (a == 0) partial = true;

    if (partial) {
        // Partial definition: need to check each values tuple separately
        for (const auto& tuple : values) {
            bool fits = true;
            for (int i = 0; i < args.size(); i++) {
                if (args[i] != 0 && args[i] != tuple[i]) fits = false;
            }
            if (fits) return conjunction == CONJUNCTION_OR;
        }
        return conjunction == CONJUNCTION_NOR;    
    }

    // Not partial: quick set containment query
    if (values.count(args)) return conjunction == CONJUNCTION_OR;
    return conjunction == CONJUNCTION_NOR;
}

bool test(const std::vector<int>& ref, const std::vector<int>& vals) const {
    std::vector<int> normVals; normVals.reserve(reference.size());
    for (int i = 0; i < reference.size(); i++) {
        for (int j = 0; j < ref.size(); j++) {
            if (reference[i] == ref[j]) {
                normVals.push_back(vals[j]);
            }
        }
        if (normVals.size() <= i) normVals.push_back(0);
    }
    return test(normVals);
}

bool referencesSubsetOf(const std::vector<int>& otherReference) const {
    if (otherReference.size() < reference.size()) return false;
    // Each q-constant from THIS reference must be contained in OTHER reference
    for (const auto& q : reference) {
        if (std::find(otherReference.begin(), otherReference.end(), q) == otherReference.end()) {
            // Not contained in alleged superset
            return false;
        }
    }
    return true;
}

ValueSet getIntersection(const ValueSet& vals) const {
    ValueSet result;

    for (const auto& tuple : vals) {
        if (values.count(tuple) == (conjunction == CONJUNCTION_OR)) {
            // fine - (N)OR is satisfied
            result.insert(tuple);
        }
    }

    return result;
}

QConstantCondition simplify(const std::vector<int>& ref, const std::vector<int>& vals) {
    assert(ref.size() < reference.size());
    std::vector<int> newRef;
    for (int i = 0; i < ref.size(); i++) {
        const auto& it = std::find(reference.begin(), reference.end(), ref[i]);
        assert(it != reference.end());
        newRef.push_back(*it);
    }
    QConstantCondition newCond(newRef, conjunction);
    for (const auto& tuple : values) {

    }
}

bool operator==(const QConstantCondition& other) const {
    return conjunction == other.conjunction && reference == other.reference && values == other.values;
}

bool operator!=(const QConstantCondition& other) const {
    return !(*this == other);
}

};

struct QConstCondHasher {
std::size_t operator()(const QConstantCondition* q) const {
    size_t hash = q->conjunction;
    hash_combine(hash, q->reference.size());
    for (const auto& ref : q->reference) {
        hash_combine(hash, ref);
    }
    hash_combine(hash, q->values.size());
    for (const auto& tuple : q->values) {
        for (const int& arg : tuple) {
            hash_combine(hash, arg);
        }
    }
    return hash;
}
};

struct QConstCondEquals {
bool operator()(const QConstantCondition* left, const QConstantCondition* right) const {
    return *left == *right;
}
};

class QConstantDatabase {

private:
    FlatHashSet<int> _registered_q_constants;
    std::vector<QConstantCondition*> _conditions;
    NodeHashMap<int, NodeHashSet<QConstantCondition*, QConstCondHasher, QConstCondEquals>> _conditions_per_qconst;

public:

    bool isRegistered(int qconst);
    void registerQConstant(int qconst);

    /*
    Adds a new q-constant condition to the database (except if it already exists).
    */
    void add(const std::vector<int>& reference, char conjunction, const ValueSet& values);

    /*
    For a given set of q-constants with corresponding substituted values,
    test whether this is a valid substitution according to the contained conditions.
    */
    bool test(const std::vector<int>& refQConsts, const std::vector<int>& vals);

    ~QConstantDatabase() {
        for (const auto& cond : _conditions) {
            delete cond;
        }
    }

private:
    void forEachFitCondition(const std::vector<int>& qConsts, std::function<bool(QConstantCondition*)> onVisit);

};

#endif