
#include "assert.h"

#include "util/params.h"
#include "util/log.h"

/**
 * Taken from Hordesat:ParameterProcessor.h by Tomas Balyo.
 */
void Parameters::init(int argc, char** argv) {
    setDefaults();
    for (int i = 1; i < argc; i++) {
        char* arg = argv[i];
        if (arg[0] != '-') {
            if (_domain_filename == "") _domain_filename = std::string(arg);
            else if (_problem_filename == "") _problem_filename = std::string(arg);
            else {
                log("Unrecognized parameter %s.", arg);
                printUsage();
                exit(1);
            }
            continue;
        }
        char* eq = strchr(arg, '=');
        if (eq == NULL) {
            _params[arg+1];
        } else {
            *eq = 0;
            char* left = arg+1;
            char* right = eq+1;
            _params[left] = right;
        }
    }
    if (_problem_filename == "") {
        log("Please specify both a domain file and a problem file.\n");
        printUsage();
        exit(1);
    }
}

void Parameters::setDefaults() {
    setParam("aamo"); // All at-most-one constraints
    //setParam("cs"); // check solvability (without assumptions)
    setParam("d", "0"); // max depth (= num iterations)
    //setParam("nps"); // non-primitive fact supports
    //setParam("of"); // output formula to f.cnf
    //setParam("aamo"); // ALL At-most-one constraints, also for reductions
    //setParam("pvn"); // print variable names
    //setParam("q"); // q-constants
    //setParam("qq"); // no instantiation of preconditions
    //setParam("rrp"); // remove rigid predicates
}

void Parameters::printUsage() {

    log("Usage: treerexx <domainfile> <problemfile> [options]\n");
    log("  <domainfile>  Path to domain file in HDDL format.\n");
    log("  <problemfile> Path to problem file in HDDL format.\n");
    log("\n");
    log("Option syntax: -OPTION or -OPTION=VALUE .\n");
    log(" -aamo       Add ALL At-most-one constraints, also for reductions\n");
    log(" -cs         Check solvability: When some layer is UNSAT, re-run SAT solver without assumptions\n");
    log("             to see whether the formula has become generally unsatisfiable\n");
    log(" -d=<depth>  Maximum depth to explore (0 : no limit)\n");
    log("             default: %i\n", getIntParam("d"));
    log(" -nps        Nonprimitive support: Enable encoding explicit fact supports for reductions\n");
    log(" -of         Output generated formula to text file \"f.cnf\" (with assumptions used in final call)\n");
    log(" -pvn        Print variable names\n");
    log(" -q          For each action and reduction, introduces q-constants for any ambiguous free parameters\n");
    log("             after fully instantiating all preconditions\n");
    log(" -qq         For each action and reduction, introduces q-constants for ALL ambiguous free parameters (replaces -q)\n");
    log(" -rrp        Remove rigid predicates\n");
}

std::string Parameters::getDomainFilename() {
  return _domain_filename;
}
std::string Parameters::getProblemFilename() {
  return _problem_filename;
}

void Parameters::printParams() {
    std::string out = "";
    for (std::map<std::string,std::string>::iterator it = _params.begin(); it != _params.end(); it++) {
        if (it->second.empty()) {
            out += it->first + ", ";
        } else {
            out += it->first + "=" + it->second + ", ";
        }
    }
    out = out.substr(0, out.size()-2);
    log("Called with parameters: %s\n", out.c_str());
}

void Parameters::setParam(const char* name) {
    _params[name];
}

void Parameters::setParam(const char* name, const char* value) {
    _params[name] = value;
}

bool Parameters::isSet(const std::string& name) const {
    return _params.count(name);
}

std::string Parameters::getParam(const std::string& name, const std::string& defaultValue) {
    if (isSet(name)) {
        return _params[name];
    } else {
        return defaultValue;
    }
}

std::string Parameters::getParam(const std::string& name) {
    return getParam(name, "ndef");
}

int Parameters::getIntParam(const std::string& name, int defaultValue) {
    if (isSet(name)) {
        return atoi(_params[name].c_str());
    } else {
        return defaultValue;
    }
}

int Parameters::getIntParam(const std::string& name) {
    assert(isSet(name));
    return atoi(_params[name].c_str());
}

float Parameters::getFloatParam(const std::string& name, float defaultValue) {
    if (isSet(name)) {
        return atof(_params[name].c_str());
    } else {
        return defaultValue;
    }
}

float Parameters::getFloatParam(const std::string& name) {
    assert(isSet(name));
    return atof(_params[name].c_str());
}
