
# Makefile for building Lilotane as an IPASIR application
# (see github.com/biotomas/ipasir )

TARGET=$(shell basename "`pwd`")
IPASIRSOLVER ?= picosat961

all:
	mkdir -p build
	cd build && cmake .. -DCMAKE_BUILD_TYPE=RELEASE -DIPASIRSOLVER=$(IPASIRSOLVER) -DIPASIRDIR=../../sat && make && cp lilotane .. && cd ..

clean:
	rm -rf $(TARGET) build/
