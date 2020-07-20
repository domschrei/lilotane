
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
                Log::e("Unrecognized parameter %s.", arg);
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
        Log::e("Please specify both a domain file and a problem file.\n");
        printUsage();
        exit(1);
    }
}

void Parameters::setDefaults() {
    setParam("amor", "20"); // Max. num reductions such that At-most-one constraints are added for reductions
    //setParam("cs"); // check solvability (without assumptions)
    setParam("qcm", "0"); // q-constant mutexes: size threshold
    setParam("d", "0"); // min depth to start SAT solving at
    setParam("D", "0"); // max depth (= num iterations)
    //setParam("nps"); // non-primitive fact supports
    //setParam("of"); // output formula to f.cnf
    //setParam("p"); // encode predecessor operations
    //setParam("pvn"); // print variable names
    setParam("qit", "0"); // q-constant instantiation threshold
    setParam("qrf", "0"); // q-constant rating factor
    //setParam("q"); // q-constants
    //setParam("qq"); // no instantiation of preconditions
    //setParam("rrp"); // remove rigid predicates
    setParam("v", "2"); // verbosity
}

void Parameters::printUsage() {

    Log::e("Usage: treerexx <domainfile> <problemfile> [options]\n");
    Log::e("  <domainfile>  Path to domain file in HDDL format.\n");
    Log::e("  <problemfile> Path to problem file in HDDL format.\n");
    Log::e("\n");
    Log::e("Option syntax: -OPTION or -OPTION=VALUE .\n");
    Log::e(" -amor=<threshold>   Add At-most-one constraints for reductions if there are at most <threshold> \
reductions at the current position (0 : no AMO constraints for reductions)\n");
    Log::e(" -cs           Check solvability: When some layer is UNSAT, re-run SAT solver without assumptions\n");
    Log::e("               to see whether the formula has become generally unsatisfiable\n");
    Log::e(" -qcm=<limit>  Collect up to <limit> q-constant mutexes per tuple of q-constants\n");
    Log::e(" -d=<depth>    Minimum depth to begin SAT solving at\n");
    Log::e(" -D=<depth>    Maximum depth to explore (0 : no limit)\n");
    Log::e("               default: %i\n", getIntParam("D"));
    Log::e(" -nps          Nonprimitive support: Enable encoding explicit fact supports for reductions\n");
    Log::e(" -of           Output generated formula to text file \"f.cnf\" (with assumptions used in final call)\n");
    Log::e(" -p            Encode predecessor operations\n");
    Log::e(" -pvn          Print variable names\n");
    Log::e(" -qrf=<limit>  If -q or -qq, do NOT introduce q-constant if an operation has at most <limit> instantiations\n");
    Log::e(" -qrf=<factor> If -q or -qq, multiply precondition rating used for q-constant identification with <factor>\n");
    Log::e(" -q            For each action and reduction, introduces q-constants for any ambiguous free parameters\n");
    Log::e("               after fully instantiating all preconditions\n");
    Log::e(" -qq           For each action and reduction, introduces q-constants for ALL ambiguous free parameters (replaces -q)\n");
    Log::e(" -rrp          Remove rigid predicates\n");
    Log::e(" -v=<verb>     Verbosity: 0=essential 1=warnings 2=information 3=verbose 4=debug\n");
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
    Log::i("Called with parameters: %s\n", out.c_str());
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
