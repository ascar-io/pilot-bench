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

#include <boost/format.hpp>
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
#include "csv.h"
#include <fstream>
#include "pilot/libpilot.h"
#include "libpilotcpp.h"
#include "pilot/pilot_tui.hpp"
#include "pilot/pilot_workload_runner.hpp"
#include <vector>
#include "workload.hpp"

using namespace pilot;
using namespace std;
using boost::format;
using boost::timer::cpu_timer;
using boost::timer::nanosecond_type;

extern vector<int> EDM_percent(const double *Z, int n, int min_size, double percent, int degree);

namespace pilot {

stringstream g_in_mem_log_buffer;
boost::shared_ptr< boost::log::sinks::synchronous_sink< boost::log::sinks::text_ostream_backend> > g_console_log_sink;
pilot_log_level_t g_log_level = lv_info;

// We store the log in memory to prevent generating I/O, which may interfere
// with the benchmark. TODO: compress the log.
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

bool g_lib_self_check_done = false;

void pilot_lib_self_check(int vmajor, int vminor, size_t nanosecond_type_size) {
    // Only need to run once
    if (g_lib_self_check_done) return;

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
    g_console_log_sink = logging::add_console_log(std::cout, logging::keywords::format =
    (
            expr::format("%1%:[%2%] <%3%> %4%")
                % expr::attr< unsigned int >("LineID")
                % expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S")
                % logging::trivial::severity
                % expr::smessage
    ));
    g_lib_self_check_done = true;
}

void pilot_free(void *p) {
    free(p);
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
        bool reading_must_satisfy, bool unit_reading_must_satisfy,
        pilot_mean_method_t reading_mean_type,
        pilot_mean_method_t unit_reading_mean_type)
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

pilot_workload_t* pilot_new_workload(const char *workload_name) {
    pilot_workload_t *wl = new pilot_workload_t(workload_name);
    return wl;
}

void pilot_set_workload_data(pilot_workload_t* wl, void *data) {
    ASSERT_VALID_POINTER(wl);
    wl->workload_data_ = data;
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
    wl->set_num_of_pi(num_of_pi);
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

void pilot_set_warm_up_removal_percentage(pilot_workload_t* wl, double percent) {
    ASSERT_VALID_POINTER(wl);
    if (percent < 0 || percent >= 1) {
        fatal_log << "warm-up removal percentage must be within [0, 1), aborting...";
        abort();
    }
    wl->warm_up_removal_percentage_ = percent;
}

void pilot_set_short_workload_check(pilot_workload_t* wl, bool check_short_workload) {
    ASSERT_VALID_POINTER(wl);
    wl->short_workload_check_ = check_short_workload;
}

int pilot_run_workload(pilot_workload_t *wl) {
    // sanity check
    ASSERT_VALID_POINTER(wl);
    if (wl->workload_func_ == nullptr) {
        return ERR_NOT_INIT;
    }
    if (WL_RUNNING == wl->status_) {
        fatal_log << "Workload is already running";
        abort();
    }
    shared_ptr<void> wl_status_scope_guard(NULL,
            [&wl](void*) mutable {wl->status_ = WL_NOT_RUNNING;});
    wl->status_ = WL_RUNNING;

    int result = 0;
    size_t num_of_unit_readings;
    double **unit_readings;
    double *readings;
    unique_ptr<cpu_timer> round_timer;

    // ready to start the workload
    size_t work_amount;
    nanosecond_type measured_round_duration, reported_round_duration, round_duration;
    auto session_start_time = std::chrono::steady_clock::now();
    while (true) {
        unit_readings = NULL;
        readings = NULL;

        if (!wl->calc_next_round_work_amount(&work_amount)) {
            info_log << "Analytical requirement achieved, exiting";
            break;
        }
        if (wl->wholly_rejected_rounds_ > 100) {
            info_log << "Too many rounds are wholly rejected. Stopping. Check the workload.";
            result = ERR_TOO_MANY_REJECTED_ROUNDS;
            break;
        }

        if (wl->hook_pre_workload_run_ && !wl->hook_pre_workload_run_(wl)) {
            info_log << "pre_workload_run hook returns false, exiting";
            result = ERR_STOPPED_BY_HOOK;
            break;
        }

        if (WL_STOP_REQUESTED == wl->status_) {
            info_log << "Stop requested, exiting workload";
            result = ERR_STOPPED_BY_REQUEST;
            break;
        }

        string infos = str(format("Starting workload round %1% with work_amount %2%")
                           % wl->rounds_ % work_amount);
        if (wl->adjusted_min_work_amount_ > 0) {
            infos += str(format(", expected duration %1% seconds")
                         % (wl->duration_to_work_amount_ratio() * work_amount));
        }
        info_log << infos;

        reported_round_duration = 0;
        round_timer.reset(new cpu_timer);
        int rc =
        wl->workload_func_(wl, wl->rounds_, work_amount, &pilot_malloc_func,
                           &num_of_unit_readings, &unit_readings,
                           &readings, &reported_round_duration, wl->workload_data_);
        measured_round_duration = round_timer->elapsed().wall;
        info_log << "Finished workload round " << wl->rounds_;
        round_duration = reported_round_duration == 0 ? measured_round_duration : reported_round_duration;

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

        // refresh UI
        if (wl->tui_) {
            unique_ptr<pilot_analytical_result_t> wi(wl->get_analytical_result());
            *(wl->tui_) << *wi;
        } else {
            unique_ptr<pilot_analytical_result_t> wi(wl->get_analytical_result());
            stringstream ss;
            ss << setw(3) << left << wl->rounds_ - 1 << " | ";
            if (!wl->num_of_pi_) {
                ss << "no PI";
            }
            for (size_t piid = 0; piid < wl->num_of_pi_; ++piid) {
                if (0 != piid) ss << "; ";
                ss << wl->pi_info_[piid].name << ": ";
                if (4 < wl->analytical_result_.readings_num[piid]) {
                    ss << "R m" << setprecision(4) << wl->analytical_result_.readings_mean_formatted[piid];
                    if (wl->analytical_result_.readings_required_sample_size[piid] > 0) {
                        ss << " c" << wl->analytical_result_.readings_optimal_subsession_ci_width_formatted[piid]
                           << " v" << wl->analytical_result_.readings_optimal_subsession_var_formatted[piid]
                           << " ";
                    } else {
                        ss << " c v ";
                    }
                } else {
                    // No need to print anything when there is no readings data
                }
                if (4 < wl->analytical_result_.unit_readings_num[piid]) {
                    ss << "UR m" << setprecision(4) << wl->analytical_result_.unit_readings_mean_formatted[piid];
                    if (wl->analytical_result_.unit_readings_optimal_subsession_size[piid] > 0) {
                        ss << " c" << wl->analytical_result_.unit_readings_optimal_subsession_ci_width_formatted[piid]
                           << " v" << wl->analytical_result_.unit_readings_optimal_subsession_var_formatted[piid];
                    } else {
                        ss << " c v ";
                    }
                } else {
                    // No need to print anything when there is no unit readings data
                }
            }
            if (wl->wps_enabled()) {
                ss << " WPS ";
                if (wi->wps_has_data) {
                    ss << str(format("a %1%, v %2%, v_ci %3% (%4%%%)")
                              % wi->wps_alpha % wi->wps_v_formatted
                              % wi->wps_v_ci_formatted
                              % (100.0 * wi->wps_v_ci_formatted / wi->wps_v_formatted));
                } else {
                    ss << "no data";
                }
            }
            info_log << ss.str();
        }

        // handle hooks
        if (wl->hook_post_workload_run_ && !wl->hook_post_workload_run_(wl)) {
            info_log << "post_workload_run hook returns false, exiting";
            result = ERR_STOPPED_BY_HOOK;
            break;
        }

        // check session duration limit
        std::chrono::duration<double> diff = std::chrono::steady_clock::now() - session_start_time;
        wl->analytical_result_.session_duration = diff.count();
        if (0 != wl->session_duration_limit_in_sec_) {
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

void pilot_stop_workload(pilot_workload_t *wl) {
    ASSERT_VALID_POINTER(wl);
    wl->status_ = WL_STOP_REQUESTED;
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

        // refresh analytical result
        wl->refresh_analytical_result();
        filename.str(string());
        filename << dirname << "/" << "wps_analysis.csv";
        of.exceptions(ofstream::failbit | ofstream::badbit);
        of.open(filename.str().c_str());
        of << "wps_naive_v,wps_naive_v_formatted,wps_naive_v_err,wps_naive_v_err_percent,wps_alpha,wps_alpha_formatted,wps_v,wps_v_formatted,wps_v_ci,wps_v_ci_formatted,wps_err,wps_err_percent" << endl;
        of << wl->analytical_result_.wps_harmonic_mean << ","
           << wl->analytical_result_.wps_harmonic_mean_formatted << ","
           << wl->analytical_result_.wps_naive_v_err << ","
           << wl->analytical_result_.wps_naive_v_err_percent << ",";
        if (wl->analytical_result_.wps_has_data) {
            of << wl->analytical_result_.wps_alpha << ","
               << wl->analytical_result_.wps_v << ","
               << wl->analytical_result_.wps_v_formatted << ","
               << wl->analytical_result_.wps_v_ci << ","
               << wl->analytical_result_.wps_v_ci_formatted << ","
               << wl->analytical_result_.wps_err << ","
               << wl->analytical_result_.wps_err_percent << endl;
        } else {
            of << ",,,,,,,," << endl;
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

        filename.str(string());
        filename << dirname << "/" << "summary.csv";
        of.exceptions(ofstream::failbit | ofstream::badbit);
        of.open(filename.str().c_str());
        of << "workload name,duration,total rounds" << endl;
        of << format("%1%,%2%,%3%") % wl->workload_name_
              % wl->analytical_result_.session_duration % wl->rounds_;
        of << endl;
        of.close();

        filename.str(string());
        filename << dirname << "/" << "pi_results.csv";
        of.exceptions(ofstream::failbit | ofstream::badbit);
        of.open(filename.str().c_str());
        of << "piid,readings_num,readings_mean,readings_mean_formatted,"
              "readings_subsession_var,readings_subsession_var_formatted,readings_subsession_ci,readings_subsession_ci_formatted,"
              "unit_readings_num,unit_readings_mean,unit_readings_mean_formatted,"
              "unit_readings_var,unit_readings_var_formatted,"
              "unit_readings_subsession_var,unit_readings_subsession_var_formatted,"
              "unit_readings_ci_width,unit_readings_ci_width_formatted,"
              "unit_readings_optimal_subsession_size" << endl;
        for (size_t piid = 0; piid < wl->num_of_pi_; ++piid) {
            of << piid << "," << wl->analytical_result_.readings_num[piid] << ",";
            if (0 != wl->analytical_result_.readings_num[piid]) {
                of << wl->analytical_result_.readings_mean[piid] << ","
                   << wl->analytical_result_.readings_mean_formatted[piid] << ","
                   << wl->analytical_result_.readings_optimal_subsession_var[piid] << ","
                   << wl->analytical_result_.readings_optimal_subsession_var_formatted[piid] << ","
                   << wl->analytical_result_.readings_optimal_subsession_ci_width[piid] << ","
                   << wl->analytical_result_.readings_optimal_subsession_ci_width_formatted[piid] << ",";
            } else {
                of << ",,,,,,";
            }

            of << wl->analytical_result_.unit_readings_num[piid] << ",";
            if (0 != wl->analytical_result_.unit_readings_num[piid]) {
                of << wl->analytical_result_.unit_readings_mean[piid] << ","
                   << wl->analytical_result_.unit_readings_mean_formatted[piid] << ","
                   << wl->analytical_result_.unit_readings_var[piid] << ","
                   << wl->analytical_result_.unit_readings_var_formatted[piid] << ",";
                if (wl->analytical_result_.unit_readings_optimal_subsession_size[piid] > 0) {
                   of << wl->analytical_result_.unit_readings_optimal_subsession_var[piid] << ","
                      << wl->analytical_result_.unit_readings_optimal_subsession_var_formatted[piid] << ","
                      << wl->analytical_result_.unit_readings_optimal_subsession_ci_width[piid] << ","
                      << wl->analytical_result_.unit_readings_optimal_subsession_ci_width_formatted[piid] << ","
                      << wl->analytical_result_.unit_readings_optimal_subsession_size[piid];
                } else {
                    of << ",,,,";
                }
            } else {
                of << ",,,,,,,,";
            }
            of << endl;
        }
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
    ASSERT_VALID_POINTER(g_console_log_sink);
    g_log_level = log_level;
    // We only change the verbose level on the console log sink. The backend always stores
    // every piece of log.
    g_console_log_sink->set_filter
    (
        boost::log::trivial::severity >= (boost::log::trivial::severity_level)log_level
    );
}

const char* pilot_get_last_log_lines(size_t n) {
    return sstream_get_last_lines(g_in_mem_log_buffer, n);
}

pilot_log_level_t pilot_get_log_level(void) {
    return g_log_level;
}

pilot_round_info_t* pilot_round_info(const pilot_workload_t *wl, size_t round, pilot_round_info_t *info) {
    ASSERT_VALID_POINTER(wl);
    return wl->round_info(round, info);
}

pilot_analytical_result_t* pilot_analytical_result(const pilot_workload_t *wl, pilot_analytical_result_t *info) {
    ASSERT_VALID_POINTER(wl);
    return wl->get_analytical_result(info);
}

void pilot_free_analytical_result(pilot_analytical_result_t *result) {
    ASSERT_VALID_POINTER(result);
    delete result;
}

void pilot_analytical_result_t::_free_all_field() {
#define FREE_AND_NULL(field) free(field); field = NULL

    FREE_AND_NULL(readings_num);
    FREE_AND_NULL(readings_mean_method);
    FREE_AND_NULL(readings_dominant_segment_begin);
    FREE_AND_NULL(readings_dominant_segment_size);
    FREE_AND_NULL(readings_mean);
    FREE_AND_NULL(readings_mean_formatted);
    FREE_AND_NULL(readings_var);
    FREE_AND_NULL(readings_var_formatted);
    FREE_AND_NULL(readings_autocorrelation_coefficient);
    FREE_AND_NULL(readings_optimal_subsession_size);
    FREE_AND_NULL(readings_optimal_subsession_var);
    FREE_AND_NULL(readings_optimal_subsession_var_formatted);
    FREE_AND_NULL(readings_optimal_subsession_autocorrelation_coefficient);
    FREE_AND_NULL(readings_optimal_subsession_ci_width);
    FREE_AND_NULL(readings_optimal_subsession_ci_width_formatted);
    FREE_AND_NULL(readings_required_sample_size);
    FREE_AND_NULL(readings_raw_mean);
    FREE_AND_NULL(readings_raw_mean_formatted);
    FREE_AND_NULL(readings_raw_var);
    FREE_AND_NULL(readings_raw_var_formatted);
    FREE_AND_NULL(readings_raw_autocorrelation_coefficient);
    FREE_AND_NULL(readings_raw_optimal_subsession_size);
    FREE_AND_NULL(readings_raw_optimal_subsession_var);
    FREE_AND_NULL(readings_raw_optimal_subsession_var_formatted);
    FREE_AND_NULL(readings_raw_optimal_subsession_autocorrelation_coefficient);
    FREE_AND_NULL(readings_raw_optimal_subsession_ci_width);
    FREE_AND_NULL(readings_raw_optimal_subsession_ci_width_formatted);
    FREE_AND_NULL(readings_raw_required_sample_size);

    FREE_AND_NULL(unit_readings_num);
    FREE_AND_NULL(unit_readings_mean);
    FREE_AND_NULL(unit_readings_mean_formatted);
    FREE_AND_NULL(unit_readings_mean_method);
    FREE_AND_NULL(unit_readings_var);
    FREE_AND_NULL(unit_readings_var_formatted);
    FREE_AND_NULL(unit_readings_autocorrelation_coefficient);
    FREE_AND_NULL(unit_readings_optimal_subsession_size);
    FREE_AND_NULL(unit_readings_optimal_subsession_var);
    FREE_AND_NULL(unit_readings_optimal_subsession_var_formatted);
    FREE_AND_NULL(unit_readings_optimal_subsession_autocorrelation_coefficient);
    FREE_AND_NULL(unit_readings_optimal_subsession_ci_width);
    FREE_AND_NULL(unit_readings_optimal_subsession_ci_width_formatted);
    FREE_AND_NULL(unit_readings_required_sample_size);
    FREE_AND_NULL(unit_readings_required_sample_size_is_from_user);

#undef FREE_AND_NULL
}

void pilot_analytical_result_t::_copyfrom(const pilot_analytical_result_t &a) {
    num_of_pi = a.num_of_pi;
    num_of_rounds = a.num_of_rounds;

#define COPY_ARRAY(field) if (a.field) { field = (typeof(field[0])*)realloc(field, sizeof(field[0]) * num_of_pi); \
    memcpy(field, a.field, sizeof(field[0]) * num_of_pi); } else { field = NULL; }

    COPY_ARRAY(readings_num);
    COPY_ARRAY(readings_mean_method);
    COPY_ARRAY(readings_dominant_segment_begin);
    COPY_ARRAY(readings_dominant_segment_size);
    COPY_ARRAY(readings_mean);
    COPY_ARRAY(readings_mean_formatted);
    COPY_ARRAY(readings_var);
    COPY_ARRAY(readings_var_formatted);
    COPY_ARRAY(readings_autocorrelation_coefficient);
    COPY_ARRAY(readings_optimal_subsession_size);
    COPY_ARRAY(readings_optimal_subsession_var);
    COPY_ARRAY(readings_optimal_subsession_var_formatted);
    COPY_ARRAY(readings_optimal_subsession_autocorrelation_coefficient);
    COPY_ARRAY(readings_optimal_subsession_ci_width);
    COPY_ARRAY(readings_optimal_subsession_ci_width_formatted);
    COPY_ARRAY(readings_required_sample_size);
    COPY_ARRAY(readings_raw_mean);
    COPY_ARRAY(readings_raw_mean_formatted);
    COPY_ARRAY(readings_raw_var);
    COPY_ARRAY(readings_raw_var_formatted);
    COPY_ARRAY(readings_raw_autocorrelation_coefficient);
    COPY_ARRAY(readings_raw_optimal_subsession_size);
    COPY_ARRAY(readings_raw_optimal_subsession_var);
    COPY_ARRAY(readings_raw_optimal_subsession_var_formatted);
    COPY_ARRAY(readings_raw_optimal_subsession_autocorrelation_coefficient);
    COPY_ARRAY(readings_raw_optimal_subsession_ci_width);
    COPY_ARRAY(readings_raw_optimal_subsession_ci_width_formatted);
    COPY_ARRAY(readings_raw_required_sample_size);

    COPY_ARRAY(unit_readings_num);
    COPY_ARRAY(unit_readings_mean);
    COPY_ARRAY(unit_readings_mean_formatted);
    COPY_ARRAY(unit_readings_mean_method);
    COPY_ARRAY(unit_readings_var);
    COPY_ARRAY(unit_readings_var_formatted);
    COPY_ARRAY(unit_readings_autocorrelation_coefficient);
    COPY_ARRAY(unit_readings_optimal_subsession_size);
    COPY_ARRAY(unit_readings_optimal_subsession_var);
    COPY_ARRAY(unit_readings_optimal_subsession_var_formatted);
    COPY_ARRAY(unit_readings_optimal_subsession_autocorrelation_coefficient);
    COPY_ARRAY(unit_readings_optimal_subsession_ci_width);
    COPY_ARRAY(unit_readings_optimal_subsession_ci_width_formatted);
    COPY_ARRAY(unit_readings_required_sample_size);
    COPY_ARRAY(unit_readings_required_sample_size_is_from_user);
#undef COPY_ARRAY

#define COPY_FIELD(field) field = a.field;
    COPY_FIELD(wps_subsession_sample_size);
    COPY_FIELD(wps_harmonic_mean);
    COPY_FIELD(wps_harmonic_mean_formatted);
    COPY_FIELD(wps_naive_v_err);
    COPY_FIELD(wps_naive_v_err_percent)
    COPY_FIELD(wps_has_data);
    if (a.wps_has_data) {
        COPY_FIELD(wps_alpha);
        COPY_FIELD(wps_v);
        COPY_FIELD(wps_v_formatted);
        COPY_FIELD(wps_v_ci);
        COPY_FIELD(wps_v_ci_formatted);
        COPY_FIELD(wps_optimal_subsession_size);
        COPY_FIELD(wps_err);
        COPY_FIELD(wps_err_percent);
    }
    COPY_FIELD(session_duration);
#undef COPY_FIELD
}

pilot_analytical_result_t::pilot_analytical_result_t() {
    memset(this, 0, sizeof(*this));
}

pilot_analytical_result_t::pilot_analytical_result_t(const pilot_analytical_result_t &a) {
    memset(this, 0, sizeof(*this));
    _copyfrom(a);
}

pilot_analytical_result_t::~pilot_analytical_result_t() {
    _free_all_field();
}

void pilot_analytical_result_t::set_num_of_pi(size_t new_num_of_pi) {
    size_t old_num_of_pi = num_of_pi;
    num_of_pi = new_num_of_pi;
#define INIT_FIELD(field) field = (typeof(field[0])*)realloc(field, sizeof(field[0]) * num_of_pi)
// SET_VAL only touches newly allocated space and doesn't change existing value
#define SET_VAL(field, val) if (new_num_of_pi > old_num_of_pi) for (size_t i=old_num_of_pi; i<new_num_of_pi; ++i) field[i] = val

    INIT_FIELD(readings_num);
    SET_VAL(readings_num, 0);
    INIT_FIELD(readings_mean_method);
    INIT_FIELD(readings_dominant_segment_begin);
    INIT_FIELD(readings_dominant_segment_size);
    INIT_FIELD(readings_mean);
    INIT_FIELD(readings_mean_formatted);
    INIT_FIELD(readings_var);
    INIT_FIELD(readings_var_formatted);
    INIT_FIELD(readings_autocorrelation_coefficient);
    INIT_FIELD(readings_required_sample_size);
    SET_VAL(readings_required_sample_size, -1);
    INIT_FIELD(readings_optimal_subsession_size);
    SET_VAL(readings_optimal_subsession_size, -1);
    INIT_FIELD(readings_optimal_subsession_var);
    INIT_FIELD(readings_optimal_subsession_var_formatted);
    INIT_FIELD(readings_optimal_subsession_autocorrelation_coefficient);
    INIT_FIELD(readings_optimal_subsession_ci_width);
    INIT_FIELD(readings_optimal_subsession_ci_width_formatted);
    INIT_FIELD(readings_raw_mean);
    INIT_FIELD(readings_raw_mean_formatted);
    INIT_FIELD(readings_raw_var);
    INIT_FIELD(readings_raw_var_formatted);
    INIT_FIELD(readings_raw_autocorrelation_coefficient);
    INIT_FIELD(readings_raw_required_sample_size);
    SET_VAL(readings_raw_required_sample_size, -1);
    INIT_FIELD(readings_raw_optimal_subsession_size);
    SET_VAL(readings_raw_optimal_subsession_size, -1);
    INIT_FIELD(readings_raw_optimal_subsession_var);
    INIT_FIELD(readings_raw_optimal_subsession_var_formatted);
    INIT_FIELD(readings_raw_optimal_subsession_autocorrelation_coefficient);
    INIT_FIELD(readings_raw_optimal_subsession_ci_width);
    INIT_FIELD(readings_raw_optimal_subsession_ci_width_formatted);

    INIT_FIELD(unit_readings_num);
    INIT_FIELD(unit_readings_mean);
    INIT_FIELD(unit_readings_mean_formatted);
    INIT_FIELD(unit_readings_mean_method);
    INIT_FIELD(unit_readings_var);
    INIT_FIELD(unit_readings_var_formatted);
    INIT_FIELD(unit_readings_autocorrelation_coefficient);
    INIT_FIELD(unit_readings_optimal_subsession_size);
    SET_VAL(unit_readings_optimal_subsession_size, -1);
    INIT_FIELD(unit_readings_optimal_subsession_var);
    INIT_FIELD(unit_readings_optimal_subsession_var_formatted);
    INIT_FIELD(unit_readings_optimal_subsession_autocorrelation_coefficient);
    INIT_FIELD(unit_readings_optimal_subsession_ci_width);
    INIT_FIELD(unit_readings_optimal_subsession_ci_width_formatted);
    INIT_FIELD(unit_readings_required_sample_size);
    SET_VAL(unit_readings_required_sample_size, -1);
    INIT_FIELD(unit_readings_required_sample_size_is_from_user);
#undef SET_VAL
#undef INIT_FIELD
}

pilot_analytical_result_t& pilot_analytical_result_t::operator=(const pilot_analytical_result_t &a) {
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

double __attribute__ ((const)) pilot_calc_deg_of_freedom(double var1, double var2, size_t size1, size_t size2) {
    assert (size1 > 1);
    assert (size2 > 1);
    double num = pow(var1 / static_cast<double>(size1) + var2 / static_cast<double>(size2), 2);
    double denom = pow(var1 / static_cast<double>(size1), 2) / (static_cast<double>(size1) - 1) +
                   pow(var2 / static_cast<double>(size2), 2) / (static_cast<double>(size2) - 1);
    return num / denom;
}

double pilot_p_eq(double mean1, double mean2, size_t size1, size_t size2,
                  double var1, double var2, double *ci_left, double *ci_right,
                  double confidence_level) {
    using namespace boost::math;
    if (var1 < 0 || var2 < 0) {
        fatal_log << __func__ << "(): variance must be greater than or equal 0";
        abort();
    }
    if (size1 < 2 || size2 < 2) {
        info_log << __func__ << "(): sample size (" << size1 << ", " << size2 << ") too small";
        // p = 0.5 means we cannot decide whether they are equal
        return 0.5;
    }

    double d = mean1 - mean2;
    double sc = sqrt(var1 / double(size1) + var2 / double(size2));
    double t = d / sc;

    double deg_of_freedom = pilot_calc_deg_of_freedom(var1, var2, size1, size2);
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

int pilot_optimal_sample_size_for_eq_test(double baseline_mean,
        size_t baseline_sample_size, double baseline_var,
        double new_mean, size_t new_sample_size, double new_var,
        double required_p, size_t *opt_new_sample_size) {
    using namespace boost::math;
    if (baseline_var < 0 || new_var < 0) {
        info_log << __func__ << "(): variance must be greater than or equal 0";
        return -1;
    }
    if (baseline_sample_size < 2 || new_sample_size < 2) {
        info_log << __func__ << "(): sample size (" << baseline_sample_size
                 << ", " << new_sample_size << ") too small";
        // p = 0.5 means we cannot decide whether they are equal
        return ERR_NOT_ENOUGH_DATA;
    }

    double deg_of_freedom = pilot_calc_deg_of_freedom(baseline_var, new_var, baseline_sample_size, new_sample_size);
    students_t dist(deg_of_freedom);
    double t = quantile(dist, required_p / 2);

    double d = baseline_mean - new_mean;
    double opt_ss = new_var / (pow(d / t, 2) -
                             baseline_var / baseline_sample_size);
    *opt_new_sample_size = static_cast<size_t>(opt_ss);
    return 0;
}

bool
pilot_optimal_sample_size_p(const double *data, size_t n,
                            double confidence_interval_width,
                            pilot_mean_method_t mean_method,
                            size_t *q, size_t *opt_sample_size,
                            double confidence_level,
                            double max_autocorrelation_coefficient) {
    ASSERT_VALID_POINTER(q);
    ASSERT_VALID_POINTER(opt_sample_size);
    return pilot_optimal_sample_size(data, n, confidence_interval_width,
                                     mean_method,
                                     q, opt_sample_size,
                                     confidence_level,
                                     max_autocorrelation_coefficient);
}

int pilot_changepoint_detection(const double *data, size_t n,
                                int **changepoints, size_t *cp_n,
                                int min_size, double percent, int degree) {
    ASSERT_VALID_POINTER(data);
    ASSERT_VALID_POINTER(changepoints);
    ASSERT_VALID_POINTER(cp_n);
    if (n < 24) {
        error_log << __func__ << "() requires at least 24 data points";
        return ERR_NOT_ENOUGH_DATA;
    }
    vector<int> t = EDM_percent(data, n, min_size, percent, degree);

    // prepare result array from vector
    size_t result_bytes = sizeof(int) * t.size();
    *changepoints = (int*)malloc(result_bytes);
    memcpy(*changepoints, t.data(), result_bytes);
    *cp_n = t.size();
    return 0;
}

int pilot_find_dominant_segment(const double *data, size_t n, size_t *begin,
        size_t *end, int min_size, double percent, int degree) {
    ASSERT_VALID_POINTER(data);
    ASSERT_VALID_POINTER(begin);
    ASSERT_VALID_POINTER(end);
    if (n < 24) {
        debug_log << __func__ << "() requires at least 24 data points";
        return ERR_NOT_ENOUGH_DATA;
    }
    vector<int> cps = EDM_percent(data, n, min_size, percent, degree);
    if (cps.size() > 0) {
        stringstream ss;
        ss << cps;
        info_log << "Changepoints detected: " << ss.str();
    } else {
        info_log << "No changepoint detected.";
    }
    // we always add the last point for easy of calculation below
    cps.push_back(n);
    struct segment_t {
        size_t begin;
        size_t end;
        size_t length;
    };
    segment_t longest_seg{0, 0, 0};
    segment_t cur_seg{0, 0, 0};
    for (int cp : cps) {
        cur_seg.end = cp;
        cur_seg.length = cp - cur_seg.begin;
        if (cur_seg.length > longest_seg.length) {
            longest_seg = cur_seg;
            if (longest_seg.length > n / 2) {
                *begin = longest_seg.begin;
                *end = longest_seg.end;
                return 0;
            }
        }
        cur_seg.begin = cp;
    }
    return ERR_NO_DOMINANT_SEGMENT;
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

int pilot_warm_up_removal_detect(const pilot_workload_t *wl,
                                 const double *data,
                                 size_t n,
                                 boost::timer::nanosecond_type round_duration,
                                 pilot_warm_up_removal_detection_method_t method,
                                 size_t *begin, size_t *end) {
    ASSERT_VALID_POINTER(wl);
    ASSERT_VALID_POINTER(begin);
    ASSERT_VALID_POINTER(end);
    if (NO_WARM_UP_REMOVAL == method) {
        *begin = 0;
        *end = n;
        return 0;
    }

    // we reject any round that is too short
    if (round_duration < wl->short_round_detection_threshold_) {
        info_log << "Round duration shorter than the lower bound ("
                 << wl->short_round_detection_threshold_ / ONE_SECOND
                 << "s), rejecting";
        *begin = n;
        *end = n;
        return ERR_ROUND_TOO_SHORT;
    }

    switch (method) {
    case FIXED_PERCENTAGE:
        *begin = static_cast<size_t>(round(wl->warm_up_removal_percentage_ * n));
        *end = n;
        if (*begin == n && n != 0) {
            // we leave at least one data
            --(*begin);
        }
        return 0;
        break;
    case EDM:
        return pilot_find_dominant_segment(data, n, begin, end);
        break;
    default:
        fatal_log << "Unknown method";
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
    wl->raw_data_changed_time_ = chrono::steady_clock::now();

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
    bool at_least_one_piid_got_new_data = false;
    for (size_t piid = 0; piid < wl->num_of_pi_; ++piid) {
        // first subtract the number of the old unit readings from total_num_of_unit_readings_
        // before updating the data
        if (round != wl->rounds_) {
            // remove old unit_readings_num
            wl->total_num_of_unit_readings_[piid] -= wl->unit_readings_[piid][round].size()
                                                     - wl->warm_up_phase_len_[piid][round];
        }

        if (round == wl->rounds_) {
            // inserting a new round
            if (unit_readings && unit_readings[piid]) {
                debug_log << "new round num_of_unit_readings = " << num_of_unit_readings;
                wl->unit_readings_[piid].emplace_back(vector<double>(unit_readings[piid], unit_readings[piid] + num_of_unit_readings));
            } else {
                debug_log << str(format("[PI %1%] has no unit readings data in round %2%") % piid % round);
                wl->unit_readings_[piid].emplace_back(vector<double>());
            }
        } else {
            debug_log << "replacing data for an existing round";
            wl->unit_readings_[piid][round].resize(num_of_unit_readings);
            if (unit_readings && unit_readings[piid])
                memcpy(wl->unit_readings_[piid][round].data(), unit_readings[piid], sizeof(double) * num_of_unit_readings);
        }

        // warm-up removal
        size_t dominant_begin = 0, dominant_end = 0;
        if (unit_readings) {
            info_log << "Running changepoint detection on UR data";
            int res = pilot_warm_up_removal_detect(wl, unit_readings[piid],
                                                   num_of_unit_readings,
                                                   round_duration,
                                                   wl->warm_up_removal_detection_method_,
                                                   &dominant_begin, &dominant_end);
            if (res != 0) {
                switch (res) {
                case ERR_NOT_ENOUGH_DATA:
                    info_log << "Skipping non-stable phases detection because we have fewer than 24 URs in the last round. Ingesting all URs in the last round.";
                    // When we cannot detect non-stable phases we just use the whole section for now
                    dominant_begin = 0;
                    dominant_end = num_of_unit_readings;
                    break;
                case ERR_NO_DOMINANT_SEGMENT:
                    info_log << "No dominant section can be found in last round UR data, this can be caused by 1) round too short; 2) variance too high; 3) unknown temporal or spatial drift of PI. Pilot will not ingest URs from this round, because high varaince data would make it harder to converge.";
                    dominant_begin = num_of_unit_readings;
                    dominant_end = num_of_unit_readings;
                    break;
                default:
                    info_log << str(format("Non-stable phase detection failed on PI %1% at round %2% (error %3%). Ignoring UR data in last round.")
                                    % piid % wl->rounds_ % res);
                    dominant_begin = num_of_unit_readings;
                    dominant_end = num_of_unit_readings;
                }

            } else {
                info_log << "Detected dominant segment in UR data ["
                          << dominant_begin << ", " << dominant_end << ")";
                // FIXME: store dominant_end instead of chopping away the data
                wl->unit_readings_[piid][round].resize(dominant_end);
            }
        }
        if (round == wl->rounds_) {
            wl->warm_up_phase_len_[piid].push_back(dominant_begin);
        } else {
            wl->warm_up_phase_len_[piid][round] = dominant_begin;
        }
        int new_urs = int(wl->unit_readings_[piid][round].size()) - int(dominant_begin);
        if (new_urs > 0) {
            // increase total number of unit readings (the number of old unit readings are
            // subtracted at the beginning of the for loop).
            info_log << str(format("Ingested %1% URs from last round") % new_urs);
            wl->total_num_of_unit_readings_[piid] += new_urs;
            at_least_one_piid_got_new_data = true;
        }

        // handle readings
        if (readings) {
            at_least_one_piid_got_new_data = true;
            if (round == wl->rounds_) {
                wl->readings_[piid].push_back(readings[piid]);
                ++wl->total_num_of_readings_[piid];
            } else {
                wl->readings_[piid][round] = readings[piid];
            }
        }
    } // for loop for PI
    if (wl->num_of_pi_ != 0 && !at_least_one_piid_got_new_data) {
        info_log << "No data ingested in round " << round;
        ++wl->wholly_rejected_rounds_;
    }

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

bool pilot_next_round_work_amount(const pilot_workload_t *wl, size_t *needed_work_amount) {
    ASSERT_VALID_POINTER(wl);
    return wl->calc_next_round_work_amount(needed_work_amount);
}

void pilot_set_short_round_detection_threshold(pilot_workload_t *wl, size_t threshold) {
    ASSERT_VALID_POINTER(wl);
    wl->short_round_detection_threshold_ = ONE_SECOND * threshold;
}

void pilot_set_required_confidence_interval(pilot_workload_t *wl, double percent_of_mean, double absolute_value) {
    ASSERT_VALID_POINTER(wl);
    wl->set_required_ci_percent_of_mean(percent_of_mean);
    wl->set_required_ci_absolute_value(absolute_value);
}

bool calc_next_round_work_amount_meet_lower_bound(const pilot_workload_t *wl, size_t *needed_work_amount) {
    // We can't do anything if this workload doesn't support setting work amount.
    if (0 == wl->max_work_amount_) {
        *needed_work_amount = 0;
        return false;
    }
    // No need to do anything for first round.
    if (0 == wl->rounds_) {
        *needed_work_amount = 0;
        return true;
    }

    if (wl->round_durations_.back() < wl->short_round_detection_threshold_) {
        info_log << "Previous round duration (" << double(wl->round_durations_.back()) / ONE_SECOND << " s) "
                 << "is shorter than the lower bound (" << double(wl->short_round_detection_threshold_) / ONE_SECOND << " s).";
        if (wl->round_work_amounts_.back() == wl->max_work_amount_) {
            fatal_log << "Running at max_work_amount_ still cannot meet round duration requirement. Please increase the max work amount upper limit.";
            *needed_work_amount = wl->max_work_amount_;
        } else {
            if (wl->round_work_amounts_.back() == wl->max_work_amount_) {
                fatal_log << "The round using max_work_amount_ is still shorter than the lower bound,"
                             "please change your settings.";
                abort();
            } else if (wl->round_work_amounts_.back() > wl->max_work_amount_ / 2) {
                *needed_work_amount = wl->max_work_amount_;
                info_log << str(format("Proposing to using max_work_amount (%1%).") % *needed_work_amount);
            } else {
                *needed_work_amount = min(wl->round_work_amounts_.back() * 2, wl->max_work_amount_);
                info_log << str(format("Proposing to using previous round's work amount x 2 (%1%).") % *needed_work_amount);
            }
        }
        return true;
    } else if (wl->adjusted_min_work_amount_ < 0) {
        info_log << "Setting adjusted_min_work_amount to " << wl->round_work_amounts_.back();
        wl->adjusted_min_work_amount_ = wl->round_work_amounts_.back();
        wl->finish_runtime_analysis_plugin(&calc_next_round_work_amount_meet_lower_bound);
        // We return the lower bound (wl->adjusted_min_work_amount_) because
        // the caller wouldn't pick up the new wl->adjusted_min_work_amount_
        // until next round.
        *needed_work_amount = wl->adjusted_min_work_amount_;
        return false;
    }
    SHOULD_NOT_REACH_HERE;
    return true;
}

bool calc_next_round_work_amount_from_readings(const pilot_workload_t *wl, size_t *needed_work_amount) {
    // No need to do anything for first round.
    if (0 == wl->rounds_) {
        *needed_work_amount = 0;
        return true;
    }

    if (0 == wl->max_work_amount_) {
        *needed_work_amount = 0;
    } else {
        *needed_work_amount = wl->init_work_amount_;
    }

    for (size_t piid = 0; piid != wl->num_of_pi_; ++piid) {
        if (!wl->pi_info_[piid].reading_must_satisfy) {
            continue;
        }
        if (0 == wl->total_num_of_readings_[piid]) {
            // no need to do anything if this PIID has no unit reading
            continue;
        }
        ssize_t req = wl->required_num_of_readings(piid);
        debug_log << "[PI " << piid << "] required readings sample size (-1 means not enough data): " << req;
        if (req > 0) {
            if (static_cast<size_t>(req) <= wl->total_num_of_readings_[piid]) {
                info_log << "[PI " << piid << "] already has enough readings";
                continue;
            } else {
                info_log << str(format("[PI %1%] needs %2% more readings") % piid % (static_cast<size_t>(req) - wl->total_num_of_readings_[piid]));
            }
        } else {
            info_log << "[PI " << piid << "] doesn't have enough readings for calculating required sample size, continuing to next round";
        }
        // return true when any PI needs more samples
        return true;
    }
    return false;
}

bool calc_next_round_work_amount_from_unit_readings(const pilot_workload_t *wl, size_t *needed_work_amount) {
    // No need to do anything for first round.
    if (0 == wl->rounds_) {
        *needed_work_amount = 0;
        return true;
    }

    bool need_more_rounds = false;
    size_t max_work_amount_needed = 0;
    for (size_t piid = 0; piid != wl->num_of_pi_; ++piid) {
        if (!wl->pi_info_[piid].unit_reading_must_satisfy) {
            continue;
        }
        if (0 == wl->total_num_of_unit_readings_[piid]) {
            if (0 == wl->max_work_amount_) {
                return true;
            } else {
                if (wl->round_work_amounts_.back() == 0) {
                    max_work_amount_needed = max(max_work_amount_needed, wl->init_work_amount_);
                    if (0 == max_work_amount_needed) {
                        max_work_amount_needed = wl->max_work_amount_ / 10;
                    }
                    if (0 == max_work_amount_needed) {
                        max_work_amount_needed = wl->max_work_amount_;
                    }
                } else {
                    max_work_amount_needed = min(wl->max_work_amount_, wl->round_work_amounts_.back() * 2);
                }
                need_more_rounds = true;
                continue;
            }
        }
        size_t work_amount_for_this_pi = 0;
        if (0 != wl->max_work_amount_ && abs(wl->calc_avg_work_unit_per_amount(piid)) < 0.00000001) {
            error_log << "[PI " << piid << "] average work per unit reading is 0 (you probably need to report a bug)";
            // when there's not enough information, we double the previous work amount
            work_amount_for_this_pi = 2 * wl->round_work_amounts_.back();
        } else {
            size_t num_of_ur_needed;
            ssize_t req = wl->required_num_of_unit_readings(piid);
            if (req > 0) {
                ssize_t subsession_size = wl->analytical_result_.unit_readings_optimal_subsession_size[piid];
                if (subsession_size > 1) {
                    info_log << str(format("[PI %1%] has high autocorrelation (%2%), merging every %3% samples to make URs indepedent.")
                                           % piid % (wl->analytical_result_.unit_readings_autocorrelation_coefficient[piid])
                                           % subsession_size);
                }
                string tmps = str(format("[PI %1%] required unit readings sample size %2% ") % piid % req);
                if (wl->analytical_result_.unit_readings_required_sample_size_is_from_user[piid]) {
                    tmps += "(supplied by the calc_required_unit_readings_func)";
                } else {
                    tmps += str(format("(required sample size %1% x subsession size %2%)") % (req / subsession_size) % subsession_size);
                }
                info_log << tmps;
                if (static_cast<size_t>(req) < wl->total_num_of_unit_readings_[piid]) {
                    debug_log << "[PI " << piid << "] already has enough samples";
                    continue;
                }
                num_of_ur_needed = req - wl->total_num_of_unit_readings_[piid];
            } else {
                // in case when there's not enough data to calculate the number of UR,
                // we try to double the total number of unit readings
                info_log << "[PI " << piid << "] doesn't have enough information for calculating required sample size";
                num_of_ur_needed = wl->total_num_of_unit_readings_[piid];
            }
            if (0 == wl->max_work_amount_) {
                *needed_work_amount = 0;
                // no need to check others if this workload doesn't support setting work amount
                return true;
            } else {
                work_amount_for_this_pi = size_t(1.2 * num_of_ur_needed * wl->calc_avg_work_unit_per_amount(piid));
            }
        }
        need_more_rounds = true;
        max_work_amount_needed = max(max_work_amount_needed, work_amount_for_this_pi);
        if (max_work_amount_needed >= wl->max_work_amount_) {
            *needed_work_amount = wl->max_work_amount_;
            return true;
        }
    }
    *needed_work_amount = max_work_amount_needed;
    return need_more_rounds;
}

bool calc_next_round_work_amount_from_wps(const pilot_workload_t *wl, size_t *needed_work_amount) {
    *needed_work_amount = 0;
    if (0 == wl->max_work_amount_) {
        warning_log << "max_work_amount is not set, skipping WPS analysis";
        return false;
    }
    if (wl->adjusted_min_work_amount_ < 0) {
        debug_log << "WPS analysis won't start until round duration lower bound is reached";
        *needed_work_amount = wl->init_work_amount_;
        return true;
    }
    size_t min_wa = max(wl->adjusted_min_work_amount_, ssize_t(wl->min_work_amount_));
    if (min_wa == wl->max_work_amount_) {
        warning_log << "min_work_amount == max_work_amount, WPS analysis is impossible, skipping.";
        return false;
    }
    if (0 == wl->wps_slices_) {
        // Calculating the initial slice size. See [Li16] Equation (5).
        // Basically we want to do n rounds within session_desired_duration_in_sec_.
        int n = 10;
        if (wl->max_work_amount_ - min_wa <= size_t(n)) {
            // Handle the rare case when the max_work_amount is very close to min_wa
            warning_log << "max_work_amount - min_work_amount is too small. WPS analysis may never get enough samples to finish.";
            wl->wps_slices_ = wl->max_work_amount_ - min_wa;
        } else {
            double t = double(ONE_SECOND) * wl->session_desired_duration_in_sec_;
            double s = double(wl->round_durations_.back());
            // next round should be at least 1 sec longer than previous round
            double k = max(double(ONE_SECOND), (2 * t - 2 * s * n) / (n * n - n));
            double work_amount_per_nanosec = double(wl->round_work_amounts_.back()) / wl->round_durations_.back();
            double slice_size_float = k * work_amount_per_nanosec;
            // 5 is a fail-safe value
            wl->wps_slices_ = max(size_t(5), size_t( (wl->max_work_amount_ - min_wa) / slice_size_float ));
            info_log << str(format("Calculated initial number of WPS slices %1% with slice size %2%")
                                   % wl->wps_slices_ % ((wl->max_work_amount_ - min_wa) / wl->wps_slices_));
        }
    }
    size_t wa_slice_size = (wl->max_work_amount_ - min_wa) / wl->wps_slices_;
    if (0 == wl->rounds_) {
        *needed_work_amount = wa_slice_size;
        return wl->wps_must_satisfy_;
    }

    if (wl->rounds_ > 3) {
        wl->refresh_wps_analysis_results();

        if (wl->analytical_result_.wps_has_data) {
            const size_t kWPSSubsessionSampleSizeThreshold = 20;
            if (wl->analytical_result_.wps_subsession_sample_size > kWPSSubsessionSampleSizeThreshold) {
                if (wl->analytical_result_.wps_v > 0 && wl->analytical_result_.wps_v_ci > 0 &&
                    wl->analytical_result_.wps_v_ci < wl->get_required_ci(wl->analytical_result_.wps_v)) {
                    info_log << "WPS confidence interval small enough";
                    return false;
                } else {
                    if (wl->wps_must_satisfy_) {
                        info_log << "WPS confidence interval not small enough, needs more samples";
                        // TODO: calculate how many more samples are needed
                    }
                }
            } else {
                if (wl->wps_must_satisfy_) {
                    info_log << str(format("WPS analysis needs more samples (proposed subsession size %1%, probably needs %2% more samples)")
                                           % wl->analytical_result_.wps_optimal_subsession_size
                                           % ((kWPSSubsessionSampleSizeThreshold - wl->analytical_result_.wps_subsession_sample_size) * wl->analytical_result_.wps_optimal_subsession_size) );
                }
            }
        }
    }

    size_t last_round_wa = wl->round_work_amounts_.back();
    if (last_round_wa < min_wa) {
        *needed_work_amount = min_wa + wa_slice_size;
        return wl->wps_must_satisfy_;
    }
    if (last_round_wa > wl->max_work_amount_ - wa_slice_size) {
        if (1 == wa_slice_size) {
            warning_log << "It is impossible to further decrease WPS slice size. WPS analysis may never finish. Consider increasing max_work_amount.";
        } else {
            wl->wps_slices_ *= 2;
            wa_slice_size /= 2;
        }
        *needed_work_amount = min_wa + wa_slice_size;
        return wl->wps_must_satisfy_;
    } else {
        // calculate the location of the next slice from last_round_wa
        *needed_work_amount = min_wa + ((last_round_wa - min_wa) / wa_slice_size + 1) * wa_slice_size;
        return wl->wps_must_satisfy_;
    }
}

bool calc_next_round_work_amount_for_comparison(const pilot_workload_t *wl, size_t *needed_work_amount) {
    size_t max_work_amount_needed = 0;
    bool need_more_rounds = false;
    for (size_t piid = 0; piid != wl->num_of_pi_; ++piid) {
        // TODO: handle comparison of readings

        // handling comparison of unit readings
        if (wl->baseline_of_unit_readings_[piid].set) {
            if (0 == wl->total_num_of_unit_readings_[piid]) {
                // no need to do anything if this PIID has no unit reading
                warning_log << __func__ << "(): baseline of PI " << piid << " exists but no unit reading data";
                continue;
            }
            size_t work_amount_for_this_pi = 0;
            if (0 != wl->max_work_amount_ && abs(wl->calc_avg_work_unit_per_amount(piid)) < 0.00000001) {
                error_log << "[PI " << piid << "] average work per unit reading is 0 (you probably need to report a bug)";
                // when there's not enough information, we double the previous work amount
                work_amount_for_this_pi = 2 * wl->round_work_amounts_.back();
            } else {
                size_t num_of_ur_needed;
                ssize_t req = wl->required_num_of_unit_readings_for_comparison(piid);
                if (req > 0) {
                    debug_log << "[PI " << piid << "] the comparison against baseline requires " << req << " unit readings";
                    if (static_cast<size_t>(req) < wl->total_num_of_unit_readings_[piid]) {
                        debug_log << "[PI " << piid << "] already has enough samples for comparison against baseline";
                        continue;
                    }
                    num_of_ur_needed = req - wl->total_num_of_unit_readings_[piid];
                } else {
                    // in case when there's not enough data to calculate the number of UR,
                    // we try to double the total number of unit readings
                    debug_log << "[PI " << piid << "] doesn't have enough information for calculating required sample size, using the current total sample size instead";
                    num_of_ur_needed = wl->total_num_of_unit_readings_[piid];
                }
                if (0 == wl->max_work_amount_) {
                    return true;
                } else {
                    work_amount_for_this_pi = size_t(1.2 * num_of_ur_needed * wl->calc_avg_work_unit_per_amount(piid));
                }
            }
            need_more_rounds = true;
            max_work_amount_needed = max(max_work_amount_needed, work_amount_for_this_pi);
            if (max_work_amount_needed >= wl->max_work_amount_) {
                *needed_work_amount = wl->max_work_amount_;
                return true;
            }
        } // if (!wl->baseline_of_unit_readings_[piid].set)
    }
    *needed_work_amount = max_work_amount_needed;
    return need_more_rounds;
}

int pilot_set_wps_analysis(pilot_workload_t *wl,
        pilot_pi_display_format_func_t *format_wps_func,
        bool enabled, bool wps_must_satisfy) {
    wl->format_wps_.format_func_ = format_wps_func;
    return wl->set_wps_analysis(enabled, wps_must_satisfy);
}

size_t pilot_set_session_desired_duration(pilot_workload_t *wl, size_t sec) {
    return wl->set_session_desired_duration(sec);
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

void pilot_set_baseline(pilot_workload_t *wl, size_t piid, pilot_reading_type_t rt,
        double baseline_mean, size_t baseline_sample_size, double baseline_var) {
    ASSERT_VALID_POINTER(wl);
    wl->set_baseline(piid, rt, baseline_mean, baseline_sample_size, baseline_var);
}

int pilot_get_baseline(const pilot_workload_t *wl, size_t piid, pilot_reading_type_t rt,
        double *p_baseline_mean, size_t *p_baseline_sample_size,
        double *p_baseline_var) {
    ASSERT_VALID_POINTER(wl);
    ASSERT_VALID_POINTER(p_baseline_mean);
    ASSERT_VALID_POINTER(p_baseline_sample_size);
    ASSERT_VALID_POINTER(p_baseline_var);
    if (0 == wl->num_of_pi_) {
        return ERR_NOT_INIT;
    }
    die_if(piid >= wl->num_of_pi_, ERR_WRONG_PARAM, "PIID out of bound");

    switch (rt) {
    case READING_TYPE:
        if (!wl->baseline_of_readings_[piid].set)
            return ERR_NOT_INIT;
        *p_baseline_mean        = wl->baseline_of_readings_[piid].mean;
        *p_baseline_sample_size = wl->baseline_of_readings_[piid].sample_size;
        *p_baseline_var         = wl->baseline_of_readings_[piid].var;
        break;
    case UNIT_READING_TYPE:
        if (!wl->baseline_of_unit_readings_[piid].set)
            return ERR_NOT_INIT;
        *p_baseline_mean        = wl->baseline_of_unit_readings_[piid].mean;
        *p_baseline_sample_size = wl->baseline_of_unit_readings_[piid].sample_size;
        *p_baseline_var         = wl->baseline_of_unit_readings_[piid].var;
        break;
    case WPS_TYPE:
        fatal_log << __func__ << "(): unimplemented yet";
        abort();
        break;
    default:
        fatal_log << __func__ << "(): invalid parameter: rt: " << rt;
        abort();
    }
    return 0;
}

int pilot_load_baseline_file(pilot_workload_t *wl, const char *filename) {
    ASSERT_VALID_POINTER(wl);
    return wl->load_baseline_file(filename);
}

size_t pilot_set_min_sample_size(pilot_workload_t *wl, size_t min_sample_size) {
    ASSERT_VALID_POINTER(wl);
    return wl->set_min_sample_size(min_sample_size);
}

int _simple_workload_func_runner(const pilot_workload_t *wl,
                       size_t round,
                       size_t total_work_amount,
                       pilot_malloc_func_t *lib_malloc_func,
                       size_t *num_of_work_unit,
                       double ***unit_readings,
                       double **readings,
                       nanosecond_type *round_duration,
                       void *data) {
    ASSERT_VALID_POINTER(data);
    pilot_simple_workload_func_t *func = (pilot_simple_workload_func_t*)data;
    const size_t num_of_pi = 1;
    *num_of_work_unit = total_work_amount;
    *unit_readings = (double**)lib_malloc_func(sizeof(double*) * num_of_pi);
    (*unit_readings)[0] = (double*)lib_malloc_func(sizeof(double) * *num_of_work_unit);

    int rc;
    cpu_timer timer;
    nanosecond_type start_time, end_time;
    for (size_t i = 0; i != total_work_amount; ++i) {
        start_time = timer.elapsed().wall;
        rc = func();
        end_time = timer.elapsed().wall;
        (*unit_readings)[0][i] = double((end_time - start_time)) / ONE_SECOND;
        if (rc)
            return rc;
    }
    return 0;
}

int _simple_workload_func_with_wa_runner(const pilot_workload_t *wl,
                       size_t round,
                       size_t total_work_amount,
                       pilot_malloc_func_t *lib_malloc_func,
                       size_t *num_of_work_unit,
                       double ***unit_readings,
                       double **readings,
                       nanosecond_type *round_duration,
                       void *data) {
    ASSERT_VALID_POINTER(data);
    pilot_simple_workload_with_wa_func_t *func = (pilot_simple_workload_with_wa_func_t*)data;
    return func(total_work_amount);
}

int _simple_runner(pilot_simple_workload_func_t func,
                   const char* benchmark_name) {
    PILOT_LIB_SELF_CHECK;
    int res, wl_res;

    pilot_set_log_level(lv_info);
    shared_ptr<pilot_workload_t> wl(pilot_new_workload(benchmark_name), pilot_destroy_workload);
    pilot_set_num_of_pi(wl.get(), 1);
    pilot_set_pi_info(wl.get(), 0, "Duration", "second", NULL, NULL, false, true, ARITHMETIC_MEAN);
    pilot_set_wps_analysis(wl.get(), NULL, false, false);
    pilot_set_init_work_amount(wl.get(), 0);
    pilot_set_work_amount_limit(wl.get(), ULONG_MAX);
    pilot_set_workload_data(wl.get(), (void*)func);
    pilot_set_workload_func(wl.get(), _simple_workload_func_runner);
    pilot_set_short_round_detection_threshold(wl.get(), 1);

    wl_res = pilot_run_workload(wl.get());
    if (0 == wl_res) {
        info_log << "Benchmark finished successfully";
    } else {
        error_log << "Benchmark finished with error code " << wl_res << " (" << pilot_strerror(wl_res) << ")";
    }

    res = pilot_export(wl.get(), benchmark_name);
    if (0 == res) {
        info_log << "Benchmark results are saved in directory " << benchmark_name;
    } else {
        error_log << "Error on saving benchmark results: " << pilot_strerror(res);
    }

    return wl_res;
}

int _simple_runner_with_wa(pilot_simple_workload_with_wa_func_t func,
                           const char* benchmark_name,
                           size_t min_wa, size_t max_wa,
                           size_t short_round_threshold) {
    PILOT_LIB_SELF_CHECK;
    pilot_set_log_level(lv_warning);
    shared_ptr<pilot_workload_t> wl(pilot_new_workload(benchmark_name), pilot_destroy_workload);
    pilot_set_num_of_pi(wl.get(), 0);                   // we use WPS analysis only
    pilot_set_init_work_amount(wl.get(), min_wa);
    pilot_set_work_amount_limit(wl.get(), max_wa);
    pilot_set_workload_data(wl.get(), (void*)func);
    pilot_set_workload_func(wl.get(), _simple_workload_func_with_wa_runner);
    pilot_set_wps_analysis(wl.get(), NULL, true, true);
    pilot_set_short_round_detection_threshold(wl.get(), short_round_threshold);
    return pilot_run_workload(wl.get());
}

} // namespace pilot
