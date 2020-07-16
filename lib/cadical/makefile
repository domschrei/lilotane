#-----------------------------------------------------------------------#
#- GLOBAL DEFS ---------------------------------------------------------#
#-----------------------------------------------------------------------#

# Keep this as generic as possible.

NAME=cadical

#-----------------------------------------------------------------------#
# Solver signatures have to be both valid file names and C symbols.
# Since Picosat uses a dash '-' for the package name, we have to
# differentiate between with (directory / package name) and without
# dash (the signature).

SIG=$(NAME)
DIR=$(NAME)
TARGET=libipasir$(SIG).a

#-----------------------------------------------------------------------#

CXX=g++  

CXXFLAGS= -g -std=c++11 -Wall -DNDEBUG -O3

#-----------------------------------------------------------------------#
#- REQUIRED TOP RULES --------------------------------------------------#
#-----------------------------------------------------------------------#

all: $(TARGET)

clean:
	rm -f *.o *.a

#-----------------------------------------------------------------------#
#- INVISIBLE INTERNAL SUB RULES ----------------------------------------#
#-----------------------------------------------------------------------#

libipasir$(SIG).a: 
	cd $(DIR); ./configure; make; cd ..
	@#
	@# compile glue code
	@#
	#make ipasir$(NAME)glue.o
	@#
	@# merge library and glue code into target
	@#
	cp $(DIR)/build/libcadical.a $(TARGET)
	#ar r $(TARGET) ipasir$(NAME)glue.o

#-----------------------------------------------------------------------#
#- LOCAL GLUE RULES ----------------------------------------------------#
#-----------------------------------------------------------------------#

ipasir$(NAME)glue.o: ipasir$(NAME)glue.cpp ipasir.h makefile
	$(CXX) $(CXXFLAGS) \
	  -I$(DIR) -c ipasir$(NAME)glue.cpp

#-----------------------------------------------------------------------#

.PHONY: all clean
