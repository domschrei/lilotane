
#ifndef DOMPASCH_LILOTANE_SIGNAL_MANAGER_H
#define DOMPASCH_LILOTANE_SIGNAL_MANAGER_H

class SignalManager {

private:
    static bool exiting;

public:
    static inline void signalExit() {
        exiting = true;
    }
    static inline bool isExitSet() {
        return exiting;
    }
};

#endif
