#!/bin/bash
export CFLAGS="$CFLAGS -fPIC -O3 -fvisibility=hidden"
export CXXFLAGS="$CFLAGS"
"$1"
