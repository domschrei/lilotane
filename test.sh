#!/bin/bash

timeout=10

set -e

make cleantr
make

solved=0
unsolved=0

for domain in rover transport ; do
    dfile=instances/$domain/domain.hddl
    
    for pfile in instances/$domain/p*.hddl; do
        
        set +e
        timeout $timeout ./treerexx $dfile $pfile > OUT
        set -e
        
        if cat OUT|grep -q "<=="; then
            ./pandaPIparser $dfile $pfile -vvverify OUT
            solved=$((solved+1))
        else
            echo "No plan found on $pfile."
            unsolved=$((unsolved+1))
        fi
    done
done

echo "No verification problems occurred."
echo "$solved/$unsolved solved within $timeout seconds."
