#!/bin/bash

if [ "x$TIMEOUT" == "x" ]; then
	timeout=10
else
	timeout="$TIMEOUT"
fi
rating_timeout=1800
domains="ipc2020-feature-test-forall miconic gripper smartphone satellite umtranslog woodworking zenotravel childsnack rover barman depots transport hiking blocksworld factories entertainment" 

function header() {
    echo -ne "[$((solved+unsolved+1))/$all] "
}

function rating() {
    time="$1"
    if (( $(echo "$time <= 1"|bc -l) )); then
        echo "1"
    else 
        r=$(echo "1 - l($time) / l($rating_timeout)"|bc -l)
        if (( $(echo "$r > 1"|bc -l) )); then
            echo "1"
        else
            echo "$r"
        fi
    fi
}

function end() {
    echo "$solved/$all solved within ${timeout}s per instance."
    echo -ne "Total score for T=${rating_timeout}s: ${green}"
    LC_ALL=C printf "%.4f" $score
    echo "${reset}."
    exit $1
}

exit_on_error=true
exit_on_verify_fail=true

set -e
set -o pipefail
trap 'echo "Interrupted."; end 1' INT

red=`tput setaf 1`
green=`tput setaf 2`
yellow=`tput setaf 3`
blue=`tput setaf 4`
reset=`tput sgr0`

solved=0
unsolved=0
all=0
score=0

# Count instances
for domain in $domains ; do    
    for pfile in instances/$domain/p*.hddl; do
        all=$((all+1))
    done
done

# Output chosen parameters of treerexx
echo "${blue}$(./treerexx $@|tail -1)${reset}"

# Attempt to solve each instance
for domain in $domains ; do
    dfile=instances/$domain/domain.hddl
    
    for pfile in instances/$domain/p*.hddl; do
        
        logdir="logs/${BASHPID}/${domain}_$(basename $pfile .hddl)_$@_$(date +%s)/"
        mkdir -p "$logdir"
        outfile="$logdir/OUT"
        verifile="$logdir/VERIFY"
        
        set +e
        header
        echo -ne "${blue}$pfile${reset} ... "

        command="./treerexx $dfile $pfile $@"

        start=$(date +%s.%N)
        /usr/bin/timeout $timeout $command > "$outfile" & wait -n
        retval="$?"
        end=$(date +%s.%N)    
        runtime=$(python -c "print(${end} - ${start})")
        thisscore=0

        if [ "$retval" == "0" ]; then
            echo -ne "exit code ${green}$retval${reset}. "
            thisscore=$(rating "$runtime")
            score=$(echo "$score + $thisscore"|bc -l)
        else
            echo -ne "${yellow}exit code $retval.${reset} "
            if [ "$retval" == "134" -o "$retval" == "139" -o "$retval" == "6" -o "$retval" == "11" ]; then
                if $exit_on_error ; then
                    echo "Try:"
                    echo "valgrind $command" 
                    echo "${red}Exiting.${reset}"
                    end 1
                fi
            fi
        fi
        set -e
        
        if cat "$outfile"|grep -q "<=="; then
            echo -ne "Verifying ... "
            ./pandaPIparser $dfile $pfile -verify "$outfile" > "$verifile"
            if grep -q "false" "$verifile"; then
                echo -ne "${red}Verification error!${reset}"
                if $exit_on_verify_fail ; then
                    echo " Output:"
                    cat "$verifile"
                    exit 1
                else
                    echo ""
                fi
            else
                echo "${green}All ok.${reset}"
                echo -ne "$(header | sed -e 's/./ /g')"
                LC_ALL=C printf "TIME %.2f SCORE %.2f\n" $runtime $thisscore
            fi
            solved=$((solved+1))
        else
            echo ""
            unsolved=$((unsolved+1))
        fi
    done
done

end 0
