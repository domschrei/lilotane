
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
                Log::w("Unrecognized parameter %s.", arg);
                printUsage();
                exit(1);
            }
            continue;
        }
        char* eq = strchr(arg, '=');
        if (eq == NULL) {
            char* left = arg+1;
            auto it = _params.find(left);
            if (it != _params.end() && it->second == "0") it->second = "1";
            else _params[left];
        } else {
            *eq = 0;
            char* left = arg+1;
            char* right = eq+1;
            _params[left] = right;
        }
    }
}

void Parameters::setDefaults() {
    setParam("alo", "0"); // explicitly encode "at-least-one" over elements at each position
    setParam("bamot", "50"); // Binary at-most-one threshold
    setParam("cleanup", "0"); // clean up before exit?
    setParam("co", "1"); // colored output
    setParam("cs", "0"); // check solvability (without assumptions)
    setParam("d", "0"); // min depth to start SAT solving at
    setParam("D", "0"); // max depth (= num iterations)
    setParam("edo", "1"); // eliminate dominated operations
    setParam("el", "0"); // extra layers after initial solution (-1: expand indefinitely)
    setParam("ip", "0"); // implicit primitiveness
    setParam("mp", "2"); // mine preconditions
    setParam("nps", "0"); // non-primitive fact supports
    setParam("of", "0"); // optimization factor
    setParam("p", "1"); // encode predecessor operations
    setParam("pvn", "0"); // print variable names
    setParam("qcm", "0"); // q-constant mutexes: size threshold
    setParam("plc", "0"); // print learnt clauses
    setParam("qit", "0"); // q-constant instantiation threshold
    setParam("qrf", "0"); // q-constant rating factor
    setParam("q", "0"); // q-constants while always instantiating all preconditions
    setParam("qq", "1"); // q-constants without instantiation of preconditions
    setParam("s", "0"); // random seed
    setParam("sace", "0"); // split actions with (potentially) conflicting effects
    setParam("sqq", "1"); // share q-constants
    setParam("srfa", "1"); // skip redundant frame axioms
    setParam("stats", "0"); // output domain statistics and exit
    setParam("stl", "0"); // SAT time limit
    setParam("psr", "1"); // primitivize simple reductions
    setParam("svp", "0"); // set variable phases
    setParam("T", "0"); // max. time (secs) for finding an initial plan
    setParam("tc", "1"); // tree conversion for DNF2CNF
    setParam("v", "2"); // verbosity
    setParam("aar", "1"); // acknowledge action repetitions
    setParam("vp", "0"); // verify plan before printing it
    setParam("wf", "0"); // output formula to f.cnf
}

void Parameters::printUsage() {

    Log::setForcePrint(true);

    Log::i("Usage: lilotane <domainfile> <problemfile> [options]\n");
    Log::i("  <domainfile>  Path to domain file in HDDL format.\n");
    Log::i("  <problemfile> Path to problem file in HDDL format.\n");
    Log::i("\n");
    Log::i("Option syntax: -OPTION or -OPTION=VALUE .\n");
    Log::i("\n");
    Log::i(" -aar=<0|1>          Acknowledge action repetitions and encode them in a reduced form\n");
    Log::i(" -alo=<0|1>          Explicitly encode at-least-one constraints over operations at each position\n");
    Log::i(" -bamot=<int>        Binary at-most-one threshold\n");
    Log::i(" -cleanup=<0|1>      0 to immediately exit through syscall after solution has been printed; 1 to exit normally\n");
    Log::i(" -co=<0|1>           Colored terminal output\n");
    Log::i(" -cs=<0|1>           Check solvability: When some layer is UNSAT, re-run SAT solver without assumptions\n");
    Log::i("                     to see whether the formula has become generally unsatisfiable\n");
    Log::i(" -d=<depth>          Minimum depth to begin SAT solving at\n");
    Log::i(" -D=<depth>          Maximum depth to explore (0 : no limit)\n");
    Log::i(" -el=<int>           Number of extra layers to encode after an initial solution was found (use with -of=...)\n");
    Log::i(" -ip=<0|1>           Implicit primitiveness instead of defining each op as primitive XOR nonprimitive\n");
    Log::i(" -mp=<0|1|2>         Mine preconditions for reductions from their (recursive) subtasks:\n");
    Log::i("                     0=none, 1=use mined prec. for instantiation only, 2=use mined prec. everywhere\n");
    Log::i(" -nps=<0|1>          Nonprimitive support: Enable encoding explicit fact supports for reductions\n");
    Log::i(" -of=<factor>        Plan length optimization factor: spend up to <factor> * <original solving time> for optimization\n");
    Log::i("                     (-1 for exhaustive optimization)\n");
    Log::i(" -p=<0|1>            Encode predecessor operations\n");
    Log::i(" -psr=<0|1>          Primitivize simple reductions\n");
    Log::i(" -pvn=<0|1>          Print variable names\n");
    Log::i(" -qcm=<limit>        Collect up to <limit> q-constant mutexes per tuple of q-constants\n");
    Log::i(" -qit=<threshold>    Q-constant instantiation threshold: fully instantiate up to <threshold> operations\n");
    Log::i(" -qrf=<factor>       If -q or -qq, multiply precondition rating used for q-constant identification with <factor>\n");
    Log::i(" -q=<0|1>            For each action and reduction, introduces q-constants for any ambiguous free parameters\n");
    Log::i("                     after fully instantiating all preconditions\n");
    Log::i(" -qq=<0|1>           For each action and reduction, introduces q-constants for ALL ambiguous free parameters (replaces -q)\n");
    Log::i(" -s=<int>            Random seed\n");
    Log::i(" -sqq=<0|1>          Share q-constants among operations of a position if they have the same effective domain\n");
    Log::i(" -srfa=<0|1>         Skip redundant frame axioms\n");
    Log::i(" -stats=<0|1>        Output domain statistics and exit\n");
    Log::i(" -stl=<limit>        SAT time limit: Set limit in seconds for a SAT solver call. Limit is discarded after first such interrupt.\n");
    Log::i(" -T=<0|secs>         Try finding an initial plan for up to #secs (without optimization: total allowed runtime; 0: no limit)\n");
    Log::i(" -tc=<0|1>           Use tree conversion for DNF 2 CNF transformation instead of distributive law\n");
    Log::i(" -v=<verb>           Verbosity: 0=essential 1=warnings 2=information 3=verbose 4=debug\n");
    Log::i(" -vp=<0|1>           Verify plan (using pandaPIparser) before printing it\n");
    Log::i(" -wf=<0|1>           Write generated formula to text file \"f.cnf\" (with assumptions used in final call)\n");
    Log::i("\n");
    printParams();
    Log::setForcePrint(false);
}

std::string Parameters::getDomainFilename() {
  return _domain_filename;
}
std::string Parameters::getProblemFilename() {
  return _problem_filename;
}

void Parameters::printParams() {
    std::string out = "";
    for (auto it = _params.begin(); it != _params.end(); ++it) {
        if (it->second.empty()) {
            out += "-" + it->first + " ";
        } else {
            out += "-" + it->first + "=" + it->second + " ";
        }
    }
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
