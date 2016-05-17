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

vector<double> extract_csv_fields(const string &csvstr, const vector<int> &columns) {
    vector<string> pidata_strs;
    boost::split(pidata_strs, csvstr, boost::is_any_of(" \n\t,"));
    vector<double> r(pidata_strs.size());
    for (int i = 0; i < (int)columns.size(); ++i) {
        int col = columns[i];
        if (col >= static_cast<int>(pidata_strs.size())) {
            throw runtime_error(str(format("Error parsing client program's output: %1%; expected column: %2%") % csvstr % col));
        }
        r[i] = lexical_cast<double>(pidata_strs[col]);
    }
    return r;
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
                  double **readings) {
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
        vector<double> rs = extract_csv_fields(prog_stdout, g_pi_col);
        assert(g_pi_col.size() == static_cast<size_t>(g_num_of_pi));
        for (int i = 0; i < g_num_of_pi; ++i) {
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
    po::options_description desc("Usage: " + string(argv[0]) + " [options] -- program_path [program_options]");
    desc.add_options()
            ("help", "help message for run_command")
            ("min-sample-size,m", po::value<size_t>(), "lower threshold for sample size (default to 100)")
            ("no-tui", "disable the text user interface")
            ("pi,p", po::value<string>(), "PI(s) to read from stdout of the program, which is expected to be csv\n"
                    "Format:     name,column,type,ci_percent:...\n"
                    "name:       name of the PI, can be empty\n"
                    "unit:       unit of the PI, can be empty (the name and unit are used only for display purpose)\n"
                    "column:     the column of the PI in the csv output of the client program (0-based)\n"
                    "type:       0 - ordinary value (like time, bytes, etc.), 1 - ratio (like throughput, speed)\n"
                    "            setting the correct type ensures Pilot uses the correct mean calculation method\n"
                    "ci_percent: the desired width of CI as the percent of mean. You can leave it empty, and Pilot\n"
                    "            will still collect and do analysis, but won't take this PI as a stop requirement.\n"
                    "            (default to 0.05)\n"
                    "more than one PI's info. can be separated by :")
            ("quiet,q", "quiet mode")
            ("result-dir,r", po::value<string>(), "set result directory name")
            ("verbose,v", "print debug information")
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

    bool use_tui = true;
    if (vm.count("no-tui"))
        use_tui = false;

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
    // this workload doesn't need work amount
    pilot_set_work_amount_limit(g_wl.get(), 0);

    size_t min_sample_size = 100;
    if (vm.count("min-sample-size"))
        min_sample_size = vm["min-sample-size"].as<size_t>();
    info_log << "Setting min-sample-size to " << min_sample_size;
    pilot_set_min_sample_size(g_wl.get(), min_sample_size);

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
                g_pi_col.push_back(lexical_cast<double>(pidata[2]));
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
            throw runtime_error("Error: PI information missing");
        }
    } catch (const runtime_error &e) {
        cerr << e.what() << endl;
        return 2;
    }

    pilot_set_wps_analysis(g_wl.get(), NULL, false, false);
    pilot_set_workload_func(g_wl.get(), workload_func);
    pilot_set_autocorrelation_coefficient(g_wl.get(), 0.1);
    info_log << "Setting the limit of autocorrelation coefficient to 0.1";
    int wl_res;
    if (use_tui)
        pilot_run_workload_tui(g_wl.get());
    else {
        wl_res = pilot_run_workload(g_wl.get());
        if (0 != wl_res && ERR_STOPPED_BY_REQUEST != wl_res) {
            cerr << pilot_strerror(wl_res) << endl;
        }
        if (g_quiet) {
            shared_ptr<pilot_analytical_result_t> r(pilot_analytical_result(g_wl.get(), NULL), pilot_free_analytical_result);
            for (size_t piid = 0; piid < r->num_of_pi; ++piid) {
                // format: piid,mean,ci,var,ds_begin,raw_mean,raw_ci,raw_var,...
                cout << format("%1%,") % piid;
                if (0 != r->readings_num) {
                    cout << format("%1%,%2%,%3%,%4%,%5%,%6%,%7%,")
                            % r->readings_mean_formatted[piid]
                            % r->readings_optimal_subsession_ci_width_formatted[piid]
                            % r->readings_optimal_subsession_var[piid]
                            % r->readings_dominant_segment_begin[piid]
                            % r->readings_raw_mean_formatted[piid]
                            % r->readings_raw_optimal_subsession_ci_width_formatted[piid]
                            % r->readings_raw_optimal_subsession_var_formatted[piid];
                } else {
                    cout << ",,,,,,,";
                }
            }
            cout << r->session_duration << endl;
        } else {
            shared_ptr<char> psummary(pilot_text_workload_summary(g_wl.get()), pilot_free_text_dump);
            cout << psummary;
        }
    }
    int res = pilot_export(g_wl.get(), g_result_dir.c_str());
    if (res != 0) {
        cout << pilot_strerror(res) << endl;
        return res;
    }
    info_log << "Results saved in " << g_result_dir;

    return wl_res;
}
