
#ifndef DOMPASCH_LILOTANE_SIGNAL_MANAGER_H
#define DOMPASCH_LILOTANE_SIGNAL_MANAGER_H

#include <stdlib.h>

class SignalManager {

private:
    static bool exiting;
    static int numSignals;

public:
    static inline void signalExit() {
        exiting = true;
        numSignals++;
        if (numSignals == 5) {
            // Forced exit
            exit(0);
        }
    }
    static inline bool isExitSet() {
        return exiting;
    }
};

#endif
