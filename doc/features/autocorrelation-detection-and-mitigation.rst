========================================
Autocorrelation Detection and Mitigation
========================================

A benchmark session needs to be long enough so that we can collect
enough samples to calculate the CI at the desired confidence
level. The more samples we have, the narrower the CI can be
made. However, a crucial issue that is often overlooked in many
published benchmark results is the autocorrelation among
samples. Autocorrelation is the cross-correlation of a sequence of
measurements with itself at different points in time. Conceptually, a
high autocorrelation means that previous data points can be used to
predict future data points, and that would invalid the calculation of
CI no matter how large the sample size is. Most measurements in
computer systems are autocorrelated because of the stateful nature of
computer systems. For instance, most computer systems have one or more
schedulers, which allocate time slice to jobs. The measured
performance of such jobs would be highly correlated when they are
taken within a single time slice, and would change significantly
between time slices if the duration of a measurement unit is not
significantly longer than the size of a time slice. The
autocorrelation in the samples must be properly handled before we can
go on to the next step to calculate the sample's CI.

Autocorrelation is measured by the autocorrelation coefficient of a
sequence, which is calculated as the covariance between measurements
from the same sequence as

.. math::

    R(\tau) = \frac{\operatorname{E}[(X_t - \mu)(X_{t+\tau} - \mu)]}{\sigma^2},

where :math:`\tau` is the time-lag. The autocorrelation coefficient is
a number in range :math:`[-1,1]`, where :math:`-1` means the sample
data are reversely correlated and :math:`1` means the data is
autocorrelated. In statistics, :math:`[-0.1, 0.1]` is deemed to be a
valid range for declaring the sample data has negligible
autocorrelation [ferrari:78]_.

Subsession analysis [ferrari:78]_ is a statistical method for handling
autocorrelation in sample data. :math:`n`-subsession analysis models
the test data and combines every :math:`n` samples into a new
sample. Pilot calculates the autocorrelation coefficient of
measurement data after performing data sanitizing, such as non-stable
phases removal, and gradually increases $n$ until the autocorrelation
coefficient is reduced to within the desired range.

.. [ferrari:78] Domenic Ferrari. *Computer Systems Performance
                Evaluation*. Prentice-Hall, 1978.

