#!/bin/bash

set -e

# Fetch a clean state of pandaPIparser
if [ ! -d pandaPIparser ]; then
    echo "Fetching pandaPIparser ..."
    git clone https://github.com/panda-planner-dev/pandaPIparser.git
    cd pandaPIparser
else
    cd pandaPIparser
    git clean -f
fi

# Checkout correct commit (can be updated but must be manually checked to build cleanly)
git config --global advice.detachedHead false
git checkout 95bbe291c5bdb9fb517c1ad55f5136d45450c644

# Patch pandaPIparser with adapted makefile and "library" header
cp ../panda_makefile makefile
cp ../libpanda.hpp src/

# Build library (internally does a patch of pandaPIparser's main.cpp)
make library
cp build/libpandaPIparser.a ../../lib/
echo "Copied libpandaPIparser.a into lib/ directory."

# Build standalone executable for debugging purposes
make executable
cp pandaPIparser ../../
echo "Copied pandaPIparser executable into lilotane root directory."
