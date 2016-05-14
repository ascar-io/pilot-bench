/*
 * workload.cc: functions for workload manipulation
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

#include <algorithm>
#include <chrono>
#include "common.h"
#include "csv.h"
#include "libpilot.h"
#include "libpilotcpp.h"
#include <numeric>
#include <sstream>
#include <vector>
#include "workload.hpp"

using namespace std;
using namespace pilot;

void pilot_workload_t::set_num_of_pi(size_t num_of_pi) {
    if (num_of_pi_ != 0) {
        fatal_log << "Changing the number of performance indices is not supported";
        abort();
    }
    num_of_pi_ = num_of_pi;
    pi_info_.resize(num_of_pi);
    readings_.resize(num_of_pi);
    unit_readings_.resize(num_of_pi);
    warm_up_phase_len_.resize(num_of_pi);
    total_num_of_unit_readings_.resize(num_of_pi);
    total_num_of_readings_.resize(num_of_pi);
    baseline_of_readings_.resize(num_of_pi);
    baseline_of_unit_readings_.resize(num_of_pi);
    analytical_result_.set_num_of_pi(num_of_pi);
    analytical_result_update_time_ = chrono::steady_clock::time_point::min();
}

double pilot_workload_t::unit_readings_mean(int piid) const {
    // TODO: add support for HARMONIC_MEAN
    return pilot_subsession_mean(pilot_pi_unit_readings_iter_t(this, piid),
                                 total_num_of_unit_readings_[piid], ARITHMETIC_MEAN);
}

double pilot_workload_t::unit_readings_var(int piid, size_t q) const {
    return pilot_subsession_var(pilot_pi_unit_readings_iter_t(this, piid),
                                total_num_of_unit_readings_[piid],
                                q,
                                unit_readings_mean(piid), ARITHMETIC_MEAN);
}

double pilot_workload_t::unit_readings_autocorrelation_coefficient(int piid, size_t q,
        pilot_mean_method_t mean_method) const {
    return pilot_subsession_autocorrelation_coefficient(pilot_pi_unit_readings_iter_t(this, piid),
                                                        total_num_of_unit_readings_[piid], q,
                                                        unit_readings_mean(piid), mean_method);
}

ssize_t pilot_workload_t::required_num_of_readings(int piid) const {
    if (calc_required_readings_func_) {
        return calc_required_readings_func_(this, piid);
    }

    if (total_num_of_readings_[piid] < 3) {
        debug_log << "Don't have enough data to calculate required readings sample size yet";
        return -1;
    }

    double ci_width;
    if (required_ci_percent_of_mean_ > 0) {
        double sm = pilot_subsession_mean(readings_[piid].begin(), readings_[piid].size(),
                                          pi_info_[piid].reading_mean_method);
        ci_width = sm * required_ci_percent_of_mean_;
    } else {
        ci_width = required_ci_absolute_value_;
    }

    size_t q, opt_sample_size;
    if (!pilot_optimal_sample_size(readings_[piid].begin(), readings_[piid].size(),
                                   ci_width, pi_info_[piid].reading_mean_method,
                                   &q, &opt_sample_size)) {
        debug_log << "Don't have enough data to calculate required readings sample size yet";
        return -1;
    } else {
        if (opt_sample_size < min_sample_size_) {
            debug_log << __func__ << "(): optimal sample size ("
                     << opt_sample_size << ") is smaller than the sample size lower threshold ("
                     << min_sample_size_
                     << "). Using the lower threshold instead.";
            opt_sample_size = min_sample_size_;
        }
        return q * opt_sample_size;
    }
}

ssize_t pilot_workload_t::required_num_of_unit_readings_for_comparison(int piid) const {
    assert (baseline_of_unit_readings_[piid].set);
    // refresh the analytical result
    refresh_analytical_result();
    ssize_t q = analytical_result_.unit_readings_optimal_subsession_size[piid];
    if (q < 0)
        return -1;

    size_t opt_sample_size;
    int res = pilot_optimal_sample_size_for_eq_test(baseline_of_unit_readings_[piid].mean,
            baseline_of_unit_readings_[piid].sample_size,
            baseline_of_unit_readings_[piid].var,
            analytical_result_.unit_readings_mean[piid],
            analytical_result_.unit_readings_num[piid] / q,
            analytical_result_.unit_readings_optimal_subsession_var[piid],
            desired_p_value_, &opt_sample_size);
    if (0 != res) {
        info_log << __func__ << "(): cannot calculate sample size for eq test";
        return -abs(res);
    }
    if (opt_sample_size < min_sample_size_) {
        info_log << __func__ << "(): optimal sample size for eq test ("
                 << opt_sample_size << ") is smaller than the sample size lower threshold ("
                 << min_sample_size_
                 << "). Using the lower threshold instead.";
        opt_sample_size = min_sample_size_;
    }
    return q * opt_sample_size;
}

ssize_t pilot_workload_t::required_num_of_unit_readings(int piid) const {
    if (calc_required_unit_readings_func_) {
        return calc_required_unit_readings_func_(this, piid);
    }

    double ci_width;
    if (required_ci_percent_of_mean_ > 0) {
        double sm = pilot_subsession_mean(pilot_pi_unit_readings_iter_t(this, piid),
                                          total_num_of_unit_readings_[piid],
                                          pi_info_[piid].unit_reading_mean_method);
        ci_width = sm * required_ci_percent_of_mean_;
    } else {
        ci_width = required_ci_absolute_value_;
    }

    size_t q, opt_sample_size;
    if (!pilot_optimal_sample_size(pilot_pi_unit_readings_iter_t(this, piid),
                                   total_num_of_unit_readings_[piid], ci_width,
                                   pi_info_[piid].unit_reading_mean_method,
                                   &q, &opt_sample_size)) {
        // we don't have enough data
        return -1;
    } else {
        if (opt_sample_size < min_sample_size_) {
            debug_log << __func__ << "(): optimal sample size ("
                     << opt_sample_size << ") is smaller than the sample size lower threshold ("
                     << min_sample_size_
                     << "). Using the lower threshold instead.";
            opt_sample_size = min_sample_size_;
        }
        return q * opt_sample_size;
    }

}

bool pilot_workload_t::calc_next_round_work_amount(size_t * const needed_work_amount) const {
    if (next_round_work_amount_hook_) {
        return next_round_work_amount_hook_(this, needed_work_amount);
    }
    bool more_rounds_needed = false;

    *needed_work_amount = 0;
    for (auto &r : runtime_analysis_plugins_) {
        if (!r.enabled || r.finished) continue;
        size_t nwa;
        if (r.calc_next_round_work_amount(this, &nwa)) {
            more_rounds_needed = true;
            *needed_work_amount = max(*needed_work_amount, nwa);
        }
    }

    if (0 == rounds_ && 0 == *needed_work_amount) {
        if (0 == max_work_amount_) {
            // workload doesn't support setting work amount
            return true;
        } else {
            *needed_work_amount = (0 == init_work_amount_) ? max_work_amount_ / 10 : init_work_amount_;
            return true;
        }
    } else {
        return more_rounds_needed;
    }
    SHOULD_NOT_REACH_HERE;
}

pilot_analytical_result_t* pilot_workload_t::get_analytical_result(pilot_analytical_result_t *info) const {
    refresh_analytical_result();
    if (!info) {
        info = new pilot_analytical_result_t(analytical_result_);
    } else {
        *info = analytical_result_;
    }
    return info;
}

void pilot_workload_t::refresh_analytical_result(void) const {
    if (raw_data_changed_time_ <= analytical_result_update_time_) {
        debug_log << "No need to refresh analytical result";
        return;
    }
    analytical_result_update_time_ = chrono::steady_clock::now();
    analytical_result_.num_of_pi = num_of_pi_;
    analytical_result_.num_of_rounds = rounds_;

    for (size_t piid = 0; piid < num_of_pi_; ++piid) {
        // Readings analysis
        analytical_result_.readings_num[piid] = readings_[piid].size();
        if (2 <= analytical_result_.readings_num[piid]) {
            analytical_result_.readings_mean_method[piid] = pi_info_[piid].reading_mean_method;
            analytical_result_.readings_mean[piid] = pilot_subsession_mean(readings_[piid].begin(),
                    readings_[piid].size(),
                    analytical_result_.readings_mean_method[piid]);
            double sm = analytical_result_.readings_mean[piid];
            analytical_result_.readings_mean_formatted[piid] = format_reading(piid, analytical_result_.readings_mean[piid]);
            analytical_result_.readings_num[piid] = readings_[piid].size();
            analytical_result_.readings_var[piid] = pilot_subsession_var(readings_[piid].begin(),
                    readings_[piid].size(),
                    1,
                    analytical_result_.readings_mean[piid],
                    analytical_result_.readings_mean_method[piid]);
            double var_rt = analytical_result_.readings_var[piid] / sm;
            analytical_result_.readings_var_formatted[piid] = analytical_result_.readings_mean_formatted[piid] * var_rt;
            analytical_result_.readings_autocorrelation_coefficient[piid] =
                    pilot_subsession_autocorrelation_coefficient(readings_[piid].begin(),
                            readings_[piid].size(), 1, analytical_result_.readings_mean[piid],
                            analytical_result_.readings_mean_method[piid]);
            analytical_result_.readings_optimal_subsession_size[piid] =
                    pilot_optimal_subsession_size(readings_[piid].begin(), readings_[piid].size(),
                            analytical_result_.readings_mean_method[piid]);
            if (analytical_result_.readings_optimal_subsession_size[piid] > 0) {
                analytical_result_.readings_optimal_subsession_var[piid] =
                        pilot_subsession_var(readings_[piid].begin(), readings_[piid].size(),
                                analytical_result_.readings_optimal_subsession_size[piid], analytical_result_.readings_mean[piid],
                                analytical_result_.readings_mean_method[piid]);
                double subsession_var_rt = analytical_result_.readings_optimal_subsession_var[piid] / sm;
                analytical_result_.readings_optimal_subsession_var_formatted[piid] = analytical_result_.readings_mean_formatted[piid] * subsession_var_rt;
                analytical_result_.readings_optimal_subsession_autocorrelation_coefficient[piid] =
                        pilot_subsession_autocorrelation_coefficient(readings_[piid].begin(),
                                readings_[piid].size(), analytical_result_.readings_optimal_subsession_size[piid],
                                analytical_result_.readings_mean[piid], analytical_result_.readings_mean_method[piid]);
                analytical_result_.readings_optimal_subsession_ci_width[piid] =
                        pilot_subsession_confidence_interval(readings_[piid].begin(),
                                readings_[piid].size(), analytical_result_.readings_optimal_subsession_size[piid],
                                confidence_level_, analytical_result_.readings_mean_method[piid]);
                double ci = analytical_result_.readings_optimal_subsession_ci_width[piid];
                double cif_low = format_reading(piid, sm - ci/2);
                double cif_high = format_reading(piid, sm + ci/2);
                analytical_result_.readings_optimal_subsession_ci_width_formatted[piid] = abs(cif_high - cif_low);
            }
            analytical_result_.readings_required_sample_size[piid] = required_num_of_readings(piid);
        }

        // Unit readings analysis
        double sm = unit_readings_mean(piid);
        analytical_result_.unit_readings_num[piid] = total_num_of_unit_readings_[piid];
        if (0 == total_num_of_unit_readings_[piid]) {
            // no data for the following calculation
            continue;
        }
        analytical_result_.unit_readings_mean_method[piid] = pi_info_[piid].unit_reading_mean_method;
        analytical_result_.unit_readings_mean[piid] = sm;
        analytical_result_.unit_readings_mean_formatted[piid] = format_unit_reading(piid, sm);
        analytical_result_.unit_readings_var[piid] = unit_readings_var(piid, 1);
        double var_rt = analytical_result_.unit_readings_var[piid] / sm;
        analytical_result_.unit_readings_var_formatted[piid] = var_rt * analytical_result_.unit_readings_mean_formatted[piid];
        analytical_result_.unit_readings_autocorrelation_coefficient[piid] = unit_readings_autocorrelation_coefficient(piid, 1, ARITHMETIC_MEAN);
        ssize_t q = pilot_optimal_subsession_size(pilot_pi_unit_readings_iter_t(this, piid),
                total_num_of_unit_readings_[piid], ARITHMETIC_MEAN);
        analytical_result_.unit_readings_optimal_subsession_size[piid] = q;
        if (analytical_result_.unit_readings_optimal_subsession_size[piid] > 0) {
            analytical_result_.unit_readings_optimal_subsession_var[piid] = unit_readings_var(piid, q);
            double subsession_var_rt = analytical_result_.unit_readings_optimal_subsession_var[piid] / sm;
            analytical_result_.unit_readings_optimal_subsession_var_formatted[piid] = subsession_var_rt * analytical_result_.unit_readings_mean_formatted[piid];
            analytical_result_.unit_readings_optimal_subsession_autocorrelation_coefficient[piid] = unit_readings_autocorrelation_coefficient(piid, q, ARITHMETIC_MEAN);
            analytical_result_.unit_readings_optimal_subsession_ci_width[piid] =
                    pilot_subsession_confidence_interval(pilot_pi_unit_readings_iter_t(this, piid), total_num_of_unit_readings_[piid], q, .95, ARITHMETIC_MEAN);
            double ci = analytical_result_.unit_readings_optimal_subsession_ci_width[piid];
            double cif_low = format_unit_reading(piid, sm - ci / 2);
            double cif_high = format_unit_reading(piid, sm + ci / 2);
            analytical_result_.unit_readings_optimal_subsession_ci_width_formatted[piid] = abs(cif_high - cif_low);
        }
        analytical_result_.unit_readings_required_sample_size[piid] = required_num_of_unit_readings(piid);
    }
    // WPS analysis data
    refresh_wps_analysis_results();
}

pilot_round_info_t* pilot_workload_t::round_info(size_t round, pilot_round_info_t *rinfo) const {
    die_if(round >= rounds_, ERR_WRONG_PARAM, string(__func__) + "(): round out-of-bound");
    if (!rinfo) {
        rinfo = (pilot_round_info_t*)malloc(sizeof(pilot_round_info_t));
        die_if(!rinfo, ERR_NOMEM, string(__func__) + "() cannot allocate memory");
        memset(rinfo, 0, sizeof(*rinfo));
    }
    rinfo->num_of_unit_readings = (size_t*)realloc(rinfo->num_of_unit_readings, sizeof(size_t) * num_of_pi_);
    rinfo->warm_up_phase_lens = (size_t*)realloc(rinfo->warm_up_phase_lens, sizeof(size_t) * num_of_pi_);

    rinfo->work_amount = round_work_amounts_[round];
    rinfo->round_duration = round_durations_[round];
    for (size_t piid = 0; piid < num_of_pi_; ++piid) {
        rinfo->num_of_unit_readings[piid] = unit_readings_[piid][round].size();
        rinfo->warm_up_phase_lens[piid] = warm_up_phase_len_[piid][round];
    }
    return rinfo;
}


char* pilot_workload_t::text_round_summary(size_t round) const {
    die_if(round >= rounds_, ERR_WRONG_PARAM, string(__func__) + "(): round out-of-bound");
    stringstream s;
    pilot_round_info_t *ri = round_info(round, NULL);
    for (size_t piid = 0; piid < num_of_pi_; ++piid) {
        if (piid != 0) s << endl;
        s << "# Performance Index " << piid << " #" << endl;
        s << "work amount: " << ri->work_amount << endl;
        s << "round duration: " << double(ri->round_duration) / ONE_SECOND << " s" << endl;
        s << "number of unit readings: " << ri->num_of_unit_readings[piid] << endl;
        s << "warm-up phase length: " << ri->warm_up_phase_lens[piid] << " units" << endl;
    }
    pilot_free_round_info(ri);
    size_t len = s.str().size() + 1;
    char *result = new char[len];
    memcpy(result, s.str().c_str(), len);
    return result;
}

char* pilot_workload_t::text_workload_summary(void) const {
    stringstream s;
    // refresh analytical result
    refresh_analytical_result();

    // Keep the text in Markdown format
    s << setprecision(4);
    for (size_t piid = 0; piid < num_of_pi_; ++piid) {
        double sm = analytical_result_.unit_readings_mean[piid];
        double smf = analytical_result_.unit_readings_mean_formatted[piid];
        double var = analytical_result_.unit_readings_var[piid];
        size_t cur_ur = analytical_result_.unit_readings_num[piid];
        if (piid != 0) s << endl;
        s << "# Performance Index " << piid << " #" << endl;
        s << "sample size: " << cur_ur << endl;
        if (0 == cur_ur)
            continue;
        s << "sample mean: " << analytical_result_.unit_readings_mean_formatted[piid] << " " << pi_info_[piid].unit << endl;
        double var_rt = var / sm;
        s << "sample variance: " << analytical_result_.unit_readings_var_formatted[piid] << " " << pi_info_[piid].unit << endl;
        s << "sample variance to sample mean ratio: " << var_rt * 100 << "%" << endl;
        s << "sample autocorrelation coefficient: " << analytical_result_.unit_readings_autocorrelation_coefficient[piid] << endl;
        size_t q = analytical_result_.unit_readings_optimal_subsession_size[piid];
        s << "optimal subsession size (q): " << q << endl;
        s << "subsession variance (q=" << q << "): " << analytical_result_.unit_readings_optimal_subsession_var_formatted[piid] << endl;
        s << "subsession variance (q=" << q << ") to sample mean ratio: " << analytical_result_.unit_readings_optimal_subsession_var[piid] * 100 / sm << "%" << endl;
        size_t min_ur = analytical_result_.unit_readings_required_sample_size[piid];
        s << "minimum numbers of unit readings required (q=" << q << "): " << min_ur << endl;
        s << "current number of significant unit readings: " << cur_ur << endl;
        if (cur_ur >= min_ur) {
            s << "We have a large enough sample size." << endl;
        } else {
            if (cur_ur / q < min_sample_size_) {
                s << "sample size is smaller than the sample size threshold (" << min_sample_size_ << ")" << endl;
            } else if (cur_ur < min_ur) {
                s << "sample size is not yet large enough to achieve a confidence interval width of " << required_ci_percent_of_mean_ << " * sample_mean." << endl;
            }
        }
        double ci = analytical_result_.unit_readings_optimal_subsession_ci_width_formatted[piid];
        double ci_low = smf - ci / 2;
        double ci_high = smf + ci / 2;
        s << "95% confidence interval: [" << ci_low << ", " << ci_high << "] " << pi_info_[piid].unit << endl;
        s << "95% confidence interval width: " << ci << " " << pi_info_[piid].unit << endl;
        s << "95% confidence interval width is " << analytical_result_.unit_readings_optimal_subsession_ci_width[piid] * 100 / sm << "% of sample_mean" << endl;
    }

    s << "== WORK-PER-SECOND ANALYSIS ==" << endl;
    s << "naive mean: " << analytical_result_.wps_harmonic_mean_formatted << endl;
    s << "naive mean err: " << analytical_result_.wps_naive_v_err << " (" << analytical_result_.wps_naive_v_err_percent * 100 <<  "%)" << endl;
    if (analytical_result_.wps_has_data) {
        s << "WPS alpha: " << analytical_result_.wps_alpha_formatted << endl;
        s << "WPS v: " << analytical_result_.wps_v_formatted << endl;
        s << "WPS v CI: " << analytical_result_.wps_v_ci_formatted << endl;
        s << "WPS err: " << analytical_result_.wps_err << " (" << analytical_result_.wps_err_percent << "%)" << endl;
    } else {
        s << "Not enough data for WPS analysis" << endl;
    }

    /* wi_.wps_v_dw_method = -1 if not enough data */
    if (analytical_result_.wps_v_dw_method > 0 && analytical_result_.wps_v_ci_dw_method > 0) {
        s << "warm-up removed v (dw mtd): " << analytical_result_.wps_v_dw_method << endl;
        s << "v CI width (dw mth): " << analytical_result_.wps_v_ci_dw_method << endl;
    } else {
        s << "Not enough data for dw method analysis" << endl;
    }

    size_t len = s.str().size() + 1;
    char *result = new char[len];
    memcpy(result, s.str().c_str(), len);
    return result;
}

void pilot_workload_t::set_wps_analysis(bool enabled, bool wps_must_satisfy) {
    if (wps_must_satisfy && !enabled) {
        fatal_log << __func__ << "(): WPS analysis is not enabled yet satisfaction is required";
        abort();
    }
    wps_must_satisfy_ = wps_must_satisfy;
    load_runtime_analysis_plugin(calc_next_round_work_amount_from_wps, enabled);
}

void pilot_workload_t::refresh_wps_analysis_results(void) const {
    if (rounds_ < 3) {
        debug_log << __func__ << "(): need more than 3 rounds data for wps analysis";
        return;
    }
    // calculate naive_v and its error
    size_t sum_of_work_amount =
            accumulate(round_work_amounts_.begin(), round_work_amounts_.end(), static_cast<size_t>(0));
    nanosecond_type sum_of_round_durations =
            accumulate(round_durations_.begin(), round_durations_.end(), static_cast<nanosecond_type>(0));
    analytical_result_.wps_harmonic_mean = double(sum_of_work_amount) / ( double(sum_of_round_durations) / ONE_SECOND );
    analytical_result_.wps_harmonic_mean_formatted = format_wps(analytical_result_.wps_harmonic_mean);
    analytical_result_.wps_naive_v_err = 0;
    for (size_t i = 0; i < rounds_; ++i) {
        double wa  = double(round_work_amounts_[i]);
        double dur = double(round_durations_[i]) / ONE_SECOND;
        analytical_result_.wps_naive_v_err += pow(wa / analytical_result_.wps_harmonic_mean - dur, 2);
    }
    analytical_result_.wps_naive_v_err_percent = sqrt(analytical_result_.wps_naive_v_err) / sum_of_round_durations;

    // the obsoleted dw method
    pilot_wps_warmup_removal_dw_method(round_work_amounts_.size(),
            round_work_amounts_.begin(),
            round_durations_.begin(), confidence_interval_,
            autocorrelation_coefficient_limit_,
            &analytical_result_.wps_v_dw_method,
            &analytical_result_.wps_v_ci_dw_method);

    // the linear regression method
    nanosecond_type duration_threshold;
    int r = 0;
    do {
        if (analytical_result_.wps_has_data) {
            duration_threshold = analytical_result_.wps_alpha < 0? nanosecond_type(-analytical_result_.wps_alpha) : 0;
        } else {
            duration_threshold = 0;
        }
        debug_log << __func__ << "(): round " << r << " WPS regression (duration_threshold = " << duration_threshold << ")";
        int res = pilot_wps_warmup_removal_lr_method(round_work_amounts_.size(),
                                                     round_work_amounts_.begin(),
                                                     round_durations_.begin(),
                                                     autocorrelation_coefficient_limit_,
                                                     duration_threshold,
                                                     &analytical_result_.wps_alpha,
                                                     &analytical_result_.wps_v,
                                                     &analytical_result_.wps_v_ci,
                                                     &analytical_result_.wps_err,
                                                     &analytical_result_.wps_err_percent,
                                                     &analytical_result_.wps_subsession_sample_size);
        if (ERR_NOT_ENOUGH_DATA == res) {
            debug_log << "Not enough data for calculating WPS warm-up removal (duration_threshold = " << duration_threshold << ")";
            analytical_result_.wps_has_data = false;
            analytical_result_.wps_alpha = -1;
            analytical_result_.wps_v = -1;
            analytical_result_.wps_v_ci = -1;
            return;
        } else {
            analytical_result_.wps_has_data = true;
            analytical_result_.wps_alpha_formatted = format_wps(analytical_result_.wps_alpha);
            analytical_result_.wps_v_formatted = format_wps(analytical_result_.wps_v);
            double v_ci_low  = format_wps(analytical_result_.wps_v - analytical_result_.wps_v_ci / 2);
            double v_ci_high = format_wps(analytical_result_.wps_v + analytical_result_.wps_v_ci / 2);
            analytical_result_.wps_v_ci_formatted = abs(v_ci_high - v_ci_low);
        }
    } while (analytical_result_.wps_has_data &&
             analytical_result_.wps_alpha < 0 &&
             duration_threshold != static_cast<nanosecond_type>(-analytical_result_.wps_alpha));
}

size_t pilot_workload_t::set_session_duration_limit(size_t sec) {
    size_t old_val = session_duration_limit_in_sec_;
    session_duration_limit_in_sec_ = sec;
    return old_val;
}

size_t pilot_workload_t::set_min_sample_size(size_t min_sample_size) {
    size_t old_min_sample_size = min_sample_size_;
    min_sample_size_ = min_sample_size;
    // invalidate the cache because re-calculation is needed
    analytical_result_update_time_ = chrono::steady_clock::time_point::min();
    return old_min_sample_size;
}

void pilot_workload_t::load_runtime_analysis_plugin(next_round_work_amount_hook_t *p, bool enabled) const {
    for (auto &c : runtime_analysis_plugins_) {
        if (p == c.calc_next_round_work_amount) {
            c.enabled = enabled;
            return;
        }
    }
    // not found, need to add it
    runtime_analysis_plugins_.emplace_back(enabled, p);
}

void pilot_workload_t::finish_runtime_analysis_plugin(next_round_work_amount_hook_t *p) const {
    for (auto &c : runtime_analysis_plugins_) {
        if (p == c.calc_next_round_work_amount) {
            c.finished = true;
            return;
        }
    }
    fatal_log << "Trying to set a non-exist plugin as finished: " << p;
    abort();
}

void pilot_workload_t::set_baseline(size_t piid, pilot_reading_type_t rt,
        double baseline_mean, size_t baseline_sample_size, double baseline_var) {
    if (num_of_pi_ <= piid) {
        fatal_log << __func__ << "(): invalid parameter: piid: " << piid;
        abort();
    }
    switch (rt) {
    case READING_TYPE:
        baseline_of_readings_[piid].mean = baseline_mean;
        baseline_of_readings_[piid].sample_size = baseline_sample_size;
        baseline_of_readings_[piid].var = baseline_var;
        baseline_of_readings_[piid].set = true;
        break;
    case UNIT_READING_TYPE:
        baseline_of_unit_readings_[piid].mean = baseline_mean;
        baseline_of_unit_readings_[piid].sample_size = baseline_sample_size;
        baseline_of_unit_readings_[piid].var = baseline_var;
        baseline_of_unit_readings_[piid].set = true;
        break;
    case WPS_TYPE:
        fatal_log << __func__ << "(): unimplemented yet";
        abort();
        break;
    default:
        fatal_log << __func__ << "(): invalid parameter: rt: " << rt;
        abort();
    }
    enable_runtime_analysis_plugin(&calc_next_round_work_amount_for_comparison);
}


int pilot_workload_t::load_baseline_file(const char *filename) {
    // piid, readings_num, readings_mean, reading_var, ur_num, ur_mean, ur_var
    typedef tuple<size_t, ssize_t, double, double, ssize_t, double, double> rec_t;
    size_t piid;
    ssize_t r_num, ur_num;
    double r_mean, r_var, ur_mean, ur_var;
    vector<rec_t> data;

    ASSERT_VALID_POINTER(filename);
    using namespace csv;

    debug_log << __func__ << "(): starting to load baseline from " << filename;
    CSVReader<14> in(filename);
    in.read_header(ignore_missing_column, "piid", "readings_num",
            "readings_mean", "readings_mean_formatted", "readings_var",
            "readings_var_formatted", "unit_readings_num",
            "unit_readings_mean", "unit_readings_mean_formatted",
            "unit_readings_var", "unit_readings_var_formatted",
            "unit_readings_ci_width", "unit_readings_ci_width_formatted",
            "unit_readings_optimal_subsession_size");
    double ignore;
    r_num = ur_num = -1;
    r_mean = r_var = ur_mean = ur_var = 0;
    while (in.read_row(piid, r_num, r_mean, ignore, r_var, ignore,
                      ur_num, ur_mean, ignore, ur_var, ignore,
                      ignore, ignore, ignore)) {
        data.emplace_back(piid, r_num, r_mean, r_var,
                          ur_num, ur_mean, ur_var);
        r_num = ur_num = -1;
    }

    if (0 != num_of_pi_ && data.size() > num_of_pi_) {
        fatal_log << __func__ << "(): the input file (" << filename
                  << ") has " << data.size() << " lines, which are greater "
                  << "than the number of PIs (" << num_of_pi_ << "). "
                  << "Are you trying to load the wrong file?";
        abort();
    }
    if (0 == num_of_pi_) {
        set_num_of_pi(data.size());
    }
    int line = 0;
    for (const auto &a : data) {
        // check piid
        piid = get<0>(a);
        if (piid >= num_of_pi_) {
            fatal_log << __func__ << "(): PIID out of range at line " << line;
            abort();
        }
        // process readings
        r_num = get<1>(a);
        if (r_num <= 0) {
            baseline_of_readings_[piid].set = false;
        } else {
            baseline_of_readings_[piid].set = true;
            r_mean = get<2>(a);
            r_var  = get<3>(a);
            baseline_of_readings_[piid].sample_size = r_num;
            baseline_of_readings_[piid].mean = r_mean;
            baseline_of_readings_[piid].var = r_var;
        }
        // process unit readings
        ur_num = get<4>(a);
        if (ur_num <= 0) {
            baseline_of_unit_readings_[piid].set = false;
        } else {
            baseline_of_unit_readings_[piid].set = true;
            ur_mean = get<5>(a);
            ur_var  = get<6>(a);
            baseline_of_unit_readings_[piid].sample_size = ur_num;
            baseline_of_unit_readings_[piid].mean = ur_mean;
            baseline_of_unit_readings_[piid].var = ur_var;
            info_log << __func__ << "(): loaded unit reading baseline for PI "
                     << piid << ": mean " << ur_mean << ", sample_size "
                     << ur_num << ", var " << ur_var;
        }

        ++line;
    }
    enable_runtime_analysis_plugin(&calc_next_round_work_amount_for_comparison);
    return 0;
}
