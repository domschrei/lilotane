# Lilotane

\['li·lo·tein], **Li**fted **Lo**gic for **Ta**sk **Ne**tworks:  
SAT-driven Totally-ordered Hierarchical Task Network (HTN) Planning  

NOTE: The main (development) repository of this project is [located here on Github](https://github.com/domschrei/lilotane).

## Overview

This planner for totally-ordered HTN planning problems makes use of incremental Satisfiability (SAT) solving. In short, it reads a given planning problem and encodes a small portion of it into propositional logic. If an internally launched SAT solver reports that the formula is unsatisfiable, the next layer of the problem is encoded and added to the existing formula. This procedure is repeated until a plan is found or the problem is deemed to be overall unsatisfiable.

Lilotane introduces new techniques to the field of SAT-based HTN planning, namely lazy instantiation and a lifted encoding that enables it to skip the full grounding of the problem. More information is provided in the JAIR paper [0] and, to a lesser extent, in Lilotane's IPC planner description [1].

Lilotane was runner-up in the Total Order track of the [International Planning Conference (IPC) 2020](http://ipc2020.hierarchical-task.net/), performing best on a diverse set of planning domains.

### Valid Inputs

Lilotane operates on totally-ordered HTN planning problems given as HDDL files [2]. The provided HTN domain may be recursive or non-recursive.

In short, Lilotane supports exactly the HDDL specification from the International Planning Competition (IPC) 2020 provided in [3] and [4].
It handles type systems, STRIPS-style actions with positive and negative conditions, method preconditions, equality and "sort-of" constraints, and universal quantifications in preconditions and effects.
Lilotane _cannot_ handle conditional effects, existential quantifications, or any other extended formalisms going beyond the mentioned concepts.

### Output

Lilotane outputs a plan in accordance to [5]. Basically everything in between "`==>`" and "`<==`" is the found plan, and everything inside that plan in front of the word `root` is the sequence of classical actions to execute. 
(Don't worry about the weird IDs which are assigned to the actions and reductions – they correspond (more or less) to the Boolean variables they were encoded with.)

## Building

You can build Lilotane like this:

```
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=RELEASE -DIPASIRSOLVER=glucose4
make
cd ..
```

The SAT solver to link Lilotane with can be set with the `IPASIRSOLVER` variable. Valid values are `cadical`, `cryptominisat`, `glucose4`, `lingeling`, and `riss`.

Note that the Makefile in the base directory is only supposed to be used for building Lilotane [as an IPASIR application](https://github.com/biotomas/ipasir).

## Usage

Lilotane uses the HDDL file format.

Execute the planner executable like this:
```
./lilotane path/to/domain.hddl path/to/problem.hddl [options]
```

The full options can be seen by running Lilotane without any arguments.
Please note that some of the options for instantiation and encoding are experimental: Some combinations of options may be invalid and/or some options may be completely disfunctional. 
Here are the more interesting options for normal general purpose usage of the planner which should all function properly:

* `-v=<verb>`: Verbosity of the planner. Use 0 if you absolutely only care about the plan. Use 1 for warnings, 2 for general information, 3 for verbose output and 4 for full debug messages.
* `-co=0`: Disable colored output. Use this when the call to the planner is part of some workflow and the plan is automatically extracted.
* `-d=<depth>`: The **minimum** depth for which Lilotane will attempt to solve the generated formula. 
* `-D=<depth>`: Limit the **maximum** depth to explore. After the specified amount of layers, if no solution was found Lilotane will report unsatisfiability (for this amount of layers) and exit. Useful together with `-of` if you want to generate a CNF file for some specific number of layers.
* `-cs`: Check solvability. When this option is set and Lilotane finds unsatisfiability at layer k, it will re-run the SAT solver, this time without assumptions. If this SAT call returns unsatisfiability, too, then the formula is generally unsatisfiable and it will always remain unsatisfiable no matter the following iterations. In that case, wither something is wrong with the internals of the used Lilotane configuration, or the provided planning problem is unsolvable. Lilotane exits in that case. If the SAT call returns satisfiability, Lilotane proceeds to instantiate the next layer.
* `-wf`: Write the generated formula to `./f.cnf`. As Lilotane works incrementally, the formula will consist of all clauses added during program execution. Additionally, when the program exits, the assumptions used in the final SAT call will be added to the formula as well.
* `-pvn` Print variable names – prints one line `VARMAP <int> <Signature>` for each encoded propositional variable. Remember to set verbosity to DEBUG (`-v=4`). Useful for debugging together with `-cs -wf`: You can use a SAT solver such as picosat to extract the UNSAT core of an unsolvable problem formula (`./picosat f.cnf -c <core-output>`) and then translate the core back into the original variable names with `python3 get_failed_reason.py <core-output> <planner-output-file>`.

## License

The code of Lilotane is published under the GNU GPLv3. Consult the LICENSE file for details.  
The planner uses [pandaPIparser](https://github.com/panda-planner-dev/pandaPIparser) [2] which is also GPLv3 licensed.

Note that depending on the SAT solver compiled into the planner, usage and redistribution rights may be subject to their licensing.
Specifically: CaDiCaL, Cryptominisat, Lingeling and Riss are Free Software while Glucose technically is not.

## Background and References

This planner is being developed by Dominik Schreiber <dominik.schreiber@kit.edu>. Its direct predecessor is Tree-REX which originated from [6] and was published in [7]. Lilotane is an entirely new codebase, written from scratch in C++ (in contrast to Tree-REX which was partially written in Java).

If you use Lilotane in academic work, please cite [0]. If you specifically reference Lilotane as a participant of the IPC, you can additionally cite [1].

---

[0] Schreiber, D. (2021). [**Lilotane: A Lifted SAT-based Approach to Hierarchical Planning.**](https://doi.org/10.1613/jair.1.12520) In Journal of Artificial Intelligence Research (JAIR) 2021 (70), pp. 1117-1181.

[1] Schreiber, D. (2020). [**Lifted Logic for Task Networks: TOHTN Planner Lilotane Entering IPC 2020.**](https://dominikschreiber.de/papers/2020-ipc-lilotane.pdf) In International Planning Competition (IPC) 2020.**

[2] Behnke, G., Höller, D., Schmid, A., Bercher, P., & Biundo, S. (2020). [**On Succinct Groundings of HTN Planning Problems.**](https://www.uni-ulm.de/fileadmin/website_uni_ulm/iui.inst.090/Publikationen/2020/AAAI-BehnkeG.1770.pdf) In AAAI (pp. 9775-9784).

[3] Höller, D., Behnke, G., Bercher, P., Biundo, S., Fiorino, H., Pellier, D., & Alford, R. (2020). [**HDDL: An Extension to PDDL for Expressing Hierarchical Planning Problems.**](https://www.uni-ulm.de/fileadmin/website_uni_ulm/iui.inst.090/Publikationen/2020/Hoeller2020HDDL.pdf) In AAAI (pp. 9883-9891).

[4] Behnke, G. et al. (2020). [**HDDL - Addendum.**](http://gki.informatik.uni-freiburg.de/competition/hddl.pdf) Universität Freiburg.

[5] Behnke, G. et al. (2020). [**Plan verification.**](http://gki.informatik.uni-freiburg.de/ipc2020/format.pdf) Universität Freiburg.

[6] Schreiber, D. (2018). [**Hierarchical task network planning using SAT techniques.**](https://baldur.iti.kit.edu/theses/schreiber.pdf) Master’s thesis, Grenoble Institut National Polytechnique, Karlsruhe Institute of Technology. Also see code and resources in [this repository](https://gitlab.com/domschrei/htn-sat).

[7] Schreiber, D.; Balyo, T.; Pellier, D.; and Fiorino, H. (2019). [**Tree-REX: SAT-based tree exploration for efficient and high-quality HTN planning.**](https://algo2.iti.kit.edu/balyo/papers/treerex.pdf) In ICAPS, volume 29. No. 1. 2019.
