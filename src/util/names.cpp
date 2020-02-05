
#include <unordered_map>
#include <iostream>

#include "util/names.h"

std::unordered_map<int, std::string>* nbt;

bool fail(std::string msg) {
    printf(msg.c_str());
    return false;
}

namespace Names {
    
    void init(std::unordered_map<int, std::string>& nameBackTable) {
        nbt = &nameBackTable;
    }

    std::string to_string(int nameId) {
        if (nameId < 0) return std::to_string(nameId);
        assert(nbt->count(nameId) || fail("No name known with ID " + std::to_string(nameId) + "!\n"));
        return nbt->at(nameId);
    }

    std::string to_string(Signature sig) {
        std::string out = "";
        if (sig._negated) out += "!";
        out += "(" + to_string(sig._name_id);
        for (int arg : sig._args) {
            out += " " + to_string(arg);
        }
        out += ")";
        return out;
    }

    std::string to_string(std::unordered_map<int, int> s) {
        std::string out = "";
        for (auto pair : s) {
            out += "[" + to_string(pair.first) + "/" + to_string(pair.second) + "]";
        }
        return out;
    }
}