#!/bin/bash

if [ ! -d cryptominisat ]; then
    git clone https://github.com/msoos/cryptominisat.git
    cd cryptominisat
else
    cd cryptominisat
    git pull
fi

mkdir -p build
cd build
cmake -DIPASIR=1 ..
make
cp lib/libipasircryptominisat5.so.5.8 ../../
cp lib/libipasircryptominisat5.so.5.8 ../../libipasircryptominisat.so
