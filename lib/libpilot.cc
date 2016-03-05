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

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/basic_sink_backend.hpp>
#include <boost/log/sinks/frontend_requirements.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/math/distributions/students_t.hpp>
#include <boost/shared_ptr.hpp>
#include <algorithm>
#include "common.h"
#include "config.h"
#include <cstdio>
#include <fstream>
#include "libpilot.h"
#include "libpilotcpp.h"
#include "pilot_tui.hpp"
#include "pilot_workload_runner.hpp"
#include <vector>
#include "workload.hpp"

using namespace pilot;
using namespace std;
using boost::timer::cpu_timer;
using boost::timer::nanosecond_type;

namespace pilot {

stringstream g_in_mem_log_buffer;
boost::shared_ptr< boost::log::sinks::sink > g_console_log_sink;

class PilotInMemLogBackend :
        public boost::log::sinks::basic_formatted_sink_backend<
        char,
        boost::log::sinks::concurrent_feeding
        > {
public:
    explicit PilotInMemLogBackend() {}

    void consume(boost::log::record_view const& rec, string_type const& msg) {
        g_in_mem_log_buffer << msg << std::endl;
    }
};

void pilot_lib_self_check(int vmajor, int vminor, size_t nanosecond_type_size) {
    die_if(vmajor != PILOT_VERSION_MAJOR || vminor != PILOT_VERSION_MINOR,
            ERR_LINKED_WRONG_VER, "libpilot header files and library version mismatch");
    die_if(nanosecond_type_size != sizeof(boost::timer::nanosecond_type),
            ERR_LINKED_WRONG_VER, "size of current compiler's int_least64_t does not match the library");

    // we set up two sinks here, one to console, the other to our in-mem log buffer
    namespace logging = boost::log;
    namespace src = boost::log::sources;
    namespace sinks = boost::log::sinks;
    namespace keywords = boost::log::keywords;
    namespace expr = boost::log::expressions;

    logging::add_common_attributes();
    boost::shared_ptr< logging::core > core = logging::core::get();

    // Create a backend and attach a stream to it
    boost::shared_ptr<PilotInMemLogBackend> backend = boost::make_shared<PilotInMemLogBackend>();

    // Wrap it into the frontend and register in the core.
    // The backend requires synchronization in the frontend.
    typedef sinks::synchronous_sink<PilotInMemLogBackend> sink_t;
    boost::shared_ptr<sink_t> sink(new sink_t(backend));
    sink->set_formatter
    (
            expr::format("%1%:[%2%] <%3%> %4%")
    % expr::attr< unsigned int >("LineID")
    % expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S")
    % logging::trivial::severity
    % expr::smessage
    );
    core->add_sink(sink);
    g_console_log_sink = logging::add_console_log();
}

void pilot_remove_console_log_sink(void) {
    namespace logging = boost::log;
    boost::shared_ptr< logging::core > core = logging::core::get();
    assert (g_console_log_sink.get() != NULL);
    core->remove_sink(g_console_log_sink);
}

void pilot_set_pi_info(pilot_workload_t* wl, int piid,
        const char *pi_name,
        const char *pi_unit,
        pilot_pi_display_format_func_t *format_reading_func,
        pilot_pi_display_format_func_t *format_unit_reading_func,
        bool reading_must_satisfy, bool unit_reading_must_satisfy)
{
    ASSERT_VALID_POINTER(wl);
    ASSERT_VALID_POINTER(pi_name);
    // we allow pi_unit be NULL
    wl->pi_info_[piid].name.assign(pi_name ? pi_name : "");
    wl->pi_info_[piid].unit.assign(pi_unit ? pi_unit : "");
    wl->pi_info_[piid].format_reading.format_func_ = format_reading_func;
    wl->pi_info_[piid].format_unit_reading.format_func_ = format_unit_reading_func;
    wl->pi_info_[piid].reading_must_satisfy = reading_must_satisfy;
    wl->pi_info_[piid].unit_reading_must_satisfy = unit_reading_must_satisfy;
}

void pilot_wps_setting(pilot_workload_t* wl,
        pilot_pi_display_format_func_t *format_wps_func,
        bool wps_must_satisfy) {
    ASSERT_VALID_POINTER(wl);
    wl->format_wps_.format_func_ = format_wps_func;
    wl->wps_must_satisfy_ = wps_must_satisfy;
}

pilot_workload_t* pilot_new_workload(const char *workload_name) {
    pilot_workload_t *wl = new pilot_workload_t(workload_name);
    return wl;
}

void pilot_set_next_round_work_amount_hook(pilot_workload_t* wl, next_round_work_amount_hook_t *f) {
    ASSERT_VALID_POINTER(wl);
    ASSERT_VALID_POINTER(f);
    wl->next_round_work_amount_hook_ = f;
}

void pilot_set_calc_required_readings_func(pilot_workload_t* wl,
        calc_required_readings_func_t *f) {
    ASSERT_VALID_POINTER(wl);
    ASSERT_VALID_POINTER(f);
    wl->calc_required_readings_func_ = f;
}

void pilot_set_calc_required_unit_readings_func(pilot_workload_t* wl,
        calc_required_readings_func_t *f) {
    ASSERT_VALID_POINTER(wl);
    ASSERT_VALID_POINTER(f);
    wl->calc_required_unit_readings_func_ = f;
}

void pilot_set_num_of_pi(pilot_workload_t* wl, size_t num_of_pi) {
    ASSERT_VALID_POINTER(wl);
    if (wl->num_of_pi_ != 0) {
        fatal_log << "Changing the number of performance indices is not supported";
        abort();
    }
    wl->num_of_pi_ = num_of_pi;
    wl->pi_info_.resize(num_of_pi);
    wl->readings_.resize(num_of_pi);
    wl->unit_readings_.resize(num_of_pi);
    wl->warm_up_phase_len_.resize(num_of_pi);
    wl->total_num_of_unit_readings_.resize(num_of_pi);
    wl->total_num_of_readings_.resize(num_of_pi);
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
    wl->max_work_amount_ = t;
}

int pilot_get_work_amount_limit(const pilot_workload_t* wl, size_t *p_work_amount_limit) {
    ASSERT_VALID_POINTER(wl);
    ASSERT_VALID_POINTER(p_work_amount_limit);
    *p_work_amount_limit = wl->max_work_amount_;
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
        wl->max_work_amount_ == 0) return 11;

    int result = 0;
    size_t num_of_unit_readings;
    double **unit_readings;
    double *readings;
    unique_ptr<cpu_timer> round_timer;

    // ready to start the workload
    size_t work_amount;
    nanosecond_type round_duration;
    auto session_start_time = std::chrono::steady_clock::now();
    while (true) {
        unit_readings = NULL;
        readings = NULL;

        work_amount = wl->calc_next_round_work_amount();
        if (0 == work_amount) {
            info_log << "enough readings collected, exiting";
            break;
        }
        if (wl->wholly_rejected_rounds_ > 100) {
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

        round_timer.reset(new cpu_timer);
        int rc =
        wl->workload_func_(work_amount, &pilot_malloc_func,
                          &num_of_unit_readings, &unit_readings,
                          &readings);
        round_duration = round_timer->elapsed().wall;
        info_log << "finished workload round " << wl->rounds_;

        // result check first
        if (0 != rc)   { result = ERR_WL_FAIL; break; }

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
            for (size_t piid = 0; piid < wl->num_of_pi_; ++piid) {
                if (unit_readings[piid])
                    free(unit_readings[piid]);
            }
            free(unit_readings);
        }

        // refresh TUI
        if (wl->tui_) {
            unique_ptr<pilot_workload_info_t> wi(wl->workload_info());
            *(wl->tui_) << *wi;
        }

        // handle hooks
        if (wl->hook_post_workload_run_ && !wl->hook_post_workload_run_(wl)) {
            info_log << "post_workload_run hook returns false, exiting";
            result = ERR_STOPPED_BY_HOOK;
            break;
        }

        // check session duration limit
        if (0 != wl->session_duration_limit_in_sec_) {
            std::chrono::duration<double> diff = std::chrono::steady_clock::now() - session_start_time;
            if (diff.count() > wl->session_duration_limit_in_sec_) {
                info_log << "reached session duration limit";
                result = ERR_STOPPED_BY_DURATION_LIMIT;
                break;
            }
        }
    }
    return result;
}

int pilot_run_workload_tui(pilot_workload_t *wl) {
    unique_ptr<PilotTUI> pilot_tui;
    try {
        pilot_tui.reset(new PilotTUI(&(wl->pi_info_), wl->format_wps_));
    } catch (const tui_exception &ex) {
        // when TUI can't be constructed, switch back to old plain cout
        cerr << ex.what() << endl;
        cerr << "TUI disabled" << endl;
        return pilot_run_workload(wl);
    }
    wl->tui_ = pilot_tui.get();
    WorkloadRunner<PilotTUI> workload_runner(wl, *pilot_tui);
    workload_runner.start();
    pilot_tui->event_loop();
    workload_runner.join();
    wl->tui_ = NULL;
    return workload_runner.get_workload_result();
}

static void _pilot_ui_printf(pilot_workload_t *wl, const char* prefix, const char* format, va_list args) {
    ASSERT_VALID_POINTER(prefix);
    unique_ptr<char[]> buf;
    va_list args_copy;
    va_copy(args_copy, args);

    size_t size = vsnprintf(buf.get(), 0, format, args_copy);
    va_end(args_copy);
    buf.reset(new char[size + 1]);
    vsnprintf(buf.get(), size + 1, format, args);

    if (NULL == wl->tui_) {
        cout << prefix << buf.get();
    } else {
        *(wl->tui_) << prefix << buf.get();
    }
    g_in_mem_log_buffer << buf.get();
}

void pilot_ui_printf(pilot_workload_t *wl, const char* format, ...) {
    va_list args;
    va_start(args, format);
    _pilot_ui_printf(wl, "", format, args);
    va_end(args);
}

void pilot_ui_printf_hl(pilot_workload_t *wl, const char* format, ...) {
    va_list args;
    va_start(args, format);
    if (NULL == wl->tui_) {
        _pilot_ui_printf(wl, "", format, args);
    } else {
        _pilot_ui_printf(wl, "</13>", format, args);
    }
    va_end(args);
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
    case ERR_STOPPED_BY_DURATION_LIMIT: return "Stopped after reaching time limit";
    case ERR_STOPPED_BY_HOOK: return "Execution is stopped by a hook function";
    case ERR_TOO_MANY_REJECTED_ROUNDS: return "Too many rounds are wholly rejected. Stopping. Check the workload.";
    case ERR_NOT_IMPL:    return "Not implemented";
    default:
        error_log << "Unknown error code: " << errnum;
        return "Unknown error code";
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

int pilot_export(const pilot_workload_t *wl, const char *dirname) {
    ASSERT_VALID_POINTER(wl);
    ASSERT_VALID_POINTER(dirname);

    int res;
    /* The behavior of mkdir is undefined for anything other than the "permission" bits */
    res = mkdir(dirname, 0777);
    if (0 == res) {
        /* So we need to set the sticky/executable bits explicitly with chmod after calling mkdir */
        if (chmod(dirname, 07777)) {
            error_log << __func__ << "(): chmod failed, errno " << errno;
        }
    } else if (EEXIST != errno) {
        error_log << __func__ << "(): mkdir failed, errno " << errno;
        return errno;
    }


    stringstream filename;
    ofstream of;
    try {
        filename.str(string());
        filename << dirname << "/" << "session_log.txt";
        of.exceptions(ofstream::failbit | ofstream::badbit);
        of.open(filename.str().c_str());
        of << g_in_mem_log_buffer.str();
        g_in_mem_log_buffer.str(string());
        of.close();

        wl->refresh_wps_analysis_results();
        filename.str(string());
        filename << dirname << "/" << "wps_analysis.csv";
        of.exceptions(ofstream::failbit | ofstream::badbit);
        of.open(filename.str().c_str());
        of << "wps_naive_v,wps_naive_v_err,wps_naive_v_err_percent,wps_alpha,wps_v,wps_v_ci,wps_err,wps_err_percent,wps_v_dw_method,wps_v_ci_dw_method" << endl;
        of << wl->wps_harmonic_mean_ << "," << wl->wps_naive_v_err_ << "," << wl->wps_naive_v_err_percent_ << ",";
        if (wl->wps_has_data_) {
            of << wl->wps_alpha_ << ","
               << wl->wps_v_ << ","
               << wl->wps_v_ci_ << ","
               << wl->wps_err_ << ","
               << wl->wps_err_percent_ << ","
               << wl->wps_v_dw_method_ << ","
               << wl->wps_v_ci_dw_method_ << endl;
        } else {
            of << ",,,,,," << endl;
        }
        of.close();

        filename.str(string());
        filename << dirname << "/" << "rounds.csv";
        of.exceptions(ofstream::failbit | ofstream::badbit);
        of.open(filename.str().c_str());
        of << "round,work_amount,round_duration" << endl;
        for (size_t round = 0; round < wl->rounds_; ++round) {
            of << round << ","
               << wl->round_work_amounts_[round] << ","
               << wl->round_durations_[round] << endl;
        }
        of.close();

        filename.str(string());
        filename << dirname << "/" << "readings.csv";
        of.exceptions(ofstream::failbit | ofstream::badbit);
        of.open(filename.str().c_str());
        of << "piid,round,readings" << endl;
        for (size_t piid = 0; piid < wl->num_of_pi_; ++piid)
            for (size_t round = 0; round < wl->rounds_; ++round) {
                of << piid << "," << round << ",";
                if (wl->readings_[piid].size() > round) {
                    of << wl->readings_[piid][round] << ",";
                } else {
                    of << ",";
                }
                of << endl;
        }
        of.close();

        filename.str(string());
        filename << dirname << "/" << "unit_readings.csv";
        of.exceptions(ofstream::failbit | ofstream::badbit);
        of.open(filename.str().c_str());
        of << "piid,round,unit_reading,formatted_unit_reading" << endl;
        for (size_t piid = 0; piid < wl->num_of_pi_; ++piid)
            for (size_t round = 0; round < wl->rounds_; ++round) {
                if (wl->unit_readings_[piid][round].size() != 0) {
                    for (size_t ur = 0; ur < wl->unit_readings_[piid][round].size(); ++ur) {
                        of << piid << ","
                           << round << ","
                           << wl->unit_readings_[piid][round][ur] << ","
                           << wl->format_unit_reading(piid, wl->unit_readings_[piid][round][ur])
                           << endl;
                    }
                } else {
                    of << piid << ","
                       << round << ",," << endl;
                }
            } /* round loop */
        of.close();
    } catch (ofstream::failure &e) {
        error_log << "I/O error: " << strerror(errno);
        return ERR_IO;
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
    delete info;
}

void pilot_workload_info_t::_free_all_field() {
#define FREE_AND_NULL(field) free(field); field = NULL

    FREE_AND_NULL(readings_arithmetic_mean);
    FREE_AND_NULL(readings_harmonic_mean);
    FREE_AND_NULL(readings_var);
    FREE_AND_NULL(readings_autocorrelation_coefficient);
    FREE_AND_NULL(readings_optimal_subsession_size);
    FREE_AND_NULL(readings_optimal_subsession_var);
    FREE_AND_NULL(readings_optimal_subsession_autocorrelation_coefficient);
    FREE_AND_NULL(readings_optimal_subsession_confidence_interval);
    FREE_AND_NULL(readings_required_sample_size);

    FREE_AND_NULL(total_num_of_unit_readings);
    FREE_AND_NULL(unit_readings_mean);
    FREE_AND_NULL(unit_readings_var);
    FREE_AND_NULL(unit_readings_autocorrelation_coefficient);
    FREE_AND_NULL(unit_readings_optimal_subsession_size);
    FREE_AND_NULL(unit_readings_optimal_subsession_var);
    FREE_AND_NULL(unit_readings_optimal_subsession_autocorrelation_coefficient);
    FREE_AND_NULL(unit_readings_optimal_subsession_confidence_interval);
    FREE_AND_NULL(unit_readings_required_sample_size);

#undef FREE_AND_NULL
}

void pilot_workload_info_t::_copyfrom(const pilot_workload_info_t &a) {
    num_of_pi = a.num_of_pi;
    num_of_rounds = a.num_of_rounds;

#define COPY_ARRAY(field) if (a.field) { field = (typeof(field[0])*)realloc(field, sizeof(field[0]) * num_of_pi); \
    memcpy(field, a.field, sizeof(field[0]) * num_of_pi); } else { field = NULL; }

    COPY_ARRAY(readings_arithmetic_mean);
    COPY_ARRAY(readings_harmonic_mean);
    COPY_ARRAY(readings_var);
    COPY_ARRAY(readings_autocorrelation_coefficient);
    COPY_ARRAY(readings_optimal_subsession_size);
    COPY_ARRAY(readings_optimal_subsession_var);
    COPY_ARRAY(readings_optimal_subsession_autocorrelation_coefficient);
    COPY_ARRAY(readings_optimal_subsession_confidence_interval);
    COPY_ARRAY(readings_required_sample_size);

    COPY_ARRAY(total_num_of_unit_readings);
    COPY_ARRAY(unit_readings_mean);
    COPY_ARRAY(unit_readings_var);
    COPY_ARRAY(unit_readings_autocorrelation_coefficient);
    COPY_ARRAY(unit_readings_optimal_subsession_size);
    COPY_ARRAY(unit_readings_optimal_subsession_var);
    COPY_ARRAY(unit_readings_optimal_subsession_autocorrelation_coefficient);
    COPY_ARRAY(unit_readings_optimal_subsession_confidence_interval);
    COPY_ARRAY(unit_readings_required_sample_size);
#undef COPY_ARRAY

#define COPY_FIELD(field) field = a.field;
    COPY_FIELD(wps_harmonic_mean);
    COPY_FIELD(wps_naive_v_err);
    COPY_FIELD(wps_naive_v_err_percent)
    if (a.wps_has_data) {
        COPY_FIELD(wps_has_data);
        COPY_FIELD(wps_alpha);
        COPY_FIELD(wps_v);
        COPY_FIELD(wps_v_ci);
        COPY_FIELD(wps_err);
        COPY_FIELD(wps_err_percent);
    }
    COPY_FIELD(wps_v_dw_method);
    COPY_FIELD(wps_v_ci_dw_method);
#undef COPY_FIELD
}

pilot_workload_info_t::pilot_workload_info_t() {
    memset(this, 0, sizeof(*this));
}

pilot_workload_info_t::pilot_workload_info_t(const pilot_workload_info_t &a) {
    memset(this, 0, sizeof(*this));
    _copyfrom(a);
}

pilot_workload_info_t::~pilot_workload_info_t() {
    _free_all_field();
}

pilot_workload_info_t& pilot_workload_info_t::operator=(const pilot_workload_info_t &a) {
    _free_all_field();
    _copyfrom(a);
    return *this;
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

double pilot_subsession_mean_p(const double *data, size_t n, pilot_mean_method_t mean_method) {
    return pilot_subsession_mean(data, n, mean_method);
}

double pilot_subsession_auto_cov_p(const double *data, size_t n, size_t q, double sample_mean, pilot_mean_method_t mean_method) {
    return pilot_subsession_auto_cov(data, n, q, sample_mean, mean_method);
}

double pilot_subsession_var_p(const double *data, size_t n, size_t q, double sample_mean, pilot_mean_method_t mean_method) {
    return pilot_subsession_var(data, n, q, sample_mean, mean_method);
}

double pilot_subsession_autocorrelation_coefficient_p(const double *data, size_t n, size_t q, double sample_mean, pilot_mean_method_t mean_method) {
    return pilot_subsession_autocorrelation_coefficient(data, n, q, sample_mean, mean_method);
}

int pilot_optimal_subsession_size_p(const double *data, size_t n, pilot_mean_method_t mean_method, double max_autocorrelation_coefficient) {
    return pilot_optimal_subsession_size(data, n, mean_method, max_autocorrelation_coefficient);
}

double pilot_subsession_confidence_interval_p(const double *data, size_t n, size_t q, double confidence_level, pilot_mean_method_t mean_method) {
    return pilot_subsession_confidence_interval(data, n, q, confidence_level, mean_method);
}

double pilot_p_eq(double mean1, double mean2, size_t size1, size_t size2,
                  double var1, double var2, double *ci_left, double *ci_right,
                  double confidence_level) {
    using namespace boost::math;

    double d = mean1 - mean2;
    double sc = sqrt(var1 / double(size1) + var2 / double(size2));
    double t = d / sc;

    size_t deg_of_freedom = min(size1, size2) - 1;
    students_t dist(deg_of_freedom);
    double p = cdf(dist, -abs(t));
    // two sided
    p *= 2;

    // calculate CI
    t = quantile(complement(dist, (1 - confidence_level) / 2));
    if (ci_left)  *ci_left = d - t * sc;
    if (ci_right) *ci_right = d + t * sc;
    return p;
}

ssize_t
pilot_optimal_sample_size_p(const double *data, size_t n,
                            double confidence_interval_width,
                            pilot_mean_method_t mean_method,
                            double confidence_level,
                            double max_autocorrelation_coefficient) {
    return pilot_optimal_sample_size(data, n, confidence_interval_width,
                                     mean_method,
                                     confidence_level,
                                     max_autocorrelation_coefficient);
}

int pilot_wps_warmup_removal_lr_method_p(size_t rounds, const size_t *round_work_amounts,
        const nanosecond_type *round_durations,
        float autocorrelation_coefficient_limit, nanosecond_type duration_threshold,
        double *alpha, double *v,
        double *ci_width, double *ssr_out, double *ssr_out_percent, size_t *subsession_sample_size) {
    return pilot_wps_warmup_removal_lr_method(rounds, round_work_amounts,
                                              round_durations,
                                              autocorrelation_coefficient_limit,
                                              duration_threshold,
                                              alpha, v, ci_width, ssr_out, ssr_out_percent,
                                              subsession_sample_size);
}

int pilot_wps_warmup_removal_dw_method_p(size_t rounds, const size_t *round_work_amounts,
        const nanosecond_type *round_durations, float confidence_level,
        float autocorrelation_coefficient_limit, double *v, double *ci_width) {
    return pilot_wps_warmup_removal_dw_method(rounds, round_work_amounts,
                                              round_durations, confidence_level,
                                              autocorrelation_coefficient_limit,
                                              v, ci_width);
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

void pilot_import_benchmark_results(pilot_workload_t *wl, size_t round,
                                    size_t work_amount,
                                    boost::timer::nanosecond_type round_duration,
                                    const double *readings,
                                    size_t num_of_unit_readings,
                                    const double * const *unit_readings) {
    ASSERT_VALID_POINTER(wl);
    die_if(round > wl->rounds_, ERR_WRONG_PARAM, string("Invalid round value for ") + __func__);

    // update work_amount
    if (round != wl->rounds_)
        wl->round_work_amounts_[round] = work_amount;
    else
        wl->round_work_amounts_.push_back(work_amount);

    // update round_dueration
    if (round != wl->rounds_)
        wl->round_durations_[round] = round_duration;
    else
        wl->round_durations_.push_back(round_duration);

    if (!unit_readings) num_of_unit_readings = 0;
    for (size_t piid = 0; piid < wl->num_of_pi_; ++piid) {
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
        if (num_of_unit_readings != 0 && static_cast<size_t>(wul) == num_of_unit_readings)
            ++wl->wholly_rejected_rounds_;
        // increase total number of unit readings (the number of old unit readings are
        // subtracted at the beginning of the for loop).
        wl->total_num_of_unit_readings_[piid] += num_of_unit_readings - wul;

        // handle readings
        if (readings) {
            if (round == wl->rounds_) {
                wl->readings_[piid].push_back(readings[piid]);
            } else {
                wl->readings_[piid][round] = readings[piid];
            }
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

size_t pilot_next_round_work_amount(pilot_workload_t *wl) {
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

size_t calc_next_round_work_amount_from_readings(pilot_workload_t *wl) {
    // use the default value for first round
    if (0 == wl->rounds_) return 0;
    // not implemented yet
    return 0;
}

size_t calc_next_round_work_amount_from_unit_readings(pilot_workload_t *wl) {
    if (0 == wl->rounds_)
        return 0 == wl->init_work_amount_ ? wl->max_work_amount_ / 10 : wl->init_work_amount_;

    size_t max_work_amount_needed = 0;
    for (size_t piid = 0; piid != wl->num_of_pi_; ++piid) {
        size_t num_of_ur_needed;
        if (abs(wl->calc_avg_work_unit_per_amount(piid)) < 0.00000001) {
            error_log << "[PI " << piid << "] average work per unit reading is 0, using work_amount_limit instead (you probably need to report a bug)";
            return wl->max_work_amount_;
        }
        if (0 != wl->total_num_of_unit_readings_[piid]) {
            ssize_t req = wl->required_num_of_unit_readings(piid);
            info_log << "[PI " << piid << "] required unit readings sample size: " << req;
            if (req > 0) {
                if (static_cast<size_t>(req) < wl->total_num_of_unit_readings_[piid]) {
                    info_log << "[PI " << piid << "] already has enough samples";
                    continue;
                }
                num_of_ur_needed = req - wl->total_num_of_unit_readings_[piid];
            } else {
                // in case when there's not enough data to calculate the number of UR,
                // we try to double the total number of unit readings
                info_log << "[PI " << piid << "] doesn't have enough information for calculating required sample size, using the current total sample size instead";
                num_of_ur_needed = wl->total_num_of_unit_readings_[piid];
            }
            size_t work_amount_needed = size_t(1.2 * num_of_ur_needed * wl->calc_avg_work_unit_per_amount(piid));
            max_work_amount_needed = max(max_work_amount_needed, work_amount_needed);
        }
    }
    return min(max_work_amount_needed, wl->max_work_amount_);
}

size_t calc_next_round_work_amount_from_wps(pilot_workload_t *wl) {
    if (!wl->wps_must_satisfy_) return 0;
    size_t wa_slice_size = wl->max_work_amount_ / wl->wps_slices_;
    if (0 == wl->rounds_) return wa_slice_size;

    if (wl->rounds_ > 3) {
        wl->refresh_wps_analysis_results();

        if (wl->wps_has_data_ && wl->wps_subsession_sample_size_ > 10 &&
            wl->wps_v_ > 0 && wl->wps_v_ci_ > 0 &&
            wl->wps_v_ci_ < wl->required_ci_percent_of_mean_ * wl->wps_v_) {
            info_log << "WPS confidence interval small enough";
            return 0;
        }
    }

    size_t last_round_wa = wl->round_work_amounts_.back();
    if (last_round_wa > wl->max_work_amount_ - wa_slice_size) {
        wl->wps_slices_ *= 2;
        wa_slice_size /= 2;
        return wa_slice_size;
    }
    return (last_round_wa / wa_slice_size + 1) * wa_slice_size;
}

bool pilot_set_wps_analysis(pilot_workload_t *wl, bool enabled) {
    return wl->set_wps_analysis(enabled);
}

size_t pilot_set_session_duration_limit(pilot_workload_t *wl, size_t sec) {
    return wl->set_session_duration_limit(sec);
}

double pilot_set_autocorrelation_coefficient(pilot_workload_t *wl, double ac) {
    ASSERT_VALID_POINTER(wl);
    double old_ac = wl->autocorrelation_coefficient_limit_;
    wl->autocorrelation_coefficient_limit_ = ac;
    return old_ac;
}

} // namespace pilot
