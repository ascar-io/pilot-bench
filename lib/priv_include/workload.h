/*
 * workload.h: functions for workload manipulation
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

#ifndef LIB_PRIV_INCLUDE_WORKLOAD_H_
#define LIB_PRIV_INCLUDE_WORKLOAD_H_

#include "libpilot.h"

struct pilot_workload_t {
    std::string workload_name_;

    typedef std::vector<double> reading_data_t; //! The data of one reading of all rounds
    typedef std::vector<double> unit_reading_data_per_round_t;
    typedef std::vector<unit_reading_data_per_round_t> unit_reading_data_t; //! Per round unit reading data

    size_t num_of_pi_;                      //! Number of performance indices to collect for each round
    size_t rounds_;                         //! Number of rounds we've done so far
    size_t init_work_amount_;
    size_t work_amount_limit_;
    pilot_workload_func_t *workload_func_;

    std::vector<boost::timer::nanosecond_type> round_durations_; //! The duration of each round
    std::vector<reading_data_t> readings_;       //! Reading data of each round. Format: readings_[piid][round_id].
    std::vector<unit_reading_data_t> unit_readings_; //! Unit reading data of each round. Format: unit_readings_[piid][round_id].
    std::vector<std::vector<size_t> > warm_up_phase_len_;          //! The length of the warm-up phase of each PI per round. Format: warm_up_phase_len_[piid][round_id].
    std::vector<size_t> total_num_of_unit_readings_; //! Total number of unit readings per PI
    std::vector<size_t> total_num_of_readings_;      //! Total number of readings per PI
    std::vector<size_t> work_amount_per_round_;      //! The work amount we used in each round

    double confidence_interval_;
    double confidence_level_;
    double autocorrelation_coefficient_limit_;

    bool short_workload_check_;
    pilot_warm_up_removal_detection_method_t warm_up_removal_detection_method_;
    double warm_up_removal_moving_average_window_size_in_seconds_;

    // Hook functions
    calc_readings_required_func_t *calc_unit_readings_required_func_; //! The hook function that calculates how many unit readings (samples) are needed
    calc_readings_required_func_t *calc_readings_required_func_;      //! The hook function that calculates how many readings (samples) are needed
    general_hook_func_t *hook_pre_workload_run_;
    general_hook_func_t *hook_post_workload_run_;

    pilot_workload_t(const char *wl_name) :
                         num_of_pi_(0), rounds_(0), init_work_amount_(0),
                         work_amount_limit_(0), workload_func_(nullptr),
                         confidence_interval_(0.05), confidence_level_(.95),
                         autocorrelation_coefficient_limit_(0.1),
                         short_workload_check_(true),
                         warm_up_removal_detection_method_(NO_WARM_UP_REMOVAL),
                         warm_up_removal_moving_average_window_size_in_seconds_(3),
                         calc_unit_readings_required_func_(&default_calc_unit_readings_required_func),
                         calc_readings_required_func_(&default_calc_readings_required_func),
                         hook_pre_workload_run_(NULL), hook_post_workload_run_(NULL) {
        if (wl_name) workload_name_ = wl_name;
    }
};


#endif /* LIB_PRIV_INCLUDE_WORKLOAD_H_ */
