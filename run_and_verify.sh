#!/bin/bash

set -e

dfile="$1"
pfile="$2"
shift 2

> OUT
./treerexx "$dfile" "$pfile" $@ |tee OUT
if cat OUT|grep -q "<=="; then
    ./pandaPIparser "$dfile" "$pfile" -verify OUT
fi