Measuring the duration of running C++ functions
===============================================

Programmers often need to do quick measurements of functions in their programs. Most of them are done in an ad hoc way and produce unreliable and irreproducible results. We can use libpilot to handle all the details so that you can get scientifically and reliable results easily and fast, and you can also use libpilot to compare results.

All tutorials assume that you have already either installed a Pilot package or compiled it successfully from the source. Check [system requirements and installation instructions](Requirements and Installation Instructions) if you haven't done so.

In the following sample we plan to measure the performance of a hash function (courtesy to [Daniel Lemire](http://lemire.me/blog/2015/10/22/faster-hashing-without-effort/) for the hash function code).

## Sample 1: benchmark a hash function
We want to measure the duration of running the following hash function, which hashes a continuous memory region of 1 GB pseudo random data.

```C++
// We use global variables to store the result so the compiler would not
// optimize them away.
const size_t max_len = 1024 * 1024 * 1024;
char buf[max_len];
int hash1;

int hash_func_one(void) {
    hash1 = 1;
    for (size_t i = 0; i < max_len; ++i) {
        hash1 = 31 * hash1 + buf[i];
    }
    // 0 means correct
    return 0;
}
```

Benchmarking this function is quite simple:
```C++
int main() {
    // init buffer with some pseudo random data
    for (size_t i = 0; i != max_len; ++i) {
        buf[i] = (char)(i * 42);
    }
    simple_runner(hash_func_one);
}
```

The only thing you need to do is:

1.  Including ``<pilot/libpilot.h>``.
2.  Passing the function name to ``simple_runner()``, which requires the function to take no parameter and return 0 on success. If your workload function does not match this requirement, just wrap it in a wrapper function.

To compile and run it, run
```bash
g++ -lpilot -o benchmark_hash benchmark_hash.cc
./benchmark_hash
```

Alternatively, just go to the ``example`` directory as shipped as part of the Pilot package or source code, and run ``./make.sh`` to compile all examples.

## What Pilot Does For You

In technical terms, Pilot's ``simple_runner`` runs the benchmark for as many times as needed to narrow down the result confidence interval (CI) to be within 10% of the sample mean. Reading on for explanation.

Pilot wraps the execution of your function with timers to measure the duration. That part is simple. What not simple is that the execution time is a random variable because your computer has billions of transistors and runs billions line of code, also currently there are probably more than a few dozens process running at the same time on your computer.

A simple way to understand a random variable is to measure its [expected value](https://en.wikipedia.org/wiki/Expected_value) (or _mean_ using layman's term) and [variance](https://en.wikipedia.org/wiki/Variance). The expected value of a random variable describes its central tendency and is probably the most widely used way to describe the performance of a computer system. When we say "the function takes 1.5 seconds to run on average", we mean to say that the expected value of the duration of running the function is 1.5 seconds, and we expect future durations of running this function be centered around 1.5 seconds. How far those future run durations would be away from the expected value can be measured by its variance.

However we have no way to directly measure the expected value or variance of a random variable. What we can do is to take several _samples_ of the random variable and calculate the _sample mean_ and _sample variance_. Sample mean and sample variance will be a good approximation of the expected value and variance of the random variable if a series of conditions can be met, among which the two most important ones are:

1.  The samples have to be independent and identically distributed ([_i.i.d._](https://en.wikipedia.org/wiki/Independent_and_identically_distributed_random_variables))
2.  The sample size has to be sufficiently large (at least several hundreds)

There are many steps that you have to carry out to make sure your measurement meet these conditions so that you can know that your measured sample mean is not very far from the real expected value. If you are to use make some decision based on these measurements, like picking a faster algorithm or choosing a faster Internet Service Provider, you wouldn't want to bet your decision on wrongly measured means.

On a high level, Pilot does the following for you when measuring the duration of running this function:

1.  Checke if the samples are i.i.d. by calculating the autocorrelation among samples, and take necessary steps to mitigate the autocorrelation if it is too high.
2.  Calculate the sample mean's [_confidence interval_, or CI](https://en.wikipedia.org/wiki/Confidence_interval), which describes a range within which the expected value will likely fall.
3.  Use student _t_-distribution to calculate how many samples are needed to make the CI tight so that you can know the expected value is how far from the measured sample mean.
4.  Detect and remove the warm-up and cool-down phases.
5.  Run the benchmark just long enough to collect enough samples so that you can get an accurate measurement and does not waste time to collect more than necessary samples.

If you want to know more about these steps, please read the [Pilot paper](https://github.com/ascar-io/pilot-bench/releases/download/v0.1.0/pilot-mascots16-draft-2016-06-20.pdf).

## Interpreting the benchmark output

By default ``simple_runner()`` runs the benchmark workload and prints the log to stdout. You will see the log like the following:

```
2:[2016-07-01 20:40:08] <info> Starting workload round 0 with work_amount 1
3:[2016-07-01 20:40:10] <info> Finished workload round 0
```

Pilot organizes the running of the workload into _rounds_. In each round, Pilot decides how many times to repeat calling the workload function and collects the duration of each function call. The duration of each function call is called a _unit reading_ or _UR_. The times Pilot repeat the function call in a round is called the round's _work amount_. So from the above log message we can see that, in round 0, Pilot tried to run the function call for once (``with work_amount 1``). Pilot does this in order to get an estimation of the possible duration of the function call.

```
4:[2016-07-01 20:40:10] <info> Running changepoint detection on UR data
5:[2016-07-01 20:40:10] <info> Skipping non-stable phases detection because we have fewer than 24 URs in the last round. Ingesting all URs in the last round.
6:[2016-07-01 20:40:10] <info> Ingested 1 URs from last round
7:[2016-07-01 20:40:10] <info> [PI 0] analyzing results
```

Changepoint detection is for detecting the warm-up and cool-down section. They are not useful until we have at least 24 URs in a round (that is to say, repeating the function call for at least 24 times). In round 0, Pilot ingested 1 Unit Reading sample.

```
8:[2016-07-01 20:40:10] <info> 0   | Duration: R has no summary, UR has no summary
```

This line is a summary of the current result. It means in round 0 Pilot does not yet have enough data to calculate the summary for either _Readings_ (_R_) or _Unit Readings_ (_UR_). You can ignore the Readings for now because it is not used in this benchmark. 

```
9:[2016-07-01 20:40:10] <info> Setting adjusted_min_work_amount to 1
```

Pilot detected that the last round's duration is longer than the short round detection threshold. A too short round is bad for benchmark, because it be dominated by overhead instead of the real workload. Pilot has a builtin mechanism to make sure no round is too short.  ``adjusted_min_work_amount`` is the minimum amount of `work_amount` that can make the round no shorter than the threshold.

```
10:[2016-07-01 20:40:10] <info> [PI 0] doesn't have enough information for calculating required sample size
```

Pilot needs about 10 to 20 samples (more in some cases) before it can calculate how many samples are needed to meet the statistical requirement, which defaults to narrowing down the confidence interval to be narrower than 10% of the sample mean.

```
34:[2016-07-01 20:40:21] <info> 3   | Duration: R has no summary, UR m1.423 c0.04202 v0.0007472
```

This line shows that after round 3, Pilot calculated that the UR (duration of running your function) has a sample mean of 1.423 second, with a confidence interval of 0.04202, and variance 0.0007472. Statistically speaking, this means that there is a 95% chance that the expected value falls within m plus-minus c/2. That is to say, [1.423-0.04202/2, 1.423+0.04202/2].

```
35:[2016-07-01 20:40:21] <info> [PI 0] required unit readings sample size 200 (required sample size 200 x subsession size 1)
```

Pilot calculated that it requires **at least** 200 samples to achieve the desired CI. Even though we can see that the CI as calculated above has already been pretty tight (about 2% of mean), the sample size is too low. According to [the law of large numbers](https://en.wikipedia.org/wiki/Law_of_large_numbers), we need at least hundreds of independent samples for the sample mean to be close to the expected value of the random variable.

```
36:[2016-07-01 20:40:21] <info> Limiting next round's work amount to 10 (no more than 5 times of the average round work amount)
```

There is a heuristics in Pilot that prevents it from running a very long round in the early stage of a benchmark session, when the data on which the decision is made may not be very reliable. So instead of running a workload round with 200 work amount, Pilot starts the next round using work amount 10.

Pilot will keep running the benchmark until the desired confidence interval and required minimum sample size is reached. Depending on your computer this may take hours to days (if there are many other processes running that are causing high variance in the measurement). You can stop the benchmark if you feel the data is good enough for your need. If you just need to get a rough understanding of the performance, getting a few dozens of samples should be enough. If you are deciding which ISP to use for the following years, you'd better let it run over a few days to iron out all the variance that are caused by changes in the environment.

The other log lines that we omitted should be self explanatory. If you have any questions feel free to join the [pilot-users](https://groups.google.com/forum/#!forum/pilot-users) mailing list and ask a question.

## Troubleshooting
### I get error "Running at max_work_amount_ still cannot meet round duration requirement. Please increase the max work amount upper limit."

Pilot has a builtin safety mechanism that checks that the benchmark rounds are not too short. The default short round threshold is one second. A very short benchmark round usually indicates that there is an error in the benchmark setup or running process, and very short rounds usually have very high overhead and cannot reflect the real property you want to benchmark.

If this error happens, you should try to increase the duration of a benchmark round, probably by increasing the ``max_work_amount``.