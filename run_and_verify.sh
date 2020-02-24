#!/bin/bash

set -e

dfile="$1"
pfile="$2"
shift 2

./treerexx "$dfile" "$pfile" $@ |tee OUT
./pandaPIparser "$dfile" "$pfile" -verify OUT
