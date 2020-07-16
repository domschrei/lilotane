
#ifndef DOMPASCH_TREEREXX_Q_CONSTANT_CONDITION_H
#define DOMPASCH_TREEREXX_Q_CONSTANT_CONDITION_H

#include <vector>
#include <algorithm>
#include <assert.h>

#include "hashmap.h"
#include "util/hash.h"
#include "util/log.h"
#include "data/htn_op.h"
#include "util/names.h"

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

int originOp;
std::vector<int> reference;
char conjunction;
ValueSet values;

QConstantCondition(int originOp, const std::vector<int>& reference, char conjunction) 
            : originOp(originOp), reference(reference), conjunction(conjunction) {}
QConstantCondition(int originOp, const std::vector<int>& reference, char conjunction, const ValueSet& values) 
            : originOp(originOp), reference(reference), conjunction(conjunction), values(values) {}
QConstantCondition(const QConstantCondition& other) 
            : originOp(other.originOp), reference(other.reference), conjunction(other.conjunction), values(other.values) {}

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

bool operator==(const QConstantCondition& other) const {
    return originOp == other.originOp 
        && conjunction == other.conjunction 
        && reference == other.reference 
        && values == other.values;
}

bool operator!=(const QConstantCondition& other) const {
    return !(*this == other);
}

std::string toStr() const {
    std::string out = "QCC{";
    for (int arg : reference) out += Names::to_string(arg) + ",";
    out += " : ";
    out += conjunction == QConstantCondition::CONJUNCTION_OR ? "OR " : "NOR ";
    for (const auto& tuple : values) {
        out += "(";
        for (int arg : tuple) out += Names::to_string(arg) + ",";
        out += ") ";
    }
    out += "}";
    return out;
}
};

struct QConstCondHasher {
std::size_t operator()(const QConstantCondition* q) const {
    size_t hash = q->conjunction;
    hash_combine(hash, q->originOp);
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
struct QConstCondEqualsNoOpCheck {
bool operator()(const QConstantCondition* left, const QConstantCondition* right) const {
    return left->conjunction == right->conjunction 
            && left->reference == right->reference 
            && left->values == right->values;
}
};

struct PositionedUSig {
    int layer; int pos; USignature usig;
    PositionedUSig(int layer, int pos, const USignature& usig) : layer(layer), pos(pos), usig(usig) {}
    bool operator==(const PositionedUSig& other) const {
        return layer == other.layer && pos == other.pos && usig == other.usig;
    }
};
struct PositionedUSigHasher {
    USignatureHasher usigHasher;
    std::size_t operator()(const PositionedUSig& x) const {
        size_t hash = x.layer;
        hash_combine(hash, x.pos);
        hash_combine(hash, usigHasher(x.usig));
        return hash;
    }
};

class QConstantDatabase {

private:
    std::function<bool(int)> _is_q_constant;

    std::vector<std::vector<QConstantCondition*>> _conditions_per_op;
    NodeHashMap<int, std::pair<int, NodeHashSet<QConstantCondition*, QConstCondHasher, QConstCondEquals>>> _q_const_map;

    FlatHashMap<PositionedUSig, int, PositionedUSigHasher> _op_ids;
    std::vector<PositionedUSig> _op_sigs;
    std::vector<std::vector<int>> _op_possible_parents;
    // 1st dim: operation IDs. 2nd dim: offset. 3rd dim: enumeration of children. 
    std::vector<std::vector<std::vector<int>>> _op_children_at_offset;

public:
    static const PositionedUSig PSIG_ROOT;

    QConstantDatabase(const std::function<bool(int)>& isQConstant);

    int addOp(const HtnOp& op, int layer, int pos, const PositionedUSig& parent, int offset);

    /*
    Adds a new q-constant condition to the database (except if it already exists).
    */
    QConstantCondition* addCondition(int op, const std::vector<int>& reference, char conjunction, const ValueSet& values);

    /*
    For a given set of q-constants with corresponding substituted values,
    test whether this is a valid substitution according to the contained conditions.
    */
    bool test(const std::vector<int>& refQConsts, const std::vector<int>& vals);

    int getRootOp(int qconst);
    const NodeHashSet<QConstantCondition*, QConstCondHasher, QConstCondEquals>& getConditions(int qconst);

    bool isUniversal(QConstantCondition* cond);
    bool isParentAndChild(int parent, int child);

    NodeHashSet<PositionedUSig, PositionedUSigHasher> backpropagateConditions(int layer, int pos, const NodeHashMap<USignature, int, USignatureHasher>& leafOps);

    ~QConstantDatabase() {
        for (const auto& entry : _conditions_per_op) {
            for (const auto& qcc : entry) {
                delete qcc;
            }
        }
    }

private:
    void forEachFitCondition(const std::vector<int>& qConsts, std::function<bool(QConstantCondition*)> onVisit);

};

#endif