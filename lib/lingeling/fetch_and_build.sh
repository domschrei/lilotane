#!/bin/bash

if [ ! -d lingeling ]; then
    git clone https://github.com/arminbiere/lingeling.git
fi

make
