#!/bin/bash

if [ "x$TIMEOUT" == "x" ]; then
	timeout=10
else
	timeout="$TIMEOUT"
fi
if [ "x$MEMOUT" == "x" ]; then
	memout=8000000
else
	memout="$MEMOUT"
fi
if [ "x$MODE" == "x" ]; then
    # lilotane panda_to panda_po panda_opt
    mode=lilotane
else
    mode="$MODE"
fi
rating_timeout=1800

export LD_LIBRARY_PATH="lib/cryptominisat/:$LD_LIBRARY_PATH"
export PATH=".:$PATH"

function header() {
    echo -ne "[$instid/$all] "
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
    echo "$nsolved/$(($nsolved+$nunsolved)) solved within ${timeout} per instance."
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

nsolved=0
nunsolved=0
all=0
score=0

inputfile=instances/ipc/instances_ipc_to.txt
#inputfile=instances/instances_test.txt
outdir=tests/tests_$(date +%s)
mkdir -p $outdir

# Count instances
all=$(cat $inputfile|wc -l)

# Output chosen parameters of lilotane
echo "${blue}$(./lilotane $@ -h|tail -1)${reset}"

# Attempt to solve each instance
while read -r line ; do
    pfile=$(echo $line|awk '{print $2}')
    dfile=$(echo $line|awk '{print $3}')
    instid=$((nsolved+nunsolved+1))

    header
    echo -ne "${blue}$pfile${reset} ... "
    
    if [ $mode == lilotane ]; then
        command="./lilotane $dfile $pfile $@"
    elif [ $mode == panda_to ]; then
        command="/usr/lib/jvm/java-8-openjdk-amd64/bin/java -jar PANDA.jar -systemConfig AAAI-2018-totSAT(cryptominisat) -programPath cryptominisat=./cryptominisat5 $dfile $pfile"
    elif [ $mode == panda_po ]; then
        command="/usr/lib/jvm/java-8-openjdk-amd64/bin/java -jar PANDA.jar -systemConfig sat-exists-forbidden-implication(cms55) -programPath cryptominisat55=./cryptominisat5 $dfile $pfile"
    elif [ $mode == panda_opt ]; then
        command="/usr/lib/jvm/java-8-openjdk-amd64/bin/java -jar PANDA.jar -systemConfig sat-exists-forbidden-implication-optimise(bin)(cms55) -programPath cryptominisat55=./cryptominisat5 $dfile $pfile"
    fi

    outfile="$outdir/$instid/rw"
    echo "$instid $command" > CMD
    
    set +e
    ./runwatch -p 1 -T $timeout -M $memout -d "$outdir" -q CMD
    retval="$?"
    set -e
    
    resultline=$(tail -50 "$outfile"|grep RUNWATCH_RESULT|grep -oE "RUNWATCH_RESULT (EXIT|TIMEOUT|MEMOUT) RETVAL -?[0-9]+ TIME_SECS [0-9\.]+ MEMPEAK_KBS [0-9\.]+")
    status=$(echo $resultline|awk '{print $2}')
    runtime=$(echo $resultline|awk '{print $6}')
    mempeak=$(echo $resultline|awk '{print $8}')
    thisscore=$(rating "$runtime")
    
    verifile="$outdir/$instid/verify"
    
    if [ "$status" != "EXIT" ]; then
        echo "${yellow}$status.${reset}"
        if [ "$retval" == "134" -o "$retval" == "139" -o "$retval" == "6" -o "$retval" == "11" ]; then
            if $exit_on_error ; then
                echo "Try:"
                echo "valgrind $command" 
                echo "${red}Exiting.${reset}"
                end 1
            fi
        fi
        nunsolved=$(($nunsolved+1))
        continue
    fi
    
    echo -ne "${green}exited properly${reset}. "
    
    solved=false
    
    if [ $mode == lilotane ]; then
    
        if grep -q "<==" "$outfile" ; then
            solved=true
            nsolved=$((nsolved+1))
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
            fi
        else
            echo ""
            nunsolved=$((nunsolved+1))
        fi
    
    elif echo $mode|grep -q panda ; then
        if [ "$status" == "EXIT" ] && grep -q "planner result = SOLUTION" "$outfile"; then
            solved=true
            nsolved=$((nsolved+1))
            echo "${green}Solved.${reset}"
        else
            nunsolved=$((nunsolved+1))
            if [ "$status" == "EXIT" ]; then echo "${yellow}Unsolved.${reset}"
            else echo ""; fi
        fi
    fi
    
    if $solved ; then
        echo -ne "$(header | sed -e 's/./ /g')" # clean indentation
        LC_ALL=C printf "TIME %.2f SCORE %.2f\n" $runtime $thisscore
        score=$(echo "$score + $thisscore"|bc -l)
    fi
done < $inputfile

end 0
