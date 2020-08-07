#!/bin/bash

set -e
set -o pipefail

red=$(tput setaf 1)
green=$(tput setaf 2)
yellow=$(tput setaf 3)
blue=$(tput setaf 4)
reset=$(tput sgr0)

dfile="$1"
pfile="$2"
timeout="$3"
shift 3

logdir="logs/$(echo $dfile|sed 's,/,_,g')_$(basename $pfile .hddl)_$@_$(date +%s)/"
logdir=${logdir// /_}
mkdir -p "$logdir"
outfile="$logdir/OUT"
timefile="$logdir/TIME"
verifile="$logdir/VERIFY"

echo -ne "Running lilotane on ${blue}$pfile${reset} ... "
/usr/bin/time -o "$timefile" /usr/bin/timeout $timeout ./lilotane $dfile $pfile $@ > "$outfile"

retval="0"
if grep -q non-zero "$timefile"; then
    retval=$(grep non-zero "$timefile"|grep -oE "[0-9]+")
fi

if [ "$retval" == "0" ]; then
    echo -ne "exit code ${green}$retval${reset}. "
else
    echo -ne "${yellow}exit code $retval.${reset} "
fi

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
else
    echo ""
fi
