
CC=g++

CWARN=-Wno-unused-parameter
CERROR=

COMPILEFLAGS=-O3 -g -pipe -Wall -Wextra -pedantic -std=c++17 $(CWARN) $(CERROR)
LINKERFLAGS=-O3 -lm 
#-flto -static -static-libgcc

INCLUDES=-Isrc -Ibuild

#COMPILEFLAGS=-O0 -ggdb -pipe -Wall -Wextra -pedantic -std=c++17 $(CWARN) $(CERROR)
#LINKERFLAG=-O0 -ggdb

.PHONY = clean clear

SOURCES:=$(shell find src/ -name '*.cpp'|grep -vE "src/main.cpp")


executable: pandaPIparser
library: build/libpandaPIparser.a


pandaPIparser: build/hddl-token.o build/hddl.o build/main.o $(patsubst src/%.cpp,build/%.o,${SOURCES})
	${CC} ${INCLUDES} ${LINKERFLAGS} $^ -o pandaPIparser 

build/libpandaPIparser.a: build/hddl-token.o build/hddl.o build/libmain.o $(patsubst src/%.cpp,build/%.o,${SOURCES})
	ar rcs build/libpandaPIparser.a $^


build/hddl-token.o: build/hddl-token.cpp
	${CC} ${INCLUDES} ${COMPILEFLAGS} -o build/hddl-token.o -c build/hddl-token.cpp

build/hddl.o: build/hddl.cpp
	${CC} ${INCLUDES} ${COMPILEFLAGS} -o build/hddl.o -c build/hddl.cpp

build/libmain.o: build/libmain.cpp
	${CC} ${INCLUDES} ${COMPILEFLAGS} -o build/libmain.o -c build/libmain.cpp
	
build/hddl-token.cpp: build/hddl.cpp src/hddl-token.l
	flex --yylineno -o build/hddl-token.cpp src/hddl-token.l

build/hddl.cpp: src/hddl.y
	mkdir -p $(@D)
	bison -v -d -o build/hddl.cpp src/hddl.y

build/libmain.cpp:
	sed 's/^int main(int argc, char\*\* argv)/int run_pandaPIparser(int argc, char\*\* argv, ParsedProblem\& pp)/' src/main.cpp \
	| sed -z 's/^/\n#include "libpanda.hpp"\n\n/' \
	| sed 's,simple_hddl_output(,//simple_hddl_output(,g' \
	| sed 's/^}/\tCOPY_INTO_PARSED_PROBLEM(pp)\n\treturn 0;\n}/' > build/libmain.cpp

build/hddl.hpp: build/hddl.cpp

	
build/%.o: src/%.cpp src/%.hpp build/hddl.hpp
	${CC} ${INCLUDES} ${COMPILEFLAGS} -o $@ -c $<

build/%.o: src/%.cpp build/hddl.hpp
	${CC} ${INCLUDES} ${COMPILEFLAGS} -o $@ -c $<
	
build/%.o: src/%.cpp
	${CC} ${INCLUDES} ${COMPILEFLAGS} -o $@ -c $<


clear:
	rm -rf build/

clean: clear
