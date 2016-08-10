# INTRODUCTION

The Pilot Benchmark Framework provides a tool (bench) and a library
(libpilot) to automate computer performance measurement tasks. This is
a project under active development. The project is designed to answer
questions like these that rise often from performance measurement:

*  How long shall I run this benchmark to get a precise result?
*  Is this new scheduler really 3% faster than our old one or is that a
   measurement error?
*  Which setting is faster for my database, 20 worker threads or 25 threads?

Our design goals include:

*  Be as intelligent as possible so users do not have to go through rigorous
   statistics or computer science training.
*  Results must be statistical valid (accurate, precise, and repeatable).
*  Using the shortest possible time.

To achieve these goals Pilot has the following functions:

*  Automatically deciding what statistical analysis method is suitable based on
   system configuration, measuring requirements, and existing results.
*  Deciding the shortest length for running a benchmark to achieve the desired
   measurement goal such as confidence interval.
*  Save the test results in a standard format for sharing and comparing results.
*  And much more...

To learn how to use Pilot, read the tutorials on the wiki:
https://github.com/ascar-io/pilot-tool/wiki

Pilot is written in C++ for doing fast in place analysis. It is
released in dual BSD 3-clause and GPLv2+ license. Currently it should
compile on major Linux distros and Mac OS X.

Join our Slack to give feedback and dicsuss your ideas:
https://pilot-tool.slack.com/signup

# INSTALL

Pilot is currently in the alpha test stage. Binary packages can be
found at the [Releases
page](https://github.com/ascar-io/pilot-bench/releases). You can also
[compile from
source](https://github.com/ascar-io/pilot-tool/wiki/Requirements-and-Installation-Instructions).

# RUN IT

We are working on the documentation, but at the same time, take a look
at the [wiki](https://github.com/ascar-io/pilot-tool/wiki) and feel
free to edit or write a new tutorial if you find Pilot useful for your
project.

Measuring the performance of a C function is [super
easy](https://github.com/ascar-io/pilot-bench/wiki/Performance-measurement-of-a-C-function). The
C macro interface is simple to use, but if you want to learn about all
the analytical power of Pilot, take a look at the sample program
[build/lib/func_test_seq_write](https://github.com/ascar-io/pilot-bench/blob/master/lib/test/func_test_seq_write.cc). It
is a sample workload that does sequential I/O to an output file you
specify.  It uses libpilot to scientifically measure the sequential
write I/O throughput. Run "func_test_seq_write" (you can find it in
build/lib when you build from source; we will include it in our next
binary release) without an option to get the help. You can plot its
result csv to a line graph by using
"lib/test/plot_seq_write_throughput.py". Make sure you have Python 2.*
and matplotlib installed correctly.

# DEVELOPMENT

We partially follow Google's C++ coding standard:
https://google-styleguide.googlecode.com/svn/trunk/cppguide.html . The
exception is we use four spaces for indentation (Google's style uses
two spaces).

# ACKNOWLEDGMENTS

This is a research project from the [Storage Systems Research
Center](http://www.ssrc.ucsc.edu/) in [UC Santa
Cruz](http://ucsc.edu).  This research was supported in part by the
National Science Foundation under awards IIP-1266400, CCF-1219163,
CNS-1018928, CNS-1528179, by the Department of Energy under award
DE-FC02-10ER26017/DESC0005417, by a Symantec Graduate Fellowship, by a
grant from Intel Corporation, and by industrial members of the Center
for Research in Storage Systems.

This project does not reflect the opinon or endorsement of the sponsors
listed above.
