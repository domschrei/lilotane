
#include <iostream>

#include "util/hashmap.h"
#include "util/names.h"
#include "util/log.h"

NodeHashMap<int, std::string>* nbt;

namespace Names {
    
    void init(NodeHashMap<int, std::string>& nameBackTable) {
        nbt = &nameBackTable;
    }

    std::string to_string(int nameId) {
        if (nameId <= 0) return std::to_string(nameId);
        assert(nbt->count(nameId) || Log::e("No name known with ID %i!\n", nameId));
        return nbt->at(nameId);
    }

    std::string to_string(const std::vector<int>& nameIds) {
        std::string out = "(";
        bool first = true;
        for (const int& id : nameIds) {
            out += (first ? "" : " ") + std::string(id < 0 ? "-" : "") + Names::to_string(std::abs(id));
            first = false;
        }
        return out + ")";
    }

    std::string to_string(const std::vector<IntPair>& nameIdPairs) {
        std::string out = "(";
        bool first = true;
        for (const auto& [left, right] : nameIdPairs) {
            out += (first ? "(" : " (") 
                    + std::string(left < 0 ? "-" : "") + Names::to_string(std::abs(left))
                    + "," 
                    + std::string(right < 0 ? "-" : "") + Names::to_string(std::abs(right))
                + ")";
            first = false;
        }
        return out + ")";
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

    std::string to_string(const PositionedUSig& sig) {
        std::string out = to_string(sig.usig) + "@(" + std::to_string(sig.layer) + "," + std::to_string(sig.pos) + ")";
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

    std::string to_string(const USigSet& set) {
        std::string out = "{ ";
        for (const USignature& sig : set) {
            out += to_string(sig) + " ";
        }
        return out + "}";
    }
    std::string to_string(const SigSet& set) {
        std::string out = "{ ";
        for (const Signature& sig : set) {
            out += to_string(sig) + " ";
        }
        return out + "}";
    }

    std::string to_string(const FactFrame& f) {
        std::string out = "{\n";
        for (const Signature& pre : f.preconditions) {
            out += "  " + to_string(pre) + "\n";
        }
        out += "} " + to_string(f.sig) + " {\n";
        for (const auto& eff : f.effects) {
            out += "  " + to_string(eff) + "\n";
        }
        return out + "}";
    }

    std::string to_string(const Substitution& s) {
        std::string out = "";
        for (const auto& [src, dest] : s) {
            out += "[" + to_string(src) + "/" + to_string(dest) + "]";
        }
        return out;
    }
}