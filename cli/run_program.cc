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
#include <iostream>
#include <memory>
#include "pilot-cli.h"
#include <signal.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/types.h>   // for kill()
#include <signal.h>      // for kill()
#include <unistd.h>
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

static const char**   g_client_cmd;
static size_t         g_client_cmd_len;
static string         g_client_name;
static int            g_client_pid = 0;
static FILE*          g_client_out_fs = NULL;
static size_t         g_duration_col = (size_t)-1; // column of the round duration
static int            g_num_of_pi = 0;
static vector<int>    g_pi_col;          // column of each PI in client program's output
static string         g_output_dir;
static string         g_round_results_dir;
static bool           g_quiet = false;
static bool           g_verbose = false;
static shared_ptr<pilot_workload_t> g_wl;
static string         g_prog_stdout;

void sigint_handler(int dummy) {
    if (g_wl)
        pilot_stop_workload(g_wl.get());
}

/**
 * \brief Our own version of popen that returns the PID of the child process
 * @param command[in] the command to run
 * @param infp[out] the new fd for stdin
 * @param outfp[out] the new fd for stdout of the child process
 * @return
 */
static pid_t popen2(char * const*command, FILE **infp, FILE **outfp)
{
    const int READ = 0, WRITE = 1;
    int p_stdin[2], p_stdout[2];
    pid_t pid;

    if (pipe(p_stdin) != 0 || pipe(p_stdout) != 0)
        throw runtime_error("Failed to create pipes");

    pid = fork();

    if (pid < 0)
        throw runtime_error("Fork failed");
    else if (pid == 0)    /* Child */
    {
        dup2(p_stdin[READ], READ);
        close(p_stdin[WRITE]);
        close(p_stdin[READ]);
        dup2(p_stdout[WRITE], WRITE);
        close(p_stdout[READ]);
        close(p_stdout[WRITE]);
        execvp(*command, command);
        perror("execvp");
        exit(1);
    }
    close(p_stdin[READ]);
    close(p_stdout[WRITE]);

    if (infp == NULL)
        close(p_stdin[WRITE]);
    else
        *infp = fdopen(p_stdin[WRITE], "w");

    if (outfp == NULL)
        close(p_stdout[READ]);
    else
        *outfp = fdopen(p_stdout[READ], "r");

    return pid;
}

static int pclose2(pid_t pid) {
    int internal_stat;
    waitpid(pid, &internal_stat, 0);
    return WEXITSTATUS(internal_stat);
}

/**
 * \brief Execute cmd and return one line of the stdout of the cmd
 *
 * The client program can output many lines and they will all be read in. Each
 * line should contain exactly one sample.
 * @param cmd
 * @param stdout the stdout of the client program
 * @return one line of the stdout from running the cmd
 */
string exec(char* const* cmd) {
    char buffer[128];
    clearerr(stdin);
    string result;
    for (int loop_time = 0; loop_time < 3; ++loop_time) {
        if (0 == g_client_pid) {
            g_client_pid = popen2(cmd, NULL, &g_client_out_fs);
        }
        for(;;) {
            size_t newline_pos = g_prog_stdout.find('\n');
            if (string::npos != newline_pos) {
                result = g_prog_stdout.substr(0, newline_pos);
                g_prog_stdout = g_prog_stdout.substr(newline_pos+1, g_prog_stdout.size()-newline_pos-1);
                if (result == "") {
                    // Skip empty lines
                    continue;
                }
                return result;
            }

            if (feof(g_client_out_fs))
                break;

            if (fgets(buffer, 128, g_client_out_fs) != NULL) {
                g_prog_stdout += buffer;
            } else {
                // eof reached
                break;
            }
        }
        // We reach here when eof is detected
        fclose(g_client_out_fs);
        g_client_out_fs = NULL;
        int rc = pclose2(g_client_pid);
        // pclose() returns -1 when the client is already exited
        if (0 != rc && -1 != rc) {
            throw runtime_error(str(format("Client program returned %1%") % rc).c_str());
        }
        g_client_pid = 0;
        // Return whatever we have no matter if it ends with a \n
        if (g_prog_stdout.size() != 0) {
            result = g_prog_stdout;
            g_prog_stdout = "";
            return result;
        }
        // If we still have nothing so far, run the benchmark command again
    }
    // We've already tried twice and didn't get anything, give up to prevent dead loop.
    throw runtime_error("Client program does not generate output");
}

static void _free_argv_vector(vector<char*> &v) {
    for (char* p : v)
        free(p);
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
    vector<char*> my_cmd(g_client_cmd_len);
    for (size_t i = 0; i < g_client_cmd_len; ++i) {
        string tmp(g_client_cmd[i]);
        replace_all(tmp, "%RESULT_DIR%", my_result_dir);
        replace_all(tmp, "%WORK_AMOUNT%", to_string(total_work_amount));
        my_cmd[i] = strdup(tmp.c_str());
    }

    stringstream ss;
    ss << "Executing client program:";
    for (const char* p : my_cmd) {
        ss << " ";
        ss << *p;
    }
    debug_log << ss.str();

    string prog_stdout;
    try {
        prog_stdout = exec((char* const*)my_cmd.data());
    } catch (const runtime_error& e) {
        error_log << e.what();
        _free_argv_vector(my_cmd);
        return 1;
    }
    _free_argv_vector(my_cmd);
    // remove trailing \n
    int i = prog_stdout.size() - 1;
    while (i >= 0 && '\n' == prog_stdout[i])
        --i;
    prog_stdout.resize(i + 1);

    info_log << "Got output from client program: " << prog_stdout;

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
            ("duration-col,d", po::value<size_t>(), "Set the column (0-based) of the round duration in seconds for WPS analysis.")
            ("help", "Print help message for run_command.")
            ("ac,a", po::value<double>(), "Set the required range of autocorrelation coefficient. arg should be a value within (0, 1], and the range will be set to [-arg,arg]")
            ("ci,c", po::value<double>(), "The required width of confidence interval (absolute value). Setting it to -1 to disable CI (absolute value) check.")
            ("ci-perc", po::value<double>(), "The required width of confidence interval (as the percentage of mean). Setting it to -1 disables CI (percent of mean) check. If both ci and ci-perc are set, the narrower one will be used. See preset below for the default value.")
            ("min-sample-size,m", po::value<size_t>(), "The required minimum subsession sample size (default to 30, also see Preset Modes below)")
            ("tui", "Enable the text user interface")
            ("output-dir,o", po::value<string>(), "Set output directory name to arg")
            ("pi,p", po::value<string>(), "PI(s) to read from stdout of the program, which is expected to be csv\n"
                    "Format:     \tname,unit,column,type,must_satisfy:...\n"
                    "name:       \tname of the PI, can be empty\n"
                    "unit:       \tunit of the PI, can be empty (the name and unit are used only for display purpose)\n"
                    "column:     \tthe column of the PI in the csv output of the client program (0-based)\n"
                    "type:       \t0 - ordinary value (like time, bytes, etc.), 1 - ratio (like throughput, speed). "
                    "Setting the correct type ensures Pilot uses the correct mean calculation method.\n"
                    "must_satisfy: \t1 - if this PI's CI must satisfy the requirement of CI width; 0 (or missing) - record data only, no need to satisfy\n"
                    "more than one PI's information can be separated by colon (:)")
            ("preset", po::value<string>(), "preset modes control the statistical requirements for the results to be satisfactory\n"
                    "quick:      \t(default) autocorrelation limit: 0.8,\n"
                    "            \tconfidence interval: 20% of mean,\n"
                    "            \tmin. subsession sample size: 30,\n"
                    "            \tworkload round duration threshold: 3 seconds (only used when work amount is set)\n"
                    "normal:     \tautocorrelation limit: 0.2,\n"
                    "            \tconfidence interval: 10% of mean,\n"
                    "            \tmin. subsession sample size: 50,\n"
                    "            \tworkload round duration threshold: 10 seconds (only used when work amount is set)\n"
                    "strict:     \tautocorrelation limit: 0.1,\n"
                    "            \tconfidence interval: 10% of mean,\n"
                    "            \tmin. subsession sample size: 200,\n"
                    "            \tworkload round duration threshold: 20 seconds (only used when work amount is set)")
            ("quiet,q", "Enable quiet mode")
            ("session-limit,s", po::value<int>(), "Set the session duration limit in seconds. Pilot will stop with error code 13 if the session runs longer (default: unlimited).")
            ("verbose,v", "Print debug information")
            ("work-amount,w", po::value<string>(), "Set the valid range of work amount [min,max]")
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
        print_read_the_doc_info();
        cerr << endl;
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

    if (vm.count("output-dir")) {
        g_output_dir = vm["output-dir"].as<string>();
    } else {
        g_output_dir = "pilot_result_" + get_timestamp();
    }
    info_log << "Saving results to directory " << g_output_dir;
    g_round_results_dir = g_output_dir + "/round_results";
    create_directories(g_round_results_dir);

    // parse program_cmd
    if (0 == program_path_start_loc || program_path_start_loc == argc - 1) {
        fatal_log << "Error: program_path is required" << endl;
        cerr << desc << endl;
        return 2;
    }
    ++program_path_start_loc;   // move pass "--"
    g_client_cmd = &(argv[program_path_start_loc]);
    g_client_cmd_len = argc - program_path_start_loc;
    string client_cmd_str = g_client_name = string(argv[program_path_start_loc]);
    while (++program_path_start_loc < argc) {
        client_cmd_str += " ";
        client_cmd_str += argv[program_path_start_loc];
    }
    info_log << GREETING_MSG;
    debug_log << "Program path and args: " << client_cmd_str;

    // create the workload
    g_wl.reset(pilot_new_workload(g_client_name.c_str()), pilot_destroy_workload);
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

    // parse session limit
    if (vm.count("session-limit")) {
        int session_limit = vm["session-limit"].as<int>();
        if (session_limit <= 0) {
            fatal_log << "Session limit must be greater than 0, exiting...";
            return 2;
        }
        info_log << format("Setting session limit to %1% seconds") % session_limit;
        pilot_set_session_duration_limit(g_wl.get(), static_cast<size_t>(session_limit));
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

            int num_of_PIs_must_satisfy = 0;
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
                    reading_must_satisfy = lexical_cast<bool>(pidata[4]);
                    if (reading_must_satisfy)
                        ++num_of_PIs_must_satisfy;
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
            if (0 == num_of_PIs_must_satisfy) {
                throw runtime_error("Error: at least one PI needs to have must_satisfy set.");
            }
        } else {
            if ((size_t)-1 != g_duration_col) {
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
        if ((size_t)-1 == g_duration_col) {
            cerr << "Duration column must be set for WPS analysis";
            return 2;
        }
        if (!vm.count("work-amount")) {
            cerr << "Work amount must be set for WPS analysis";
            return 2;
        }
        pilot_set_wps_analysis(g_wl.get(), NULL, true, true);
        info_log << "WPS analysis enabled";
    } else if ((size_t)-1 != g_duration_col && vm.count("work-amount")) {
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
        double ac;       // autocorrelation coefficient threshold
        double ci = -1;  // CI as absolute value
        double ci_perc;  // CI as percent of mean
        size_t ms;       // min. subsession sample size
        size_t sr;       // short round threshold
        string msg = "Preset mode activated: ";
        if ("quick" == preset_mode) {
            msg += "quick";
            ac = 0.8; ci_perc = 0.2; ms = 30; sr = 3;
        } else if ("normal" == preset_mode) {
            msg += "normal";
            ac = 0.2; ci_perc = 0.1; ms = 50; sr = 10;
        } else if ("strict" == preset_mode) {
            msg += "strict";
            ac = 0.1; ci_perc = 0.1; ms = 200; sr = 20;
        } else {
            cerr << str(format("Unknown preset mode \"%1%\", exiting...") % preset_mode);
            return 2;
        }
        info_log << msg;

        // read individual values that may override our preset
        if (vm.count("ci-perc")) {
            ci_perc = vm["ci-perc"].as<double>();
        }
        if (vm.count("ci")) {
            ci = vm["ci"].as<double>();
        }
        if (ci < 0 && ci_perc < 0) {
            fatal_log << "Error: CI (percent of mean) and CI (absolute value) cannot be both disabled. At least one must be set.";
            return 2;
        }
        pilot_set_required_confidence_interval(g_wl.get(), ci_perc, ci);
        if (ci_perc > 0) {
            info_log << str(format("Setting the required width of confidence interval to %1%%% of mean") % (ci_perc * 100));
        }
        if (ci > 0) {
            info_log << str(format("Setting the required width of confidence interval to %1%") % ci);
        }

        if (vm.count("ac")) {
            ac = vm["ac"].as<double>();
            if (ac <= 0 || ac > 1) {
                fatal_log << "Valid range for the autocorrelation coefficient arg is (0,1], exiting...";
                return 2;
            }
        }
        pilot_set_autocorrelation_coefficient(g_wl.get(), ac);
        info_log << "Setting the limit of autocorrelation coefficient to " << ac;

        if (vm.count("min-sample-size")) {
            ms = vm["min-sample-size"].as<size_t>();
            info_log << "Overriding preset's required minimum subsession sample size with " << ms;
        } else {
            info_log << "Setting the required minimum subsession sample size to " << ms;
        }
        pilot_set_min_sample_size(g_wl.get(), ms);

        if (vm.count("work-amount")) {
            pilot_set_short_round_detection_threshold(g_wl.get(), sr);
            info_log << "Setting the short round threshold to " << sr << " second(s)";
        } else {
            pilot_set_short_round_detection_threshold(g_wl.get(), 0);
            info_log << "Disabled short round detection because work amount information is not set.";
        }

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
        } else {
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
                                    % r->readings_last_changepoint[piid]
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
        } /* if in quiet output mode */
    }

    int res = pilot_export(g_wl.get(), g_output_dir.c_str());
    if (res != 0) {
        cout << pilot_strerror(res) << endl;
        return res;
    }
    info_log << "Results saved in " << g_output_dir;

    if (g_client_pid != 0) {
        kill(g_client_pid, SIGTERM);
    }

    return wl_res;
}
