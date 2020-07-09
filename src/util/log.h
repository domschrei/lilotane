
#ifndef DOMPASCH_TREE_REXX_LOG_H
#define DOMPASCH_TREE_REXX_LOG_H

#include <string>


class Log {

public:
    static const int V0_ESSENTIAL = 0;
    static const int V1_WARNINGS = 1;
    static const int V2_INFORMATION = 2;
    static const int V3_VERBOSE = 3;
    static const int V4_DEBUG = 4;

private:
    static int verbosity;
    static bool coloredOutput;

public:
    static void init(int verbosity, bool coloredOutput);

    // Debug message
    static bool d(const char* str, ...);
    // Verbose info message
    static bool v(const char* str, ...);
    // Info message
    static bool i(const char* str, ...);
    // Warn message
    static bool w(const char* str, ...);
    // Error message
    static bool e(const char* str, ...);

    static bool log_notime(int verb, const char* str, ...);

    static bool log(int verb, const char* str, va_list& vl);

private:
    static void printColorModifier(int verb);
    static void resetColorModifier();

};

#endif