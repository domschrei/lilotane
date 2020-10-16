#!/bin/bash

if [ "x$TIMEOUT" == "x" ]; then
	timeout=10
else
	timeout="$TIMEOUT"
fi
rating_timeout=1800

# All collected relevant domains, sorted approximately by difficulty (for Lilotane)
domains="miconic gripper satellite umtranslog smartphone woodworking zenotravel childsnack barman depots entertainment\
 hiking blocksworld ipc-blocks ipc-logistics rover transport HikingG Elevator minecraft ipc-minecraft-house\
 ipc-minecraft-player ipc-rover ipc-entertainment-1 ipc-entertainment-2 ipc-entertainment-3 ipc-entertainment-4\
 RoverG SatelliteG factories ipc-freecell"

# Domains of comparison of Lilotane vs. Tree-REX
#domains="barman blocksworld childsnack depots Elevator entertainment gripper HikingG RoverG SatelliteG transport Zenotravel"

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

outdir=tests/tests_$(date +%s)
mkdir $outdir

# Count instances
for domain in $domains ; do    
    for pfile in instances/$domain/p*.hddl; do
        all=$((all+1))
    done
done

# Output chosen parameters of lilotane
echo "${blue}$(./lilotane $@ -h|tail -1)${reset}"

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

        command="./lilotane $dfile $pfile $@"

        start=$(date +%s.%N)
        /usr/bin/timeout $timeout $command > "$outfile" & wait -n
        retval="$?"
        end=$(date +%s.%N)    
        runtime=$(echo "${end} ${start}" | awk '{printf("%.3f\n", $1-$2)}')
        thisscore=$(rating "$runtime")

        if [ "$retval" == "0" ]; then
            echo -ne "exit code ${green}$retval${reset}. "
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
            ./pandaPIparser $dfile $pfile -verify "$outfile" > "$verifile" || :
            if grep -qE "Plan verification result:.*false" "$verifile"; then
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
                echo -ne "$(header | sed -e 's/./ /g')" # clean indentation
                LC_ALL=C printf "TIME %.2f SCORE %.2f\n" $runtime $thisscore
                score=$(echo "$score + $thisscore"|bc -l)

                cp "$outfile" "$outdir/log_$((solved+unsolved+1))_$domain"
                grep -E "PLO (BEGIN|UPDATE|END)" "$outfile" > "$outdir/plo_$((solved+unsolved+1))_$domain" || :
                sed -e '1,/Total amount of clauses encoded/ d' "$outfile" | grep -E "^[0-9\.]+ - .* : [0-9]+" \
                    | awk '{print $3,$5}' > "$outdir/cls_$((solved+unsolved+1))_$domain" || :
            fi
            solved=$((solved+1))
        else
            echo ""
            unsolved=$((unsolved+1))
        fi
    done
done

end 0
