
# Options: minisat220 lingeling glucose4
IPASIRSOLVER?=glucose4

TREEREXX_VERSION?=dbg-$(shell date --iso-8601=seconds)-${IPASIRSOLVER}

SOLVERLIB=lib/${IPASIRSOLVER}/libipasir${IPASIRSOLVER}.a
CC=g++
CWARN=-Wno-unused-parameter -Wno-sign-compare -Wno-format -Wno-format-security
CERROR=

#-DNDEBUG 
COMPILEFLAGS=-O3 -g -pipe -Wall -Wextra -pedantic -std=c++17 $(CWARN) $(CERROR) -DIPASIRSOLVER=\"${IPASIRSOLVER}\" -DTREEREXX_VERSION=\"${TREEREXX_VERSION}\"
LINKERFLAG=-O3 -lm -Llib/${IPASIRSOLVER} -lipasir${IPASIRSOLVER} -lz

#COMPILEFLAGS=-O0 -ggdb -pipe -Wall -Wextra -pedantic -std=c++17 $(CWARN) $(CERROR)
#LINKERFLAG=-O0 -ggdb
INCLUDES=-Isrc -Isrc/parser

.PHONY = parser clean

treerexx: src/parser/hddl.o src/parser/hddl-token.o $(patsubst %.cpp,%.o,$(wildcard src/parser/*.cpp src/data/*.cpp src/planner/*.cpp src/sat/*.cpp src/util/*.cpp)) src/main.o
	cd lib/${IPASIRSOLVER} && bash fetch_and_build.sh
	${CC} ${INCLUDES} $^ -o treerexx ${LINKERFLAG}

src/parser/hddl.o:
	cd src/parser && make
	
src/parser/%.o: src/parser/%.cpp src/parser/%.hpp
	cd src/parser && make
	
src/main.o: src/main.cpp
	${CC} ${COMPILEFLAGS} ${INCLUDES} -o $@ -c $<
	
%.o: %.cpp %.h
	${CC} ${COMPILEFLAGS} ${INCLUDES} -o $@ -c $<

${SOLVERLIB}:
	cd lib/${IPASIRSOLVER} && bash fetch_and_build.sh

clean:
#	[ ! -e libpandaPIparser.a ] || rm libpandaPIparser.a
	cd src/parser && make clear
	[ ! -e treerexx ] || rm treerexx
	touch NONE && rm NONE $(wildcard lib/${IPASIRSOLVER}/*.a)
	rm $(wildcard src/*.o src/*/*.o) 2> /dev/null

cleantr:
	[ ! -e treerexx ] || rm treerexx
	touch NONE && rm NONE $(wildcard src/*.o src/data/*.o src/planner/*.o src/sat/*.o src/util/*.o)
