
#include <cstring>
#include <cstdlib>
#include <stdarg.h>
#include <iostream>
#include <string>

#include "util/log.h"
#include "util/timer.h"

int Log::verbosity;
bool Log::coloredOutput;
bool Log::forcePrint;

void Log::init(int verbosity, bool coloredOutput) {
    Log::verbosity = verbosity;
    Log::coloredOutput = coloredOutput;
    if (coloredOutput) resetColorModifier();
}

void Log::setForcePrint(bool force) {
    forcePrint = force;
}

bool Log::d(const char* str, ...) {
    va_list vl;
    va_start(vl, str);
    bool res = log(V4_DEBUG, str, vl);
    va_end(vl);
    return res;
}
bool Log::v(const char* str, ...) {
    va_list vl;
    va_start(vl, str);
    bool res = log(V3_VERBOSE, str, vl);
    va_end(vl);
    return res;
}
bool Log::i(const char* str, ...) {
    va_list vl;
    va_start(vl, str);
    bool res = log(V2_INFORMATION, str, vl);
    va_end(vl);
    return res;
}
bool Log::w(const char* str, ...) {
    va_list vl;
    va_start(vl, str);
    bool res = log(V1_WARNINGS, str, vl);
    va_end(vl);
    return res;
}
bool Log::e(const char* str, ...) {
    va_list vl;
    va_start(vl, str);
    bool res = log(V0_ESSENTIAL, str, vl);
    va_end(vl);
    return res;
}
bool Log::log_notime(int verb, const char* str, ...) {

    if (!forcePrint && verb > verbosity) {
        return false;
    }

    if (coloredOutput) printColorModifier(verb);
    va_list vl;
    va_start(vl, str);
    vprintf(str, vl);
    va_end(vl);
    if (coloredOutput) resetColorModifier();
    
    return false;
}

bool Log::log(int verb, const char* str, va_list& vl) {

    if (verb > verbosity) {
        return false;
    }

    // Begin line with current time
    float time = Timer::elapsedSeconds();
    if (coloredOutput) printColorModifier(V4_DEBUG);
    printf("%.3f ", time);
    if (coloredOutput) resetColorModifier();

    // Print formatted message
    if (coloredOutput) printColorModifier(verb);
    vprintf(str, vl);
    if (coloredOutput) resetColorModifier();
    
    return false;
}

void Log::printColorModifier(int verb) {
    static Modifier white(FG_WHITE);
    static Modifier green(FG_GREEN);
    static Modifier red(FG_LIGHT_RED);
    static Modifier blue(FG_BLUE);
    static Modifier gray(FG_DARK_GRAY);

    if (verb == V0_ESSENTIAL) printf("%s", blue.str());
    if (verb == V1_WARNINGS) printf("%s", red.str());
    if (verb == V2_INFORMATION) printf("%s", white.str());
    if (verb == V3_VERBOSE || verb == V4_DEBUG) printf("%s", gray.str());
}

void Log::resetColorModifier() {
    static Modifier resetFg(FG_DEFAULT);
    printf("%s", resetFg.str());
}