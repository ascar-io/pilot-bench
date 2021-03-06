#!/usr/bin/env bash
# Run dd and extract duration from dd's output
# Author: Yan Li <yanli@tuneup.ai>
# This file is in public domain.
set -euo pipefail

OUTPUT_FILE=$1
IO_COUNT=$2

dd if=/dev/zero of=$OUTPUT_FILE bs=1m count=$IO_COUNT 2>&1 | \
    grep "bytes transferred" | \
    sed "s/^.*transferred in \([\.0-9][\.0-9]*\) secs.*/\1/g"
