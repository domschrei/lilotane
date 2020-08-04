
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
            char* left = arg+1;
            if (_params.count(left) && _params[left] == "0") _params[left] = "1";
            else _params[left];
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
    setParam("amor", "100"); // Max. num reductions such that At-most-one constraints are added for reductions
    setParam("co", "1"); // colored output
    setParam("cs", "0"); // check solvability (without assumptions)
    setParam("d", "0"); // min depth to start SAT solving at
    setParam("D", "0"); // max depth (= num iterations)
    setParam("ip", "0"); // implicit primitiveness
    setParam("mp", "1"); // mine preconditions
    setParam("nps", "0"); // non-primitive fact supports
    setParam("of", "0"); // output formula to f.cnf
    setParam("p", "1"); // encode predecessor operations
    setParam("pvn", "0"); // print variable names
    setParam("qcm", "0"); // q-constant mutexes: size threshold
    setParam("qit", "0"); // q-constant instantiation threshold
    setParam("qrf", "0"); // q-constant rating factor
    setParam("q", "0"); // q-constants while always instantiating all preconditions
    setParam("qq", "1"); // q-constants without instantiation of preconditions
    setParam("sace", "0"); // split actions with (potentially) conflicting effects
    setParam("stl", "0"); // SAT time limit
    setParam("surr", "1"); // replace surrogate methods
    setParam("svp", "0"); // set variable phases
    setParam("tc", "0"); // tree conversion for DNF2CNF
    setParam("v", "2"); // verbosity
}

void Parameters::printUsage() {

    Log::e("Usage: treerexx <domainfile> <problemfile> [options]\n");
    Log::e("  <domainfile>  Path to domain file in HDDL format.\n");
    Log::e("  <problemfile> Path to problem file in HDDL format.\n");
    Log::e("\n");
    Log::e("Option syntax: -OPTION or -OPTION=VALUE .\n");
    Log::e(" -amor=<threshold>   Add At-most-one constraints for reductions if there are at most <threshold> reductions\n");
    Log::e("                     at the current position (0 : no AMO constraints for reductions)\n");
    Log::e(" -co=<0|1>           Colored terminal output\n");
    Log::e(" -cs=<0|1>           Check solvability: When some layer is UNSAT, re-run SAT solver without assumptions\n");
    Log::e("                     to see whether the formula has become generally unsatisfiable\n");
    Log::e(" -d=<depth>          Minimum depth to begin SAT solving at\n");
    Log::e(" -D=<depth>          Maximum depth to explore (0 : no limit)\n");
    Log::e(" -ip=<0|1>           Implicit primitiveness instead of defining each op as primitive XOR nonprimitive\n");
    Log::e(" -mp=<0|1>           Mine preconditions for reductions from their (recursive) subtasks\n");
    Log::e(" -nps=<0|1>          Nonprimitive support: Enable encoding explicit fact supports for reductions\n");
    Log::e(" -of=<0|1>           Output generated formula to text file \"f.cnf\" (with assumptions used in final call)\n");
    Log::e(" -p=<0|1>            Encode predecessor operations\n");
    Log::e(" -pvn=<0|1>          Print variable names\n");
    Log::e(" -qcm=<limit>        Collect up to <limit> q-constant mutexes per tuple of q-constants\n");
    Log::e(" -qit=<threshold>    Q-constant instantiation threshold: fully instantiate up to <threshold> operations\n");
    Log::e(" -qrf=<factor>       If -q or -qq, multiply precondition rating used for q-constant identification with <factor>\n");
    Log::e(" -q=<0|1>            For each action and reduction, introduces q-constants for any ambiguous free parameters\n");
    Log::e("                     after fully instantiating all preconditions\n");
    Log::e(" -qq=<0|1>           For each action and reduction, introduces q-constants for ALL ambiguous free parameters (replaces -q)\n");
    Log::e(" -sace=<0|1>         Split actions with (potentially) conflicting effects into two actions\n");
    Log::e(" -stl=<limit>        SAT time limit: Set limit in seconds for a SAT solver call. Limit is discarded after first such interrupt.\n");
    Log::e(" -surr=<0|1>         Replace surrogate methods with their only subtask (supplied with additional preconditions)\n");
    Log::e(" -tc=<0|1>           Use tree conversion for DNF 2 CNF transformation instead of distributive law\n");
    Log::e(" -v=<verb>           Verbosity: 0=essential 1=warnings 2=information 3=verbose 4=debug\n");
    printParams(/*forcePrint=*/true);
}

std::string Parameters::getDomainFilename() {
  return _domain_filename;
}
std::string Parameters::getProblemFilename() {
  return _problem_filename;
}

void Parameters::printParams(bool forcePrint) {
    std::string out = "";
    for (std::map<std::string,std::string>::iterator it = _params.begin(); it != _params.end(); ++it) {
        if (it->second.empty()) {
            out += it->first + ", ";
        } else {
            out += it->first + "=" + it->second + ", ";
        }
    }
    out = out.substr(0, out.size()-2);
    (forcePrint ? Log::e : Log::i)("Called with parameters: %s\n", out.c_str());
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

bool Parameters::isNonzero(const std::string& intParamName) const {
    return atoi(_params.at(intParamName).c_str()) != 0;
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
