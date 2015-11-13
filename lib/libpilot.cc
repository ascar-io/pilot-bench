/*
 * libpilot.cc: routines for handling workloads
 *
 * Copyright (c) 2015, University of California, Santa Cruz, CA, USA.
 *
 * Developers:
 *   Yan Li <yanli@cs.ucsc.edu>
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

#include <algorithm>
#include <boost/math/distributions/students_t.hpp>
#include <cmath>
#include "common.h"
#include "config.h"
#include <fstream>
#include "interface_include/libpilot.h"
#include <vector>

using namespace pilot;
using namespace std;
using boost::timer::cpu_timer;
using boost::timer::nanosecond_type;

// all consts go here, they should be named either k_something, or SOMETHING
nanosecond_type const ONE_SECOND = 1000000000LL;
size_t const MEGABYTE = 1024*1024;

struct pilot_workload_t {
    string workload_name_;

    typedef vector<double> reading_data_t; //! The data of one reading of all rounds
    typedef vector<double> unit_reading_data_per_round_t;
    typedef vector<unit_reading_data_per_round_t> unit_reading_data_t; //! Per round unit reading data

    size_t num_of_pi_;                      //! Number of performance indices to collect for each round
    size_t rounds_;                         //! Number of rounds we've done so far
    size_t init_work_amount_;
    size_t work_amount_limit_;
    pilot_workload_func_t *workload_func_;

    vector<nanosecond_type> round_durations_; //! The duration of each round
    vector<reading_data_t> readings_;       //! Reading data of each round. Format: readings_[piid][round_id].
    vector<unit_reading_data_t> unit_readings_; //! Unit reading data of each round. Format: unit_readings_[piid][round_id].

    double confidence_interval_;
    double confidence_level_;
    double autocorrelation_coefficient_limit_;

    bool warm_up_removal_;
    enum warm_up_removal_detection_method_t {
        MOVING_AVERAGE,
    } warm_up_removal_detection_method_;
    double warm_up_removal_moving_average_window_size_in_seconds_;

    pilot_workload_t(const char *wl_name) :
                         num_of_pi_(0), rounds_(0), init_work_amount_(0),
                         work_amount_limit_(0), workload_func_(nullptr),
                         confidence_interval_(0.05), confidence_level_(.95),
                         autocorrelation_coefficient_limit_(0.1),
                         warm_up_removal_detection_method_(MOVING_AVERAGE),
                         warm_up_removal_(true),
                         warm_up_removal_moving_average_window_size_in_seconds_(3) {
        if (wl_name) workload_name_ = wl_name;
    }
};

pilot_workload_t* pilot_new_workload(const char *workload_name) {
    pilot_workload_t *wl = new pilot_workload_t(workload_name);
    return wl;
}

void pilot_set_num_of_pi(pilot_workload_t* wl, size_t num_of_readings) {
    ASSERT_VALID_POINTER(wl);
    if (wl->num_of_pi_ != 0) {
        fatal_log << "Changing the number of performance indices is not supported";
        abort();
    }
    wl->num_of_pi_ = num_of_readings;
    wl->readings_.resize(num_of_readings);
    wl->unit_readings_.resize(num_of_readings);
}

int pilot_get_num_of_pi(pilot_workload_t* wl, size_t *p_num_of_pi) {
    ASSERT_VALID_POINTER(wl);
    ASSERT_VALID_POINTER(p_num_of_pi);
    if (0 == wl->num_of_pi_) {
        error_log << __func__ << "(): workload is not properly initialized yet";
        return ERR_NOT_INIT;
    }
    *p_num_of_pi = wl->num_of_pi_;
    return 0;
}

void pilot_set_workload_func(pilot_workload_t* wl, pilot_workload_func_t *wf) {
    ASSERT_VALID_POINTER(wl);
    wl->workload_func_ = wf;
}

void pilot_set_work_amount_limit(pilot_workload_t* wl, size_t t) {
    ASSERT_VALID_POINTER(wl);
    wl->work_amount_limit_ = t;
}

int pilot_get_work_amount_limit(pilot_workload_t* wl, size_t *p_work_amount_limit) {
    ASSERT_VALID_POINTER(wl);
    ASSERT_VALID_POINTER(p_work_amount_limit);
    *p_work_amount_limit = wl->work_amount_limit_;
    return 0;
}

void pilot_set_init_work_amount(pilot_workload_t* wl, size_t init_work_amount) {
    ASSERT_VALID_POINTER(wl);
    wl->init_work_amount_ = init_work_amount;
}

int pilot_get_init_work_amount(pilot_workload_t* wl, size_t *p_init_work_amount) {
    ASSERT_VALID_POINTER(wl);
    ASSERT_VALID_POINTER(p_init_work_amount);
    *p_init_work_amount = wl->init_work_amount_;
    return 0;
}

int pilot_run_workload(pilot_workload_t *wl) {
    ASSERT_VALID_POINTER(wl);
    // sanity check
    if (wl->workload_func_ == nullptr || wl->num_of_pi_ == 0 ||
        wl->work_amount_limit_ == 0) return 11;
    // ready to start the workload
    size_t num_of_work_units;
    double **unit_readings;
    double *readings;

    unit_readings = NULL;
    readings = NULL;

    cpu_timer timer;
    int rc =
    wl->workload_func_(wl->work_amount_limit_, &pilot_malloc_func,
                      &num_of_work_units, &unit_readings,
                      &readings);
    nanosecond_type round_duration = timer.elapsed().wall;

    // result check first
    if (0 != rc)   return 12;
    if (!readings) return 13;

    //! TODO validity check
    // Get the total_elapsed_time and avg_time_per_unit = total_elapsed_time / num_of_work_units.
    // If avg_time_per_unit is not at least 100 times longer than the CPU time resolution then
    // the results cannot be used. See FB#2808 and
    // http://www.boost.org/doc/libs/1_59_0/libs/timer/doc/cpu_timers.html


    // move all data into the permanent location
    wl->round_durations_.push_back(round_duration);
    for (int piid = 0; piid < wl->num_of_pi_; ++piid)
        wl->readings_[piid].push_back(readings[piid]);
    // it is ok for unit_readings to be NULL
    if (unit_readings) {
        debug_log << "got num_of_work_units = " << num_of_work_units;
        for (int piid = 0; piid < wl->num_of_pi_; ++piid) {
            wl->unit_readings_[piid].emplace_back(vector<double>(unit_readings[piid], unit_readings[piid] + num_of_work_units));
            free(unit_readings[piid]);
        }
    } else {
        info_log << "no unit readings in this round";
        for (int piid = 0; piid < wl->num_of_pi_; ++piid) {
            wl->unit_readings_[piid].emplace_back(vector<double>());
        }
    }

    //! TODO: save the data to a database

    // clean up
    if (readings) free(readings);
    if (unit_readings) free(unit_readings);
    ++wl->rounds_;
    return 0;
}

const char *pilot_strerror(int errnum) {
    switch (errnum) {
    case NO_ERROR:        return "No error";
    case ERR_WRONG_PARAM: return "Parameter error";
    case ERR_IO:          return "I/O error";
    case ERR_NOT_INIT:    return "Workload not properly initialized yet";
    case ERR_WL_FAIL:     return "Workload failure";
    case ERR_NO_READING:  return "Workload did not return readings";
    case ERR_NOT_IMPL:    return "Not implemented";
    default:              return "Unknown error code";
    }
}

void* pilot_malloc_func(size_t size) {
    return malloc(size);
}

int pilot_get_num_of_rounds(const pilot_workload_t *wl) {
    ASSERT_VALID_POINTER(wl);
    return wl->rounds_;
}

const double* pilot_get_pi_readings(const pilot_workload_t *wl, size_t piid) {
    ASSERT_VALID_POINTER(wl);
    if (piid >= wl->num_of_pi_) {
        error_log << "piid out of range";
        return NULL;
    }
    return wl->readings_[piid].data();
}

const double* pilot_get_pi_unit_readings(const pilot_workload_t *wl,
    size_t piid, size_t round, size_t *num_of_work_units) {
    ASSERT_VALID_POINTER(wl);
    if (piid >= wl->num_of_pi_) {
        error_log << "piid out of range";
        return NULL;
    }
    if (round >= wl->rounds_) {
        error_log << "round out of range";
        return NULL;
    }
    *num_of_work_units = wl->unit_readings_[piid][round].size();
    return wl->unit_readings_[piid][round].data();
}

int pilot_export(const pilot_workload_t *wl, pilot_export_format_t format,
                 const char *filename) {
    ASSERT_VALID_POINTER(wl);
    ASSERT_VALID_POINTER(filename);

    switch (format) {
    case CSV:
        try {
            ofstream of;
            of.exceptions(ofstream::failbit | ofstream::badbit);
            of.open(filename);
            of << "piid,round,reading,unit_reading" << endl;
            for (int piid = 0; piid < wl->num_of_pi_; ++piid)
                for (int round = 0; round < wl->rounds_; ++round) {
                    if (wl->unit_readings_[piid][round].size() != 0) {
                        for (int ur=0; ur < wl->unit_readings_[piid][round].size(); ++ur) {
                            if (0 == ur)
                            of << piid << ","
                            << round << ","
                            << wl->readings_[piid][round] << ","
                            << wl->unit_readings_[piid][round][ur] << endl;
                            else
                            of << piid << ","
                            << round << ","
                            << ","
                            << wl->unit_readings_[piid][round][ur] << endl;
                        }
                    } else {
                        of << piid << ","
                        << round << ","
                        << wl->readings_[piid][round] << "," << endl;
                    }
                } /* round loop */
            of.close();
        } catch (ofstream::failure e) {
            error_log << "I/O error: " << strerror(errno);
            return ERR_IO;
        }
        break;
    default:
        error_log << "Unknown format: " << format;
        return ERR_WRONG_PARAM;
    }
    return 0;
}

int pilot_destroy_workload(pilot_workload_t *wl) {
    ASSERT_VALID_POINTER(wl);
    delete wl;
    return 0;
}

void pilot_set_log_level(pilot_log_level_t log_level) {
    boost::log::core::get()->set_filter
    (
        boost::log::trivial::severity >= (boost::log::trivial::severity_level)log_level
    );
}

double pilot_set_confidence_interval(pilot_workload_t *wl, double ci) {
    ASSERT_VALID_POINTER(wl);
    double old_ci = wl->confidence_interval_;
    wl->confidence_interval_ = ci;
    return old_ci;
}

using namespace boost::accumulators;

double pilot_subsession_mean(const double *data, size_t n) {
    using namespace std::placeholders;
    accumulator_set< double, features<tag::mean > > acc;
    for_each(data, data + n, bind<void>( ref(acc), _1 ) );
    return mean(acc);
}

double pilot_subsession_cov(const double *data, size_t n, size_t q, double sample_mean) {
    accumulator_set< double, features<tag::mean > > cov_acc;
    size_t h = n/q;
    assert (h >= 2);

    double uae, ube;
    accumulator_set< double, features<tag::mean > > ua_acc;
    for (size_t a = 0; a < q; ++a)
        ua_acc(data[a]);
    uae = mean(ua_acc) - sample_mean;

    for (size_t i = 1; i < h; ++i) {
        accumulator_set< double, features<tag::mean > > ub_acc;
        for (size_t b = 0; b < q; ++b)
            ub_acc(data[i*q+b]);
        ube = mean(ub_acc) - sample_mean;

        cov_acc(uae * ube);
        uae = ube;
    }
    return mean(cov_acc);
}

double pilot_subsession_var(const double *data, size_t n, size_t q, double sample_mean) {
    double s = 0;
    size_t h = n/q;
    for (size_t i = 0; i < h; ++i) {
        accumulator_set< double, features<tag::mean > > acc;
        for (size_t j = 0; j < q; ++j)
            acc(data[i*q + j]);

        s += pow(mean(acc) - sample_mean, 2);
    }
    return s / (h - 1);
}

double pilot_subsession_autocorrelation_coefficient(const double *data, size_t n, size_t q, double sample_mean) {
    return pilot_subsession_cov(data, n, q, sample_mean) /
           pilot_subsession_var(data, n, q, sample_mean);
}

int pilot_optimal_subsession_size(const double *data, size_t n, double max_autocorrelation_coefficient) {
    double sm = pilot_subsession_mean(data, n);
    for (size_t q = 1; q != n + 1; ++q) {
        if (pilot_subsession_autocorrelation_coefficient(data, n, q, sm) <= max_autocorrelation_coefficient)
            return q;
    }
    return -1;
}

double pilot_subsession_confidence_interval(const double *data, size_t n, size_t q, double confidence_level) {
    // See http://www.boost.org/doc/libs/1_59_0/libs/math/doc/html/math_toolkit/stat_tut/weg/st_eg/tut_mean_intervals.html
    // for explanation of the code
    using namespace boost::math;

    students_t dist(n-1);
    // T is called z' in [Ferrari78], page 60.
    double T = ::boost::math::quantile(complement(dist, (1 - confidence_level) / 2));

    double sm = pilot_subsession_mean(data, n);
    double var = pilot_subsession_var(data, n, q, sm);
    return T * sqrt(var / double(n));
}

int pilot_optimal_length(const double *data, size_t n, double confidence_interval_width, double confidence_level, double max_autocorrelation_coefficient) {
    using namespace boost::math;

    int q = pilot_optimal_subsession_size(data, n, max_autocorrelation_coefficient);
    if (q < 0) return q;
    debug_log << "optimal subsession size (q) = " << q;

    size_t h = n / q;
    students_t dist(h-1);
    // T is called z' in [Ferrari78], page 60.
    double T = ::boost::math::quantile(complement(dist, (1 - confidence_level) / 2));
    debug_log << "T score for " << 100*confidence_level << "% confidence level = " << T;

    double sm = pilot_subsession_mean(data, n);
    double var = pilot_subsession_var(data, n, q, sm);
    return ceil(var * pow(T / confidence_interval_width, 2));
}
