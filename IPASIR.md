
# Lilotane as an Application Benchmark for Incremental SAT

This is to document the employment of Lilotane as an application benchmark for incremental SAT solvers.

## Setting up

Due to pandaPIparser, `flex` and `bison` must be installed as dependencies (e.g., via `apt install`).

The cloned directory `lilotane` can be moved to [app/](https://github.com/biotomas/ipasir/tree/master/app) and can then be built normally with `mkone.sh` or `mkall.sh`. This build will also produce an executable called `pandaPIparser` which is used to verify found plans.

## Benchmarks

Any benchmarks from the [IPC 2020 Total order track](https://github.com/panda-planner-dev/ipc2020-domains/tree/master/total-order) can be fed to Lilotane. If running Lilotane on all instances is out of scope, here is a small exemplary selection of domains which should scale well and might produce interesting results:

* Childsnack
* Factories-simple
* Towers
* Rover-GTOHP
* Transport

Fine grained per-domain analyses of Lilotane's properties can be found in the [JAIR paper](https://jair.org/index.php/jair/article/view/12520).

## Running

Given a domain file and a problem file (both in HDDL format), execute Lilotane like this:

`bin/lilotane-<solver> path/to/domain.hddl path/to/problem.hddl -co=0 -v=3 -of=-1`

The last option `-of=-1` enables a postprocessing stage where the initially found plan is optimized via incremental SAT solving. Without this option, Lilotane exits as soon as some initial plan has been found, and employing different SAT solvers may result in entirely different plans. If Lilotane is executed _with_ this option on a particular problem and properly terminates, then the length of the final found plan should be the same regardless of the SAT solver that is employed.

The output of Lilotane should be verified as follows:

`./pandaPIparser path/to/domain.hddl path/to/problem.hddl -verify path/to/output`

(If a verification error does occur, please do not immediately blame the SAT solver but maybe contact me with the concerned benchmark instance and/or test the same instance with one of Lilotane's default solvers such as Glucose or Lingeling.)

In the IPC 2020 all planners were executed on a time limit of 1800 seconds. Lower time limits can also be acceptable (e.g., down to maybe 300 seconds), but for lower time limits the SAT solver performance tends to have a lower relative impact on the total run time of Lilotane.
