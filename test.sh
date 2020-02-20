#!/bin/bash

timeout=10

set -e

make cleantr
make

solved=0
unsolved=0

for domain in transport ; do
    dfile=instances/$domain/domain.hddl
    
    for pfile in instances/$domain/p*.hddl; do
        
        set +e
        echo "Running treerexx on $pfile ..."
        timeout $timeout ./treerexx $dfile $pfile > OUT
        echo "treerexx terminated."
        set -e
        
        if cat OUT|grep -q "<=="; then
            ./pandaPIparser $dfile $pfile -verify OUT
            solved=$((solved+1))
        else
            echo "No plan found on $pfile."
            unsolved=$((unsolved+1))
        fi
    done
done

echo "No verification problems occurred."
echo "$solved/$((solved+unsolved)) solved within $timeout seconds."
