
# Options: lingeling glucose4 cadical cryptominisat
IPASIRSOLVER?=glucose4

SOLVERLIB=lib/${IPASIRSOLVER}/libipasir${IPASIRSOLVER}.a
CC=g++
CWARN=-Wno-unused-parameter -Wno-sign-compare -Wno-format -Wno-format-security

COMPILEFLAGS=-pipe -Wall -Wextra -pedantic -std=c++17 $(CWARN) $(CERROR) -DIPASIRSOLVER=\"${IPASIRSOLVER}\" -DLILOTANE_VERSION=\"${LILOTANE_VERSION}\"
LINKERFLAGS=-lm -Llib -Llib/${IPASIRSOLVER} -lipasir${IPASIRSOLVER} -lz -lpandaPIparser $(shell cat lib/${IPASIRSOLVER}/LIBS || echo "")

INCLUDES=-Isrc -Isrc/pandaPIparser/src

.PHONY = release debug parser clean

release: LILOTANE_VERSION:=rls-$(shell date --iso-8601=seconds)-${IPASIRSOLVER}
release: COMPILEFLAGS += -DNDEBUG -Wno-unused-variable -O3 -flto
release: LINKERFLAGS += -O3 -flto
release: parser
release: lilotane

debug: LILOTANE_VERSION:=dbg-$(shell date --iso-8601=seconds)-${IPASIRSOLVER}
debug: COMPILEFLAGS += -O3 -g 
debug: LINKERFLAGS += -O3 -g 
debug: parser
debug: lilotane

parser: lib/libpandaPIparser.a 

lib/libpandaPIparser.a:
	cd src && bash fetch_and_build_parser.sh

lilotane: $(patsubst src/%.cpp,build/%.o,$(wildcard src/data/*.cpp src/planner/*.cpp src/sat/*.cpp src/util/*.cpp)) build/main.o
	cd lib/${IPASIRSOLVER} && bash fetch_and_build.sh
	${CC} ${INCLUDES} $^ -o lilotane ${LINKERFLAGS}
	
build/main.o: src/main.cpp
	${CC} ${COMPILEFLAGS} ${INCLUDES} -o $@ -c $<
	
build/%.o: src/%.cpp src/%.h
	mkdir -p $(@D)
	${CC} ${COMPILEFLAGS} ${INCLUDES} -o $@ -c $<

clean:
#	[ ! -e libpandaPIparser.a ] || rm libpandaPIparser.a
	[ ! -e lilotane ] || rm lilotane
	touch NONE && rm NONE $(wildcard lib/${IPASIRSOLVER}/*.a)
	rm -rf build
	rm -rf src/pandaPIparser/build
