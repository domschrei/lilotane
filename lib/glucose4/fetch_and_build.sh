#!/bin/bash

if [ ! -d sat/glucose4 ]; then
    wget https://baldur.iti.kit.edu/sat-competition-2017/solvers/incremental/glucose-ipasir.zip
    unzip glucose-ipasir.zip
    rm glucose-ipasir.zip
fi
cd sat/glucose4
make
cp libipasirglucose4.a ../../
