#!/bin/bash
# Unit test for Pilot CLI tool run benchmark with binomial proportion inputs
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
set -e -u

TMPFILE=`mktemp`
rm -f /tmp/pilot_mock_benchmark_binomial_proportion_round.txt
# We use 0.9 as CI required percent because our sample size is small
./bench run_program --ci-perc 0.9 --min-sample-size 10 --pi "success rate,,0,2,1" \
    -- ./mock_benchmark_binomial_proportion.sh >"$TMPFILE" 2>&1

grep -q "success rate: R m0.55 c0.4657 v0.2605" "$TMPFILE"
grep -q "\[PI 0\] Reading mean: 0.55" "$TMPFILE"
grep -q "\[PI 0\] Reading CI: 0.4657" "$TMPFILE"
grep -q "\[PI 0\] Reading variance: 0.2605" "$TMPFILE"
grep -q "\[PI 0\] Reading optimal subsession size: 1" "$TMPFILE"

# test quiet mode
rm -f /tmp/pilot_mock_benchmark_binomial_proportion_round.txt
OUTPUT_DIR=`mktemp -d -u`
./bench run_program --ci-perc 0.9 --min-sample-size 10 --pi "success rate,,0,2,1" \
    --quiet -o ${OUTPUT_DIR} \
    -- ./mock_benchmark_binomial_proportion.sh >"$TMPFILE" 2>&1
# We can't directly compare the output with an expected file, because the
# session_duration on the first line will always be different
grep -q "0,0.55,0.465668,0.260526,0,0.55,0.465668,0.260526," "$TMPFILE"
# even we are running in quiet mode log should still contain <debug> information
grep -q '<debug>' "${OUTPUT_DIR}/session_log.txt"
# check correctness of saved results
grep -q "0,20,0.55,0.55,0.260526,0.260526,0.465668,0.465668,0" "${OUTPUT_DIR}/pi_results.csv"

# TODO: add more checks here
