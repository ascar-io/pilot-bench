/*
 * libpilotcpp.h: the C++ binding (under construction)
 *
 * This file is still under construction and is not fully tested yet.
 * Use at your own risk. Feedbacks are welcome.
 *
 * Copyright (c) 2015, University of California, Santa Cruz, CA, USA.
 * Created by Yan Li <yanli@ucsc.edu, elliot.li.tech@gmail.com>,
 * Department of Computer Science, Baskin School of Engineering.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Storage Systems Research Center, the
 *       University of California, nor the names of its contributors
 *       may be used to endorse or promote products derived from this
 *       software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * REGENTS OF THE UNIVERSITY OF CALIFORNIA BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LIB_PRIV_INCLUDE_LIBPILOTCPP_H_
#define LIB_PRIV_INCLUDE_LIBPILOTCPP_H_

#include <boost/math/distributions/students_t.hpp>
#include <cmath>
#include <dlib/optimization.h>
#include "libpilot.h"
#include <limits>
#include <map>
#include <memory>
#include <vector>
#include "workload.hpp"

namespace pilot {

class accumulator_base {                      // All accumulators should inherit from
public:
    virtual void operator()(double data) = 0;
    virtual double result() const = 0;
    virtual ~accumulator_base() {};
};

class arithmetic_mean_accumulator : public accumulator_base {
public:
    arithmetic_mean_accumulator() : n(0), sum(0) {}

    virtual void operator ()(double data) {
        sum += data;
        ++n;
    }

    virtual double result() const {
        return sum / static_cast<double>(n);
    }

    ~arithmetic_mean_accumulator() {}
private:
    int    n;
    double sum;
};

class harmonic_mean_accumulator : public accumulator_base {
public:
    harmonic_mean_accumulator() : n(0), har_sum(0) {}

    virtual void operator ()(double data) {
        har_sum += 1.0 / data;
        ++n;
    }

    virtual double result() const {
        return static_cast<double>(n) / har_sum;
    }

    ~harmonic_mean_accumulator() {}
private:
    int    n;
    double har_sum;
};

inline accumulator_base* accumulator_factory(pilot_mean_method_t mean_method) {
    switch (mean_method) {
    case ARITHMETIC_MEAN:
        return new arithmetic_mean_accumulator();
    case HARMONIC_MEAN:
        return new harmonic_mean_accumulator();
    }
    abort();
}

template <typename InputIterator>
double pilot_subsession_cov(InputIterator first, size_t n, size_t q, double sample_mean,
        enum pilot_mean_method_t mean_method = ARITHMETIC_MEAN) {
    // we always use arithmetic mean for the covariance accumulator
    arithmetic_mean_accumulator cov_acc;
    size_t h = n/q;
    if (1 == h) {
        error_log << "cannot calculate covariance for one sample";
        abort();
    }

    double uae, ube;
    std::unique_ptr<accumulator_base> ua_acc(accumulator_factory(mean_method));
    for (size_t a = 0; a < q; ++a)
        (*ua_acc)(*first++);
    uae = ua_acc->result() - sample_mean;

    for (size_t i = 1; i < h; ++i) {
        std::unique_ptr<accumulator_base> ub_acc(accumulator_factory(mean_method));
        for (size_t b = 0; b < q; ++b)
            (*ub_acc)(*first++);
        ube = ub_acc->result() - sample_mean;

        cov_acc(uae * ube);
        uae = ube;
    }
    return cov_acc.result();
}

template <typename InputIterator>
double pilot_subsession_var(InputIterator first, size_t n, size_t q,
        double sample_mean, pilot_mean_method_t mean_method = ARITHMETIC_MEAN) {
    double s = 0;
    size_t h = n/q;  // subsession sample size
    for (size_t i = 0; i < h; ++i) {
        std::unique_ptr<accumulator_base> acc(accumulator_factory(mean_method));
        for (size_t j = 0; j < q; ++j)
            (*acc)(*first++);

        s += pow(acc->result() - sample_mean, 2);
    }
    return s / (h - 1);
}

template <typename InputIterator>
double pilot_subsession_autocorrelation_coefficient(InputIterator first,
        size_t n, size_t q, double sample_mean, pilot_mean_method_t mean_method) {
    double res = pilot_subsession_cov(first, n, q, sample_mean, mean_method) /
                 pilot_subsession_var(first, n, q, sample_mean, mean_method);

    // res can be NaN when the variance is 0, in this case we just return 1,
    // which means the result is very autocorrelated.
    if (isnan(res))
        return 1;
    else
        return res;
}

template <typename InputIterator>
double pilot_subsession_mean(InputIterator first, size_t n, pilot_mean_method_t mean_method) {
    std::unique_ptr<accumulator_base> acc(accumulator_factory(mean_method));
    while (n-- != 0)
        (*acc)(*first++);
    return acc->result();
}

template <typename InputIterator>
int pilot_optimal_subsession_size(InputIterator first, const size_t n,
                                  pilot_mean_method_t mean_method,
                                  double max_autocorrelation_coefficient = 0.1) {
    if (n <= 1) {
        error_log << "cannot calculate covariance for " << n << " sample(s)";
        abort();
    }
    double sm = pilot_subsession_mean(first, n, mean_method);
    double cov;
    for (size_t q = 1; q != n / 2 + 1; ++q) {
        cov = pilot_subsession_autocorrelation_coefficient(first, n, q, sm, mean_method);
        info_log << __func__ << "(): subsession size: " << q << ", auto. cor. coef.: " << cov;
        if (std::abs(cov) <= max_autocorrelation_coefficient)
            return q;
    }
    return -1;
}

template <typename InputIterator>
double pilot_subsession_confidence_interval(InputIterator first, size_t n,
        size_t q, double confidence_level, pilot_mean_method_t mean_method) {
    // See http://www.boost.org/doc/libs/1_59_0/libs/math/doc/html/math_toolkit/stat_tut/weg/st_eg/tut_mean_intervals.html
    // for explanation of the code. The same formula can also be found at
    // [Ferrari78], page 59 and [Le Boudec15], page 34.
    using namespace boost::math;

    size_t h = n / q;
    students_t dist(h - 1);
    // T is called z' in [Ferrari78], page 60.
    double T = ::boost::math::quantile(complement(dist, (1 - confidence_level) / 2));

    double sm = pilot_subsession_mean(first, n, mean_method);
    double var = pilot_subsession_var(first, n, q, sm, mean_method);
    return T * sqrt(var / double(h)) * 2;
}


template <typename InputIterator> ssize_t
pilot_optimal_sample_size(InputIterator first, size_t n, double confidence_interval_width,
                          pilot_mean_method_t mean_method,
                          double confidence_level = 0.95,
                          double max_autocorrelation_coefficient = 0.1) {
    using namespace boost::math;

    int q = pilot_optimal_subsession_size(first, n, mean_method, max_autocorrelation_coefficient);
    if (q < 0) return q;
    debug_log << "optimal subsession size (q) = " << q;

    size_t h = n / q;
    students_t dist(h-1);
    // T is called z' in [Ferrari78], page 60.
    double T = ::boost::math::quantile(complement(dist, (1 - confidence_level) / 2));
    debug_log << "T score for " << 100*confidence_level << "% confidence level = " << T;
    debug_log << "expected CI: " << confidence_interval_width;
    double e = confidence_interval_width / 2;

    double sm = pilot_subsession_mean(first, n, mean_method);
    double var = pilot_subsession_var(first, n, q, sm, mean_method);
    size_t sample_size_req = ceil(var * pow(T / e, 2));
    debug_log << "subsession sample size required: " << sample_size_req;
    size_t ur_req = sample_size_req * q;
    debug_log << "number of unit readings required: " << ur_req;
    return ur_req;
}

/* DEBUG CODE
inline double residual (const std::pair<double, double>& data,
    const parameter_vector& params) {
    return params(0) + params(1) * data.first - data.second;
}

inline parameter_vector residual_derivative (const std::pair<double, double>& data,
    const parameter_vector& params) {
    parameter_vector der;
    der(0) = 1;
    der(1) = data.first;
    return der;
}
*/

/**
 * Our own regression stop strategy that depends on how many percentage the
 * changed is to total objective value
 */
class percent_delta_stop_strategy {
public:
    explicit percent_delta_stop_strategy (
            double percent = 1
    ) : verbose_(false), been_used_(false), percent_(percent), max_iter_(0), cur_iter_(0), prev_funct_value_(0)
    {
        assert (percent > 0);
    }

    percent_delta_stop_strategy (
            double percent,
            unsigned long max_iter
    ) : verbose_(false), been_used_(false), percent_(percent), max_iter_(max_iter), cur_iter_(0), prev_funct_value_(0)
    {
        assert (percent > 0 && max_iter > 0);
    }

    percent_delta_stop_strategy& be_verbose(
    )
    {
        verbose_ = true;
        return *this;
    }

    template <typename T>
    bool should_continue_search (
            const T& ,
            const double funct_value,
            const T&
    )
    {
        if (verbose_)
        {
            using namespace std;
            std::cout << "iteration: " << cur_iter_ << "   objective: " << funct_value;
        }

        ++cur_iter_;
        if (been_used_)
        {
            // Check if we have hit the max allowable number of iterations.  (but only
            // check if _max_iter is enabled (i.e. not 0)).
            if (max_iter_ != 0 && cur_iter_ > max_iter_)
                return false;

            // check if the function change was too small
            double percent_changed = std::abs(funct_value - prev_funct_value_) / prev_funct_value_;
            if (verbose_) {
                std::cout << "   percent changed: " << percent_changed * 100 << std::endl;
            }
            if (percent_changed < percent_)
                return false;
        }

        been_used_ = true;
        prev_funct_value_ = funct_value;
        return true;
    }

private:
    bool verbose_;

    bool been_used_;
    double percent_;
    unsigned long max_iter_;
    unsigned long cur_iter_;
    double prev_funct_value_;
};


/**
 * Perform warm-up phase detection and removal on readings using the linear
 * regression method
 * @param rounds
 * @param round_work_amounts
 * @param round_durations
 * @param autocorrelation_coefficient_limit
 * @param duration_threshold any round whose duration is less than this threshold is discarded
 * @param v
 * @param ci_width
 * @return 0 on success; ERR_NOT_ENOUGH_DATA when there is not enough sample
 * for calculate v; ERR_NOT_ENOUGH_DATA_FOR_CI when there is enough data for
 * calculating v but not enough for calculating confidence interval.
 */
template <typename WorkAmountInputIterator, typename RoundDurationInputIterator>
int pilot_wps_warmup_removal_lr_method(size_t rounds, WorkAmountInputIterator round_work_amounts_raw,
        RoundDurationInputIterator round_durations_raw,
        float autocorrelation_coefficient_limit, nanosecond_type duration_threshold,
        double *alpha, double *v,
        double *v_ci, double *ssr_out = NULL) {
    // first we create copies of round_work_amounts and round_durations with
    // rounds that are shorter than round_durations filtered out
    std::vector<size_t> round_work_amounts;
    std::vector<nanosecond_type> round_durations;
    for (size_t i = 0; i < rounds; ++i) {
        if (*round_durations_raw > duration_threshold) {
            round_work_amounts.push_back(*round_work_amounts_raw);
            round_durations.push_back(*round_durations_raw);
        }
        ++round_work_amounts_raw;
        ++round_durations_raw;
    }

    if (round_work_amounts.size() < 3) {
        info_log << __func__ << "() doesn't have enough samples after filtering using duration threshold";
        return ERR_NOT_ENOUGH_DATA;
    }

    // then check for auto-correlation
    std::vector<double> naive_v;
    for (size_t i = 0; i < round_work_amounts.size(); ++i) {
        naive_v.push_back(static_cast<double>(round_work_amounts[i]) / static_cast<double>(round_durations[i]));
    }
    int q = pilot_optimal_subsession_size(naive_v.begin(), naive_v.size(), HARMONIC_MEAN, autocorrelation_coefficient_limit);
    if (q < 0) {
        info_log << __func__ << "() samples' autocorrelation coefficient too high; need more samples";
        return ERR_NOT_ENOUGH_DATA;
    }
    debug_log << "WPS analysis: optimal subsession size (q) = " << q;
    size_t h = round_work_amounts.size() / q;
    if (h < 3) {
        info_log << __func__ << "() doesn't have enough samples after subsession grouping";
        return ERR_NOT_ENOUGH_DATA;
    }

    // prepare data for regression analysis
    // See http://dlib.net/least_squares_ex.cpp.html and
    // http://dlib.net/dlib/optimization/optimization_least_squares_abstract.h.html#solve_least_squares_lm
    // for more information on using dlib.
    typedef dlib::matrix<double,2,1> parameter_vector;
    parameter_vector param_vec;
    // set all elements to 1, this is needed for solve_least_squares_lm() to work
    param_vec = 1;
    std::vector<std::pair<double, double> > samples;
    size_t subsession_sum_wa = 0;
    nanosecond_type subsession_sum_dur = 0;
    // convert input into subsession data by grouping every q samples
    for (size_t i = 0; i < rounds; ++i) {
        subsession_sum_wa  += round_work_amounts[i];
        subsession_sum_dur += round_durations[i];
        if (i % q == q - 1) {
            samples.push_back(std::make_pair(subsession_sum_wa, subsession_sum_dur));
            subsession_sum_wa = 0;
            subsession_sum_dur = 0;
        }
    }

    /* DEBUG CODE
    // Before we do anything, let's make sure that our derivative function defined above matches
    // the approximate derivative computed using central differences (via derivative()).
    // If this value is big then it means we probably typed the derivative function incorrectly.
    // std::cout << "derivative error: " << dlib::length(residual_derivative(samples[0], param_vec) -
    //        derivative(residual)(samples[0], param_vec) ) << std::endl;

    dlib::solve_least_squares_lm(dlib::objective_delta_stop_strategy(1e-7).be_verbose(),
            residual,
            residual_derivative,
            samples,
            param_vec);
    */
    //std::cout << "samples: " << samples << std::endl;
    // We are experimenting using samples_naive_mean / 10 as the stopping threshold.
    dlib::solve_least_squares_lm(percent_delta_stop_strategy(.01, 50),
            [](const std::pair<double, double>& data, const parameter_vector& params) {
                return params(0) + params(1) * data.first - data.second;
            },
            [](const std::pair<double, double>& data, const parameter_vector& params) {
                parameter_vector der;
                der(0) = 1;
                der(1) = data.first;
                return der;
            },
            samples,
            param_vec);
    *alpha = param_vec(0);
    double inv_v = param_vec(1);
    *v = 1 / inv_v;

    double ssr = 0;
    for (auto &s : samples) {
        double wa = static_cast<double>(s.first);
        double dur = static_cast<double>(s.second);
        ssr += pow(*alpha + inv_v * wa - dur, 2);
    }
    if (ssr_out) *ssr_out = ssr;
    //std::cout << "SSR: " << ssr << std::endl;
    double sigma_sqr = ssr / (samples.size() - 2);
    //std::cout << "sigma^2: " << sigma_sqr << std::endl;
    double wa_mean = pilot_subsession_mean(round_work_amounts.begin(), round_work_amounts.size(), ARITHMETIC_MEAN);
    double sum_var = pilot_subsession_var(round_work_amounts.begin(), round_work_amounts.size(), q, wa_mean, ARITHMETIC_MEAN) * (rounds -1);
    //std::cout << "sum_var: " << sum_var << std::endl;
    double std_err_v = sqrt(sigma_sqr / sum_var);
    double inv_v_ci = 2 * std_err_v;
    *v_ci = 1.0 / (inv_v - inv_v_ci) - 1 / (inv_v + inv_v_ci);
    return 0;
}

/**
 * Perform warm-up phase detection and removal on readings using the (now
 * deprecated) dw method
 * @param rounds
 * @param round_work_amounts
 * @param round_durations
 * @param confidence_level
 * @param autocorrelation_coefficient_limit
 * @param v
 * @param ci_width
 * @return
 */
template <typename WorkAmountInputIterator, typename RoundDurationInputIterator>
int pilot_wps_warmup_removal_dw_method(size_t rounds, WorkAmountInputIterator round_work_amounts,
        RoundDurationInputIterator round_durations, float confidence_level,
        float autocorrelation_coefficient_limit, double *v, double *ci_width) {
    struct dw_info_t {
        size_t sum_dw;
        nanosecond_type sum_dt;
        std::vector<double> vs; //! the v of all rounds that share the same dw
        ssize_t calc_q;          //! the calculated optimal size of q
        double calc_v;          //! the single calculated v
        double calc_ci_width;   //! the calculated width of confidence interval
        dw_info_t() : sum_dw(0), sum_dt(0) {}
    };

    *v = -1; *ci_width = -1;

    if (rounds < 2) {
        error_log << __func__ << "(): called without enough input data";
        return ERR_NOT_ENOUGH_DATA;
    }

    std::map<size_t, dw_info_t> dw_infos;

    // go through the data and calculate sum_dw, sum_dt for each different dw group
    size_t prev_work_amount = *round_work_amounts;
    nanosecond_type prev_round_duration = *round_durations;
    size_t dw;
    nanosecond_type dt;
    size_t total_sum_dw = 0;
    nanosecond_type total_sum_dt = 0;
    for (size_t i = 1; i < rounds; ++i) {
        ++round_work_amounts;
        ++round_durations;

        if (*round_work_amounts > prev_work_amount) {
            dw = *round_work_amounts - prev_work_amount;
            dt = *round_durations - prev_round_duration;
        } else {
            dw = prev_work_amount - *round_work_amounts;
            dt = prev_round_duration - *round_durations;
        }

        dw_infos[dw].sum_dw += dw;
        dw_infos[dw].sum_dt += dt;
        total_sum_dw += dw;
        total_sum_dt += dt;
        dw_infos[dw].vs.push_back(double(dw) / dt);

        prev_work_amount = *round_work_amounts;
        prev_round_duration = *round_durations;
    }

    // calculate v and ci_width for each dw group
    *ci_width = std::numeric_limits<double>::infinity();
    *v = (double(total_sum_dw) / total_sum_dt) * ONE_SECOND;
    bool has_seen_valid_ci = false;
    for (auto & c : dw_infos) {
        c.second.calc_v = double(c.second.sum_dw) / c.second.sum_dt;
        if (c.second.vs.size() < 2) {
            // sample size too small
            c.second.calc_q = -1;
            c.second.calc_ci_width = -1;
            continue;
        }
        c.second.calc_q = pilot_optimal_subsession_size(c.second.vs.begin(), c.second.vs.size(), ARITHMETIC_MEAN, autocorrelation_coefficient_limit);
        if (c.second.calc_q < 0) {
            c.second.calc_ci_width = -1;
        } else {
            c.second.calc_ci_width =
                pilot_subsession_confidence_interval(c.second.vs.begin(), c.second.vs.size(),
                                                     c.second.calc_q, confidence_level, ARITHMETIC_MEAN);
        }
        if (c.second.calc_ci_width < *ci_width) {
            *ci_width = c.second.calc_ci_width * ONE_SECOND;
            has_seen_valid_ci = true;
        }
    }

    if (has_seen_valid_ci)
        return 0;
    else {
        *ci_width = -1;
        return ERR_NOT_ENOUGH_DATA_FOR_CI;
    }
}

} // namespace pilot

#endif /* LIB_PRIV_INCLUDE_LIBPILOTCPP_H_ */
