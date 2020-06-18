
#include <iostream>

#include "data/hashmap.h"
#include "util/names.h"
#include "util/log.h"

NodeHashMap<int, std::string>* nbt;

namespace Names {
    
    void init(NodeHashMap<int, std::string>& nameBackTable) {
        nbt = &nameBackTable;
    }

    std::string to_string(int nameId) {
        if (nameId <= 0) return std::to_string(nameId);
        assert(nbt->count(nameId) || fail("No name known with ID " + std::to_string(nameId) + "!\n"));
        return nbt->at(nameId);
    }

    std::string to_string(const USignature& sig) {
        std::string out = "";
        out += "(" + to_string(sig._name_id);
        for (int arg : sig._args) {
            out += " " + to_string(arg);
        }
        out += ")";
        return out;
    }

    std::string to_string(const Signature& sig) {
        std::string out = "";
        if (sig._negated) out += "!";
        out += "(" + to_string(sig._usig._name_id);
        for (int arg : sig._usig._args) {
            out += " " + to_string(arg);
        }
        out += ")";
        return out;
    }

    std::string to_string_nobrackets(const USignature& sig) {
        std::string out = "";
        out += to_string(sig._name_id);
        for (int arg : sig._args) {
            out += " " + to_string(arg);
        }
        return out;
    }

    std::string to_string(const FlatHashMap<int, int>& s) {
        std::string out = "";
        for (const auto& pair : s) {
            out += "[" + to_string(pair.first) + "/" + to_string(pair.second) + "]";
        }
        return out;
    }

    std::string to_string(const Action& a) {
        std::string out = "{ ";
        for (const Signature& pre : a.getPreconditions()) {
            out += to_string(pre) + " ";
        }
        out += "} " + to_string(a.getSignature()) + " { ";
        for (const Signature& eff : a.getEffects()) {
            out += to_string(eff) + " ";
        }
        return out + "}";
    }
}