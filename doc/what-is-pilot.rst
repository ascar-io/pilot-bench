.. include:: global-ref.rst

What is Pilot?
==============

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

To learn how to use Pilot, please head to the :doc:`tutorial-list`.

Pilot is written in C++ for doing fast in place analysis. It is
released in dual BSD 3-clause and GPLv2+ license. Currently it
compiles on major Linux distros and Mac OS X.

Our recommended channel for support is Slack:
http://ascar-slack-signup.stamplayapp.com/. You can also use the
following mailing lists:

* pilot-news for announcements: https://groups.google.com/forum/#!forum/pilot-news
* pilot-users for discussion: https://groups.google.com/forum/#!forum/pilot-users


Why Should I Try Pilot?
-----------------------

Everyone needs to do performance measurement from time to time:
home users need to know their Internet speed, an engineer may need to
figure out the bottleneck of a system, a researcher may need to
calculate the performance improvement of a new algorithm. Yet not
everyone has received rigorous training in statistics and computer
performance evaluation. That is one of the re why we can find
many incomplete or irreplicable benchmark results, even in
peer-reviewed scientific publications. A widely reported study
published in *Science* [osc:science15]_ found that 60% of
the psychology experiments could not be replicated. Computer science
cannot afford to be complacent. Torsten Hoefle and Roberto Belli analyzed 95
papers from HPC-related conferences and discovered that most papers
are flawed in experimental design or analyses [hoefler:sc15]_.

Basically, if any published number is derived from fewer than 20
samples, or is presented as a mean without variance or confidence
interval (CI), the authors are likely doing it wrong. The following
problems can often be found in published results:

* **Imprecise**: the result may not be a good approximation of the
  "real" value; often caused by failing to consider the width of CI
  and not collecting enough samples.

* **Inaccurate**: the result may not reflect what you need to measure;
  often caused by hidden bottleneck in the system.

* **Ignoring the overhead**: not measuring or documenting the
  measurement and instrumentation overhead.

* **Presenting improvements or comparisons without providing the
  p-value**: this makes it impossible to know how reliable the
  improvements are.

Time is another limiting factor. People usually do not allocate a lot
of time to performance evaluation or tuning, yet few newly designed or
deployed systems can meet the expected performance without a length
tuning process. A large part of the tuning process is spent on running
customer's benchmark workloads for model construction or testing
candidate parameters. Designers are usually given only a few days for
these tasks and have to rush the results by cutting corners, which
often leads to unreliable benchmark results and sub-optimal system
tuning decisions.

We want to improve this process by getting statistical valid results
in a short time. We begin with a high level overview of what
analytical methods are necessary to generate results that meet the
statistical requirements, then design heuristic methods to automate
and accelerate them. We cover methods to measure and correct the
autocorrelation of samples, to use *t*-distribution to estimate the
optimal test duration from existing samples, to detect the duration of
the warm-up and cool-down phases, to detect shifts of mean in samples,
and to use *t*-test to estimate the optimal test duration for
comparing benchmark results. In addition to these heuristics, we also
propose two new algorithms: a simple and easy-to-use linear
performance model that makes allowance for warm-up and cool-down
phases using only total work amount and workload duration, and a
method to detect and measure the system's performance bottleneck while
keeping the overhead within an acceptable range.

To encourage people to use these methods, we implement them in Pilot,
which can automate many performance evaluation tasks based on
real-time time series analytical results, and can help people who have
insufficient statistics knowledge to get good performance
results. Pilot can also free experienced researchers from
painstakingly executing benchmarks so they can get scientifically
correct results using the shortest possible time.

.. [hoefler:sc15] Torsten Hoefle and Roberto Belli. Scientific
                  benchmarking of parallel computing systems. In
                  *Proceedings of the 2015 International Conference
                  for High Performance Computing, Networking, Storage
                  and Analysis (SC15)*, 2015.

.. [osc:science15] Open Science Collaboration. Estimating the
                   reproducibility of psychological
                   science. *Science*, 349(6251), 2015.

