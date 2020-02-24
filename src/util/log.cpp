
#include <cstring>
#include <cstdlib>
#include <stdarg.h>
#include <iostream>

#include "util/log.h"

bool fail(std::string msg) {
    printf(msg.c_str());
    return false;
}

void log(const char* str, ...) {

    // Printing via printf.
    va_list vl;
    va_start(vl, str);
    vprintf(str, vl);
    va_end(vl);
    
    // Printing via std::cout.
    /*
    int size = 1024;
    std::string out;
    while (true) {
        char buffer[size];

        va_list vl;
        va_start(vl, str);
        int strlen = vsnprintf(buffer, size, str, vl);
        va_end(vl);

        if (strlen >= size) size *= 2;
        else {
            out = std::string(buffer);
            break;
        }
    }
    std::cout << out;
    */
}