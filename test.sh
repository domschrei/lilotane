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
        outfile="OUT_$(date +%s)_$@"
        
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
            ./pandaPIparser $dfile $pfile -verify "$outfile" > "VERIFY_$outfile"
            if grep -q "false" "VERIFY_$outfile"; then
                echo "${red}Verification error!${reset} Output:"
                cat "VERIFY_$outfile"
                exit 1
            else
                echo "${green}All ok.${reset}"
            fi
            solved=$((solved+1))
        else
            echo ""
            unsolved=$((unsolved+1))
        fi
        
        set +e
        rm "$outfile" "VERIFY_$outfile" 2> /dev/null
        set -e
    done
done

echo "No verification problems occurred."
echo "$solved/$all solved within $timeout seconds."

rm $cleanfiles 2> /dev/null
