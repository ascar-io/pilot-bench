/*
 * func_test_seq_write.cc: functional test for libpilot using a
 * sequential write I/O workload
 *
 * The program may seem long, but actually the logic of this program is simple:
 * main() processes command line options and sets up a simple workload using
 * the libpilot interface, using workload_func() as the workload function.
 * libpilot will call this workload function one or multiple time to collect
 * enough samples to meet the statistics requirement and produce results.
 * Also please read the "Performance measurement of a sequential write
 * workload" wiki page for more information.
 *
 * Copyright (c) 2015, 2016 University of California, Santa Cruz, CA, USA.
 * Created by Yan Li <yanli@ascar.io>,
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

#include <boost/program_options.hpp>
#include <boost/timer/timer.hpp>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <signal.h>
#include <string>
#include <vector>
#include "../interface_include/libpilot.h"

using namespace pilot;
using namespace std;
namespace po = boost::program_options;
using boost::timer::cpu_timer;
using boost::timer::nanosecond_type;
nanosecond_type const ONE_SECOND = 1000000000LL;
size_t const MEGABYTE = 1024*1024;
size_t g_io_size = 1024*1024*1;
string g_output_file_name;
char *g_io_buf;
bool g_fsync = false;
bool g_quiet_mode = false;
static shared_ptr<pilot_workload_t> g_wl;

// We are using two PIs. 0 is time per I/O, 1 is throughput per I/O.
enum piid_t {
    time_pi = 0,
    //tp_pi = 1
};
static const size_t num_of_pi = 1;

void sigint_handler(int dummy) {
    if (g_wl)
        pilot_stop_workload(g_wl.get());
}

/**
 * \brief the sequential write workload func for libpilot
 * \details This function generates a series of sequential I/O and calculate the throughput.
 * @param[in] total_work_amount
 * @param[in] lib_malloc_func
 * @param[out] num_of_work_unit
 * @param[out] reading_per_work_unit the memory can be
 * @param[out] readings
 * @return
 */
int workload_func(const pilot_workload_t *wl,
                  size_t round,
                  size_t total_work_amount,
                  pilot_malloc_func_t *lib_malloc_func,
                  size_t *num_of_work_unit,
                  double ***unit_readings,
                  double **readings,
                  nanosecond_type *round_duration, void *data) {
    *num_of_work_unit = total_work_amount / g_io_size;
    // allocate space for storing result readings
    *readings = (double*)lib_malloc_func(sizeof(double) * num_of_pi);
    *unit_readings = (double**)lib_malloc_func(sizeof(double*) * num_of_pi);
    (*unit_readings)[0] = (double*)lib_malloc_func(sizeof(double) * *num_of_work_unit);
    //(*unit_readings)[1] = (double*)lib_malloc_func(sizeof(double) * *num_of_work_unit);
    vector<nanosecond_type> work_unit_elapsed_times(*num_of_work_unit);

    int fd = open(g_output_file_name.c_str(), O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP);
    if (fd == -1) {
        cerr << "file open error: " << strerror(errno) << endl;
        return -1;
    }
    ssize_t res;
    size_t my_work_amount = total_work_amount;
    size_t io_size;
    char *io_buf;
    cpu_timer timer;
    size_t unit_id = 0;
    while (my_work_amount > 0) {
        io_buf = g_io_buf;
        io_size = min(my_work_amount, g_io_size);
        my_work_amount -= io_size;
        while(true) {
            res = write(fd, io_buf, io_size);
            if (res > 0 && static_cast<size_t>(res) == io_size)
                break;
            if (res > 0 && static_cast<size_t>(res) != io_size) {
                io_size -= res;
                io_buf += res;
                continue;
            }
            if (EAGAIN == res)
                continue;
            else {
                cerr << "I/O error: " << strerror(errno) << endl;
                close(fd);
                return errno;
            }
        }
        if (g_fsync) {
            do {
                res = fsync(fd);
            } while (EINTR == res);
            if (0 != res) {
                cerr << "fsync error: " << strerror(errno) << endl;
                close(fd);
                return errno;
            }
        }
        if (unit_id < *num_of_work_unit)
            work_unit_elapsed_times[unit_id] = timer.elapsed().wall;
        ++unit_id;
    }
    *round_duration = timer.elapsed().wall;
    // sync to make sure the tear down phase appear
    fsync(fd);
    close(fd);

    // we do calculation after finishing the workload to minimize the overhead
    nanosecond_type prev_ts = 0;
    for (size_t i = 0; i < *num_of_work_unit; ++i) {
        (*unit_readings)[time_pi][i] = (double)(work_unit_elapsed_times[i]-prev_ts) / ONE_SECOND;
        //(*unit_readings)[tp_pi][i] = ((double)g_io_size / MEGABYTE) / ((double)(work_unit_elapsed_times[i]-prev_ts) / ONE_SECOND);
        prev_ts = work_unit_elapsed_times[i];
    }

    // We don't provide readings yet, and will just rely on WPS analysis.
    (*readings)[time_pi] = double(*round_duration) / ONE_SECOND;
    //(*readings)[tp_pi] = ((double)total_work_amount / MEGABYTE) / (double(*round_duration) / ONE_SECOND);
    return 0;
}

static int g_current_round = 0;
bool pre_workload_run_hook(pilot_workload_t* wl) {
    if (g_quiet_mode) return true;

    g_current_round = pilot_get_num_of_rounds(wl);
    size_t wa;
    pilot_next_round_work_amount(wl, &wa);
    pilot_ui_printf(wl, "Round %d started with %zu MB/s work amount ...\n",
                    g_current_round, wa / MEGABYTE);
    return true;
}

bool post_workload_run_hook(pilot_workload_t* wl) {
    if (g_quiet_mode) return true;

    stringstream ss;
    char *buf;
    ss << "Round finished" << endl;
    ss << "Round " << g_current_round << " Summary" << endl;
    ss << "============================" << endl;
    buf = pilot_text_round_summary(wl, pilot_get_num_of_rounds(wl) - 1);
    ss << buf;
    pilot_free_text_dump(buf);

    ss << "Workload Summary So Far" << endl;
    ss << "============================" << endl;
    buf = pilot_text_workload_summary(wl);
    ss << buf;
    pilot_free_text_dump(buf);

    pilot_ui_printf(wl, "%s\n", ss.str().c_str());
    return true;
}

double ur_format_func(const pilot_workload_t* wl, double ur) {
    return double(g_io_size) / ur / MEGABYTE;
}
double wps_format_func(const pilot_workload_t* wl, double wps) {
    return wps / MEGABYTE;
}

int main(int argc, char **argv) {
    PILOT_LIB_SELF_CHECK;
    // Parsing the command line arguments
    po::options_description desc("Generates a non-zero sequential write I/O workload and demonstrates how to use libpilot");
    // Don't use po::value<>()->required() here or --help wouldn't work
    desc.add_options()
            ("help", "produce help message")
            ("autocorr-threshold,a", po::value<double>(), "the threshold for autocorrelation coefficient (default to 1)")
            ("baseline,b", po::value<string>(), "the input file that contains baseline data for comparison")
            ("ci,c", po::value<double>(), "the desired width of CI as percentage of its central value (default: 0.4)")
            ("disable-r", "disable readings analysis")
            ("disable-ur", "disable unit readings analysis")
            ("duration-limit,d", po::value<size_t>(), "the maximum duration of the benchmark in seconds (default: unlimited)")
            ("edm,e", "use the EDM method for warm-up detection (default)")
            ("fsync,f", "call fsync() after each I/O request")
            ("io-size,s", po::value<size_t>(), "the size of I/O operations (default to 1 MB)")
            ("length-limit,l", po::value<size_t>(), "the max. length of the workload in bytes (default to 2048*1024*1024); "
                    "the workload will not write beyond this limit")
            ("min-round-duration", po::value<size_t>(), "the min. acceptable length of a round (in seconds, default to 1 second)")
            ("init-length,i", po::value<size_t>(), "the initial length of workload in bytes (default to 1/10 of limitsize); "
                    "the workload will start from this length and be gradually repeated or increased until the desired "
                    "confidence level is reached")
            ("no-tui", "disable the text user interface")
            ("output,o", po::value<string>(), "set output file name, can be a device. WARNING: the file will be overwritten if it exists.")
            ("quiet,q", "quiet mode. Print results in this format: URResult,URCI,URVar,WPSa,WPSv,WPSvCI,WPSvVar,TestDuration. Quiet mode always enables --no-tui.")
            ("result-dir,r", po::value<string>(), "set result directory name, (default to seq-write-dir)")
            ("verbose,v", "print more debug information")
            ("warm-up-io,w", po::value<double>(), "the percent of I/O operations that will be removed from the beginning as the warm-up phase. Cannot be used together with EDM (default is use EDM)")
            ("wps", "work amount per second (WPS) CI must meet requirement (default: no)")
            ;

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    } catch (po::error &e) {
        cout << e.what() << endl;
        return 1;
    }

    if (vm.count("help")) {
        cout << desc << endl;
        return 2;
    }

    // verbose
    pilot_set_log_level(lv_info);
    if (vm.count("verbose")) {
        if (vm.count("quiet")) {
            cerr << "--verbose and --quiet cannot be used at the same time";
            return 2;
        }
        pilot_set_log_level(lv_trace);
    }

    bool use_tui = true;
    if (vm.count("no-tui"))
        use_tui = false;

    if (vm.count("quiet")) {
        pilot_set_log_level(lv_warning);
        g_quiet_mode = true;
        use_tui = false;
    }

    // fsync
    if (vm.count("fsync")) {
        g_fsync = true;
    }

    size_t io_limit = 2048*1024*1024L;
    if (vm.count("output")) {
        g_output_file_name = vm["output"].as<string>();
        if (!g_quiet_mode) {
            cout << "Output file is set to " << g_output_file_name << endl;
        }
    } else {
        cerr << "Error: output file was not set." << endl;
        cerr << desc << endl;
        return 2;
    }

    if (vm.count("io-size")) {
        if (vm["io-size"].as<size_t>() <= 0) {
            cerr << "I/O size must be larger than 0" << endl;
            return 1;
        }
        g_io_size = vm["io-size"].as<size_t>();
    }
    string result_dir_name("seq-write-results");
    if (vm.count("result-dir")) {
        result_dir_name = vm["result-dir"].as<string>();
    }

    // fill g_io_buf with some pseudo-random data to prevent some smart SSDs from compressing
    // the data
    g_io_buf = new char[g_io_size];
    for (size_t i = 0; i < g_io_size; ++i)
        g_io_buf[i] = (char)i + 42;
    if (!g_quiet_mode) {
        cout << "I/O size is set to ";
        if (g_io_size >= MEGABYTE) {
            cout << g_io_size / MEGABYTE << " MB" << endl;
        } else {
            cout << g_io_size << " bytes" << endl;
        }
    }

    if (vm.count("length-limit")) {
        if (vm["length-limit"].as<size_t>() <= 0) {
            cerr << "I/O limit must be larger than 0" << endl;
            return 1;
        }
        io_limit = vm["length-limit"].as<size_t>();
    }
    if (!g_quiet_mode) {
        cout << "I/O limit is set to ";
        if (io_limit >= MEGABYTE) {
            cout << io_limit / MEGABYTE << " MB"<< endl;
        } else {
            cout << io_limit << " bytes" << endl;
        }
    }

    size_t init_length;
    if (vm.count("init-length"))
        init_length = vm["init-length"].as<size_t>();
    else
        init_length = io_limit / 5;

    bool need_wps = false;
    if (vm.count("wps"))
        need_wps = true;
    bool disable_r = true;
    if (vm.count("disable-r"))
        disable_r = true;
    bool disable_ur = false;
    if (vm.count("disable-ur"))
        disable_ur = true;

    string baseline_file;
    if (vm.count("baseline")) {
        baseline_file = vm["baseline"].as<string>();
    }

    size_t duration_limit = 0;
    if (vm.count("duration-limit")) {
        duration_limit = vm["duration-limit"].as<size_t>();
    }

    double ci_perc = 0.4;
    if (vm.count("ci")) {
        ci_perc = vm["ci"].as<double>();
    }

    // Starting the actual work
    g_wl.reset(pilot_new_workload("Sequential write"), pilot_destroy_workload);
    if (signal(SIGINT, sigint_handler) == SIG_ERR) {
        cerr << "signal(): " << strerror(errno) << endl;
        return 1;
    }
    pilot_set_num_of_pi(g_wl.get(), num_of_pi);
    pilot_set_pi_info(g_wl.get(), 0, "Write throughput", "MB/s", NULL, ur_format_func, !disable_r, !disable_ur);
    pilot_set_wps_analysis(g_wl.get(), wps_format_func, need_wps, need_wps);
    pilot_set_work_amount_limit(g_wl.get(), io_limit);
    pilot_set_init_work_amount(g_wl.get(), init_length);
    pilot_set_workload_func(g_wl.get(), &workload_func);
    pilot_set_required_confidence_interval(g_wl.get(), ci_perc, -1);
    if (0 != baseline_file.size())
        pilot_load_baseline_file(g_wl.get(), baseline_file.c_str());
    // Set up hooks for displaying progress information
    pilot_set_hook_func(g_wl.get(), PRE_WORKLOAD_RUN, &pre_workload_run_hook);
    pilot_set_hook_func(g_wl.get(), POST_WORKLOAD_RUN, &post_workload_run_hook);
    if (0 != duration_limit) {
        pilot_set_session_duration_limit(g_wl.get(), duration_limit);
    }
    if (vm.count("min-round-duration")) {
        pilot_set_short_round_detection_threshold(g_wl.get(), vm["min-round-duration"].as<size_t>());
    }
    if (vm.count("autocorr-threshold")) {
        double at = vm["autocorr-threshold"].as<double>();
        if (at > 1 || at < -1) {
            cerr << "autocorrelation coefficient threshold must be within [-1, 1]" << endl;
            return 2;
        }
        pilot_set_autocorrelation_coefficient(g_wl.get(), at);
    } else {
        pilot_set_autocorrelation_coefficient(g_wl.get(), 1);
    }
    if (vm.count("edm") && vm.count("warm-up-io")) {
        cerr << "percentage warm-up removal cannot be used together with edm, exiting...";
        return 2;
    }
    if (vm.count("warm-up-io")) {
        double wup = vm["warm-up-io"].as<double>();
        pilot_set_warm_up_removal_method(g_wl.get(), FIXED_PERCENTAGE);
        pilot_set_warm_up_removal_percentage(g_wl.get(), wup);
    } else {
        pilot_set_warm_up_removal_method(g_wl.get(), EDM);
    }

    int res;
    if (use_tui)
        pilot_run_workload_tui(g_wl.get());
    else {
        res = pilot_run_workload(g_wl.get());
        if (0 != res && ERR_STOPPED_BY_REQUEST != res) {
            cout << pilot_strerror(res) << endl;
        }
    }

    if (!g_quiet_mode) {
        pilot_ui_printf(g_wl.get(), "Benchmark finished\n");
    }

    //const double* time_readings = pilot_get_pi_unit_readings(g_wl.get(), time_pi, 0, &num_of_work_units) + warmupio;
    //num_of_work_units -= warmupio;

    res = pilot_export(g_wl.get(), result_dir_name.c_str());
    if (res != 0) {
        cout << pilot_strerror(res) << endl;
        return res;
    }
    if (!g_quiet_mode) {
        pilot_ui_printf_hl(g_wl.get(), "Benchmark results are saved to %s\n", result_dir_name.c_str());
    } else {
        shared_ptr<pilot_analytical_result_t> r(pilot_analytical_result(g_wl.get(), NULL), pilot_free_analytical_result);
        // format: URResult,URCI,URVar,URSubsessionSize,WPSa,WPSv,WPSvCI,TestDuration
        cout << r->unit_readings_mean_formatted[0] << ","
             << r->unit_readings_optimal_subsession_ci_width_formatted[0] << ","
             << r->unit_readings_optimal_subsession_var_formatted[0] << ","
             << r->unit_readings_optimal_subsession_size[0] << ",";
        if (r->wps_has_data) {
            cout << r->wps_alpha << ","
                 << r->wps_v_formatted << ","
                 << r->wps_v_ci_formatted << ","
                 << r->wps_optimal_subsession_size << ",";
        } else {
            cout << ",,,,";
        }
        cout << r->session_duration << endl;
    }

    delete[] g_io_buf;
    return 0;
}
