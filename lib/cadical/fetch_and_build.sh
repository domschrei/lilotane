#!/bin/bash

if [ ! -d cadical ]; then
    git clone https://github.com/arminbiere/cadical.git
fi
make
