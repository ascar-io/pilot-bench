/*
 * workload.hpp: functions for workload manipulation
 *
 * Copyright (c) 2017-2019 Yan Li <yanli@tuneup.ai>. All rights reserved.
 * The Pilot tool and library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License version 2.1 (not any other version) as published by the Free
 * Software Foundation.
 *
 * The Pilot tool and library is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program in file lgpl-2.1.txt; if not, see
 * https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 *
 * Commit 033228934e11b3f86fb0a4932b54b2aeea5c803c and before were
 * released with the following license:
 * Copyright (c) 2015, 2016, University of California, Santa Cruz, CA, USA.
 * Created by Yan Li <yanli@tuneup.ai>,
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

#ifndef LIB_PRIV_INCLUDE_WORKLOAD_HPP_
#define LIB_PRIV_INCLUDE_WORKLOAD_HPP_

#include <atomic>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/timer/timer.hpp>
#include <chrono>
#include <functional>
#include <vector>
#include "common.h"
#include "pilot/libpilot.h"

namespace pilot {

class PilotTUI;

/**
 * This functor is used to format a number for human-readable display.
 */
class pilot_display_format_functor {
public:
    pilot_pi_display_format_func_t *format_func_;
    /**
     * This default format functor does nothing
     * @param wl
     * @param number
     * @return
     */
    double operator()(const pilot_workload_t* wl, double number) const {
        if (!format_func_)
            return number;
        else
            return format_func_(wl, number);
    }
    pilot_display_format_functor(pilot_pi_display_format_func_t *format_func = NULL) :
        format_func_(format_func) {}
};

class pilot_pi_info_t {
public:
    std::string name;
    std::string unit;
    pilot_display_format_functor format_reading;
    pilot_display_format_functor format_unit_reading;
    bool reading_must_satisfy;
    bool unit_reading_must_satisfy;
    pilot_mean_method_t reading_mean_method;
    pilot_mean_method_t unit_reading_mean_method;
    pilot_confidence_interval_type_t reading_ci_type;

    pilot_pi_info_t(std::string _name = "", std::string _unit = "",
           pilot_pi_display_format_func_t *_r_format_func = NULL,
           pilot_pi_display_format_func_t *_ur_format_func = NULL,
           bool _r_sat = true, bool _ur_sat = true,
           pilot_mean_method_t _reading_mean_method = ARITHMETIC_MEAN,
           pilot_mean_method_t _unit_reading_mean_method = ARITHMETIC_MEAN,
           pilot_confidence_interval_type_t _reading_ci_type = SAMPLE_MEAN) :
        name(_name), unit(_unit), format_reading(_r_format_func),
        format_unit_reading(_ur_format_func),
        reading_must_satisfy(_r_sat), unit_reading_must_satisfy(_ur_sat),
        reading_mean_method(_reading_mean_method),
        unit_reading_mean_method(_unit_reading_mean_method),
        reading_ci_type(_reading_ci_type) {}
};

struct runtime_analysis_plugin_t {
    bool enabled;
    bool finished;  //! If this plugin is finished. A finished plugin will not be run in the future.
    next_round_work_amount_hook_t *calc_next_round_work_amount;

    runtime_analysis_plugin_t(next_round_work_amount_hook_t *c) :
        enabled(true), finished(false), calc_next_round_work_amount(c) {}
    runtime_analysis_plugin_t(bool e, next_round_work_amount_hook_t *c) :
        enabled(e), finished(false), calc_next_round_work_amount(c) {}
};

struct baseline_info_t {
    bool set;
    double mean;
    size_t sample_size;
    double var;

    baseline_info_t() : set(false), mean(0), sample_size(0), var(0) {}
};

enum pilot_workload_status_t {
    WL_NOT_RUNNING,
    WL_RUNNING,
    WL_STOP_REQUESTED,
};

struct pilot_workload_t {
private:
    double required_ci_percent_of_mean_;
    double required_ci_absolute_value_;
public:  // FIXME: most of the following members should be private and controlled
         // by getters and setters
    // Essential workload information
    std::string workload_name_;
    volatile std::atomic<pilot_workload_status_t> status_;
    std::chrono::steady_clock::time_point raw_data_changed_time_; //! The time when the latest raw data came in
    size_t num_of_pi_;                               //! Number of performance indices to collect for each round
    size_t rounds_;                                  //! Number of rounds we've done so far
    size_t init_work_amount_;
    size_t max_work_amount_;                         //! The maximum work amount set by user
    size_t min_work_amount_;                         //! The minimum work amount set by user
    mutable ssize_t adjusted_min_work_amount_;       //! The minimum work amount that can make the round no shorter than short_round_detection_threshold_. -1 means not found yet.
    pilot_workload_func_t *workload_func_;
    void *workload_data_;
    std::vector<pilot_pi_info_t> pi_info_;
    pilot_display_format_functor format_wps_;
    bool wps_must_satisfy_;                          //! if the result of WPS analysis must satisfy
    size_t min_sample_size_;                         //! the lower threshold of sample size. Do not change it directly, use set_min_sample_size().

    double confidence_level_;
    double autocorrelation_coefficient_limit_;
    size_t session_desired_duration_in_sec_;
    size_t session_duration_limit_in_sec_;
    size_t round_work_amount_to_avg_amount_limit_;    //! The upper limit of next_round_work_amount/average_round_work_amount
    nanosecond_type short_round_detection_threshold_;
    double desired_p_value_;

    bool short_workload_check_;
    pilot_warm_up_removal_detection_method_t warm_up_removal_detection_method_;
    double warm_up_removal_percentage_;
    double warm_up_removal_moving_average_window_size_in_seconds_;

    // Baseline for comparison analysis
    std::vector<baseline_info_t> baseline_of_readings_;
    std::vector<baseline_info_t> baseline_of_unit_readings_;

    // Raw data
    typedef std::vector<double> reading_data_t;      //! The data of one reading of all rounds
    typedef std::vector<double> unit_reading_data_per_round_t;
    typedef std::vector<unit_reading_data_per_round_t> unit_reading_data_t; //! Per round unit reading data

    std::vector<boost::timer::nanosecond_type> round_durations_; //! The duration of each round
    std::vector<reading_data_t> readings_;           //! Reading data of each round. Format: readings_[piid][round_id].
    std::vector<unit_reading_data_t> unit_readings_; //! Unit reading data of each round. Format: unit_readings_[piid][round_id].
    std::vector<std::vector<size_t> > warm_up_phase_len_;  //! The length of the warm-up phase of each PI per round. Format: warm_up_phase_len_[piid][round_id].
    std::vector<size_t> total_num_of_unit_readings_; //! Total number of unit readings per PI
    std::vector<size_t> total_num_of_readings_;      //! Total number of readings per PI
    std::vector<size_t> round_work_amounts_;         //! The work amount we used in each round

    size_t wholly_rejected_rounds_;                  //! Number of rounds that are wholly rejected due to too short a duration

    // Analytical result
    mutable pilot_analytical_result_t analytical_result_;
    mutable std::chrono::steady_clock::time_point analytical_result_update_time_; //! The time when analytical_result_ is updated

    // WPS analysis bookkeeping
    mutable size_t wps_slices_;                              //! The total number of slices, which is used to generate work amounts for WPS analysis

    // Hook functions
    next_round_work_amount_hook_t *next_round_work_amount_hook_; //! The hook function that calculates the work amount for next round
    general_hook_func_t *hook_pre_workload_run_;
    general_hook_func_t *hook_post_workload_run_;
    calc_required_readings_func_t *calc_required_readings_func_;
    calc_required_readings_func_t *calc_required_unit_readings_func_;

    mutable std::vector<runtime_analysis_plugin_t> runtime_analysis_plugins_;

    // Runtime data structures
    PilotTUI *tui_;

    pilot_workload_t(const char *wl_name) :
                         required_ci_percent_of_mean_(0.1), required_ci_absolute_value_(-1),
                         status_(WL_NOT_RUNNING),
                         num_of_pi_(0), rounds_(0), init_work_amount_(0),
                         max_work_amount_(0), min_work_amount_(0),
                         adjusted_min_work_amount_(-1),
                         workload_func_(nullptr), workload_data_(NULL),
                         wps_must_satisfy_(false), min_sample_size_(200),
                         confidence_level_(.95),
                         autocorrelation_coefficient_limit_(0.1),
                         session_desired_duration_in_sec_(60),
                         session_duration_limit_in_sec_(0),
                         round_work_amount_to_avg_amount_limit_(5),
                         short_round_detection_threshold_(20 * pilot::ONE_SECOND),
                         desired_p_value_(0.05),
                         short_workload_check_(true),
                         warm_up_removal_detection_method_(EDM),
                         warm_up_removal_percentage_(0.1),
                         warm_up_removal_moving_average_window_size_in_seconds_(3),
                         wholly_rejected_rounds_(0),
                         analytical_result_(),
                         analytical_result_update_time_(std::chrono::steady_clock::time_point::min()),
                         wps_slices_(0),
                         next_round_work_amount_hook_(NULL),
                         hook_pre_workload_run_(NULL), hook_post_workload_run_(NULL),
                         calc_required_readings_func_(NULL),
                         calc_required_unit_readings_func_(NULL), tui_(NULL) {
        if (wl_name) workload_name_ = wl_name;
        load_runtime_analysis_plugin(&calc_next_round_work_amount_meet_lower_bound);
        load_runtime_analysis_plugin(&calc_next_round_work_amount_from_readings);
        load_runtime_analysis_plugin(&calc_next_round_work_amount_from_unit_readings);
        load_runtime_analysis_plugin(&calc_next_round_work_amount_from_wps);
    }

    /**
     * \brief Set the number of PIs
     * See pilot_set_num_of_pi() for detail.
     * @param num_of_pi the desired number of PIs
     */
    void set_num_of_pi(size_t num_of_pi);

    /**
     * \brief Return the mean of the unit readings
     * @param piid the PI ID to analyze
     * @return the mean of the unit readings
     */
    double unit_readings_mean(int piid) const;

    /**
     * \brief Return the subsession variance of the unit readings
     * @param piid the PI ID to analyze
     * @param q subsession size
     * @return the subsession variance of the unit readings
     */
    double unit_readings_var(int piid, size_t q) const;

    double unit_readings_autocorrelation_coefficient(int piid, size_t q, pilot_mean_method_t mean_method) const;

    /**
     * Calculate the total required number of readings needed to meet the CI
     * requirement
     * @return -1 if there is not enough data to calculate
     */
    ssize_t required_num_of_readings(int piid) const;

    /**
     * Calculate the required number of unit readings
     * @return negative if not enough data, otherwise the required number of unit readings
     */
    ssize_t required_num_of_unit_readings(int piid) const;

    /**
     * Calculate the required number of unit readings for comparison
     * @return negative if not enough data, otherwise the required number of unit readings
     */
    ssize_t required_num_of_unit_readings_for_comparison(int piid) const;

    /**
     * Calculate how much work amount is needed for next round
     * @param[out] needed_work_amount. A returned 0 value doesn't necessarily mean
     * no more work is needed, because the workload may not support setting work
     * amount. Always use the return value to decide if more rounds are needed.
     * @return true if more rounds are needed; false if no more rounds are needed
     */
    bool calc_next_round_work_amount(size_t * const needed_work_amount) const;

    inline double calc_avg_work_unit_per_amount(int piid) const {
        size_t total_work_units = 0;
        size_t total_work_amount = 0;
        for (auto const & c : unit_readings_[piid])
            total_work_units += c.size();
        for (auto const & c : round_work_amounts_)
            total_work_amount += c;
        double res = (double)total_work_amount / total_work_units;
        debug_log << "[PI " << piid << "] average work per unit reading: " << res;
        return res;
    }

    /**
     * \brief Refresh the analytical result (analytical_result_)
     * \details This function is used mostly by the library internally. The
     * user should consider using get_analytical_result().
     */
    void refresh_analytical_result(void) const;

    /**
     * \brief Get the analytical result of a workload
     * \details First this function update the cached analytical result if necessary.
     * If info is NULL, this function allocates memory for a
     * pilot_analytical_result_t, fills information, and returns it. If
     * info is provided, its information will be updated and no new
     * memory will be allocated. The allocated memory must eventually
     * be freed by using pilot_free_workload_info().
     * @param info (optional) if provided, it must point to a pilot_analytical_result_t
     * that was returned by a previous call to pilot_workload_info()
     * @return a pointer to a pilot_analytical_result_t struct. You must use
     * pilot_free_analytical_result() to free it (you shall not use delete).
     */
    pilot_analytical_result_t* get_analytical_result(pilot_analytical_result_t *winfo = NULL) const;

    /**
     * \brief Get the basic and statistics information of a workload round
     * \details If info is NULL, this function allocates memory for a
     * pilot_round_info_t, fills information, and returns it. If
     * info is provided, its information will be updated and no new
     * memory will be allocated. The allocated memory must eventually
     * be freed by using pilot_free_round_info().
     * @param info (optional) if provided, it must point to a pilot_round_info_t
     * that was returned by a previous call to pilot_round_info()
     * @return a pointer to a pilot_round_info_t struct. You must use
     * pilot_free_round_info() to free it (you shall not use delete).
     */
    pilot_round_info_t* round_info(size_t round, pilot_round_info_t *rinfo = NULL) const;

    /**
     * Dump the summary of a round in Markdown text format
     * @param round_id the round ID starting from 0
     * @return a memory buffer of text dump that can be directly output,
     * you need to delete it after using.
     */
    char* text_round_summary(size_t round) const;

    /**
     * Dump the workload summary in Markdown text format
     * @return a buffer of dumped text, you need to delete it after using.
     */
    char* text_workload_summary(void) const;

    inline double format_reading(const int piid, const double r) const {
        return pi_info_[piid].format_reading(this, r);
    }

    inline double format_unit_reading(const int piid, const double ur) const {
        return pi_info_[piid].format_unit_reading(this, ur);
    }

    inline double format_wps(const double wps) const {
        return format_wps_(this, wps);
    }

    int set_wps_analysis(bool enabled, bool wps_must_satisfy);
    void refresh_wps_analysis_results(void) const;

    size_t set_session_desired_duration(size_t sec);
    size_t set_session_duration_limit(size_t sec);
    size_t set_min_sample_size(size_t min_sample_size);

    /**
     * \brief Load and optionally enable a runtime analysis plugin
     * @param p pointer to the plugin func
     */
    void load_runtime_analysis_plugin(next_round_work_amount_hook_t *p, bool enabled = true) const;

    /**
     * \brief Load and enable a runtime analysis plugin
     * @param p pointer to the plugin func
     */
    inline void enable_runtime_analysis_plugin(next_round_work_amount_hook_t *p) {
        load_runtime_analysis_plugin(p, true);
    }

    /**
     * Set a runtime analysis plugin as finished
     * @param p pointer to the analysis function
     */
    void finish_runtime_analysis_plugin(next_round_work_amount_hook_t *p) const;

    void set_baseline(size_t piid, pilot_reading_type_t rt,
                      double baseline_mean, size_t baseline_sample_size,
                      double baseline_var);
    int load_baseline_file(const char *filename);

    /**
     * Check if WPS analysis is enabled
     * @return true if WPS analysis is enabled
     */
    bool wps_enabled(void) const;

    /**
     * Calculate the average round duration to work amount ratio
     * \details There must be at least one round duration data.
     * @return the ratio
     */
    double duration_to_work_amount_ratio(void) const;

    /**
     * Get the current round_work_amount_soft_limit
     * @return
     */
    size_t get_round_work_amount_soft_limit(void) const;

    inline void set_required_ci_percent_of_mean(double percent_of_mean) {
        required_ci_percent_of_mean_ = percent_of_mean;
    }

    inline void set_required_ci_absolute_value(double absolute_value) {
        required_ci_absolute_value_ = absolute_value;
    }

    /**
     * Get the desired width of CI, taking both CI_perc and CI absolute value into account
     * @param the mean
     * @return the desired width of CI
     */
    inline double get_required_ci(double mean) const {
        refresh_analytical_result();
        double result = std::numeric_limits<double>::infinity();

        if (required_ci_percent_of_mean_ < 0 && required_ci_absolute_value_ < 0) {
            fatal_log << "Fatal: neither required CI (percent of mean) or required CI (absolute value) is set.";
            abort();
        }
        if (required_ci_percent_of_mean_ > 0) {
            result = required_ci_percent_of_mean_ * mean;
        }
        if (required_ci_absolute_value_ > 0) {
            result = std::min(result, required_ci_absolute_value_);
        }
        return result;
    }
};

struct pilot_pi_unit_readings_iter_t {
    const pilot_workload_t *wl_;
    size_t piid_;
    size_t cur_round_id_;
    size_t cur_unit_reading_id_;

    pilot_pi_unit_readings_iter_t (const pilot_workload_t *wl, size_t piid) :
        wl_(wl), piid_(piid), cur_round_id_(0), cur_unit_reading_id_(0) {
        ASSERT_VALID_POINTER(wl);
        if (piid >= wl->num_of_pi_) {
            fatal_log << __func__ << "() piid has invalid value " << piid;
            abort();
        }

        if (!pilot_pi_unit_readings_iter_valid(this))
            pilot_pi_unit_readings_iter_next(this);
    }

    pilot_pi_unit_readings_iter_t (const pilot_pi_unit_readings_iter_t &a) :
        wl_(a.wl_), piid_(a.piid_), cur_round_id_(a.cur_round_id_),
        cur_unit_reading_id_(a.cur_unit_reading_id_) {}

    inline double operator*() const {
        assert (piid_ >= 0);
        assert (cur_round_id_ >= 0);
        assert (cur_unit_reading_id_ >= 0);

        if (piid_ >= wl_->num_of_pi_) {
            fatal_log << "pi_unit_readings_iter has invalid piid";
            abort();
        }
        if (cur_round_id_ >= wl_->rounds_) {
            fatal_log << "pi_unit_readings_iter has invalid cur_round_id";
            abort();
        }
        if (cur_unit_reading_id_ >= wl_->unit_readings_[piid_][cur_round_id_].size() ||
            cur_unit_reading_id_ < wl_->warm_up_phase_len_[piid_][cur_round_id_]) {
            fatal_log << "pi_unit_readings_iter has invalid cur_unit_reading_id";
            abort();
        }

        return wl_->unit_readings_[piid_][cur_round_id_][cur_unit_reading_id_];
    }

    inline pilot_pi_unit_readings_iter_t& operator++() {
        bool round_has_changed = false;

        if (cur_round_id_ >= wl_->rounds_) return *this; /*false*/
        // find next non-empty round
        while (wl_->unit_readings_[piid_][cur_round_id_].size()
               - wl_->warm_up_phase_len_[piid_][cur_round_id_] == 0) {
    NEXT_ROUND:
            ++cur_round_id_;
            round_has_changed = true;
            cur_unit_reading_id_ = 0;
            if (cur_round_id_ >= wl_->rounds_) return *this; /*false*/
        }
        if (cur_unit_reading_id_ < wl_->warm_up_phase_len_[piid_][cur_round_id_]) {
            cur_unit_reading_id_ = wl_->warm_up_phase_len_[piid_][cur_round_id_];
            if (cur_unit_reading_id_ >= wl_->unit_readings_[piid_][cur_round_id_].size()) {
                fatal_log << "workload's warm-up phase longer than total round length";
                abort();
            }
            return *this; /*true*/
        }
        if (round_has_changed) return *this; /*true*/
        if (cur_unit_reading_id_ != wl_->unit_readings_[piid_][cur_round_id_].size() - 1) {
            ++cur_unit_reading_id_;
            return *this; /*true*/
        }
        goto NEXT_ROUND;
    }

    inline pilot_pi_unit_readings_iter_t operator++(int) {
        pilot_pi_unit_readings_iter_t res(*this);
        ++(*this);
        return res;
    }

    inline bool valid(void) const {
        if (piid_ >= wl_->num_of_pi_)
            return false;

        if (cur_round_id_ >= wl_->rounds_)
            return false;

        if (cur_unit_reading_id_ >= wl_->unit_readings_[piid_][cur_round_id_].size() ||
            cur_unit_reading_id_ < wl_->warm_up_phase_len_[piid_][cur_round_id_])
            return false;

        return true;
    }
};

} // namespace pilot

#endif /* LIB_PRIV_INCLUDE_WORKLOAD_HPP_ */
