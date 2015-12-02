/*
 * libpilot.cc: routines for handling workloads
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
#include "config.h"
#include <fstream>
#include "libpilot.h"
#include "libpilotcpp.h"
#include <vector>
#include "workload.h"

using namespace pilot;
using namespace std;
using boost::timer::cpu_timer;
using boost::timer::nanosecond_type;

void pilot_lib_self_check(int vmajor, int vminor, size_t nanosecond_type_size) {
    die_if(vmajor != PILOT_VERSION_MAJOR || vminor != PILOT_VERSION_MINOR,
            ERR_LINKED_WRONG_VER, "libpilot header files and library version mismatch");
    die_if(nanosecond_type_size != sizeof(boost::timer::nanosecond_type),
            ERR_LINKED_WRONG_VER, "size of current compiler's int_least64_t does not match the library");
}

double pilot_default_pi_unit_reading_format_func(const pilot_workload_t* wl, double unit_reading) {
    return unit_reading;
}

double pilot_default_pi_reading_print_func(const pilot_workload_t* wl, double reading) {
    return reading;
}

void pilot_set_pi_unit_reading_format_func(pilot_workload_t* wl, int piid, pilot_pi_unit_reading_format_func_t *f, const char *unit) {
    ASSERT_VALID_POINTER(wl);
    ASSERT_VALID_POINTER(f);
    wl->unit_reading_format_funcs_[piid] = f;
    wl->pi_units_[piid] = unit ? string(unit) : string("");
}

void pilot_set_pi_reading_print_func(pilot_workload_t* wl, int piid, pilot_pi_reading_format_func_t *f, const char *unit) {
    ASSERT_VALID_POINTER(wl);
    ASSERT_VALID_POINTER(f);
    wl->reading_format_funcs_[piid] = f;
    wl->pi_units_[piid] = unit ? string(unit) : string("");
}

pilot_workload_t* pilot_new_workload(const char *workload_name) {
    pilot_workload_t *wl = new pilot_workload_t(workload_name);
    return wl;
}

void pilot_set_calc_readings_required_func(pilot_workload_t* wl,
        calc_readings_required_func_t *f) {
    ASSERT_VALID_POINTER(wl);
    ASSERT_VALID_POINTER(f);
    wl->calc_readings_required_func_ = f;
}

void pilot_set_calc_unit_readings_required_func(pilot_workload_t* wl,
        calc_readings_required_func_t *f) {
    ASSERT_VALID_POINTER(wl);
    ASSERT_VALID_POINTER(f);
    wl->calc_unit_readings_required_func_ = f;
}

void pilot_set_num_of_pi(pilot_workload_t* wl, size_t num_of_readings) {
    ASSERT_VALID_POINTER(wl);
    if (wl->num_of_pi_ != 0) {
        fatal_log << "Changing the number of performance indices is not supported";
        abort();
    }
    wl->num_of_pi_ = num_of_readings;
    wl->pi_units_.resize(num_of_readings);
    wl->readings_.resize(num_of_readings);
    wl->unit_readings_.resize(num_of_readings);
    wl->warm_up_phase_len_.resize(num_of_readings);
    wl->total_num_of_unit_readings_.resize(num_of_readings);
    wl->total_num_of_readings_.resize(num_of_readings);
    wl->unit_reading_format_funcs_.resize(num_of_readings, &pilot_default_pi_unit_reading_format_func);
    wl->reading_format_funcs_.resize(num_of_readings, &pilot_default_pi_reading_print_func);
}

int pilot_get_num_of_pi(const pilot_workload_t* wl, size_t *p_num_of_pi) {
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

int pilot_get_work_amount_limit(const pilot_workload_t* wl, size_t *p_work_amount_limit) {
    ASSERT_VALID_POINTER(wl);
    ASSERT_VALID_POINTER(p_work_amount_limit);
    *p_work_amount_limit = wl->work_amount_limit_;
    return 0;
}

void pilot_set_init_work_amount(pilot_workload_t* wl, size_t init_work_amount) {
    ASSERT_VALID_POINTER(wl);
    wl->init_work_amount_ = init_work_amount;
}

int pilot_get_init_work_amount(const pilot_workload_t* wl, size_t *p_init_work_amount) {
    ASSERT_VALID_POINTER(wl);
    ASSERT_VALID_POINTER(p_init_work_amount);
    *p_init_work_amount = wl->init_work_amount_;
    return 0;
}

int pilot_set_hook_func(pilot_workload_t* wl, enum pilot_hook_t hook,
                        general_hook_func_t *f) {
    ASSERT_VALID_POINTER(wl);
    general_hook_func_t old_f;
    switch(hook) {
    case PRE_WORKLOAD_RUN:
        wl->hook_pre_workload_run_ = f;
        return 0;
    case POST_WORKLOAD_RUN:
        wl->hook_post_workload_run_ = f;
        return 0;
    default:
        error_log << "Trying to set an unknown hook: " << hook;
        return ERR_UNKNOWN_HOOK;
    }
}

void pilot_set_warm_up_removal_method(pilot_workload_t* wl,
        pilot_warm_up_removal_detection_method_t m) {
    ASSERT_VALID_POINTER(wl);
    wl->warm_up_removal_detection_method_ = m;
}

void pilot_set_short_workload_check(pilot_workload_t* wl, bool check_short_workload) {
    ASSERT_VALID_POINTER(wl);
    wl->short_workload_check_ = check_short_workload;
}

int pilot_run_workload(pilot_workload_t *wl) {
    // sanity check
    ASSERT_VALID_POINTER(wl);
    if (wl->workload_func_ == nullptr || wl->num_of_pi_ == 0 ||
        wl->work_amount_limit_ == 0) return 11;

    int result = 0;
    size_t num_of_unit_readings;
    double **unit_readings;
    double *readings;
    unique_ptr<cpu_timer> ptimer;

    // ready to start the workload
    size_t work_amount;
    nanosecond_type round_duration;
    while (true) {
        unit_readings = NULL;
        readings = NULL;

        work_amount = wl->calc_next_round_work_amount();
        if (0 == work_amount) {
            info_log << "enough readings collected, exiting";
            break;
        }
        if (wl->wholly_rejected_rounds_ > 10) {
            info_log << "Too many rounds are wholly rejected. Stopping. Check the workload.";
            result = ERR_TOO_MANY_REJECTED_ROUNDS;
            break;
        }

        info_log << "starting workload round " << wl->rounds_ << " with work_amount=" << work_amount;
        if (wl->hook_pre_workload_run_ && !wl->hook_pre_workload_run_(wl)) {
            info_log << "pre_workload_run hook returns false, exiting";
            result = ERR_STOPPED_BY_HOOK;
            break;
        }

        ptimer.reset(new cpu_timer);
        int rc =
        wl->workload_func_(work_amount, &pilot_malloc_func,
                          &num_of_unit_readings, &unit_readings,
                          &readings);
        round_duration = ptimer->elapsed().wall;
        info_log << "finished workload round " << wl->rounds_;

        // result check first
        if (0 != rc)   { result = ERR_WL_FAIL; break; }
        if (!readings) { result = ERR_NO_READING; break; }

        //! TODO validity check: if (wl->short_workload_check_) ...
        // Get the total_elapsed_time and avg_time_per_unit = total_elapsed_time / num_of_work_units.
        // If avg_time_per_unit is not at least 100 times longer than the CPU time resolution then
        // the results cannot be used. See FB#2808 and
        // http://www.boost.org/doc/libs/1_59_0/libs/timer/doc/cpu_timers.html

        // move all data into the permanent location
        pilot_import_benchmark_results(wl, wl->rounds_, work_amount,
                                       round_duration, readings,
                                       num_of_unit_readings,
                                       unit_readings);

        //! TODO: save the data to a database

        // cleaning up
        if (readings) free(readings);
        if (unit_readings) {
            for (int piid = 0; piid < wl->num_of_pi_; ++piid) {
                if (unit_readings[piid])
                    free(unit_readings[piid]);
            }
            free(unit_readings);
        }

        if (wl->hook_post_workload_run_ && !wl->hook_post_workload_run_(wl)) {
            info_log << "post_workload_run hook returns false, exiting";
            result = ERR_STOPPED_BY_HOOK;
            break;
        }
    }
    return result;
}

size_t pilot_get_total_num_of_unit_readings(const pilot_workload_t *wl, int piid) {
    ASSERT_VALID_POINTER(wl);
    return wl->total_num_of_unit_readings_[piid];
}

const char *pilot_strerror(int errnum) {
    switch (errnum) {
    case NO_ERROR:        return "No error";
    case ERR_WRONG_PARAM: return "Parameter error";
    case ERR_IO:          return "I/O error";
    case ERR_UNKNOWN_HOOK: return "Unknown hook";
    case ERR_NOT_INIT:    return "Workload not properly initialized yet";
    case ERR_WL_FAIL:     return "Workload failure";
    case ERR_NO_READING:  return "Workload did not return readings";
    case ERR_STOPPED_BY_HOOK: return "Execution is stopped by a hook function";
    case ERR_TOO_MANY_REJECTED_ROUNDS: return "Too many rounds are wholly rejected. Stopping. Check the workload.";
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
        } catch (ofstream::failure &e) {
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

pilot_round_info_t* pilot_round_info(const pilot_workload_t *wl, size_t round, pilot_round_info_t *info) {
    ASSERT_VALID_POINTER(wl);
    return wl->round_info(round, info);
}

pilot_workload_info_t* pilot_workload_info(const pilot_workload_t *wl, pilot_workload_info_t *info) {
    ASSERT_VALID_POINTER(wl);
    return wl->workload_info(info);
}

void pilot_free_workload_info(pilot_workload_info_t *info) {
    ASSERT_VALID_POINTER(info);
    free(info->total_num_of_unit_readings);
    free(info->unit_readings_mean);
    free(info->unit_readings_var);
    free(info->unit_readings_autocorrelation_coefficient);
    free(info->unit_readings_optimal_subsession_size);
    free(info->unit_readings_optimal_subsession_var);
    free(info->unit_readings_optimal_subsession_autocorrelation_coefficient);
    free(info->unit_readings_optimal_subsession_confidence_interval);
    free(info->unit_readings_required_sample_size);
    free(info);
}

void pilot_free_round_info(pilot_round_info_t *info) {
    ASSERT_VALID_POINTER(info);
    if (info->num_of_unit_readings) free(info->num_of_unit_readings);
    if (info->warm_up_phase_lens)   free(info->warm_up_phase_lens);
    free(info);
}

char* pilot_text_round_summary(const pilot_workload_t *wl, size_t round_id) {
    ASSERT_VALID_POINTER(wl);
    return wl->text_round_summary(round_id);
}

char* pilot_text_workload_summary(const pilot_workload_t *wl) {
    ASSERT_VALID_POINTER(wl);
    return wl->text_workload_summary();
}

void pilot_free_text_dump(char *dump) {
    ASSERT_VALID_POINTER(dump);
    delete[] dump;
}

using namespace boost::accumulators;

double pilot_subsession_mean_p(const double *data, size_t n) {
    return pilot_subsession_mean(data, n);
}

double pilot_subsession_cov_p(const double *data, size_t n, size_t q, double sample_mean) {
    return pilot_subsession_cov(data, n, q, sample_mean);
}

double pilot_subsession_var_p(const double *data, size_t n, size_t q, double sample_mean) {
    return pilot_subsession_var(data, n, q, sample_mean);
}

double pilot_subsession_autocorrelation_coefficient_p(const double *data, size_t n, size_t q, double sample_mean) {
    return pilot_subsession_autocorrelation_coefficient(data, n, q, sample_mean);
}

int pilot_optimal_subsession_size_p(const double *data, size_t n, double max_autocorrelation_coefficient) {
    return pilot_optimal_subsession_size(data, n, max_autocorrelation_coefficient);
}

double pilot_subsession_confidence_interval_p(const double *data, size_t n, size_t q, double confidence_level) {
    return pilot_subsession_confidence_interval(data, n, q, confidence_level);
}

ssize_t
pilot_optimal_sample_size_p(const double *data, size_t n,
                            double confidence_interval_width,
                            double confidence_level,
                            double max_autocorrelation_coefficient) {
    return pilot_optimal_sample_size(data, n, confidence_interval_width,
                                     confidence_level,
                                     max_autocorrelation_coefficient);
}

ssize_t pilot_warm_up_removal_detect(const pilot_workload_t *wl,
                                     const double *readings,
                                     size_t num_of_readings,
                                     boost::timer::nanosecond_type round_duration,
                                     pilot_warm_up_removal_detection_method_t method) {
    if (NO_WARM_UP_REMOVAL == method)
        return 0;

    // we reject any round that is too short
    if (round_duration < wl->short_round_detection_threshold_) {
        info_log << "Round duration shorter than 1 second, rejecting";
        return num_of_readings;
    }

    switch (method) {
    case FIXED_PERCENTAGE:
        if (num_of_readings == 1)
            return 0;
        else {
            const int percentage = 10;
            // calculate the ceil of num_of_readings / percentage so we remove at least one
            return (num_of_readings + percentage - 1) / percentage;
        }
    case MOVING_AVERAGE:
        fatal_log << "Unimplemented";
        abort();
        break;
    default:
        fatal_log << "Unimplemented";
        abort();
        break;
    }
}

void pilot_import_benchmark_results(pilot_workload_t *wl, int round,
                                    size_t work_amount,
                                    boost::timer::nanosecond_type round_duration,
                                    const double *readings,
                                    size_t num_of_unit_readings,
                                    const double * const *unit_readings) {

    ASSERT_VALID_POINTER(wl);
    die_if(round < 0 || round > wl->rounds_, ERR_WRONG_PARAM, string("Invalid round value for ") + __func__);
    ASSERT_VALID_POINTER(readings);

    // update work_amount
    if (round != wl->rounds_)
        wl->work_amount_per_round_[round] = work_amount;
    else
        wl->work_amount_per_round_.push_back(work_amount);

    // update round_dueration
    if (round != wl->rounds_)
        wl->round_durations_[round] = round_duration;
    else
        wl->round_durations_.push_back(round_duration);

    if (!unit_readings) num_of_unit_readings = 0;
    for (int piid = 0; piid < wl->num_of_pi_; ++piid) {
        // first subtract the number of the old unit readings from total_num_of_unit_readings_
        // before updating the data
        if (round != wl->rounds_) {
            // remove old total_num_of_unit_readings
            wl->total_num_of_unit_readings_[piid] -= wl->unit_readings_[piid][round].size()
                                                     - wl->warm_up_phase_len_[piid][round];
        }

        if (round == wl->rounds_) {
            // inserting a new round
            if (unit_readings && unit_readings[piid]) {
                info_log << "new round num_of_unit_readings = " << num_of_unit_readings;
                wl->unit_readings_[piid].emplace_back(vector<double>(unit_readings[piid], unit_readings[piid] + num_of_unit_readings));
            } else {
                info_log << "no unit readings in this new round";
                wl->unit_readings_[piid].emplace_back(vector<double>());
            }
        } else {
            info_log << "replacing data for an existing round";
            wl->unit_readings_[piid][round].resize(num_of_unit_readings);
            if (unit_readings && unit_readings[piid])
                memcpy(wl->unit_readings_[piid][round].data(), unit_readings[piid], sizeof(double) * num_of_unit_readings);
        }

        // warm-up removal
        ssize_t wul; // warm-up phase len
        if (unit_readings) {
            wul = pilot_warm_up_removal_detect(wl, unit_readings[piid],
                                               num_of_unit_readings,
                                               round_duration,
                                               wl->warm_up_removal_detection_method_);
            if (wul < 0) {
                warning_log << "warm-up phase detection failed on PI "
                            << piid << " at round " << wl->rounds_;
                wul = 0;
            }
        } else {
            // this round has no unit readings
            wul = 0;
        }
        if (round == wl->rounds_) {
            wl->warm_up_phase_len_[piid].push_back(wul);
        } else {
            wl->warm_up_phase_len_[piid][round] = wul;
        }
        if (num_of_unit_readings != 0 && wul == num_of_unit_readings)
            ++wl->wholly_rejected_rounds_;
        // increase total number of unit readings (the number of old unit readings are
        // subtracted at the beginning of the for loop).
        wl->total_num_of_unit_readings_[piid] += num_of_unit_readings - wul;

        // handle readings
        if (round == wl->rounds_) {
            wl->readings_[piid].push_back(readings[piid]);
        } else {
            wl->readings_[piid][round] = readings[piid];
        }
    } // for loop for PI

    if (round == wl->rounds_)
        ++wl->rounds_;
}

pilot_pi_unit_readings_iter_t*
pilot_pi_unit_readings_iter_new(const pilot_workload_t *wl, int piid) {
    return new pilot_pi_unit_readings_iter_t(wl, piid);
}

double pilot_pi_unit_readings_iter_get_val(const pilot_pi_unit_readings_iter_t* iter) {
    return *(*iter);
}

void pilot_pi_unit_readings_iter_next(pilot_pi_unit_readings_iter_t* iter) {
    ++(*iter);
}

bool pilot_pi_unit_readings_iter_valid(const pilot_pi_unit_readings_iter_t* iter) {
    return iter->valid();
}

void pilot_pi_unit_readings_iter_destroy(pilot_pi_unit_readings_iter_t* iter) {
    ASSERT_VALID_POINTER(iter);
    delete iter;
}

ssize_t default_calc_unit_readings_required_func(const pilot_workload_t* wl, int piid) {
    return wl->required_num_of_unit_readings(piid);
}

ssize_t default_calc_readings_required_func(const pilot_workload_t* wl, int piid) {
    fatal_log << __func__ << "() unimplemented yet";
    abort();
}

size_t pilot_next_round_work_amount(const pilot_workload_t *wl) {
    ASSERT_VALID_POINTER(wl);
    return wl->calc_next_round_work_amount();
}

void pilot_set_short_round_detection_threshold(pilot_workload_t *wl, nanosecond_type threshold) {
    ASSERT_VALID_POINTER(wl);
    wl->short_round_detection_threshold_ = threshold;
}

void pilot_set_required_confidence_interval(pilot_workload_t *wl, double percent_of_mean, double absolute_value) {
    ASSERT_VALID_POINTER(wl);
    wl->required_ci_percent_of_mean_ = percent_of_mean;
    wl->required_ci_absolute_value_ = absolute_value;
}
