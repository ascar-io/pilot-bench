#!/bin/bash
# Mock benchmark for testing the binomial proportion function of Pilot CLI.
# This script displays one mock benchmark result on each run. Progress is
# stored at /tmp/pilot_mock_benchmark_round.txt.
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
DATA=(1 0 0 0 1 1 1 0 1 0 0 0 0 0 1 1 1 1 1 1)

ROUND_FILE=/tmp/pilot_mock_benchmark_binomial_proportion_round.txt

if [ -f $ROUND_FILE ]; then
    ROUND=`cat $ROUND_FILE`
else
    ROUND=0
fi
if [ $ROUND -ge ${#DATA[@]} ]; then
    rm "$ROUND_FILE"
    exit 1
fi

COLA=${DATA[$ROUND]}
COLB=`echo $COLA + 1 | bc`
echo $COLA,$COLB

ROUND=`expr $ROUND + 1`
echo $ROUND >$ROUND_FILE
