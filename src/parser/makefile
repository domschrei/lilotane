CC=g++

CWARN=-Wno-unused-parameter
CERROR=

COMPILEFLAGS=-O3 -pipe -Wall -Wextra -pedantic -std=c++17 $(CWARN) $(CERROR)
LINKERFLAG=-O3 -lm -flto
#COMPILEFLAGS=-O0 -ggdb -pipe -Wall -Wextra -pedantic -std=c++17 $(CWARN) $(CERROR)
#LINKERFLAG=-O0 -ggdb

.PHONY = parser clean

pandaPIparser: hddl-token.o hddl.o main.o sortexpansion.o parsetree.o util.o domain.o output.o parametersplitting.o cwa.o typeof.o shopWriter.o hpdlWriter.o plan.o verify.o orderingDecomposition.o
	#${CC} ${LINKERFLAG} $^ -o pandaPIparser 
	echo "Panda modules compiled."

%.o: %.cpp %.hpp hddl.hpp
	${CC} ${COMPILEFLAGS} -o $@ -c $<

%.o: %.cpp hddl.hpp
	${CC} ${COMPILEFLAGS} -o $@ -c $<

hddl-token.cpp: hddl.cpp hddl-token.l
	flex --yylineno -o hddl-token.cpp hddl-token.l

hddl.cpp: hddl.y
	bison -v -d -o hddl.cpp hddl.y

hddl.hpp: hddl.cpp

clear:
	rm hddl-token.cpp
	rm hddl.cpp
	rm hddl.hpp
	rm *o 
