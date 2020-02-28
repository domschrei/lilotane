# treerexx
SAT-driven Planning for Totally-ordered Hierarchical Task Networks (HTN)

## Overview

This planner for totally-ordered HTN planning problems makes use of incremental Satisfiability (SAT) solving. 
In short, it reads a given planning problem and encodes a small portion of it into propositional logic.
If an internally launched SAT solver reports that the formula is unsatisfiable, the next layer of the problem is encoded and added to the existing formula.
This procedure is repeated until a plan is found or the problem is deemed to be overall unsatisfiable.

### Valid Inputs

Treerexx operates on totally-ordered HTN planning problems given as HDDL files [1]. The provided HTN domain may be recursive or non-recursive.

Treerexx can handle STRIPS-style actions with positive and negative conditions. It also handles method preconditions as well as equality and "sort-of" constraints.

Treerexx _cannot_ handle conditional effects, quantifications, or any other extended formalisms going beyond STRIPS and basic HTNs.

## Building

Execute `make`. This will also fetch and build the SAT solver necessary for compiling the planner and solving problems.

The default SAT solver can be overwritten with the environment variable `IPASIRSOLVER`. Currently, `glucose4` and `lingeling` are valid choices.

## Usage

treerexx uses the HDDL file format.

Execute the planner executable like this:
```
./treerexx path/to/domain.hddl path/to/problem.hddl
```

## License

The code of the planner is published under the GNU GPLv3. Consult the LICENSE file for details.  
The planner uses (slightly adapted) code from the [https://github.com/panda-planner-dev/pandaPIparser](pandaPIparser project) [1] which is also GPLv3 licensed.

Note that depending on the SAT solver compiled into the planner, usage and redistribution rights may be subject to their licensing.
If you want to make sure that everything is Free and Open Source, I suggest to use MIT-licensed lingeling as the solver.

## Background and References

This planner is being developed by Dominik Schreiber <dominik.schreiber@kit.edu>.  
Its direct predecessor is Tree-REX by D. Schreiber, D. Pellier, H. Fiorino and T. Balyo [2]. 
However, Treerexx incorporates various conceptual and practical ideas that enable its performance gains. Most prominently, the prior grounding engine was dropped; instead, lazy "just-in-time" grounding and partially lifted encodings are employed.  
Treerexx is written from scratch in C++.

[1] Behnke, G., Höller, D., Schmid, A., Bercher, P., & Biundo, S. (2020). **On succinct groundings of HTN planning problems.** In Proceedings of the 34th AAAI Conference on Artificial Intelligence (AAAI). AAAI Press.

[2] Schreiber, D.; Balyo, T.; Pellier, D.; and Fiorino, H. 2019. 
**Tree-REX: SAT-based tree exploration for efficient and high-quality HTN planning.** 
In Proceedings of the International Conference on Automated Planning and Scheduling, volume 29. No. 1. 2019.
