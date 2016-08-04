===============================
Deciding Optimal Session Length
===============================

On a high level, a benchmark session comprises many rounds. We
calculate the CI after collecting new data from each round. The
session ends when the CIs of all PIs reach the target. Because each
round can have non-stable phases that are not contributing samples, we
should maximize the length of each round and minimize the number of
rounds. This also has the extra benefit of including those work units
that are far from the beginning of the initial work amount.

But we cannot begin the first round using the maximum work amount,
because in many cases the maximum work amount can be very large. This
is typical in storage benchmarks where the limit can be the total size
of a storage device, and it would take several dozens of hours to
finish one round that fully writes a device. In network benchmarks, we
can even set the maximum work amount to unlimited because these
workloads can keep running forever. If we start the first round of
workload with the full work amount, we risk letting the user wait too
long a time before showing the first result. Therefore, we begin the
benchmark session with a few short trial rounds to learn the duration
to work unit ratio.

Workloads that provide unit readings
------------------------------------

We treat each unit reading as one measurement. One workload round can
usually provide hundreds or thousands of measurements, making it
faster to reach the required sample size. For instance, if a
sequential write workload writes 500 MB data using 1 MB I/O, we can
get 500 throughput measurements if the workload saves the duration of
each write. Pilot sends the unit reading results through the
non-stable phases removal, performs auto-correlation reduction, and
uses the rest of the unit readings to calculate the CI. Pilot keeps
running new rounds of the workload until the desired width of the CI
is reached.

Workloads that cannot provide unit reading
------------------------------------------

These workloads are handled using the WPS method (see
:ref:`sec_wps_method`), which also performs non-stable phases
detection and removal, subsession analysis, and decides the optimal
number of rounds to achieve the desired width of CI.
