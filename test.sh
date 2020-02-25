#!/bin/bash

timeout=10
domains="umtranslog satellite transport rover entertainment"

set -e

#make cleantr
#make

solved=0
unsolved=0
all=0

# Count instances
for domain in $domains ; do    
    for pfile in instances/$domain/p*.hddl; do
        all=$((all+1))
    done
done

# Attempt to solve each instance
for domain in $domains ; do
    dfile=instances/$domain/domain.hddl
    
    for pfile in instances/$domain/p*.hddl; do
        
        if [ -f STOP ]; then
            echo "Interrupted."
            rm STOP
            exit 0
        fi
        
        set +e
        echo "[$((solved+unsolved))/$all] Running treerexx on $pfile ..."
        timeout $timeout ./treerexx $dfile $pfile $@ > OUT
        echo -ne "treerexx terminated."
        set -e
        
        if cat OUT|grep -q "<=="; then
            ./pandaPIparser $dfile $pfile -verify OUT
            solved=$((solved+1))
        else
            echo ""
            echo "No plan found on $pfile."
            unsolved=$((unsolved+1))
        fi
        
        echo ""
    done
done

echo "No verification problems occurred."
echo "$solved/$all solved within $timeout seconds."
