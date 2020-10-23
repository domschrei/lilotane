#!/bin/bash

if [ ! -d riss ]; then
    git clone https://github.com/conp-solutions/riss.git
    cd riss
else
    cd riss
    git pull
fi

mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make riss-coprocessor-lib-static
cp lib/libriss-coprocessor.a ../../libipasirriss.a
