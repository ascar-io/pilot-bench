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

To learn how to use Pilot or if you want to refresh the statistics,
please read the documentation: https://docs.ascar.io/

Pilot is written in C++ for doing fast in place analysis. It is
released in dual BSD 3-clause and GPLv2+ license.

Our recommended channel for support is Slack:
https://join.slack.com/t/tuneupai/shared_invite/enQtMzE4MDAzNTUzODQwLTRhMjM2MmQ0MzViYjc2NThkZTNkZTQzZjhmZWUzYzI1MjU2MzNjMTE5MmJjZjY0MjU5YTk4NDVhMTc0NWJhMmY. You can also use the
following mailing lists:

* pilot-news for announcements: https://groups.google.com/forum/#!forum/pilot-news
* pilot-users for discussion: https://groups.google.com/forum/#!forum/pilot-users

# INSTALL

Plese see: https://docs.ascar.io/install.html

# RUN IT

Please see the tutorials: https://docs.ascar.io/tutorial-list.html

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
grant from Intel Corporation, and by industrial members of the [Center
for Research in Storage Systems](http://www.crss.ucsc.edu/).  Any
opinions, findings, and conclusions or recommendations expressed in
this material are those of the author(s) and do not necessarily
reflect the views of the sponsors.
