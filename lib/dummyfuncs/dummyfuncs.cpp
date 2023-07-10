
extern "C" {
    #include "ipasir.h"
    void ipasir_set_decision_var (void * s, unsigned int v, bool decision_var) {}
    void ipasir_set_phase (void * s, unsigned int v, bool phase) {}
    void ipasir_set_seed (void * s, int seed) {} 
    void* mallob_ipasir_init (bool incremental) {return ipasir_init ();}
    void mallob_ipasir_branched_solve (void * solver, void * data, int (*terminate)(void * data), void (*callback_done)(int result, void* child_solver, void* data)) {}
    void mallob_ipasir_presubmit (void * solver) {}
};
