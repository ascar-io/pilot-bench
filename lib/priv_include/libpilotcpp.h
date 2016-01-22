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

    virtual ~arithmetic_mean_accumulator() {}
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

    virtual ~harmonic_mean_accumulator() {}
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
    if (1 == n) {
        error_log << "cannot calculate covariance for one sample";
        abort();
    }
    double sm = pilot_subsession_mean(first, n, mean_method);
    double cov;
    for (size_t q = 1; q != n / 2 + 1; ++q) {
        cov = pilot_subsession_autocorrelation_coefficient(first, n, q, sm, mean_method);
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
 * Perform warm-up phase detection and removal on readings using the linear
 * regression method
 * @param rounds
 * @param round_work_amounts
 * @param round_durations
 * @param autocorrelation_coefficient_limit
 * @param v
 * @param ci_width
 * @return 0 on success; ERR_NOT_ENOUGH_DATA when there is not enough sample
 * for calculate v; ERR_NOT_ENOUGH_DATA_FOR_CI when there is enough data for
 * calculating v but not enough for calculating confidence interval.
 */
template <typename WorkAmountInputIterator, typename RoundDurationInputIterator>
int pilot_wps_warmup_removal_lr_method(size_t rounds, WorkAmountInputIterator round_work_amounts,
        RoundDurationInputIterator round_durations,
        float autocorrelation_coefficient_limit, double *alpha, double *v,
        double *v_ci, double *ssr_out = NULL) {
    // first check for auto-correlation
    std::vector<double> naive_v;
    for (size_t i = 0; i < rounds; ++i) {
        naive_v.push_back(static_cast<double>(round_work_amounts[i]) / static_cast<double>(round_durations[i]));
    }
    int q = pilot_optimal_subsession_size(naive_v.begin(), naive_v.size(), HARMONIC_MEAN, autocorrelation_coefficient_limit);
    if (q < 0) return q;
    debug_log << "optimal subsession size (q) = " << q;

    typedef dlib::matrix<double,2,1> parameter_vector;
    parameter_vector param_vec = 10 * dlib::randm(2,1);
    std::vector<std::pair<double, double> > samples;
    for (size_t i = 0; i < rounds; ++i) {
        samples.push_back(std::make_pair(round_work_amounts[i], round_durations[i]));
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
    dlib::solve_least_squares_lm(dlib::objective_delta_stop_strategy(1e-7),
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
    *v = 1 / param_vec(1);

    double ssr = 0;
    for (size_t i = 0; i < rounds; ++i) {
        ssr += pow(*alpha + double(round_work_amounts[i]) / *v - round_durations[i], 2);
    }
    if (ssr_out) *ssr_out = ssr;
    //std::cout << "SSR: " << ssr << std::endl;
    double sigma_sqr = ssr / (rounds - 2);
    //std::cout << "sigma^2: " << sigma_sqr << std::endl;
    double wa_mean = pilot_subsession_mean(round_work_amounts, rounds, ARITHMETIC_MEAN);
    double sum_var = pilot_subsession_var(round_work_amounts, rounds, 1, wa_mean, ARITHMETIC_MEAN) * (rounds -1);
    //std::cout << "sum_var: " << sum_var << std::endl;
    double std_err_v = sqrt(sigma_sqr / sum_var);
    *v_ci = 2 * std_err_v;
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
