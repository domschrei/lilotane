CC=g++

# Options: minisat220 lingeling
IPASIRSOLVER=lingeling 

CWARN=-Wno-unused-parameter -Wno-sign-compare -Wno-format -Wno-format-security
CERROR=

COMPILEFLAGS=-O2 -g -pipe -Wall -Wextra -pedantic -std=c++17 $(CWARN) $(CERROR)
LINKERFLAG=-O2 -lm -Llib -lipasir${IPASIRSOLVER}

#COMPILEFLAGS=-O0 -ggdb -pipe -Wall -Wextra -pedantic -std=c++17 $(CWARN) $(CERROR)
#LINKERFLAG=-O0 -ggdb
INCLUDES=-Isrc -Isrc/parser

.PHONY = parser clean

treerexx: $(patsubst %.cpp,%.o,$(wildcard src/parser/*.cpp src/data/*.cpp src/planner/*.cpp src/sat/*.cpp src/util/*.cpp)) src/main.o
	${CC} $^ -o treerexx ${LINKERFLAG}

src/parser/%.o: src/parser/%.cpp src/parser/%.hpp
	cd src/parser && make
	
src/main.o: src/main.cpp
	${CC} ${COMPILEFLAGS} ${INCLUDES} -o $@ -c $<
	
%.o: %.cpp %.h
	${CC} ${COMPILEFLAGS} ${INCLUDES} -o $@ -c $<

#libpandaPIparser.a: $(wildcard src/parser/*.cpp)
#	ar rc libpandaPIparser.a $(patsubst %.cpp,%.o,$^)
#	ranlib libpandaPIparser.a
	
clean:
#	[ ! -e libpandaPIparser.a ] || rm libpandaPIparser.a
	[ ! -e treerexx ] || rm treerexx
	rm $(wildcard src/*.o src/*/*.o)

cleantr:
	[ ! -e treerexx ] || rm treerexx
	rm $(wildcard src/*.o src/data/*.o src/planner/*.o src/sat/*.o src/util/*.o)
