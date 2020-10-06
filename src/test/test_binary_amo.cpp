
#include "util/timer.h"
#include "util/log.h"
#include "util/params.h"

#include "sat/variable_domain.h"
#include "sat/binary_amo.h"
#include "sat/ipasir.h"

int main(int argc, char** argv) {

    Timer::init();

    Parameters params;
    params.init(argc, argv);

    int verbosity = params.getIntParam("v");
    Log::init(verbosity, /*coloredOutput=*/params.isNonzero("co"));

    void* solver = ipasir_init();

    // Test binary at-most-one constraints
    for (int n = 10; n <= 33; n++) {
        for (int states = n; states <= n+1; states++) {
            Log::d("n=%i, %i states\n", n, states);

            std::vector<int> vars;
            for (int i = 1; i <= n; i++) vars.push_back(VariableDomain::nextVar());
            for (auto c : BinaryAtMostOne(vars, states).encode()) {
                for (int lit : c) ipasir_add(solver, lit);
                ipasir_add(solver, 0);
            }

            // Generally satisfiable
            assert(ipasir_solve(solver) == 10);

            // If one, then all others not
            for (int var : vars) {
                Log::d(" - assume(%i)\n", var);
                ipasir_assume(solver, var);
                assert(ipasir_solve(solver) == 10);
                for (int otherVar : vars) if (otherVar != var) assert(ipasir_val(solver, otherVar) < 0);
            }

            if (states > n) {
                // More states than variables: exactly zero must be possible
                for (int var : vars) ipasir_assume(solver, -var);
                assert(ipasir_solve(solver) == 10);
                for (int var : vars) assert(ipasir_val(solver, var) < 0);
            } else {
                // As many states as variables: exactly zero must be impossible
                for (int var : vars) ipasir_assume(solver, -var);
                assert(ipasir_solve(solver) == 20);
            }
        }
    }
    
    ipasir_release(solver);
    return 0;
}