#!/bin/bash
set -e -u

if [ $# -eq 0 ]; then
    cat<<EOF
This script accepts same options as func_test_seq_write does.
EOF
fi

OUTPUT_FILE_NAME=seq-write.csv
declare -a OPTS
while [ $# -ge 1 ]; do
    if [ "x$1" = "x-r" ]; then
        OUTPUT_FILE_NAME=$2
    fi
    OPTS+=($1)
    shift
done
PDF_FILE_NAME=${OUTPUT_FILE_NAME%.*}.pdf
OUT_FILE_NAME=${OUTPUT_FILE_NAME%.*}.out

./func_test_seq_write "${OPTS[@]}" | tee "${OUT_FILE_NAME}"
python `dirname $0`/plot_seq_write_throughput.py "${PDF_FILE_NAME}" "${OUTPUT_FILE_NAME}"
