#!/bin/bash

timeout=10
domains="woodworking smartphone umtranslog satellite transport rover entertainment"

set -e
set -o pipefail

red=`tput setaf 1`
green=`tput setaf 2`
yellow=`tput setaf 3`
blue=`tput setaf 4`
reset=`tput sgr0`

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
        
        logdir="logs/${domain}_$(basename $pfile .hddl)_$@_$(date +%s)/"
        mkdir -p "$logdir"
        outfile="$logdir/OUT"
        verifile="$logdir/VERIFY"
        
        set +e
        echo -ne "[$((solved+unsolved))/$all] Running treerexx on ${blue}$pfile${reset} ... "
        /usr/bin/timeout $timeout ./treerexx $dfile $pfile $@ > "$outfile"
        retval="$?"
        if [ "$retval" == "0" ]; then
            echo -ne "exit code ${green}$retval${reset}. "
        else
            echo -ne "${yellow}exit code $retval.${reset} "
        fi
        set -e
        
        if cat "$outfile"|grep -q "<=="; then
            echo -ne "Verifying ... "
            ./pandaPIparser $dfile $pfile -verify "$outfile" > "$verifile"
            if grep -q "false" "$verifile"; then
                echo "${red}Verification error!${reset} Output:"
                cat "$verifile"
                exit 1
            else
                echo "${green}All ok.${reset}"
            fi
            solved=$((solved+1))
        else
            echo ""
            unsolved=$((unsolved+1))
        fi
    done
done

echo "No verification problems occurred."
echo "$solved/$all solved within $timeout seconds."
