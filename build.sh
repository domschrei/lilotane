#!/bin/bash

set -e

COMPILEFLAGS="-O2 -g -pipe -Wall -Wextra -pedantic -std=c++17"
INCLUDES="-Isrc -Isrc/parser"
LINKERFLAG="-O3 -lm -flto"

bison -v -d -o src/parser/hddl.cpp src/parser/hddl.y
flex --yylineno -o src/parser/hddl-token.cpp src/parser/hddl-token.l

for f in src/parser/*.cpp src/planner/*.cpp src/data/*.cpp src/util/*.cpp src/*.cpp; do
    o=$(echo "$f" | sed 's/cpp/o/g')
    if [ ! -f $o ] || [ $(stat -c %Y $f) -gt $(stat -c %Y $o) ]; then
        g++ -c ${COMPILEFLAGS} ${INCLUDES} $f -o $o
    fi
done

g++ ${COMPILEFLAGS} ${INCLUDES} src/parser/*.o src/planner/*.o src/data/*.o src/util/*.o src/*.o ${LINKERFLAG} -o treerexx
