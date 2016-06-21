# INTRODUCTION

Pilot is a tool and a library (libpilot) that help you run benchmarks
to get scientifically accurate and meaningful results. Specifically, it
provides the following features:

*  deciding the optimal length for running a benchmark session so that the results can be bounded by a certain confidence interval,
*  handling the correlation between test runs,
*  saving test results in a standard format for sharing and comparing results.

Tutorials and more information can be found on the wiki: https://github.com/ascar-io/pilot-tool/wiki

Join our Slack to give feedback and dicsuss your ideas:
https://pilot-tool.slack.com/signup

# INSTALL

See https://github.com/ascar-io/pilot-tool/wiki/Requirements-and-Installation-Instructions for details.

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

# ACKNOWLEDGMENTS

This is a research project from the [Storage Systems Research
Center](http://www.ssrc.ucsc.edu/) in [UC Santa
Cruz](http://ucsc.edu).  This research was supported in part by the
National Science Foundation under awards IIP-1266400, CCF-1219163,
CNS-1018928, the Department of Energy under award
DE-FC02-10ER26017/DESC0005417, Symantec Graduate Fellowship, and
industrial members of the Center for Research in Storage Systems. We
would like to thank the sponsors of the Storage Systems Research
Center (SSRC), including Avago Technologies, Center for Information
Technology Research in the Interest of Society (CITRIS of UC Santa
Cruz), Department of Energy/Office of Science, EMC, Hewlett Packard
Laboratories, Intel Corporation, National Science Foundation, NetApp,
Sandisk, Seagate Technology, Symantec, and Toshiba for their generous
support.

This project does not reflect the opinon or endorsement of the sponsors
listed above.
