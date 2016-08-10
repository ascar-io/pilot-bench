=================
Comparing Results
=================

The need to compare benchmark results using the shortest possible time
is our very first motivation for designing Pilot. Not only useful for
system design and tuning, a handy tool for comparing benchmark results
can help many tasks in systems research, such as choosing the fastest
data structure for storing certain data, finding performance
regressions in software development, and deciding the best parameters
for storage or network communication. Without using the correct
statistics method at runtime, most of these decisions were done in an
ad hoc manner, either prematurely with too little data, or blindly
wasting time to gather more than necessary data.

Suppose we have :math:`n` workloads and the results of them are
comparable, which means that they are using the same unit of
measurement and are of the same scale. Now we need to rank (order)
these workloads according to their results. In some cases we need to
run these workloads to get new results, in other cases the comparison
involves old benchmark results. For old results we need three values:
the mean, subsession sample size, and subsession variance. The
subsession results (see
:doc:`autocorrelation-detection-and-mitigation`) are needed because
i.i.d. is a hard requirement for all the analyses we use in this
section. For the rest of this section all samples are subsession
samples that have low autocorrelation.

There are two cases when we consider comparing two benchmark
results. The first case is when their CIs are not overlapped. In this
case we can be sure that one is greater than the other at the
confidence level used to calculate the CIs. The second is when the CIs
overlap. In this case we use the Welch's unequal variance *t*-test
[welch:biometrika47]_ (an adaptation of Student's *t*-test) to compare
the benchmark results, A and B. Welch's *t*-test is more reliable when
the two samples have unequal variance and unequal sample sizes, which
are true for most system benchmarks. This test can effectively tell us
the probability of rejecting a hypothesis and the required sample
size. Here the null hypothesis (the hypothesis we want to reject) is
that there is no statistical significant difference between result A
and B (A = B). We compute the probability (*p*-value) of getting
results A and B if the null hypothesis is true. Let
:math:`\overline{x}` be the mean of the result, :math:`\sigma^2` be
the variance, and :math:`n` be the sample size. We can calculate the
*p*-value:

.. math::

    t &= \frac{\overline{x_A} - \overline{x_B}}{\sqrt{\frac{\sigma_A^2}{n_A} + \frac{\sigma_B^2}{n_B}}} \\
    \nu &= \left\lfloor
      \frac{\left( \frac{\sigma_A^2}{n_A} + \frac{\sigma_B^2}{n_B} \right)^2 }{ \frac{\sigma_A^4 }{ n_A^2 (n_A - 1)} + \frac{\sigma_B^4 }{ n_B^2 (n_B - 1) } }
      \right\rfloor \\
    p &= 2 \cdf(t, \nu)

Above is the Welch-Satterthwaite equation for calculating the degree
of freedom (:math:`\nu`), and :math:`\cdf(t,n)` is the Student's
*t*-distribution with :math:`\nu` degrees of freedom. We multiply it
by 2 to calculate the two-tailed distribution. Pilot uses the first
equation to calculate the optimal subsession sample size.

The comparing result algorithm runs until:

* There are enough data for calculating the CIs,
* Each adjacent CI pair is either non-overlap or their *p*-value of
  the null hypothesis (A=B) is less than the predefined threshold
  (usually 0.01),
* Every CI is tighter than the required width (this step is optional
  but recommended because a narrower CI makes it easier to compare
  with new results in the future).

Pilot needs to decide the work amount for running each round of the workload. The value has to be chosen in such a way that we can minimize the number of rounds needed to reduce the impact of the overhead of starting a round.

.. [welch:biometrika47] B. L. Welch. The generalization of Student's
                        problem when several different population
                        variances are involved. *Biometrika*,
                        34(1-2):28–35, 1947.
