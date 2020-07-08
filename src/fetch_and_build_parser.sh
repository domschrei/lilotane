#!/bin/bash

if [ ! -d pandaPIparser ]; then
    git clone git@github.com:panda-planner-dev/pandaPIparser.git
fi

cp panda_makefile pandaPIparser/makefile
cp libpanda.hpp pandaPIparser/src/
cd pandaPIparser

make library
cp build/libpandaPIparser.a ../../lib/

make executable
cp pandaPIparser ../../

#sed 's/^int main(int argc, char\*\* argv)/int run_pandaPIparser(int argc, char\*\* argv, ParsedProblem\& pp)/' src/main.cpp \
#	| sed -z 's/#include/#include "libpanda.hpp"\n#include/' \
#	| sed 's/^}/\tCOPY_INTO_PARSED_PROBLEM(pp)\n}/' > MAIN
#mv MAIN src/main.cpp

