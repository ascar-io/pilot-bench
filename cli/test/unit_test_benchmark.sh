#!/bin/bash
# Unit test for Pilot CLI tool run benchmark
#
# Copyright (c) 2015, 2016 University of California, Santa Cruz, CA, USA.
# Created by Yan Li <yanli@ascar.io>,
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
./pilot run_program --min-sample-size 10 --no-tui --pi "response time,ms,0,0,0.3" \
    -- ./mock_benchmark.sh >"$TMPFILE" 2>&1

grep -q "response time: R m1.725 c0.2839 v0.04466" "$TMPFILE"
grep -q "\[PI 0\] Reading mean: 1.725 ms" "$TMPFILE"
grep -q "\[PI 0\] Reading CI: 0.2839 ms" "$TMPFILE"
grep -q "\[PI 0\] Reading variance: 0.04466 ms" "$TMPFILE"
grep -q "\[PI 0\] Reading optimal subsession size: 4" "$TMPFILE"

# test quiet mode
rm -f /tmp/pilot_mock_benchmark_round.txt
./pilot run_program --min-sample-size 10 --no-tui --pi "response time,ms,0,0,0.3" \
    --quiet \
    -- ./mock_benchmark.sh 2>&1 | grep -q "0,1.72477,0.283944,0.0446593"

