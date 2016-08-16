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

# Test invalid options
./bench analyze -f -1 unit_test_analyze_input.csv &>$TMPFILE || :
grep -q "the argument ('-1') for option '--field' is invalid" "$TMPFILE"
./bench analyze -m -1 unit_test_analyze_input.csv &>$TMPFILE || :
grep -q "the argument ('-1') for option '--mean-method' is invalid" "$TMPFILE"
./bench analyze -m 2 unit_test_analyze_input.csv &>$TMPFILE || :
grep -q "the argument ('2') for option '--mean-method' is invalid" "$TMPFILE"
./bench analyze -m 2 unit_test_analyze_input.csv &>$TMPFILE || :
grep -q "the argument ('2') for option '--mean-method' is invalid" "$TMPFILE"
./bench analyze -i -99 unit_test_analyze_input.csv &>$TMPFILE || :
grep -q "the argument ('-99') for option '--ignore-lines' is invalid" "$TMPFILE"
./bench analyze --ac -2 unit_test_analyze_input.csv &>$TMPFILE || :
egrep -q "the argument \('-2.*'\) for option '--ac' is invalid" "$TMPFILE"
./bench analyze --ac 0 unit_test_analyze_input.csv &>$TMPFILE || :
egrep -q "the argument \('0.*'\) for option '--ac' is invalid" "$TMPFILE"
# The following doesn't pass on clang
#./bench analyze -f -1 unit_test_analyze_input.csv - &>$TMPFILE
#grep -q "Invalid options." "$TMPFILE"

# Non-existing input file
./bench analyze NONEXISTING_FILE -f 1 &>$TMPFILE || :
grep -q 'I/O error (2): No such file or directory' "$TMPFILE"

check_file ()
{
    grep -q "sample_size 48" "$TMPFILE"
    grep -q "mean 1.756458" "$TMPFILE"
    grep -q "CI 0.157416" "$TMPFILE"
    grep -q "variance 0.073474" "$TMPFILE"
    grep -q "optimal_subsession_size 1" "$TMPFILE"
}
cat unit_test_analyze_input.csv | ./bench analyze - >$TMPFILE
check_file
cat unit_test_analyze_input_2col.csv | ./bench analyze -f 1 - >$TMPFILE
check_file
cat unit_test_analyze_input_3col.csv | ./bench analyze -f 1 - >$TMPFILE
check_file

# Test wrong field
./bench analyze -f 3 unit_test_analyze_input_2col.csv &>$TMPFILE || :
grep -q '<fatal> Failed to extract a float number from from field 3 in line 1, malformed data? Aborting. Line data: "1,1.21"' "$TMPFILE"

cat unit_test_analyze_input.csv | ./bench analyze --preset strict - >$TMPFILE
grep -q "sample_size 48" "$TMPFILE"
grep -q "mean 1.756458" "$TMPFILE"
grep -q "CI 0.291571" "$TMPFILE"
grep -q "variance 0.052647" "$TMPFILE"
grep -q "optimal_subsession_size 4" "$TMPFILE"
grep -q "subsession_autocorrelation_coefficient 0.082310" "$TMPFILE"

./bench analyze -m 1 unit_test_analyze_input.csv >$TMPFILE
grep -q "sample_size 48" "$TMPFILE"
grep -q "mean 1.713913" "$TMPFILE"
grep -q "CI 0.159384" "$TMPFILE"
grep -q "variance 0.075323" "$TMPFILE"
grep -q "optimal_subsession_size 1" "$TMPFILE"
grep -q "subsession_autocorrelation_coefficient 0.646682" "$TMPFILE"

# Test auto header skipping
./bench analyze unit_test_analyze_input_3col_with_malformed_header.csv -f 1 >$TMPFILE || :
grep -q '<fatal> Failed to extract a float number from from field 1 in line 2, malformed data? Aborting. Line data: "ID,Data,Some other data"' $TMPFILE
./bench analyze unit_test_analyze_input_3col_with_header.csv -f 1 >$TMPFILE
grep -q '<warning> Ignoring first line in input. It might be a header. Use `-i 1` to suppress this warning. Line data: "ID,Data,Some other data"' $TMPFILE
check_file
./bench analyze unit_test_analyze_input_3col_with_header.csv -f 1 -i 1 >$TMPFILE
grep -q '<warning> Ignoring first line in input. It might be a header. Use `-i 1` to suppress this warning. Line data: "ID,Data,Some other data"' $TMPFILE && false
check_file
