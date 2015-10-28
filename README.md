# INTRODUCTION

Pilot is a tool and a library (libpilot) that help you run benchmarks
to get scientifically accurate and meaningful results.

# INSTALL

## Supported OSes

We plan to support all flavors of UNIX/Linux as the first stage, and
hopefully Windows and other embedded systems in the future.

We have tested it on the following operating systems:
1. Mac OS X Version 10.11 El Capitan
2. Ubuntu 15.10 amd64

## Requirements

Pilot requires a modern C++ compiler, CMake, and the boost C++
library.

## Steps

1. make build
2. cd build
3. cmake ..
4. make test (run all the test cases)

# DEVELOPMENT

We partially follow Google's C++ coding standard:
https://google-styleguide.googlecode.com/svn/trunk/cppguide.html . The
exception is we use four spaces for indentation (Google's style uses
two spaces).

Don't use logging functions in test cases, because we should keep each
test cases as standalone as possible so that they can also be used as
samples.
