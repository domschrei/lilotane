
# Options: lingeling glucose4 cadical cryptominisat
IPASIRSOLVER?=glucose4

SOLVERLIB=lib/${IPASIRSOLVER}/libipasir${IPASIRSOLVER}.a
CC=g++
CWARN=-Wno-unused-parameter -Wno-sign-compare -Wno-format -Wno-format-security

COMPILEFLAGS:=-pipe -Wall -Wextra -pedantic -std=c++17 $(CWARN) $(CERROR)
LINKERFLAGS:=-lm -Llib -Llib/${IPASIRSOLVER} -lipasir${IPASIRSOLVER} -lz -lpandaPIparser $(shell cat lib/${IPASIRSOLVER}/LIBS || echo "")

INCLUDES=-Isrc -Isrc/pandaPIparser/src

.PHONY = release debug test parser clean rebuild solver

release: LILOTANE_VERSION:=rls-$(shell date --iso-8601=seconds)-${IPASIRSOLVER}
release: COMPILEFLAGS += -DNDEBUG -Wno-unused-variable -O3 -flto
release: LINKERFLAGS += -O3 -flto
release: parser solver rebuild lilotane

debug: LILOTANE_VERSION:=dbg-$(shell date --iso-8601=seconds)-${IPASIRSOLVER}
debug: COMPILEFLAGS += -O3 -g 
debug: LINKERFLAGS += -O3 -g
debug: parser solver rebuild lilotane

rebuild:
	rm build/main.o 2>/dev/null || :

tests: COMPILEFLAGS += -O3 -g
tests: LINKERFLAGS += -O3 -g
tests: parser solver
#test: $(patsubst src/test/%.cpp,build/test/%,$(wildcard src/test/*.cpp))
tests: build/test/test_arg_iterator

build/test/%: $(patsubst src/%.cpp,build/%.o,$(wildcard src/data/*.cpp src/planner/*.cpp src/sat/*.cpp src/util/*.cpp)) build/test/%.o
	${CC} ${INCLUDES} $^ -o $@ ${LINKERFLAGS}

build/test/%.o: src/test/%.cpp
	mkdir -p $(@D)
	${CC} ${COMPILEFLAGS} ${INCLUDES} -o $@ -c $<

solver:
	if grep -q "dummyfuncs" lib/${IPASIRSOLVER}/LIBS; then ${CC} ${COMPILEFLAGS} ${INCLUDES} -o lib/dummyfuncs/dummyfuncs.o -c lib/dummyfuncs/dummyfuncs.cpp && ar rcu lib/dummyfuncs/libdummyfuncs.a lib/dummyfuncs/dummyfuncs.o; fi
	cd lib/${IPASIRSOLVER} && bash fetch_and_build.sh

lilotane: COMPILEFLAGS += -DIPASIRSOLVER=\"${IPASIRSOLVER}\" -DLILOTANE_VERSION=\"${LILOTANE_VERSION}\"
lilotane: $(patsubst src/%.cpp,build/%.o,$(wildcard src/data/*.cpp src/planner/*.cpp src/sat/*.cpp src/util/*.cpp)) build/main.o
	${CC} ${INCLUDES} $^ -o lilotane ${LINKERFLAGS}

parser: lib/libpandaPIparser.a 

lib/libpandaPIparser.a:
	cd src && bash fetch_and_build_parser.sh
	
build/main.o: src/main.cpp
	${CC} ${COMPILEFLAGS} ${INCLUDES} -o $@ -c $<
	
build/%.o: src/%.cpp src/%.h #$(shell g++ -MM ${INCLUDES} )
	mkdir -p $(@D)
	${CC} ${COMPILEFLAGS} ${INCLUDES} -o $@ -c $<

clean:
#	[ ! -e libpandaPIparser.a ] || rm libpandaPIparser.a
	[ ! -e lilotane ] || rm lilotane
	touch NONE && rm NONE $(wildcard lib/${IPASIRSOLVER}/*.a lib/${IPASIRSOLVER}/*.so)
	rm -rf build
	rm -rf src/pandaPIparser/build
