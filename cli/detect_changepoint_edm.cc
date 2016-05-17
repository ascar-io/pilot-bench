/*
 * Pilot CLI: the detect_changepoint_edm command
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
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include <common.h>
#include <cstdio>
#include <fstream>
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
using namespace std;
using namespace pilot;

static bool           g_quiet = false;
static bool           g_verbose = false;

int handle_detect_changepoint_edm(int argc, const char** argv) {
    po::options_description desc("Usage: " + string(argv[0]) + " [options]");
    desc.add_options()
            ("help", "help message for run_command")
            ("csv-file,c", po::value<string>(), "input csv file name, use - for stdin")
            ("field,f", po::value<int>(), "the field of the csv to import")
            ("ignore-lines,i", po::value<size_t>(), "ignore the first arg lines")
            ("percent,p", po::value<double>(), "A real numbered constant used to control the amount of penalization. This value specifies the minimum percent change in the goodness of fit statistic to consider adding an additional change point. A value of 0.25 corresponds to a 25\% increase. Default to 0.25.")
            ("quiet,q", "quiet mode")
            ("verbose,v", "print debug information")
            ;
    // copy options into args
    vector<string> args;
    for (int i = 2; i != argc; ++i) {
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

    double percent = 0.25;
    if (vm.count("percent")) {
        percent = vm["percent"].as<double>();
    }
    string input_csv;
    if (vm.count("csv-file")) {
        input_csv = vm["csv-file"].as<string>();
    } else {
        cerr << "Input file missing" << endl << desc << endl;
        return 2;
    }
    int field;
    if (vm.count("field")) {
        field = vm["field"].as<int>();
    } else {
        cerr << "Field option missing" << endl << desc << endl;
        return 2;
    }
    size_t ignore_lines = 0;
    if (vm.count("ignore-lines")) {
        ignore_lines = vm["ignore-lines"].as<size_t>();
    }

    debug_log << "Loading CSV file";
    vector<double> data;
    shared_ptr<istream> infile;
    string line;
    const vector<int> fields{field};

    if ("-" == input_csv) {
        infile.reset(&cin, [](istream* a) {});
    } else {
        infile.reset(new ifstream);
        infile->exceptions(ifstream::badbit);
        dynamic_cast<ifstream*>(infile.get())->open(input_csv);
    }
    for (size_t i = 0; i != ignore_lines; ++i) {
        if (!getline(*infile, line)) {
            cerr << format("Error: input file doesn't have %1% lines to ignore") % ignore_lines << endl;
            return 3;
        }
        debug_log << "ignoring line: " << line;
    }
    while (getline(*infile, line)) {
        data.push_back(extract_csv_fields(line, fields)[0]);
        debug_log << "read " << data.back() << endl;
    }
    debug_log << "Finished loading CSV file";
    int *changepoints;
    size_t cp_n;
    pilot_changepoint_detection(data.data(), data.size(), &changepoints, &cp_n, 30, percent);
    for (size_t i = 0; i != cp_n; ++i) {
        if (0 != i) cout << ",";
        cout << changepoints[i];
    }
    cout << endl;
    pilot_free(changepoints);

    return 0;
}
