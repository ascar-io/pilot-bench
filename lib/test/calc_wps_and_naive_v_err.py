#!/usr/bin/env python
#
# Plot throughput chart from the output of func_test_seq_write
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

import numpy as np
import sys

if len(sys.argv) < 3:
    print('Usage {0} wps_csv_file round_csv_file [naive_v]'.format(sys.argv[0]))

ONE_SECOND = 1e9

wps_csv = sys.argv[1]
csv_data = np.genfromtxt(wps_csv, names=True, delimiter=',')

wps_harmonic_mean = csv_data['wps_harmonic_mean']
wps_alpha = csv_data['wps_alpha']
wps_v = csv_data['wps_v']
if len(sys.argv) >= 4:
    naive_v = float(sys.argv[3])
else:
    naive_v = wps_harmonic_mean
print('''Imported analysis results:
naive_v: {0}
wps_alpha: {1}
wps_v: {2}
'''.format(naive_v, wps_alpha, wps_v))

round_csv = sys.argv[2]
csv_data = np.genfromtxt(round_csv, names=True, delimiter=',')

sum_native_t_err = 0
sum_wps_t_err = 0
sum_duration = 0
sum_work_amount = 0
for row in csv_data:
    round_work_amount = row['work_amount']
    round_duration = row['round_duration']
    sum_work_amount += round_work_amount
    sum_duration += round_duration

    naive_t = round_work_amount / naive_v
    naive_t_err = abs(naive_t - (round_duration / ONE_SECOND)) * ONE_SECOND
    sum_native_t_err += naive_t_err

    wps_t = wps_alpha + round_work_amount / wps_v
    wps_t_err = abs(wps_t - round_duration)
    sum_wps_t_err += wps_t_err

print('mean calculated from round data: {0}'.format(sum_work_amount / sum_duration))
print('sum_native_t_err: {0}, {1}%'.format(sum_native_t_err, sum_native_t_err * 100 / sum_duration))
print('sum_wps_t_err: {0}, {1}%'.format(sum_wps_t_err, sum_wps_t_err * 100 / sum_duration))
