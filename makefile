
# Options: minisat220 lingeling glucose4
IPASIRSOLVER?=glucose4

TREEREXX_VERSION:=dbg-$(shell date --iso-8601=seconds)-${IPASIRSOLVER}

SOLVERLIB=lib/${IPASIRSOLVER}/libipasir${IPASIRSOLVER}.a
CC=g++
CWARN=-Wno-unused-parameter -Wno-sign-compare -Wno-format -Wno-format-security

# -Q -ftime-report
COMPILEFLAGS=-pipe -Wall -Wextra -pedantic -std=c++17 $(CWARN) $(CERROR) -DIPASIRSOLVER=\"${IPASIRSOLVER}\" -DTREEREXX_VERSION=\"${TREEREXX_VERSION}\"
LINKERFLAGS=-lm -Llib -Llib/${IPASIRSOLVER} -lipasir${IPASIRSOLVER} -lz -lpandaPIparser

INCLUDES=-Isrc -Isrc/pandaPIparser/src

.PHONY = clean

release: COMPILEFLAGS += -DNDEBUG -Wno-unused-variable -O3
release: LINKERFLAGS += -O3
release: treerexx

debug: COMPILEFLAGS += -O3 -g -ggdb
debug: LINKERFLAGS += -O3 -g -ggdb
debug: treerexx

treerexx: $(patsubst src/%.cpp,build/%.o,$(wildcard src/data/*.cpp src/planner/*.cpp src/sat/*.cpp src/util/*.cpp)) build/main.o #$(wildcard src/pandaPIparser/src/*.o)
	cd lib/${IPASIRSOLVER} && bash fetch_and_build.sh
	${CC} ${INCLUDES} $^ -o treerexx ${LINKERFLAGS}
	
build/main.o: src/main.cpp
	${CC} ${COMPILEFLAGS} ${INCLUDES} -o $@ -c $<
	
build/%.o: src/%.cpp
	mkdir -p $(@D)
	${CC} ${COMPILEFLAGS} ${INCLUDES} -o $@ -c $<

${SOLVERLIB}:
	cd lib/${IPASIRSOLVER} && bash fetch_and_build.sh

clean:
#	[ ! -e libpandaPIparser.a ] || rm libpandaPIparser.a
	[ ! -e treerexx ] || rm treerexx
	touch NONE && rm NONE $(wildcard lib/${IPASIRSOLVER}/*.a)
	rm -rf build/
