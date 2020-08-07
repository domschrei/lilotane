# Lilotane
SAT-driven Planning for Totally-ordered Hierarchical Task Networks (HTN)

## Overview

This planner for totally-ordered HTN planning problems makes use of incremental Satisfiability (SAT) solving. 
In short, it reads a given planning problem and encodes a small portion of it into propositional logic.
If an internally launched SAT solver reports that the formula is unsatisfiable, the next layer of the problem is encoded and added to the existing formula.
This procedure is repeated until a plan is found or the problem is deemed to be overall unsatisfiable.

### Valid Inputs

Lilotane operates on totally-ordered HTN planning problems given as HDDL files [1]. The provided HTN domain may be recursive or non-recursive.

Lilotane can handle STRIPS-style actions with positive and negative conditions. It also handles method preconditions as well as equality and "sort-of" constraints and universal quantifications in preconditions and effects.

Lilotane _cannot_ handle conditional effects, existential quantifications, or any other extended formalisms going beyond STRIPS and basic HTNs.

## Building

Execute `make`. This will also fetch and build the SAT solver necessary for compiling the planner and solving problems.

The default SAT solver can be overwritten with the environment variable `IPASIRSOLVER`. Currently, `glucose4` and `lingeling` are valid choices.

## Usage

Lilotane uses the HDDL file format.

Execute the planner executable like this:
```
./lilotane path/to/domain.hddl path/to/problem.hddl
```

Interesting options:

* `-qq`: Use this option to introduce "q-constants" during instantiation and encoding. 
Intuitively, if an action or a reduction has free arguments, normally the operator is instantiated with _all possible argument combinations_, which may become huge. For example, a reduction `(transport ?truck pkg ?loc1 x)` would normally ground `truck` and `?loc1`, introducing a new reduction for each truck-location combination. 
Instead, `-qq` makes Lilotane preserve these parameters as free arguments: `?truck` and `?loc1` will be dynamically introduced to the problem as new constants ("question mark constants", or q-constants). Depending on the domain, this technique may dramatically outperform the normal variant because no grounding is required and the encoding can be much smaller. However, some types of clauses of the encoding become much more complicated and some additional overhead is needed, so for some domains it is not worth it.
* `-aamo`: Add ALL at-most-one constraints: Introduce pairwise at-most-one constraints not only for actions, but also for reductions. When using `-qq`, you should usually also use `-aamo` as there is virtually no overhead due to the very small number of reductions.
* `-of`: Output the generated formula to `./f.cnf`. As Lilotane works incrementally, the formula will consist of all clauses added during program execution. Additionally, when the program exits, the assumptions used in the final SAT call will be added to the formula as well.
* `-d=<depth>`: The **minimum** depth for which Lilotane will attempt to solve the generated formula. 
* `-D=<depth>`: Limit the **maximum** depth to explore. After the specified amount of layers, if no solution was found Lilotane will report unsatisfiability (for this amount of layers) and exit. Useful together with `-of` if you want to generate a CNF file for some specific number of layers.
* `-cs`: Check solvability. When this option is set and Lilotane finds unsatisfiability at layer k, it will re-run the SAT solver, this time without assumptions. If this SAT call returns unsatisfiability, too, then the formula is generally unsatisfiable and it will always remain unsatisfiable no matter the following iterations. In that case, something is wrong either with the internals of Lilotane, the provided parameters or the provided planning problem; Lilotane exits. If the SAT call returns satisfiability, Lilotane proceeds to instantiate the next layer.

From experiments so far, the arguments `-qq -aamo` and nothing else seem to work best for overall good performance. 

## License

**Until the submission deadline of the [HTN IPC 2020](http://gki.informatik.uni-freiburg.de/competition.html), this repository and this code are private. Do not share.**

The code of the planner is published under the GNU GPLv3. Consult the LICENSE file for details.  
The planner uses (slightly adapted) code from the [pandaPIparser project](https://github.com/panda-planner-dev/pandaPIparser) [1] which is also GPLv3 licensed.

Note that depending on the SAT solver compiled into the planner, usage and redistribution rights may be subject to their licensing.
If you want to make sure that everything is Free and Open Source, I suggest to use MIT-licensed lingeling as the solver.

## Background and References

This planner is being developed by Dominik Schreiber <dominik.schreiber@kit.edu>.  
Its direct predecessor is Tree-REX by D. Schreiber, D. Pellier, H. Fiorino and T. Balyo [2]. 
However, Lilotane incorporates various conceptual and practical ideas that enable its performance gains. Most prominently, the prior grounding engine was dropped; instead, lazy "just-in-time" grounding and partially lifted encodings are employed.  
Lilotane is written from scratch in C++.

[1] Behnke, G., Höller, D., Schmid, A., Bercher, P., & Biundo, S. (2020). **On succinct groundings of HTN planning problems.** In Proceedings of the 34th AAAI Conference on Artificial Intelligence (AAAI). AAAI Press.

[2] Schreiber, D.; Balyo, T.; Pellier, D.; and Fiorino, H. 2019. 
**Tree-REX: SAT-based tree exploration for efficient and high-quality HTN planning.** 
In Proceedings of the International Conference on Automated Planning and Scheduling, volume 29. No. 1. 2019.
