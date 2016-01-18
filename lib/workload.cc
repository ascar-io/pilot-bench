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
#include "common.h"
#include "libpilot.h"
#include "libpilotcpp.h"
#include <numeric>
#include <sstream>
#include "workload.hpp"

using namespace std;
using namespace pilot;

double pilot_workload_t::unit_readings_mean(int piid) const {
    return pilot_subsession_mean(pilot_pi_unit_readings_iter_t(this, piid),
                                 total_num_of_unit_readings_[piid]);
}

double pilot_workload_t::unit_readings_var(int piid, size_t q) const {
    return pilot_subsession_var(pilot_pi_unit_readings_iter_t(this, piid),
                                total_num_of_unit_readings_[piid],
                                q,
                                unit_readings_mean(piid));
}

double pilot_workload_t::unit_readings_autocorrelation_coefficient(int piid, size_t q) const {
    return pilot_subsession_autocorrelation_coefficient(pilot_pi_unit_readings_iter_t(this, piid),
                                                        total_num_of_unit_readings_[piid], q,
                                                        unit_readings_mean(piid));
}

ssize_t pilot_workload_t::required_num_of_unit_readings(int piid) const {
    double ci_width;
    if (required_ci_percent_of_mean_ > 0) {
        double sm = pilot_subsession_mean(pilot_pi_unit_readings_iter_t(this, piid),
                                          total_num_of_unit_readings_[piid]);
        ci_width = sm * required_ci_percent_of_mean_;
    } else {
        ci_width = required_ci_absolute_value_;
    }

    return pilot_optimal_sample_size(pilot_pi_unit_readings_iter_t(this, piid),
                                     total_num_of_unit_readings_[piid], ci_width);
}

size_t pilot_workload_t::calc_next_round_work_amount(void) const {
    if (0 == rounds_)
        return 0 == init_work_amount_ ? work_amount_limit_ / 10 : init_work_amount_;

    size_t max_work_amount = 0;
    size_t num_of_readings_needed;
    size_t work_amount_needed;
    for (int piid = 0; piid != num_of_pi_; ++piid) {
        if (abs(calc_avg_work_unit_per_amount(piid)) < 0.00000001) {
            error_log << "[PI " << piid << "] average work per unit reading is 0, using work_amount_limit instead (you probably need to report a bug)";
            return work_amount_limit_;
        }
        if (0 != total_num_of_unit_readings_[piid]) {
            ssize_t req = calc_unit_readings_required_func_(this, piid);
            info_log << "[PI " << piid << "] required unit readings sample size: " << req;
            if (req > 0) {
                if (req < total_num_of_unit_readings_[piid]) {
                    info_log << "[PI " << piid << "] already has enough samples";
                    return 0;
                }
                num_of_readings_needed = req - total_num_of_unit_readings_[piid];
            } else {
                // in case when there's not enough data to calculate the number of UR,
                // we try to double the total number of unit readings
                info_log << "[PI " << piid << "] doesn't have enough information for calculating required sample size, using the current total sample size instead";
                num_of_readings_needed = total_num_of_unit_readings_[piid];
            }
        } else {
            // this PI has no unit readings, try readings
            if (0 != total_num_of_readings_[piid] ) {
                num_of_readings_needed = calc_readings_required_func_(this, piid)
                                         - rounds_;
            } else {
                info_log << "[PI " << piid << "] has no usable data readings, "
                        "using using work_amount_limit instead (workload is "
                        "probably too short so all readings are considered warm-up)";
                return work_amount_limit_;
            }
        }
        work_amount_needed = size_t(1.2 * num_of_readings_needed
                                        * calc_avg_work_unit_per_amount(piid));
        if (work_amount_needed >= work_amount_limit_)
            return work_amount_limit_;
        max_work_amount = max(work_amount_needed, max_work_amount);
    }
    return max_work_amount;
}

pilot_workload_info_t* pilot_workload_t::workload_info(pilot_workload_info_t *info) const {
    if (!info) {
        info = new pilot_workload_info_t;
    }
    info->num_of_pi = num_of_pi_;
    info->num_of_rounds = rounds_;

#define INIT_FIELD(field) field = (typeof(field[0])*)realloc(field, sizeof(field[0]) * num_of_pi_)

    INIT_FIELD(info->total_num_of_unit_readings);
    INIT_FIELD(info->unit_readings_mean);
    INIT_FIELD(info->unit_readings_var);
    INIT_FIELD(info->unit_readings_autocorrelation_coefficient);
    INIT_FIELD(info->unit_readings_optimal_subsession_size);
    INIT_FIELD(info->unit_readings_optimal_subsession_var);
    INIT_FIELD(info->unit_readings_optimal_subsession_autocorrelation_coefficient);
    INIT_FIELD(info->unit_readings_optimal_subsession_confidence_interval);
    INIT_FIELD(info->unit_readings_required_sample_size);
    if (readings_.size() > 0) {
        INIT_FIELD(info->readings_arithmetic_mean);
        INIT_FIELD(info->readings_harmonic_mean);
        INIT_FIELD(info->readings_var);
        INIT_FIELD(info->readings_autocorrelation_coefficient);
        INIT_FIELD(info->readings_optimal_subsession_size);
        INIT_FIELD(info->readings_optimal_subsession_var);
        INIT_FIELD(info->readings_optimal_subsession_autocorrelation_coefficient);
        INIT_FIELD(info->readings_optimal_subsession_confidence_interval);
    } else {
        info->readings_arithmetic_mean = NULL;
        info->readings_harmonic_mean = NULL;
        info->readings_var = NULL;
        info->readings_autocorrelation_coefficient = NULL;
        info->readings_optimal_subsession_size = NULL;
        info->readings_optimal_subsession_var = NULL;
        info->readings_optimal_subsession_autocorrelation_coefficient = NULL;
        info->readings_optimal_subsession_confidence_interval = NULL;
    }
#undef INIT_FIELD

    for (int piid = 0; piid < num_of_pi_; ++piid) {
        double sm = unit_readings_mean(piid);
        info->total_num_of_unit_readings[piid] = total_num_of_unit_readings_[piid];
        if (0 == total_num_of_unit_readings_[piid]) {
            // no data for the following calculation
            continue;
        }
        info->unit_readings_mean[piid] = sm;
        info->unit_readings_var[piid] = unit_readings_var(piid, 1);
        info->unit_readings_autocorrelation_coefficient[piid] = unit_readings_autocorrelation_coefficient(piid, 1);
        size_t q = pilot_optimal_subsession_size(pilot_pi_unit_readings_iter_t(this, piid),
                                                 total_num_of_unit_readings_[piid]);
        info->unit_readings_optimal_subsession_size[piid] = q;
        info->unit_readings_optimal_subsession_var[piid] = unit_readings_var(piid, q);
        info->unit_readings_optimal_subsession_autocorrelation_coefficient[piid] = unit_readings_autocorrelation_coefficient(piid, q);
        info->unit_readings_optimal_subsession_confidence_interval[piid] =
                pilot_subsession_confidence_interval(pilot_pi_unit_readings_iter_t(this, piid), total_num_of_unit_readings_[piid], q, .95);
        info->unit_readings_required_sample_size[piid] = required_num_of_unit_readings(piid);
    }
    // generate WPS analysis data
    double sum_of_work_amount =
            accumulate(round_work_amounts_.begin(), round_work_amounts_.end(), 0);
    boost::timer::nanosecond_type sum_of_round_durations =
            accumulate(round_durations_.begin(), round_durations_.end(), 0);
    info->wps_harmonic_mean = sum_of_work_amount / sum_of_round_durations;

    if (rounds_ >= 2) {
        pilot_wps_warmup_removal_dw_method(round_work_amounts_.size(),
            round_work_amounts_.begin(),
            round_durations_.begin(), confidence_interval_,
            autocorrelation_coefficient_limit_,
            &(info->wps_v_dw_method), &(info->wps_v_ci_dw_method));
    } else {
        info->wps_v_dw_method = -1;
        info->wps_v_ci_dw_method = -1;
    }

    return info;
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
    for (int piid = 0; piid < num_of_pi_; ++piid) {
        rinfo->num_of_unit_readings[piid] = unit_readings_[piid][round].size();
        rinfo->warm_up_phase_lens[piid] = warm_up_phase_len_[piid][round];
    }
    return rinfo;
}


char* pilot_workload_t::text_round_summary(size_t round) const {
    die_if(round >= rounds_, ERR_WRONG_PARAM, string(__func__) + "(): round out-of-bound");
    stringstream s;
    pilot_round_info_t *ri = round_info(round, NULL);
    for (int piid = 0; piid < num_of_pi_; ++piid) {
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
    pilot_workload_info_t *i = workload_info();

    // Keep the text in Markdown format
    s << setprecision(4);
    for (int piid = 0; piid < num_of_pi_; ++piid) {
        double sm = i->unit_readings_mean[piid];
        double var = i->unit_readings_var[piid];
        size_t cur_ur = i->total_num_of_unit_readings[piid];
        if (piid != 0) s << endl;
        s << "# Performance Index " << piid << " #" << endl;
        s << "sample size: " << cur_ur << endl;
        if (0 == cur_ur)
            continue;
        s << "sample mean: " << format_unit_reading(piid, sm) << " " << pi_info_[piid].unit << endl;
        double var_rt = var / sm;
        s << "sample variance: " << format_unit_reading(piid, sm) * var_rt << " " << pi_info_[piid].unit << endl;
        s << "sample variance to sample mean ratio: " << var_rt * 100 << "%" << endl;
        s << "sample autocorrelation coefficient: " << i->unit_readings_autocorrelation_coefficient[piid] << endl;
        size_t q = i->unit_readings_optimal_subsession_size[piid];
        s << "optimal subsession size (q): " << q << endl;
        double subvar = i->unit_readings_optimal_subsession_var[piid];
        s << "subsession variance (q=" << q << "): " << subvar << endl;
        s << "subsession variance (q=" << q << ") to sample mean ratio: " << subvar * 100 / sm << "%" << endl;
        size_t min_ur = i->unit_readings_required_sample_size[piid];
        s << "minimum numbers of unit readings according to t-distribution (q=" << q << "): " << min_ur << endl;
        s << "current number of significant unit readings: " << cur_ur << endl;
        if (cur_ur >= min_ur && cur_ur / q >= 200) {
            s << "We have a large enough sample size." << endl;
        } else {
            if (cur_ur < min_ur) {
                s << "sample size is not yet large enough to achieve a confidence interval width of " << required_ci_percent_of_mean_ << " * sample_mean." << endl;
            } else {
                s << "sample size MIGHT NOT be large enough (subsample size >200 is recommended)" << endl;
            }
        }
        double ci = i->unit_readings_optimal_subsession_confidence_interval[piid];
        double ci_rt = ci / sm;
        double ci_low = format_unit_reading(piid, sm) * (1 - ci_rt/2);
        double ci_high = format_unit_reading(piid, sm) * (1 + ci_rt/2) ;
        s << "95% confidence interval width: " << ci_high - ci_low << " " << pi_info_[piid].unit << endl;
        s << "95% confidence interval width is " << ci_rt * 100 << "% of sample_mean" << endl;
        s << "95% confidence interval: [" << ci_low << ", " << ci_high << "] " << pi_info_[piid].unit << endl;
    }
//    cout << "dumb throughput (io_limit / total_elapsed_time) (MB/s): [";
//    const double* tp_readings = pilot_get_pi_readings(wl, tp_pi);
//    size_t rounds = pilot_get_num_of_rounds(wl);
//    for(size_t i = 0; i < rounds; ++i) {
//        cout << tp_readings[i];
//        if (i != rounds-1) cout << ", ";
//    }
//    cout << "]" << endl;

    pilot_free_workload_info(i);

    size_t len = s.str().size() + 1;
    char *result = new char[len];
    memcpy(result, s.str().c_str(), len);
    return result;
}

