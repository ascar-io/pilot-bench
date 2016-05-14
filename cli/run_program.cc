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
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include <boost/timer/timer.hpp>
#include <common.h>
#include <cstdio>
#include <iostream>
#include <memory>
#include "pilot-cli.h"
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;
using namespace pilot;
namespace po = boost::program_options;
using boost::lexical_cast;
using boost::timer::cpu_timer;
using boost::timer::nanosecond_type;

int            g_num_of_pi = 0;
vector<int>    g_pi_col;          // column of each PI in client program's output
string         g_program_cmd;
string         g_program_name;
bool           g_verbose = false;

/**
 * \brief Execute cmd and return the stdout of the cmd
 * @param cmd
 * @return
 */
std::string exec(const char* cmd) {
    char buffer[128];
    string result = "";
    shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
    if (!pipe) throw std::runtime_error("popen() failed!");
    while (!feof(pipe.get())) {
        if (fgets(buffer, 128, pipe.get()) != NULL)
            result += buffer;
    }
    return result;
}

/**
 * \brief the sequential write workload func for libpilot
 * \details This function generates a series of sequential I/O and calculate the throughput.
 * @param[in] total_work_amount
 * @param[in] lib_malloc_func
 * @param[out] num_of_work_unit
 * @param[out] reading_per_work_unit the memory can be
 * @param[out] readings the final readings of this workload run. Format: readings[piid]. The user needs to allocate memory using lib_malloc_func.
 * @return
 */
int workload_func(size_t total_work_amount,
                  pilot_malloc_func_t *lib_malloc_func,
                  size_t *num_of_work_unit,
                  double ***unit_readings,
                  double **readings) {
    // allocate space for storing result readings
    *readings = (double*)lib_malloc_func(sizeof(double) * g_num_of_pi);
    // this workload has no unit readings
    *num_of_work_unit = 0;
    *unit_readings = NULL;

    const string prog_output = exec(g_program_cmd.c_str());
    info_log << "Got output from client program: " << prog_output;

    // parse the result
    return 0;
}

int handle_run_program(int argc, const char** argv) {
    po::options_description desc("Usage: " + string(argv[0]) + " [options] -- program_path [program_options]");
    desc.add_options()
            ("help", "help message for run_command")
            ("no-tui", "disable the text user interface")
            ("pi,p", po::value<string>(), "PI(s) to read from stdout of the program, which is expected to be csv\n"
                    "Format:     name,column,type,ci_percent;...\n"
                    "name:       name of the PI, can be empty\n"
                    "unit:       unit of the PI, can be empty (the name and unit are used only for display purpose)\n"
                    "column:     the column of the PI in the csv output of the client program\n"
                    "type:       0 - ordinary value (like time, bytes, etc.), 1 - ratio (like throughput, speed)\n"
                    "            setting the correct type ensures Pilot uses the correct mean calculation method\n"
                    "ci_percent: the desired width of CI as the percent of mean. You can leave it empty, and Pilot\n"
                    "            will still collect and do analysis, but won't take this PI as a stop requirement.\n"
                    "            (default to 0.05)\n"
                    "more than one PI's info. can be separated by ;")
            ("result_dir,r", po::value<string>(), "set result directory name")
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
        pilot_set_log_level(lv_debug);
    } else {
        g_verbose = false;
        pilot_set_log_level(lv_info);
    }
    bool use_tui = true;
    if (vm.count("no-tui"))
        use_tui = false;

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
    cerr << GREETING_MSG << endl;
    debug_log << "Program path and args: " << g_program_cmd;

    // create the workload
    shared_ptr<pilot_workload_t> wl(pilot_new_workload(g_program_name.c_str()), pilot_destroy_workload);
    if (!wl) {
        fatal_log << "Error: cannot create workload";
        return 3;
    }
    // this workload doesn't need work amount
    pilot_set_work_amount_limit(wl.get(), 0);

    // parse and set PI info
    g_num_of_pi = 0;
    try {
        if (vm.count("pi")) {
            vector<string> pi_info_strs;
            boost::split(pi_info_strs, vm["pi"].as<string>(), boost::is_any_of(";"));
            g_num_of_pi = pi_info_strs.size();
            if (0 == g_num_of_pi) {
                throw runtime_error("Error parsing PI information: empty string provided");
            }
            debug_log << "Total number of PIs: " << g_num_of_pi;
            pilot_set_num_of_pi(wl.get(), g_num_of_pi);

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
                double pi_ci_percent = 0.05;
                if (pidata.size() > 4) {
                    reading_must_satisfy = true;
                    pi_ci_percent = lexical_cast<double>(pidata[4]);
                }
                pilot_set_required_confidence_interval(wl.get(), pi_ci_percent, -1);
                debug_log << "PI[" << g_pi_col.size() - 1 << "] name: " << pi_name << ", "
                          << "unit: " << pi_unit << ", "
                          << "reading must satisfy: " << (reading_must_satisfy? "yes" : "no") << ", "
                          << "mean method: " << (pi_mean_method == 0? "arithmetic": "harmonic");
                pilot_set_pi_info(wl.get(), g_pi_col.size() - 1,  /* PIID */
                                  pi_name.c_str(),                /* PI name */
                                  pi_unit.c_str(),                /* PI unit */
                                  NULL, NULL,                     /* format functions */
                                  reading_must_satisfy,
                                  false,                          /* unit readings no need to satisfy */
                                  pi_mean_method);
            }
        } else {
            throw runtime_error("Error: PI information missing");
        }
    } catch (const runtime_error &e) {
        cerr << e.what() << endl;
        return 2;
    }

    pilot_set_wps_analysis(wl.get(), false);
    pilot_set_workload_func(wl.get(), workload_func);
    pilot_set_autocorrelation_coefficient(wl.get(), 0.1);
    info_log << "Setting the limit of autocorrelation coefficient to 0.1";
    int res;
    if (use_tui)
        pilot_run_workload_tui(wl.get());
    else {
        res = pilot_run_workload(wl.get());
        if (0 != res) {
            cout << pilot_strerror(res) << endl;
        }
    }

    return 0;
}
