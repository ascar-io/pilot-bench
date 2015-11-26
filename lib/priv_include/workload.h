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

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/timer/timer.hpp>
#include <functional>
#include <vector>
#include "common.h"
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
    std::vector<std::string> pi_units_;

    std::vector<boost::timer::nanosecond_type> round_durations_; //! The duration of each round
    std::vector<reading_data_t> readings_;       //! Reading data of each round. Format: readings_[piid][round_id].
    std::vector<unit_reading_data_t> unit_readings_; //! Unit reading data of each round. Format: unit_readings_[piid][round_id].
    std::vector<std::vector<size_t> > warm_up_phase_len_;  //! The length of the warm-up phase of each PI per round. Format: warm_up_phase_len_[piid][round_id].
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
    std::vector<pilot_pi_unit_reading_format_func_t*> unit_reading_format_funcs_;
    std::vector<pilot_pi_reading_format_func_t*> reading_format_funcs_;

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

    double unit_readings_autocorrelation_coefficient(int piid, size_t q) const;

    /**
     * Calculate the required number of unit readings
     * @return
     */
    ssize_t required_num_of_unit_readings(int piid) const;

    size_t calc_next_round_work_amount(void) const;

    inline double calc_avg_work_unit_per_amount(int piid) const {
        size_t total_work_units = 0;
        size_t total_work_amount = 0;
        for (auto const & c : unit_readings_[piid])
            total_work_units += c.size();
        for (auto const & c : work_amount_per_round_)
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

    inline double format_unit_reading(const int piid, const double ur) const {
        return unit_reading_format_funcs_[piid](this, ur);
    }
    inline double format_reading(const int piid, const double ur) const {
        return reading_format_funcs_[piid](this, ur);
    }
};

struct pilot_pi_unit_readings_iter_t {
    const pilot_workload_t *wl_;
    int piid_;
    int cur_round_id_;
    int cur_unit_reading_id_;

    pilot_pi_unit_readings_iter_t (const pilot_workload_t *wl, int piid) :
        wl_(wl), piid_(piid), cur_round_id_(0), cur_unit_reading_id_(0) {
        ASSERT_VALID_POINTER(wl);
        if (piid < 0 || piid >= wl->num_of_pi_) {
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
        while (wl_->unit_readings_[piid_][cur_round_id_].size() == 0) {
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
        if (piid_ < 0 || piid_ >= wl_->num_of_pi_)
            return false;

        if (cur_round_id_ < 0 || cur_round_id_ >= wl_->rounds_)
            return false;

        if (cur_unit_reading_id_ >= wl_->unit_readings_[piid_][cur_round_id_].size() ||
            cur_unit_reading_id_ < wl_->warm_up_phase_len_[piid_][cur_round_id_])
            return false;

        return true;
    }
};

#endif /* LIB_PRIV_INCLUDE_WORKLOAD_H_ */