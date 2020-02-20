
#include "assert.h"

#include "params.h"

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
                printf("Unrecognized parameter %s.", arg);
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
        printf("Please specify both a domain file and a problem file.\n");
        exit(1);
    }
}

void Parameters::setDefaults() {
    //setParam("-nps"); // non-primitive fact supports
    //setParam("-q"); // q-constants
}

void Parameters::printUsage() {

    printf("Usage: treerexx <domainfile> <problemfile> [options]\n");
    printf("  <domainfile>  Path to domain file in HDDL format.\n");
    printf("  <problemfile> Path to problem file in HDDL format.\n");
    printf("\n");
    printf("Option syntax: either -OPTION or -OPTION=VALUE .\n");
    printf(" -nps  Nonprimitive support: Enable encoding explicit fact supports for reductions\n");
    printf(" -q    Encode some variables in reduction/action signatures as virtual q-constants\n");
    printf("       instead of fully grounding them into actual constants\n");
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
    printf("Called with parameters: %s\n", out.c_str());
}

void Parameters::setParam(const char* name) {
    _params[name];
}

void Parameters::setParam(const char* name, const char* value) {
    _params[name] = value;
}

bool Parameters::isSet(const std::string& name) {
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
