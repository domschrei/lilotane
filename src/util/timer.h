
#include <chrono>

using namespace std::chrono;

class Timer {

private:
    static double startTime;

public:

    static void init(double start = -1) {
        startTime = start == -1 ? now() : start;
    }

    static double now() {
        return 0.001 * 0.001 * 0.001 * system_clock::now().time_since_epoch().count();
    }

    /**
     * Returns elapsed time since program start (since MyMpi::init) in seconds.
     */
    static float elapsedSeconds() {
        return now() - startTime;
    }
};