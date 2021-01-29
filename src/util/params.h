
#ifndef DOMPASCH_PARAMETERPROCESSOR_H_
#define DOMPASCH_PARAMETERPROCESSOR_H_

/*
 * ParameterProcessor.h
 *
 *  Created on: Dec 5, 2014
 *      Author: balyo
 */

#include "string.h"
#include <map>
#include <string>
#include <iostream>
#include "stdlib.h"

class Parameters {
private:
	std::map<std::string, std::string> _params;

	// Positional / unqualified parameters
	std::string _domain_filename = "";
	std::string _problem_filename = "";

public:
	Parameters() = default;
	void init(int argc, char** argv);
	void printUsage();
	void setDefaults();
	std::string getDomainFilename();
	std::string getProblemFilename();
	void printParams();
	void setParam(const char* name);
	void setParam(const char* name, const char* value);
	bool isSet(const std::string& name) const;
	bool isNonzero(const std::string& intParamName) const;
	std::string getParam(const std::string& name, const std::string& defaultValue);
	std::string getParam(const std::string& name);
	int getIntParam(const std::string& name, int defaultValue);
	float getFloatParam(const std::string& name, float defaultValue);
	int getIntParam(const std::string& name);
	float getFloatParam(const std::string& name);
};

#endif /* PARAMETERPROCESSOR_H_ */
