#!/bin/bash
# Unit test for Pilot CLI tool run benchmark
#
# Copyright (c) 2017-2018 Yan Li <yanli@tuneup.ai>. All rights reserved.
# The Pilot tool and library is free software; you can redistribute it
# and/or modify it under the terms of the GNU Lesser General Public
# License version 2.1 (not any other version) as published by the Free
# Software Foundation.
#
# The Pilot tool and library is distributed in the hope that it will be
# useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this program in file lgpl-2.1.txt; if not, see
# https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
#
# Commit 033228934e11b3f86fb0a4932b54b2aeea5c803c and before were
# released with the following license:
# Copyright (c) 2015, 2016, University of California, Santa Cruz, CA, USA.
# Created by Yan Li <yanli@tuneup.ai>,
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

TMPFILE=`mktemp`
rm -f /tmp/pilot_mock_benchmark_round.txt
./bench run_program --ci-perc 0.3 --min-sample-size 10 --pi "response time,ms,0,0,1" \
    -- ./mock_benchmark.sh >"$TMPFILE" 2>&1

grep -q "response time: R m1.725 c0.2839 v0.04466" "$TMPFILE"
grep -q "\[PI 0\] Reading mean: 1.725 ms" "$TMPFILE"
grep -q "\[PI 0\] Reading CI: 0.2839 ms" "$TMPFILE"
grep -q "\[PI 0\] Reading variance: 0.04466 ms" "$TMPFILE"
grep -q "\[PI 0\] Reading optimal subsession size: 4" "$TMPFILE"

# test quiet mode
rm -f /tmp/pilot_mock_benchmark_round.txt
OUTPUT_DIR=`mktemp -d -u`
./bench run_program --ci-perc 0.3 --min-sample-size 10 --pi "response time,ms,0,0,1:delay time,ms,1,0" \
    --quiet -o ${OUTPUT_DIR} \
    -- ./mock_benchmark.sh >"$TMPFILE" 2>&1
# we don't directly compare the output with an expected file, because the
# session_duration on the first line will always be different
grep -q "0,1.72477,0.283944,0.0446593,0,1.72477,0.283944,0.0446593," "$TMPFILE"
grep -q "1,2.72477,0.283944,0.0446593,0,2.72477,0.283944,0.0446593"  "$TMPFILE"
# even we are running in quiet mode log should still contain <debug> information
grep -q '<debug>' "${OUTPUT_DIR}/session_log.txt"
# check correctness of saved resuls
grep -q "0,44,1.72477,1.72477,0.0446593,0.0446593,0.283944,0.283944,0" "${OUTPUT_DIR}/pi_results.csv"
grep -q "1,44,2.72477,2.72477,0.0446593,0.0446593,0.283944,0.283944,0" "${OUTPUT_DIR}/pi_results.csv"
# TODO: add more checks here
