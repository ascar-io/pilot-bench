#!/bin/bash
set -e -u

declare -a OPTS
while [ $# -ge 1 ]; do
    if [ "x$1" = "x-r" ]; then
        OUTPUT_FILE_NAME=$2
    fi
    OPTS+=($1)
    shift
done
PDF_FILE_NAME=${OUTPUT_FILE_NAME%.*}.pdf
echo 
./func_test_seq_write "${OPTS[@]}"
python `dirname $0`/plot_seq_write_throughput.py "${PDF_FILE_NAME}" "${OUTPUT_FILE_NAME}"
