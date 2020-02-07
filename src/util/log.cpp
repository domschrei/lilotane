
#include "util/log.h"

bool fail(std::string msg) {
    printf(msg.c_str());
    return false;
}
