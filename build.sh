#!/bin/bash

COMPILEFLAGS="-O3 -pipe -Wall -Wextra -pedantic -std=c++17"
INCLUDES="-Isrc -Isrc/parser"
LINKERFLAG="-O3 -lm -flto"

bison -v -d -o src/parser/hddl.cpp src/parser/hddl.y
flex --yylineno -o src/parser/hddl-token.cpp src/parser/hddl-token.l

g++ ${COMPILEFLAGS} ${INCLUDES} src/parser/*.cpp src/planner/*.cpp src/data/*.cpp src/*.cpp ${LINKERFLAG} -o treerexx
