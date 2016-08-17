/*
 * Pilot CLI: the analyze command
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

int handle_analyze(int argc, const char** argv) {
    // We use cerr for printing option parsing errors.

    po::options_description generic("Usage: " + string(argv[0]) + " [options] input_csv_file");
    generic.add_options()
            ("help", "help message for run_command")
            ("ac,a", po::value<double>(), "Set the required range of autocorrelation coefficient. arg should be a value within (0, 1], and the range will be set to [-arg,arg]")
            ("field,f", po::value<int>(), "The field of the CSV to import (default: 0). Note: first field is 0.")
            ("ignore-lines,i", po::value<int>(), "ignore the first arg lines")
            ("mean-method,m", po::value<int>(), "0: arithmetic mean (default); 1: harmonic mean")
            ("preset", po::value<string>(), "preset modes control the statistical requirements for the results to be satisfactory\n"
                    "quick:      \t(default) autocorrelation limit: 0.8,\n"
                    "normal:     \tautocorrelation limit: 0.2,\n"
                    "strict:     \tautocorrelation limit: 0.1,\n")
            ("quiet,q", "quiet mode")
            ("verbose,v", "print debug information")
            ;
    po::options_description hidden("Hidden options");
    hidden.add_options()
            ("csv-file,c", po::value<string>(), "input csv file name, use - for stdin")
            ;
    po::positional_options_description p;
    p.add("csv-file", -1);
    po::options_description cmdline_options;
    cmdline_options.add(generic).add(hidden);

    // copy options into args
    vector<string> args;
    for (int i = 2; i != argc; ++i) {
        args.push_back(argv[i]);
    }

    // parse args
    po::variables_map vm;
    try {
        po::parsed_options parsed =
            po::command_line_parser(args).options(cmdline_options).positional(p).run();
        po::store(parsed, vm);
        po::notify(vm);
    } catch (po::error &e) {
        try {
            cerr << e.what() << endl;
        } catch (const std::out_of_range &en) {
            // https://svn.boost.org/trac/boost/ticket/11891
            // However this catch doesn't work in lldb.
            cerr << "Invalid options. Please double check the input file. There\n"
                    "can be only one input file.\n";
            return 2;
        }
        return 2;
    }

    if (vm.count("help")) {
        cerr << generic << endl;
        cerr << "Use - as input_csv_file to read from stdin." << endl << endl;
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

    pilot_mean_method_t pi_mean_method = ARITHMETIC_MEAN;
    if (vm.count("mean-method")) {
        int tmpi = vm["mean-method"].as<int>();
        if (tmpi < 0 || tmpi > 1) {
            // TODO: fix me, use Boost custom validator:
            // http://www.boost.org/doc/libs/1_61_0/doc/html/program_options/howto.html
            fprintf(stderr, "the argument ('%d') for option '--mean-method' is invalid\n", tmpi);
            return 2;
        }
        pi_mean_method = static_cast<pilot_mean_method_t>(tmpi);
    }

    string input_csv;
    if (vm.count("csv-file")) {
        input_csv = vm["csv-file"].as<string>();
    } else {
        cerr << "Input file missing" << endl << generic << endl;
        return 2;
    }
    int field = 0;
    if (vm.count("field")) {
        field = vm["field"].as<int>();
        if (field < 0) {
            fprintf(stderr, "the argument ('%d') for option '--field' is invalid\n", field);
            return 2;
        }
    }
    int ignore_lines = -1;
    if (vm.count("ignore-lines")) {
        ignore_lines = vm["ignore-lines"].as<int>();
        if (ignore_lines < 0) {
            fprintf(stderr, "the argument ('%d') for option '--ignore-lines' is invalid\n", ignore_lines);
            return 2;
        }
    }

    double ac;       // autocorrelation coefficient threshold
    string preset_mode = "quick";
    if (vm.count("preset")) {
        preset_mode = vm["preset"].as<string>();
    }
    {
        string msg = "Preset mode activated: ";
        if ("quick" == preset_mode) {
            msg += "quick";
            ac = 0.8;
        } else if ("normal" == preset_mode) {
            msg += "normal";
            ac = 0.2;
        } else if ("strict" == preset_mode) {
            msg += "strict";
            ac = 0.1;
        } else {
            cerr << str(format("Unknown preset mode \"%1%\", exiting...") % preset_mode);
            return 2;
        }
        info_log << msg;

        // read individual values that may override our preset
        if (vm.count("ac")) {
            ac = vm["ac"].as<double>();
            if (ac <= 0 || ac > 1) {
                fprintf(stderr, "the argument ('%f') for option '--ac' is invalid\n", ac);
                fprintf(stderr, "Valid range for the autocorrelation coefficient arg is (0,1], exiting...\n");
                return 2;
            }
        }
        info_log << "Setting the limit of autocorrelation coefficient to " << ac;
    }

    // From now on we use fatal_log for errors.
    vector<double> data;
    shared_ptr<istream> infile;
    string line;
    const vector<int> fields{field};

    try {
        if ("-" == input_csv) {
            // Disable deleting cin
            infile.reset(&cin, [](istream* a) {});
            debug_log << "Reading data from stdin";
        } else {
            infile.reset(new ifstream);
            infile->exceptions(ifstream::badbit | ifstream::failbit);
            debug_log << "Reading data from " << input_csv;
            dynamic_cast<ifstream*>(infile.get())->open(input_csv);
        }
        // ignore_lines may be -1, in which case we will ignore errors on line 1 later
        size_t lineno = 1;
        for (int i = 0; i < ignore_lines; ++i) {
            if (!getline(*infile, line)) {
                cerr << format("Error: input file doesn't have %1% lines to ignore") % ignore_lines << endl;
                return 3;
            }
            ++lineno;
            debug_log << "ignoring line: " << line;
        }
        // Import the actual data
        while (getline(*infile, line)) {
            // Remove the last '\r' to prevent from messing with log
            if (line.size() >= 1 && line.back() == '\r') {
                line.resize(line.size()-1);
            }
            try {
                try {
                    data.push_back(extract_csv_fields<double>(line, fields)[0]);
                    debug_log << format("Read line %1%. Data: \"%2%\"") % lineno % data.back();
                    ++lineno;
                } catch (const boost::bad_lexical_cast &e) {
                    if (1 == lineno && -1 == ignore_lines) {
                        warning_log << "Ignoring first line in input. It might be a header. Use `-i 1` to suppress this warning. Line data: \"" << line << '"';
                        ++lineno;
                        continue;
                    } else {
                        throw runtime_error("Malformed data");
                    }
                }
            } catch (const runtime_error& e) {
                fatal_log << format("Failed to extract a float number from from field %1% in line %2%, malformed data? Aborting. Line data: \"%3%\"") % field % lineno % line;
                return 6;
            }
        }

        debug_log << "Finished loading CSV file";
    } catch (const ios::failure& e) {
        // EOF also triggers failbit
        if (!infile->eof()) {
            // There is no standard way to know what error has just happened.
            // See https://codereview.stackexchange.com/questions/57829/better-option-than-errno-for-file-io-error-handling
            // e.code().message() doesn't contain any meaningful information on Mac
            cerr << "==========================================" << endl;
            cerr << "Error. Log before the error:" << endl << "..." << endl;
            shared_ptr<const char> p(pilot_get_last_log_lines(3), [](const char*p){ pilot_free((void*)p);});
            cerr << *p;
            cerr.flush();
            fatal_log << format("I/O error (%1%): %2%") % errno % strerror(errno);
            return errno;
        }
    }

    double sample_mean = pilot_subsession_mean_p(data.data(), data.size(), pi_mean_method);
    printf("sample_size %zu\n", data.size());
    printf("mean %f\n", sample_mean);
    int ss = pilot_optimal_subsession_size_p(data.data(), data.size(), pi_mean_method, ac);
    if (ss < 0) {
        fatal_log << "Autocorrelation coefficient (AC) limit (" << ac << ") cannot be met. This means\n"
                     "that your data has high autocorrelation and is unlikely i.i.d. See\n"
                     "https://docs.ascar.io/features/autocorrelation-detection-and-mitigation.html for a\n"
                     "detailed explanation. You should check the source of the data and try to reduce the\n"
                     "autocorrelation among samples. You can also set a higher AC limit (using --ac) to\n"
                     "bypass this limit.\n";
        return 5;
    }
    printf("optimal_subsession_size %d\n", ss);
    printf("CI %f\n", pilot_subsession_confidence_interval_p(data.data(), data.size(), ss, .95, pi_mean_method));
    printf("variance %f\n", pilot_subsession_var_p(data.data(), data.size(), ss, sample_mean, pi_mean_method));
    printf("subsession_autocorrelation_coefficient %f\n", pilot_subsession_autocorrelation_coefficient_p(data.data(), data.size(), ss, sample_mean, pi_mean_method));

    return 0;
}
