# INTRODUCTION

Pilot is a tool and a library (libpilot) that help you run benchmarks
to get scientifically accurate and meaningful results. Specifically, it
provides the following features:

*  deciding the optimal length for running a benchmark session so that the results can be bounded by a certain confidence interval,
*  handling the correlation between test runs,
*  saving test results in a standard format for sharing and comparing results.

Join our Slack to give feedback and dicsuss your ideas:
https://pilot-tool.slack.com/signup

# INSTALL

## Supported OSes

We plan to support all flavors of UNIX/Linux as the first stage, and
hopefully Windows and other embedded systems in the future.

We have tested it on the following operating systems:

1. Mac OS X Version 10.11 El Capitan
2. Ubuntu 15.10 x86-64

It should work on any modern *nix systems that have a decent C++
compiler. Let us know if you meet any problems during the build process.

## Requirements

Pilot requires a modern C++ compiler, CMake, the boost C++ library, and graphviz (for building the documentation).

It also uses gtest (Google Test) for running test cases, but can install gtest automatically when you run ``make'' for the first time.

Additionally, if you want to plot the results from some sample
programs, you will need Python 2.* and matplotlib.

## Steps

Install the dependencies as listed above. For Ubuntu, install the following packages:

- sudo apt-get install git cmake libboost-all-dev g++ graphviz

For Mac, make sure you have the latest Xcode and command line development tools installed. You can install [MacPort](https://www.macports.org/) then install the following packages:

- sudo port install cmake boost graphviz

Then you can build pilot-tool:

1. make build
2. cd build
3. cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
4. make
5. make test (run all the test cases)

You can change CMAKE_BUILD_TYPE to Debug if you need to disable the
compiler optimization for debugging purposes.

# RUN IT

You can find a sample program at build/lib/func_test_seq_write. It is
a sample workload that does sequential I/O to an output file you
specify.  It uses libpilot to scientifically measure the sequential
write I/O throughput. Run "func_test_seq_write" without an option to
get the help. You can plot its result csv to a line graph by using
"lib/test/plot_seq_write_throughput.py". Make sure you have Python 2.*
and matplotlib installed correctly.

# DEVELOPMENT

We partially follow Google's C++ coding standard:
https://google-styleguide.googlecode.com/svn/trunk/cppguide.html . The
exception is we use four spaces for indentation (Google's style uses
two spaces).

Don't use logging functions in test cases, because we should keep each
test cases as standalone as possible so that they can also be used as
samples.
