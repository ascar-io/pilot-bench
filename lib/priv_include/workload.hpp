/*
 * workload.hpp: functions for workload manipulation
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

#ifndef LIB_PRIV_INCLUDE_WORKLOAD_HPP_
#define LIB_PRIV_INCLUDE_WORKLOAD_HPP_

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/timer/timer.hpp>
#include <functional>
#include <vector>
#include "common.h"
#include "libpilot.h"

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
    bool wps_must_satisfy;

    pilot_pi_info_t(std::string _name = "", std::string _unit = "",
           pilot_pi_display_format_func_t *_r_format_func = NULL,
           pilot_pi_display_format_func_t *_ur_format_func = NULL,
           bool _r_sat = false, bool _ur_sat = false,
           bool _wps_sat = false) :
        name(_name), unit(_unit), format_reading(_r_format_func),
        format_unit_reading(_ur_format_func),
        reading_must_satisfy(_r_sat), unit_reading_must_satisfy(_ur_sat),
        wps_must_satisfy(_wps_sat) {}
};

typedef size_t calc_next_round_work_amount_func_t(pilot_workload_t* wl);
struct runtime_analysis_plugin_t {
    bool enabled;
    calc_next_round_work_amount_func_t *calc_next_round_work_amount;

    runtime_analysis_plugin_t(calc_next_round_work_amount_func_t *c) :
        enabled(true), calc_next_round_work_amount(c) {}
    runtime_analysis_plugin_t(bool e, calc_next_round_work_amount_func_t *c) :
        enabled(e), calc_next_round_work_amount(c) {}
};

struct pilot_workload_t {
    std::string workload_name_;

    typedef std::vector<double> reading_data_t; //! The data of one reading of all rounds
    typedef std::vector<double> unit_reading_data_per_round_t;
    typedef std::vector<unit_reading_data_per_round_t> unit_reading_data_t; //! Per round unit reading data

    size_t num_of_pi_;                      //! Number of performance indices to collect for each round
    size_t rounds_;                         //! Number of rounds we've done so far
    size_t init_work_amount_;
    size_t max_work_amount_;
    size_t min_work_amount_;
    pilot_workload_func_t *workload_func_;
    std::vector<pilot_pi_info_t> pi_info_;
    pilot_display_format_functor format_wps_;
    bool wps_must_satisfy_;

    PilotTUI *tui_;

    std::vector<boost::timer::nanosecond_type> round_durations_; //! The duration of each round
    std::vector<reading_data_t> readings_;       //! Reading data of each round. Format: readings_[piid][round_id].
    std::vector<unit_reading_data_t> unit_readings_; //! Unit reading data of each round. Format: unit_readings_[piid][round_id].
    std::vector<std::vector<size_t> > warm_up_phase_len_;  //! The length of the warm-up phase of each PI per round. Format: warm_up_phase_len_[piid][round_id].
    std::vector<size_t> total_num_of_unit_readings_; //! Total number of unit readings per PI
    std::vector<size_t> total_num_of_readings_;      //! Total number of readings per PI
    std::vector<size_t> round_work_amounts_;      //! The work amount we used in each round

    double confidence_interval_;
    double confidence_level_;
    double autocorrelation_coefficient_limit_;
    double required_ci_percent_of_mean_;
    double required_ci_absolute_value_;
    size_t session_duration_limit_in_sec_;

    nanosecond_type short_round_detection_threshold_;
    bool short_workload_check_;
    pilot_warm_up_removal_detection_method_t warm_up_removal_detection_method_;
    double warm_up_removal_moving_average_window_size_in_seconds_;
    size_t wholly_rejected_rounds_;  //! number of rounds that are wholly rejected due to too short a duration

    // WPS analysis bookkeeping
    size_t wps_slices_;
    mutable bool wps_has_data_;
    mutable size_t wps_subsession_sample_size_;
    mutable double wps_harmonic_mean_;
    mutable double wps_alpha_;
    mutable double wps_v_;
    mutable double wps_v_ci_;
    mutable double wps_v_dw_method_;
    mutable double wps_v_ci_dw_method_;

    // Hook functions
    next_round_work_amount_hook_t *next_round_work_amount_hook_; //! The hook function that calculates the work amount for next round
    general_hook_func_t *hook_pre_workload_run_;
    general_hook_func_t *hook_post_workload_run_;
    calc_required_readings_func_t *calc_required_readings_func_;
    calc_required_readings_func_t *calc_required_unit_readings_func_;

    std::vector<runtime_analysis_plugin_t> runtime_analysis_plugins_;

    pilot_workload_t(const char *wl_name) :
                         num_of_pi_(0), rounds_(0), init_work_amount_(0),
                         max_work_amount_(0), min_work_amount_(0),
                         workload_func_(nullptr),
                         wps_must_satisfy_(true), tui_(NULL),
                         confidence_interval_(0.05), confidence_level_(.95),
                         autocorrelation_coefficient_limit_(0.1),
                         required_ci_percent_of_mean_(0.1), required_ci_absolute_value_(-1),
                         session_duration_limit_in_sec_(0),
                         short_round_detection_threshold_(1 * pilot::ONE_SECOND),
                         short_workload_check_(true),
                         warm_up_removal_detection_method_(FIXED_PERCENTAGE),
                         warm_up_removal_moving_average_window_size_in_seconds_(3),
                         wholly_rejected_rounds_(0),
                         wps_slices_(kWPSInitSlices), wps_has_data_(false),
                         wps_subsession_sample_size_(0),
                         wps_alpha_(0), wps_v_(0), wps_v_ci_(0),
                         wps_v_dw_method_(-1), wps_v_ci_dw_method_(-1),
                         next_round_work_amount_hook_(NULL),
                         hook_pre_workload_run_(NULL), hook_post_workload_run_(NULL),
                         calc_required_readings_func_(NULL),
                         calc_required_unit_readings_func_(NULL) {
        if (wl_name) workload_name_ = wl_name;
        runtime_analysis_plugins_.emplace_back(true, &calc_next_round_work_amount_from_readings);
        runtime_analysis_plugins_.emplace_back(true, &calc_next_round_work_amount_from_unit_readings);
        runtime_analysis_plugins_.emplace_back(true, &calc_next_round_work_amount_from_wps);
    }

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
     * Calculate the required number of unit readings
     * @return
     */
    ssize_t required_num_of_unit_readings(int piid) const;

    size_t calc_next_round_work_amount(void);

    inline double calc_avg_work_unit_per_amount(int piid) const {
        size_t total_work_units = 0;
        size_t total_work_amount = 0;
        for (auto const & c : unit_readings_[piid])
            total_work_units += c.size();
        for (auto const & c : round_work_amounts_)
            total_work_amount += c;
        double res = (double)total_work_amount / total_work_units;
        info_log << "[PI " << piid << "] average work per unit reading: " << res;
        return res;
    }

    /**
     * \brief Get the basic and statistics information of a workload
     * \details If info is NULL, this function allocates memory for a
     * pilot_workload_info_t, fills information, and returns it. If
     * info is provided, its information will be updated and no new
     * memory will be allocated. The allocated memory must eventually
     * be freed by using pilot_free_workload_info().
     * @param info (optional) if provided, it must point to a pilot_workload_info_t
     * that was returned by a previous call to pilot_workload_info()
     * @return a pointer to a pilot_workload_info_t struct. You must use
     * pilot_free_workload_info() to free it (you shall not use delete).
     */
    pilot_workload_info_t* workload_info(pilot_workload_info_t *winfo = NULL) const;

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

    bool set_wps_analysis(bool enabled);
    void refresh_wps_analysis_results(void) const;

    size_t set_session_duration_limit(size_t sec);

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
