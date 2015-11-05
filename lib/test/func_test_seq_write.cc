/*
 * func_test_seq_write.cc: functional test for libpilot using a
 * sequential write I/O workload
 *
 * Copyright (c) 2015, University of California, Santa Cruz, CA, USA.
 *
 * Developers:
 *   Yan Li <yanli@cs.ucsc.edu>
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
#include <iostream>
#include <string>
#include <vector>
#include "../interface_include/libpilot.h"

using namespace std;
namespace po = boost::program_options;
using boost::timer::cpu_timer;
using boost::timer::nanosecond_type;

/**
 * \brief the sequential write workload func for libpilot
 * \details THis function generates a series of fsyncked sequential I/O and calculate the throughput.
 * @param[in] total_work_amount
 * @param[in] lib_malloc_func
 * @param[out] num_of_work_unit
 * @param[out] reading_per_work_unit the memory can be
 * @param[out] readings
 * @return
 */
int workload_func(size_t total_work_amount,
                  pilot_malloc_func_t *lib_malloc_func,
                  size_t *num_of_work_unit,
                  double ***reading_per_work_unit,
                  double **readings) {

    /*! \todo implement function */
    return 0;
}

int main(int argc, char **argv) {
    // Parsing the command line arguments
    po::options_description desc("Allowed options");
    // Don't use po::value<>()->required() here or --help wouldn't work
    desc.add_options()
            ("help", "produce help message")
            ("output,o", po::value<string>(), "set output file name, can be a device")
            ("io-size,s", po::value<int>(), "set I/O request size in bytes (default to 4096)")
            ("limit,l", po::value<int>(), "set I/O size in bytes (default to 100*1024*1024)")
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

    string output_file_name;
    size_t io_size  = 4096;
    size_t io_limit = 100*1024*1024;
    if (vm.count("output")) {
        output_file_name = vm["output"].as<string>();
        cout << "Output file is set to " << output_file_name << endl;
    } else {
        cout << "Error: output file was not set." << endl;
        cout << desc << endl;
        return 2;
    }
    if (vm.count("io-size")) {
        if (vm["io-size"].as<int>() <= 0) {
            cout << "I/O size must be larger than 0" << endl;
            return 1;
        }
        io_size = vm["io-size"].as<int>();
    }
    cout << "I/O size is set to " << io_size << endl;
    if (vm.count("limit")) {
        if (vm["limit"].as<int>() <= 0) {
            cout << "I/O limit must be larger than 0" << endl;
            return 1;
        }
        io_limit = vm["limit"].as<int>();
    }
    cout << "I/O limit is set to " << io_limit << endl;

    // Starting the actual work
    pilot_workload_t *wl = pilot_new_workload("Sequential write");
    pilot_set_num_of_pi(wl, 1);
    // Limit the write to 500 MB
    pilot_set_total_work_amount(wl, 500);
    pilot_set_workload_func(wl, &workload_func);

    int res = pilot_run_workload(wl);
    if (res != 0) {
        cout << pilot_strerror(res) << endl;
        return res;
    }

    res = pilot_export(wl, CSV, "/tmp/seq-write.csv");
    if (res != 0) {
        cout << pilot_strerror(res) << endl;
        return res;
    }
    return 0;
}
