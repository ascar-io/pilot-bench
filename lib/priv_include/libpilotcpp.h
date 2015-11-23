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
#include "libpilot.h"
#include <workload.h>

template <typename InputIterator>
double pilot_subsession_cov(InputIterator first, size_t n, size_t q, double sample_mean) {
    using namespace boost::accumulators;
    accumulator_set< double, features<tag::mean > > cov_acc;
    size_t h = n/q;
    assert (h >= 2);

    double uae, ube;
    accumulator_set< double, features<tag::mean > > ua_acc;
    for (size_t a = 0; a < q; ++a)
        ua_acc(*first++);
    uae = mean(ua_acc) - sample_mean;

    for (size_t i = 1; i < h; ++i) {
        accumulator_set< double, features<tag::mean > > ub_acc;
        for (size_t b = 0; b < q; ++b)
            ub_acc(*first++);
        ube = mean(ub_acc) - sample_mean;

        cov_acc(uae * ube);
        uae = ube;
    }
    return mean(cov_acc);
}

template <typename InputIterator>
double pilot_subsession_var(InputIterator first, size_t n, size_t q, double sample_mean) {
    using namespace boost::accumulators;
    double s = 0;
    size_t h = n/q;  // subsession sample size
    for (size_t i = 0; i < h; ++i) {
        accumulator_set< double, features<tag::mean > > acc;
        for (size_t j = 0; j < q; ++j)
            acc(*first++);

        s += pow(mean(acc) - sample_mean, 2);
    }
    return s / (h - 1);
}

template <typename InputIterator>
double pilot_subsession_autocorrelation_coefficient(InputIterator first, size_t n, size_t q, double sample_mean) {
    return pilot_subsession_cov(first, n, q, sample_mean) /
           pilot_subsession_var(first, n, q, sample_mean);
}

template <typename InputIterator>
double pilot_subsession_mean(InputIterator first, size_t n) {
    using namespace boost::accumulators;
    accumulator_set< double, features<tag::mean > > acc;
    while (n-- != 0)
        acc(*first++);
    return mean(acc);
}

template <typename InputIterator>
int pilot_optimal_subsession_size(InputIterator first, size_t n, double max_autocorrelation_coefficient = 0.1) {
    double sm = pilot_subsession_mean(first, n);
    for (size_t q = 1; q != n + 1; ++q) {
        if (pilot_subsession_autocorrelation_coefficient(first, n, q, sm) <= max_autocorrelation_coefficient)
            return q;
    }
    return -1;
}

template <typename InputIterator>
double pilot_subsession_confidence_interval(InputIterator first, size_t n, size_t q, double confidence_level) {
    // See http://www.boost.org/doc/libs/1_59_0/libs/math/doc/html/math_toolkit/stat_tut/weg/st_eg/tut_mean_intervals.html
    // for explanation of the code. The same formula can also be found at
    // [Ferrari78], page 59 and [Le Boudec15], page 34.
    using namespace boost::math;

    size_t h = n / q;
    students_t dist(h - 1);
    // T is called z' in [Ferrari78], page 60.
    double T = ::boost::math::quantile(complement(dist, (1 - confidence_level) / 2));

    double sm = pilot_subsession_mean(first, n);
    double var = pilot_subsession_var(first, n, q, sm);
    return T * sqrt(var / double(h)) * 2;
}


template <typename InputIterator> ssize_t
pilot_optimal_sample_size(InputIterator first, size_t n, double confidence_interval_width,
                          double confidence_level = 0.95,
                          double max_autocorrelation_coefficient = 0.1) {
    using namespace boost::math;

    int q = pilot_optimal_subsession_size(first, n, max_autocorrelation_coefficient);
    if (q < 0) return q;
    debug_log << "optimal subsession size (q) = " << q;

    size_t h = n / q;
    students_t dist(h-1);
    // T is called z' in [Ferrari78], page 60.
    double T = ::boost::math::quantile(complement(dist, (1 - confidence_level) / 2));
    debug_log << "T score for " << 100*confidence_level << "% confidence level = " << T;
    debug_log << "expected CI: " << confidence_interval_width;
    double e = confidence_interval_width / 2;

    double sm = pilot_subsession_mean(first, n);
    double var = pilot_subsession_var(first, n, q, sm);
    size_t sample_size_req = ceil(var * pow(T / e, 2));
    debug_log << "subsession sample size required: " << sample_size_req;
    size_t ur_req = sample_size_req * q;
    debug_log << "number of unit readings required: " << ur_req;
    return ur_req;
}


#endif /* LIB_PRIV_INCLUDE_LIBPILOTCPP_H_ */
