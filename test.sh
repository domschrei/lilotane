#!/bin/bash

timeout=10
domains="umtranslog satellite transport rover entertainment"

set -e
set -o pipefail

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
        outfile="OUT_$(date +%s)_$@"
        
        set +e
        echo "[$((solved+unsolved))/$all] Running treerexx on $pfile ..."
        /usr/bin/timeout $timeout ./treerexx $dfile $pfile $@ > "$outfile"
        retval="$?"
        if [ "$retval" != "0" ]; then
            echo "Exit code $retval"
        fi
        set -e
        
        if cat "$outfile"|grep -q "<=="; then
            if ./pandaPIparser $dfile $pfile -verify "$outfile"|grep -q "false"; then
                echo "Verification error! Output:"
                ./pandaPIparser $dfile $pfile -verify "$outfile"
                exit 1
            fi
            solved=$((solved+1))
        else
            echo "No plan found on $pfile."
            unsolved=$((unsolved+1))
        fi
    done
done

echo "No verification problems occurred."
echo "$solved/$all solved within $timeout seconds."
