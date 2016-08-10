.. include:: ../global-ref.rst

Statistics 101
==============

Well, not really. This is not meant to be a course that covers that
basics of statistics. Instead we want to provide just enough
information on statistics that are essential to understanding and
using Pilot. We strive to keep it simple and also provide links to
reference material if you feel like to know more on a specific topic.

Someone said that the hardest part of science is explaining a complex
idea in simple and easy-to-understanding words. If you can help to
improve this documentation, please fork it, make the change, and send
us a pull request.

Performance measurement and benchmarking
----------------------------------------

**Performance measurement** is concerned with how to measure the
performance of a running program or system, while *benchmarking*
is more about issuing a certain workload on the system in order to
measure the performance. High quality performance measurement and
benchmarking are very important for almost everyone who uses an
electronic or computer system, from researchers to consumers. For
instance, performance evaluation is critical for computer science
researchers to understand the performance of newly designed
algorithms. System designers run benchmarks and measure application
performance before design decisions can be made. System administrators
need performance data to find the most suitable products to procure,
to detect hardware and software configuration problems, and to verify
if the performance meets the requirements of applications or
Service-Level Agreement.

Performance measurement and benchmarking are similar but not
identical. We usually can control how to run benchmarks, but
measurement often needs to be done on applications or systems over
which we have little control. Our following discussion applies to both
benchmarking and measurement in general, and we use these two terms
interchangeably unless otherwise stated.

Benchmarks must meet several requirements to be useful. The
measurement results must reflect the performance property one plans to
measure (accuracy). The measurement must be a good approximation of
the real value with quantified error (precision). Multiple runs of the
same benchmark under the same condition should generate reasonably
similar results (repeatability). The overhead must be measured and
documented. If the results are meant for publication, they also must
include enough hardware and software information so that other people
can compare the results from a similar environment (comparability) or
replicate the result (replicability).

People usually use these terms, especially accuracy and precision, to
mean many different things. Here we give our definition of these
terms. It is assumed that the readers have basic statistics knowledge
and understand basic concepts such as mean, variance, and sample
size. Domenic Ferrari wrote a good reference book
for readers who want to know more about these statistical
concepts [boudec:performance10]_.

**Accuracy** reflects whether the results actually measure what the
user wants to measure. A benchmark usually involves many components of
the system. When we need to measure a certain property of the system,
such as I/O bandwidth, the benchmark needs to be designed in such a
way that no other components, such as CPU or RAM, are limiting the
measured performance. This requires carefully designing the
experiment~\cite{harris:nicta16}, measuring the usage of related
components while the benchmark is running, and checking which
component is limiting the overall performance.

**Precision** is related to accuracy but is a different
concept. Precision is the difference between the measured value and
the real value the user needs to measure. In statistical terms,
precision is the difference between a sample parameter and its
corresponding population parameter. Precision can be described by
confidence interval (CI). The CI of a sample parameter describes the
range of possible population parameter at certain likelihood. For
instance, if the CI of a throughput mean (:math:`\mu`) is :math:`C` at
the 95% confidence level, we know that there is a 95% chance that the
real system's throughput is within interval :math:`[\mu - \frac{C}{2},
\mu + \frac{C}{2}]`. In practice, CIs are typically stated at the 95%
confidence level. We can see that the common practice of presenting a
certain performance parameter using only one number, such as saying
the write performance of a disk drive is 100 MB/s, is misleading.

**Repeatability** is critical to a valid performance measurement
because the goal of most performance benchmark is to predict the
performance of future workloads, which exactly means that we want the
measurement results to be repeatable. In addition to the accuracy and
precision, errors in the measurement can have a negative impact on
repeatability. There are two kinds of errors: the systematic error and
random error. Systematic error means that the user is not measuring
what he or she plans to measure. For instance, the method of
benchmarking is wrong or the system has some hidden bottleneck that
prevents the property to be measured from reaching its maximum
value. In these cases, even though the results may look correct, it
may well be not repeatable in another environment. Random errors are
affected by "noise" outside our control, and can result in
non-repeatable measurements if the sample size is not large enough or
samples are not independent and identically distributed (`i.i.d.`_).

We will discuss how to achieve these properties in the following
sections, and one can see that scientifically performing a benchmark
demands significant knowledge of the computer system and
statistics. We cannot expect all administrators, engineers, and
consumers to have received rigorous training in both computer science
and statistics. It is not news that many vendors publish misleading,
if not utterly wrong, benchmark results to promote their
products. Many peer-reviewed research publications also suffer from
poor understanding or execution of performance
measurement [hoefler:sc15]_.

Getting results fast
--------------------

The old wisdom for running benchmark is to run it for as long as one
can tolerate and hope the law of large numbers can win over all
errors. This method is no longer suitable for today's fast changing
world, where we simply no longer have a lot of time to run
benchmark. We have heard field support engineers complaining (private
conversation with an engineer from a major HPC provider) that they are
usually only given one or two days after the installation of a new
computer cluster or distributed storage system to prove that the
system can deliver whatever performance promised by the salesperson,
very often using the customer's own benchmark programs. Modern
distributed systems can have hundreds, if not thousands, of parameters
to tune, and the performance engineer needs to run an unfamiliar
benchmark repeatedly and try different parameters. Apparently the
shorter the benchmark is the more parameters can be tested, thus
resulting in better system tuning results.

Existing analytical software packages, such as `R
<https://www.r-project.org/>`_, are either too big or slow for
run-time analysis, are hard to integrate with applications (for
instance, R is a large GPL-licensed software package), or require the
user to write complex scripts to use all its functions.

In all, we realize that we need an easy-to-use software tool that can
guide the user and help to automate most of the analytical tasks of
computer performance evaluation. We are not introducing new
statistical method in this paper. Instead we focus on two tasks:
finding the most suitable and practical methods for computer
performance evaluation, and design heuristics methods to automate and
accelerate them.

.. [boudec:performance10] Domenic Ferrari. Computer Systems
                          Performance Evaluation. Prentice-Hall, 1978.

.. [hoefler:sc15] Torsten Hoefle and Roberto Belli. Scientific
                  benchmarking of parallel computing systems. In
                  *Proceedings of the 2015 International Conference
                  for High Performance Computing, Networking, Storage
                  and Analysis (SC15)*, 2015.
