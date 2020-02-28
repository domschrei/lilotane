#!/bin/bash

timeout=300
domains="woodworking smartphone umtranslog satellite transport rover entertainment"

set -e
set -o pipefail

for domain in $domains ; do
    dfile=instances/$domain/domain.hddl
    
    for pfile in instances/$domain/p*.hddl; do
        echo "bash run_and_verify.sh $dfile $pfile $timeout $@"
    done
done
