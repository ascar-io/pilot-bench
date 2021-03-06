#!/usr/bin/env bash
# Unit test for pilot cli tool opt parsing
#
# Copyright (c) 2017-2019 Yan Li <yanli@tuneup.ai>. All rights reserved.
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
set -euo pipefail

run() {
    eval "$@" || :
    if which valgrind >/dev/null; then
        # TODO: enable valgrind here
        true
    fi
}

BASENAME=`basename $0`

run ./bench run_program 2>&1 | grep -q "Error: program_path is required"

run ./bench run_program -- 2>&1 | grep -q "Error: program_path is required"

run ./bench run_program -- true 2>&1 | grep -q "Error: no PI or duration column set, exiting..."

run ./bench run_program --ci -1 --ci-perc -1 --pi "throughput,MB/s,2,1,1" -- true 2>&1 | grep -q "Error: CI (percent of mean) and CI (absolute value) cannot be both disabled. At least one must be set."

# Check the default quick preset
TMPFILE=`mktemp`
# We need to put in some workload amount to enable short round detection
run ./bench run_program --pi "throughput,MB/s,2,1,1" -w 10,20 -- true >$TMPFILE 2>&1
grep -q "Preset mode activated: quick" $TMPFILE
grep -q "Setting the limit of autocorrelation coefficient to 0.8" $TMPFILE
grep -q "Setting the required width of confidence interval to 20% of mean" $TMPFILE
grep -q "Setting the required minimum subsession sample size to 30" $TMPFILE
grep -q "Setting the short round threshold to 3 second(s)" $TMPFILE

# Check the normal preset
run ./bench run_program --preset normal --pi "throughput,MB/s,2,1,1" -w 10,20 -- true >$TMPFILE 2>&1
grep -q "Preset mode activated: normal" $TMPFILE
grep -q "Setting the limit of autocorrelation coefficient to 0.2" $TMPFILE
grep -q "Setting the required width of confidence interval to 10% of mean" $TMPFILE
grep -q "Setting the required minimum subsession sample size to 50" $TMPFILE
grep -q "Setting the short round threshold to 10 second(s)" $TMPFILE

# Check the strict preset
run ./bench run_program --pi "throughput,MB/s,2,1,1" --preset strict -w 10,20 -- true >$TMPFILE 2>&1
grep -q "Preset mode activated: strict" $TMPFILE
grep -q "Setting the limit of autocorrelation coefficient to 0.1" $TMPFILE
grep -q "Setting the required width of confidence interval to 10% of mean" $TMPFILE
grep -q "Setting the required minimum subsession sample size to 200" $TMPFILE
grep -q "Setting the short round threshold to 20 second(s)" $TMPFILE

# Test setting CI absolute value
run ./bench run_program --ci 42 --pi "throughput,MB/s,2,1,1" --preset strict -w 10,20 -- true >$TMPFILE 2>&1
grep -q "Preset mode activated: strict" $TMPFILE
grep -q "Setting the limit of autocorrelation coefficient to 0.1" $TMPFILE
grep -q "Setting the required width of confidence interval to 10% of mean" $TMPFILE
grep -q "Setting the required width of confidence interval to 42" $TMPFILE
grep -q "Setting the required minimum subsession sample size to 200" $TMPFILE
grep -q "Setting the short round threshold to 20 second(s)" $TMPFILE

# Short round detection should be disabled when no work amount is set
run ./bench run_program --pi "throughput,MB/s,2,1,1" -- true >$TMPFILE 2>&1
grep -q "Preset mode activated: quick" $TMPFILE
grep -q "Setting the limit of autocorrelation coefficient to 0.8" $TMPFILE
grep -q "Setting the required width of confidence interval to 20% of mean" $TMPFILE
grep -q "Setting the required minimum subsession sample size to 30" $TMPFILE
grep -q "Disabled short round detection because work amount information is not set." $TMPFILE

# Test overriding CI
run ./bench run_program --ci-perc 0.12 --pi "throughput,MB/s,2,1,1" --preset strict -w 10,20 -- true >$TMPFILE 2>&1
grep -q "Preset mode activated: strict" $TMPFILE
grep -q "Setting the limit of autocorrelation coefficient to 0.1" $TMPFILE
grep -q "Setting the required width of confidence interval to 12% of mean" $TMPFILE
grep -q "Setting the required minimum subsession sample size to 200" $TMPFILE
grep -q "Setting the short round threshold to 20 second(s)" $TMPFILE

# Test overriding autocorrelation coefficient range (AC)
run ./bench run_program --ac .3 --pi "throughput,MB/s,2,1,1" --preset strict -w 10,20 -- true >$TMPFILE 2>&1
grep -q "Preset mode activated: strict" $TMPFILE
grep -q "Setting the limit of autocorrelation coefficient to 0.3" $TMPFILE
grep -q "Setting the required width of confidence interval to 10% of mean" $TMPFILE
grep -q "Setting the required minimum subsession sample size to 200" $TMPFILE
grep -q "Setting the short round threshold to 20 second(s)" $TMPFILE

# Test overriding both AC and CI
run ./bench run_program --ci-perc 0.12 -a 0.4 --pi "throughput,MB/s,2,1,1" --preset strict -w 10,20 -- true >$TMPFILE 2>&1
grep -q "Preset mode activated: strict" $TMPFILE
grep -q "Setting the limit of autocorrelation coefficient to 0.4" $TMPFILE
grep -q "Setting the required width of confidence interval to 12% of mean" $TMPFILE
grep -q "Setting the required minimum subsession sample size to 200" $TMPFILE
grep -q "Setting the short round threshold to 20 second(s)" $TMPFILE

# Test setting session limit
run ./bench run_program -v --pi "throughput,MB/s,2,1,1" --session-limit 50 -- true 2>&1 | grep -q "Setting session limit to 50 seconds"
run ./bench run_program -v --pi "throughput,MB/s,2,1,1" --session-limit -1 -- true 2>&1 | grep -q "<fatal> Session limit must be greater than 0, exiting..."

# Test other options
run ./bench run_program -v --pi "throughput,MB/s,2,1,1" -- true 2>&1 | grep -q "PI\[0\] name: throughput, unit: MB/s, reading must satisfy: yes, mean method: harmonic"

run ./bench run_program -v --pi "throughput,MB/s,2,1:latency,ms,3,0:threads,,4,0" -- true 2>&1 | grep -q "Error: at least one PI needs to have must_satisfy set."

run ./bench run_program -v --pi "throughput,MB/s,2,1,0:latency,ms,3,0:threads,,4,0" -- true 2>&1 | grep -q "Error: at least one PI needs to have must_satisfy set."

run ./bench run_program -v --pi "throughput,MB/s,2,1:latency,ms,3,0,0:threads,,4,0" -- true 2>&1 | grep -q "Error: at least one PI needs to have must_satisfy set."

# Try to run a workload that accepts work amount from 100 to 200.
# "-d 1" designates that the first column is the round duration.
rm -f /tmp/work_amount_log.txt
run ./bench run_program -w "100,200" -d 1 -- ./mock_benchmark_with_work_amount.sh %WORK_AMOUNT% /tmp/work_amount_log.txt >/tmp/${BASENAME}.out 2>&1
diff unit_test_pilot_opt_parsing_expected_work_amount.log /tmp/work_amount_log.txt

