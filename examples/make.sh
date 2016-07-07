#!/bin/bash
# Make scripts for Pilot examples
#
# Copyright (c) 2015, 2016, University of California, Santa Cruz, CA, USA.
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
cd `dirname $0`

LIB_ARG=()
HEADER_ARG=()
# Check if libpilot is installed
if ld -lpilot 2>&1 | grep -q "library not found"; then
    echo "libpilot is not installed, checking if we can find it in the source tree..."
    if ld -L../build/lib -lpilot 2>&1 | grep -q "library not found"; then
        echo "Cannot find libpilot in the source tree. Please install a Pilot package or compile Pilot first."
        exit 3
    else
        LIB_ARG+=(-L../build/lib)
        HEADER_ARG+=(-I../include -I../lib/interface_include -I../build)
    fi
fi

# Check if the header files can be found
if echo "#include <pilot/libpilot.h>"  | cc -E ${HEADER_ARG[@]} - 2>&1 | grep "file not found"; then
    echo "One or more header file cannot be found. Please install a Pilot package or compile Pilot first."
    exit 3
fi
echo "Passed header files and library check."

c++ ${HEADER_ARG[@]} ${LIB_ARG[@]} -g -O2 -o benchmark_hash benchmark_hash.cc -lpilot

echo "All examples built successfully."
