# pandaPIparser

This is the parsing utility of the pandaPI planning system. It is designed to parse HTN planning problems. Its main (and currently only) input language is HDDL (see the following [paper](http://www.uni-ulm.de/fileadmin/website_uni_ulm/iui.inst.090/Publikationen/2019/Hoeller2019HDDL.pdf)).

If you use pandaPIparser in any of your published work, we would kindly ask you to cite us (see Reference below).

## Capabilities 
The parser can currently produce two different output formats:

1. pandaPI's internal numeric format for lifted HTN planning problems
2. (J)SHOP2's input language.
3. HPDL by Juan Fernández Olivares <faro@decsai.ugr.es>

Note that the translation into (J)SHOP2 is necessarily incomplete as (J)SHOP2 cannot express arbitrary partial orders in its ordering constraints. For example a method with the five subtasks (a,b,c,d,e) and the ordering constraints a < c, a < d, b < d, and b < e cannot be formulated in (J)SHOP2.


## Compilation
To compile pandaPIparser you need g++, make, flex, and bison. No libraries are required.

To create the executable, simply run `make -j` in the root folder, which will create an executable called `pandaPIparser`

## Usage
The parser is called with at least two arguments: the domain and the problem file. Both must be written in HDDL.
By default, the parser will output the given instance in pandaPI's internal format on standard our.
If you pass a third file name, pandaPIparser will instead output the internal representation of the instance to that file.


### Usage for Compilation to (J)SHOP2
pandaPIparser also offers to option to write the output to (J)SHOP2's input format. In order to do so add `-shop` as one of the command line arguments (the position does not matter).
With `-shop` you may specify up to four files as command line arguments: the input domain, the input problem, the output domain, and the output problem.
As an example consider

```
./pandaPIParser -shop transport.hddl pfile01.hddl shop-transport.lisp shop-pfile01.lisp
```

Note that
* pandaPIparser will shift some of the contents of the HDDL problem file to the (J)SHOP2 domain file. Most notably, pandaPIparser moves the initial task network of the HDDL problem into the (J)SHOP2 domain by compiling it into a method for a new abstract task `__top`.
* any propositional goal will be ignored in the translation.
* only constant action costs (i.e. integer valued ones that do not depend on parameters and state) are supported.
* any action named `call` will be renamed to `_call`, as `call` is a keyword for (J)SHOP2.
* current `forall` statements in preconditions are fully instantiated.

### Usage for Compilation to (J)SHOP1
pandaPIparser also supports (J)SHOP1's output. This is essentially the same as the output for (J)SHOP2 with the only difference that underscores will be replaced by minuses and leading minuses are prepended with an `x`.
To call the translator in this compatibility mode, use `-shop1` instead of `-shop` as the command line argument.

### Usage for Compilation to HPDL
pandaPIparser also offers to option to write the output to HPDL. In order to do so add `-hpdl` as one of the command line arguments (the position does not matter).
Parameter-wise `-hpdl` works exactly as `-shop`

## Contact
If you have any issues with pandaPIparser -- or have any question relating to its use, please contact [Gregor Behnke](mailto:gregor.behnke@uni-ulm.de).

## Reference
If you would like to cite pandaPIparser, you may do so my referring to the following paper:

```
@inproceedings { Behnke2019Grounding,
		Title = {On Succinct Groundings of HTN Planning Problems},
		Year = {2020},
		Booktitle = {Proceedings of the 34th {AAAI} Conference on Artificial Intelligence ({AAAI} 2020)},
		Publisher = {{AAAI Press}},
		Author = {Behnke, Gregor and H{\"o}ller, Daniel and Schmid, Alexander and Bercher, Pascal and Biundo, Susanne}
}
```
