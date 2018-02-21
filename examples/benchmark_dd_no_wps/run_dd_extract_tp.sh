#!/bin/bash
# Run dd and extract duration from dd's output
# Author: Yan Li <yanli@tuneup.ai>
# This file is in public domain.
set -e -u

if [ $# -lt 2 ]; then
    cat<<EOF
Usage: $0 output_file io_count
output_file:   the file to write to
io_count:      how many MBs to write
EOF
    exit 2
fi

OUTPUT_FILE=$1
IO_COUNT=$2

dd if=/dev/zero of=$OUTPUT_FILE bs=1m count=$IO_COUNT 2>&1 | \
    grep "bytes transferred" | \
    sed "s/^.*secs (\([\.0-9][\.0-9]*\) bytes.*/\1/g"

