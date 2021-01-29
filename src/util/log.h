
#ifndef DOMPASCH_TREE_REXX_LOG_H
#define DOMPASCH_TREE_REXX_LOG_H

#include <string>

// Taken from https://stackoverflow.com/a/17469726
enum Code {
    FG_DEFAULT = 39, 
    FG_BLACK = 30, 
    FG_RED = 31, 
    FG_GREEN = 32, 
    FG_YELLOW = 33,
    FG_BLUE = 34, 
    FG_MAGENTA = 35, 
    FG_CYAN = 36, 
    FG_LIGHT_GRAY = 37, 
    FG_DARK_GRAY = 90, 
    FG_LIGHT_RED = 91, 
    FG_LIGHT_GREEN = 92, 
    FG_LIGHT_YELLOW = 93, 
    FG_LIGHT_BLUE = 94, 
    FG_LIGHT_MAGENTA = 95, 
    FG_LIGHT_CYAN = 96, 
    FG_WHITE = 97, 
    BG_RED = 41, 
    BG_GREEN = 42, 
    BG_BLUE = 44, 
    BG_DEFAULT = 49
};

class Modifier {
private:
    std::string _str;
public:
    explicit Modifier(const Code& pCode) : _str("\033[" + std::to_string(pCode) + "m") {}
    const char* str() {
        return _str.c_str();
    }
};

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
    static bool forcePrint;

public:
    static void init(int verbosity, bool coloredOutput);
    static void setForcePrint(bool force);

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