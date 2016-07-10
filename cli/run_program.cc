/*
 * run_program.cc
 *
 * Copyright (c) 2016, University of California, Santa Cruz, CA, USA.
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

#include <boost/algorithm/string.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include <boost/timer/timer.hpp>
#include <common.h>
#include <cstdio>
#include <iostream>
#include <memory>
#include "pilot-cli.h"
#include <signal.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace po = boost::program_options;
using boost::format;
using boost::lexical_cast;
using boost::replace_all;
using boost::timer::cpu_timer;
using boost::timer::nanosecond_type;
using namespace boost::filesystem;
using namespace std;
using namespace pilot;

static size_t         g_duration_col = UINT64_MAX; // column of the round duration
static int            g_num_of_pi = 0;
static vector<int>    g_pi_col;          // column of each PI in client program's output
static string         g_program_cmd;
static string         g_program_name;
static string         g_result_dir;
static string         g_round_results_dir;
static bool           g_quiet = false;
static bool           g_verbose = false;
static shared_ptr<pilot_workload_t> g_wl;

void sigint_handler(int dummy) {
    if (g_wl)
        pilot_stop_workload(g_wl.get());
}

/**
 * \brief Execute cmd and return the stdout of the cmd
 * @param cmd
 * @param stdout the stdout of the client program
 * @return the return code of the client program
 */
int exec(const char* cmd, string &prog_stdout) {
    char buffer[128];
    prog_stdout = "";
    clearerr(stdin);
    FILE* pipe = popen(cmd, "r");
    if (!pipe) throw std::runtime_error("popen() failed!");
    while (!feof(pipe)) {
        if (fgets(buffer, 128, pipe) != NULL)
            prog_stdout += buffer;
    }
    return pclose(pipe);
}

/**
 * \brief the sequential write workload func for libpilot
 * \details This function generates a series of sequential I/O and calculate the throughput.
 * @param[in] wl the workload
 * @param round the current round
 * @param[in] total_work_amount
 * @param[in] lib_malloc_func
 * @param[out] num_of_work_unit
 * @param[out] reading_per_work_unit the memory can be
 * @param[out] readings the final readings of this workload run. Format: readings[piid]. The user needs to allocate memory using lib_malloc_func.
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
    // allocate space for storing result readings
    *readings = (double*)lib_malloc_func(sizeof(double) * g_num_of_pi);
    // this workload has no unit readings
    *num_of_work_unit = 0;
    *unit_readings = NULL;

    // substitute macros
    string my_result_dir = g_round_results_dir + str(format("/%1%") % round);
    create_directories(my_result_dir);
    string my_cmd(g_program_cmd);
    replace_all(my_cmd, "%RESULT_DIR%", my_result_dir);
    replace_all(my_cmd, "%WORK_AMOUNT%", to_string(total_work_amount));

    string prog_stdout;
    debug_log << "Executing client program: " << my_cmd;
    int rc = exec(my_cmd.c_str(), prog_stdout);
    // remove trailing \n
    int i = prog_stdout.size() - 1;
    while (i >= 0 && '\n' == prog_stdout[i])
        --i;
    prog_stdout.resize(i + 1);

    info_log << "Got output from client program: " << prog_stdout;
    if (0 != rc) {
        error_log << "Client program returned: " << rc;
        return rc;
    }

    // parse the result
    try {
        vector<double> rs = extract_csv_fields<double>(prog_stdout, g_pi_col);
        assert(g_pi_col.size() == static_cast<size_t>(g_num_of_pi));
        for (int i = 0; i < g_num_of_pi; ++i) {
            debug_log << str(format("[PI %1%] new reading: %2%") % i % rs[i]);
            (*readings)[i] = rs[i];
        }
    } catch (const boost::bad_lexical_cast &e) {
        fatal_log << "Cannot parse client program's output: " << prog_stdout;
        fatal_log << "Parsing error: " << boost::diagnostic_information(e);
        return ERR_WL_FAIL;
    } catch (const exception &e) {
        fatal_log << e.what();
        return ERR_WL_FAIL;
    }

    return 0;
}

int handle_run_program(int argc, const char** argv) {
    po::options_description desc("Usage: " + string(argv[0]) + " [options] -- program_path [program_options]", 120, 120);
    desc.add_options()
            ("duration-col,d", po::value<size_t>(), "set the column (0-based) of the round duration in seconds. Pilot can use this information for WPS analysis.")
            ("help", "help message for run_command")
            ("min-sample-size,m", po::value<size_t>(), "the required minimum subsession sample size (default to 30, also see Preset Modes below)")
            ("tui", "enable the text user interface")
            ("pi,p", po::value<string>(), "PI(s) to read from stdout of the program, which is expected to be csv\n"
                    "Format:     \tname,unit,column,type,ci_percent:...\n"
                    "name:       \tname of the PI, can be empty\n"
                    "unit:       \tunit of the PI, can be empty (the name and unit are used only for display purpose)\n"
                    "column:     \tthe column of the PI in the csv output of the client program (0-based)\n"
                    "type:       \t0 - ordinary value (like time, bytes, etc.), 1 - ratio (like throughput, speed). "
                    "Setting the correct type ensures Pilot uses the correct mean calculation method.\n"
                    "ci_percent: \tthe desired width of CI as the percent of mean. You can leave it empty, and Pilot "
                    "will still collect and do analysis, but won't take this PI as a stop requirement (default to 0.05).\n"
                    "more than one PI's information can be separated by colon (:)")
            ("preset", po::value<string>(), "preset modes control the statistical requirements for the results to be satisfactory\n"
                    "quick:      \t(default) autocorrelation limit: 0.8,\n"
                    "            \tconfidence interval: 20% of mean,\n"
                    "            \tmin. subsession sample size: 30,\n"
                    "            \tworkload round duration threshold: 3 seconds\n"
                    "normal:     \tautocorrelation limit: 0.2,\n"
                    "            \tconfidence interval: 10% of mean,\n"
                    "            \tmin. subsession sample size: 5,\n"
                    "            \tworkload round duration threshold: 10 seconds\n"
                    "strict:     \tautocorrelation limit: 0.1,\n"
                    "            \tconfidence interval: 10% of mean,\n"
                    "            \tmin. subsession sample size: 200,\n"
                    "            \tworkload round duration threshold: 20 seconds")
            ("quiet,q", "quiet mode")
            ("result-dir,r", po::value<string>(), "set result directory name")
            ("verbose,v", "print debug information")
            ("work-amount,w", po::value<string>(), "set the valid range of work amount [min,max]")
            ("wps", "WPS must satisfy")
            ;
    // copy options into args and find program_path_start_loc
    vector<string> args;
    int program_path_start_loc = 0;
    for (int i = 2; i != argc; ++i) {
        if (strcmp(argv[i], "--") == 0) {
            program_path_start_loc = i;
            break;
        }
        args.push_back(argv[i]);
    }

    // parse args
    po::variables_map vm;
    try {
        po::parsed_options parsed =
            po::command_line_parser(args).options(desc).run();
        po::store(parsed, vm);
        po::notify(vm);
    } catch (po::error &e) {
        cerr << e.what() << endl;
        return 1;
    }

    if (vm.count("help")) {
        cerr << desc << endl;
        return 2;
    }
    if (vm.count("verbose")) {
        g_verbose = true;
        pilot_set_log_level(lv_trace);
    } else {
        g_verbose = false;
        pilot_set_log_level(lv_info);
    }
    if (vm.count("quiet")) {
        if (g_verbose) {
            fatal_log << "cannot active both quiet and verbose mode";
            return 2;
        }
        g_quiet = true;
        pilot_set_log_level(lv_warning);
    }

    bool use_tui = false;
    if (vm.count("tui"))
        use_tui = true;

    if (vm.count("result-dir")) {
        g_result_dir = vm["result-dir"].as<string>();
    } else {
        g_result_dir = "pilot_result_" + get_timestamp();
    }
    info_log << "Saving results to directory " << g_result_dir;
    g_round_results_dir = g_result_dir + "/round_results";
    create_directories(g_round_results_dir);

    // parse program_cmd
    if (0 == program_path_start_loc || program_path_start_loc == argc - 1) {
        fatal_log << "Error: program_path is required" << endl;
        cerr << desc << endl;
        return 2;
    }
    ++program_path_start_loc;
    g_program_cmd = g_program_name = string(argv[program_path_start_loc]);
    while (++program_path_start_loc < argc) {
        g_program_cmd += " ";
        g_program_cmd += argv[program_path_start_loc];
    }
    info_log << GREETING_MSG;
    debug_log << "Program path and args: " << g_program_cmd;

    // create the workload
    g_wl.reset(pilot_new_workload(g_program_name.c_str()), pilot_destroy_workload);
    if (signal(SIGINT, sigint_handler) == SIG_ERR) {
        fatal_log << "signal(): " << strerror(errno) << endl;
        return 1;
    }
    if (!g_wl) {
        fatal_log << "Error: cannot create workload";
        return 3;
    }

    if (vm.count("duration-col")) {
        g_duration_col = vm["duration-col"].as<size_t>();
        info_log << "Setting duration column to " << g_duration_col;
    }

    // parse work amount range
    if (vm.count("work-amount")) {
        vector<int> wa_cols{0, 1};
        vector<size_t> wa = extract_csv_fields<size_t>(vm["work-amount"].as<string>(), wa_cols);
        pilot_set_init_work_amount(g_wl.get(), wa[0]);
        pilot_set_work_amount_limit(g_wl.get(), wa[1]);
        info_log << str(format("Setting work amount range to [%1%, %2%]") % wa[0] % wa[1]);
    } else {
        // this workload doesn't need work amount
        pilot_set_work_amount_limit(g_wl.get(), 0);
    }

    // parse and set PI info
    g_num_of_pi = 0;
    try {
        if (vm.count("pi")) {
            vector<string> pi_info_strs;
            boost::split(pi_info_strs, vm["pi"].as<string>(), boost::is_any_of(":"));
            g_num_of_pi = pi_info_strs.size();
            if (0 == g_num_of_pi) {
                throw runtime_error("Error parsing PI information: empty string provided");
            }
            debug_log << "Total number of PIs: " << g_num_of_pi;
            pilot_set_num_of_pi(g_wl.get(), g_num_of_pi);

            double pi_ci_percent = 0;
            for (auto &pistr : pi_info_strs) {
                vector<string> pidata;
                boost::split(pidata, pistr, boost::is_any_of(","));
                if (pidata.size() < 4) {
                    throw runtime_error(string("PI info str \"") + pistr + "\" doesn't have enough (4) fields");
                }
                string pi_name = pidata[0];
                string pi_unit = pidata[1];
                g_pi_col.push_back(lexical_cast<int>(pidata[2]));
                int tmpi = lexical_cast<int>(pidata[3]);
                if (tmpi > 1 || tmpi < 0) {
                    throw runtime_error("Error: invalid value for PI type");
                }
                pilot_mean_method_t pi_mean_method = static_cast<pilot_mean_method_t>(tmpi);

                bool reading_must_satisfy = false;
                if (pidata.size() > 4) {
                    reading_must_satisfy = true;
                    pi_ci_percent = lexical_cast<double>(pidata[4]);
                }
                debug_log << "PI[" << g_pi_col.size() - 1 << "] name: " << pi_name << ", "
                          << "unit: " << pi_unit << ", "
                          << "reading must satisfy: " << (reading_must_satisfy? "yes" : "no") << ", "
                          << "mean method: " << (pi_mean_method == 0? "arithmetic": "harmonic");
                pilot_set_pi_info(g_wl.get(), g_pi_col.size() - 1,  /* PIID */
                                  pi_name.c_str(),                /* PI name */
                                  pi_unit.c_str(),                /* PI unit */
                                  NULL, NULL,                     /* format functions */
                                  reading_must_satisfy,
                                  false,                          /* unit readings no need to satisfy */
                                  pi_mean_method);
            }
            if (0 == pi_ci_percent) {
                throw runtime_error("Please provide at least one reading's CI requirement");
            } else {
                pilot_set_required_confidence_interval(g_wl.get(), pi_ci_percent, -1);
            }
        } else {
            if (UINT64_MAX != g_duration_col) {
                info_log << "No PI information, will do WPS analysis only";
            } else {
                throw runtime_error("Error: no PI or duration column set, exiting...");
            }
        }
    } catch (const runtime_error &e) {
        cerr << e.what() << endl;
        return 2;
    }

    if (vm.count("wps")) {
        if (UINT64_MAX == g_duration_col) {
            cerr << "Duration column must be set for WPS analysis";
            return 2;
        }
        if (!vm.count("work-amount")) {
            cerr << "Work amount must be set for WPS analysis";
            return 2;
        }
        pilot_set_wps_analysis(g_wl.get(), NULL, true, true);
        info_log << "WPS analysis enabled";
    } else if (UINT64_MAX != g_duration_col && vm.count("work-amount")) {
        pilot_set_wps_analysis(g_wl.get(), NULL, true, false);
    } else {
        pilot_set_wps_analysis(g_wl.get(), NULL, false, false);
    }
    pilot_set_workload_func(g_wl.get(), workload_func);

    string preset_mode = "quick";
    if (vm.count("preset")) {
        preset_mode = vm["preset"].as<string>();
    }
    {
        double ac;  // autocorrelation coefficient threshold
        double ci;  // CI
        size_t ms;  // min. subsession sample size
        size_t sr;  // short round threshold
        string msg = "Preset mode activated: ";
        if ("quick" == preset_mode) {
            msg += "quick";
            ac = 0.8; ci = 0.2; ms = 30; sr = 3;
        } else if ("normal" == preset_mode) {
            msg += "normal";
            ac = 0.2; ci = 0.1; ms = 50; sr = 10;
        } else if ("strict" == preset_mode) {
            msg += "strict";
            ac = 0.1; ci = 0.1; ms = 200; sr = 20;
        } else {
            cerr << str(format("Unknown preset mode \"%1%\", exiting...") % preset_mode);
            return 2;
        }
        pilot_set_autocorrelation_coefficient(g_wl.get(), ac);
        info_log << "Setting the limit of autocorrelation coefficient to " << ac;
        pilot_set_required_confidence_interval(g_wl.get(), ci, -1);
        info_log << str(format("Setting the required width of confidence interval to %1%%% of mean") % (ac * 100));

        if (vm.count("min-sample-size")) {
            ms = vm["min-sample-size"].as<size_t>();
            info_log << "Overriding preset's required minimum subsession sample size with " << ms;
        } else {
            info_log << "Setting the required minimum subsession sample size to " << ms;
        }
        pilot_set_min_sample_size(g_wl.get(), ms);

        pilot_set_short_round_detection_threshold(g_wl.get(), sr);
        info_log << "Setting the short round threshold to " << sr;
    }
    int wl_res;
    if (use_tui)
        pilot_run_workload_tui(g_wl.get());
    else {
        wl_res = pilot_run_workload(g_wl.get());
        if (0 != wl_res && ERR_STOPPED_BY_REQUEST != wl_res) {
            cerr << pilot_strerror(wl_res) << endl;
        }

        if (!g_quiet) {
            shared_ptr<char> psummary(pilot_text_workload_summary(g_wl.get()), pilot_free_text_dump);
            cout << psummary;
        }
    }
    shared_ptr<pilot_analytical_result_t> r(pilot_analytical_result(g_wl.get(), NULL), pilot_free_analytical_result);

    // print summary header
    cout << "piid,readings_mean_formatted,readings_optimal_subsession_ci_width_formatted,readings_optimal_subsession_variance_formatted,"
            "readings_dominant_segment_begin,readings_raw_mean_formatted,readings_raw_optimal_subsession_ci_width_formatted,"
            "readings_raw_optimal_subsession_variance_formatted,session_duration" << endl;
    // print summary data
    for (size_t piid = 0; piid < r->num_of_pi; ++piid) {
        // format: piid,mean,ci,var,ds_begin,raw_mean,raw_ci,raw_var,...
        cout << format("%1%,") % piid;
        if (0 != r->readings_num) {
            cout << format("%1%,%2%,%3%,%4%,%5%,%6%,%7%")
                            % r->readings_mean_formatted[piid]
                            % r->readings_optimal_subsession_ci_width_formatted[piid]
                            % r->readings_optimal_subsession_var_formatted[piid]
                            % r->readings_dominant_segment_begin[piid]
                            % r->readings_raw_mean_formatted[piid]
                            % r->readings_raw_optimal_subsession_ci_width_formatted[piid]
                            % r->readings_raw_optimal_subsession_var_formatted[piid];
        } else {
            cout << ",,,,,,";
        }
        if (0 == piid) {
            cout << "," << r->session_duration;
        }
        cout << endl;
    }

    int res = pilot_export(g_wl.get(), g_result_dir.c_str());
    if (res != 0) {
        cout << pilot_strerror(res) << endl;
        return res;
    }
    info_log << "Results saved in " << g_result_dir;

    return wl_res;
}
