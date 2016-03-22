#!/bin/bash
# A handy wrapper for func_test_seq_write
#
# Copyright (c) 2015, 2016 University of California, Santa Cruz, CA, USA.
# Created by Yan Li <yanli@ucsc.edu, elliot.li.tech@gmail.com>,
# Department of Computer Science, Baskin School of Engineering.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of the Storage Systems Research Center, the
#       University of California, nor the names of its contributors
#       may be used to endorse or promote products derived from this
#       software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# REGENTS OF THE UNIVERSITY OF CALIFORNIA BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
# OF THE POSSIBILITY OF SUCH DAMAGE.
set -e -u

if [ $# -eq 0 ]; then
    cat<<EOF
Run test_func_test_seq_write, save the result in a timestamped directory, and
plot the unit readings as PDF figures.

This script accepts same options as func_test_seq_write does. For example:
$0 -r test-out-dir -o /mnt/test-device/test-file
EOF
    exit 2
fi

OUTPUT_DIR_NAME=seq-write
declare -a OPTS
while [ $# -ge 1 ]; do
    if [ "x$1" = "x-r" ]; then
        OUTPUT_DIR_NAME=$2
    fi
    OPTS+=($1)
    shift
done
TS=`date +%F_%T`
OUTPUT_DIR_NAME=${OUTPUT_DIR_NAME}_${TS}
echo "Saving results to ${OUTPUT_DIR_NAME}"
PDF_FILE_NAME=${OUTPUT_DIR_NAME%.*}.pdf

./func_test_seq_write "${OPTS[@]}"
PLOT_DIR=${OUTPUT_DIR_NAME}/unit_reading_plot
mkdir -p "$PLOT_DIR"
python `dirname $0`/plot_seq_write_throughput.py  "${OUTPUT_DIR_NAME}/unit_readings.csv" "${PLOT_DIR}/"
