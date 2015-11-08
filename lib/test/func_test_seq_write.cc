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
#include <fcntl.h>
#include <iostream>
#include <string>
#include <vector>
#include "../interface_include/libpilot.h"

using namespace std;
namespace po = boost::program_options;
using boost::timer::cpu_timer;
using boost::timer::nanosecond_type;
nanosecond_type const ONE_SECOND = 1000000000LL;
size_t const MEGABYTE = 1000000;
size_t g_io_size = 1024*1024*1;
string g_output_file_name;
char *g_io_buf;
bool g_fsync = false;

/**
 * \brief the sequential write workload func for libpilot
 * \details THis function generates a series of sequential I/O and calculate the throughput.
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
                  double ***unit_readings,
                  double **readings) {
    *num_of_work_unit = total_work_amount / g_io_size;
    // allocate space for storing result readings
    *unit_readings = (double**)lib_malloc_func(sizeof(double*) * 1);
    (*unit_readings)[0] = (double*)lib_malloc_func(sizeof(double) * *num_of_work_unit);
    *readings = (double*)lib_malloc_func(sizeof(double));
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
            if (res == io_size)
                break;
            if (res > 0 && res != io_size) {
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
    nanosecond_type total_elapsed_time = timer.elapsed().wall;
    close(fd);

    // we do calculation after finishing the workload to minimize the overhead
    nanosecond_type prev_ts = 0;
    for (size_t i = 0; i < *num_of_work_unit; ++i) {
        (*unit_readings)[0][i] = ((double)g_io_size / MEGABYTE) / ((double)(work_unit_elapsed_times[i]-prev_ts) / ONE_SECOND);
        prev_ts = work_unit_elapsed_times[i];
    }

    (*readings)[0] = ((double)total_work_amount / MEGABYTE) / ((double)total_elapsed_time / ONE_SECOND);

    return 0;
}

int main(int argc, char **argv) {
    // Parsing the command line arguments
    po::options_description desc("Generates a non-zero sequential write I/O workload and demonstrates how to use libpilot");
    // Don't use po::value<>()->required() here or --help wouldn't work
    desc.add_options()
            ("help", "produce help message")
            ("fsync,f", "call fsync() after each I/O request")
            ("io-size,s", po::value<int>(), "set I/O request size in bytes (default to 1 MB)")
            ("limit,l", po::value<int>(), "set I/O size in bytes (default to 100*1024*1024)")
            ("output,o", po::value<string>(), "set output file name, can be a device. WARNING: the file will be overwritten if it exists.")
            ("result,r", po::value<string>(), "set result file name, (default to seq-write.csv)")
            ("verbose,v", "print more debug information")
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
    pilot_set_log_level(warning);
    bool verbose = false;
    if (vm.count("verbose")) {
        pilot_set_log_level(trace);
        verbose = true;
    }

    // fsync
    if (vm.count("fsync")) {
        g_fsync = true;
    }

    size_t io_limit = 100*1024*1024;
    if (vm.count("output")) {
        g_output_file_name = vm["output"].as<string>();
        cout << "Output file is set to " << g_output_file_name << endl;
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
        g_io_size = vm["io-size"].as<int>();
    }
    string result_file_name("seq-write.csv");
    if (vm.count("result")) {
        result_file_name = vm["result"].as<string>();
    }

    // fill g_io_buf with some pseudo-random data
    g_io_buf = new char[g_io_size];
    for (size_t i = 0; i < g_io_size; ++i)
        g_io_buf[i] = (char)i + 42;
    cout << "I/O size is set to " << g_io_size << endl;

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
    pilot_set_total_work_amount(wl, io_limit);
    pilot_set_workload_func(wl, &workload_func);

    int res = pilot_run_workload(wl);
    if (res != 0) {
        cout << pilot_strerror(res) << endl;
        return res;
    }

    // print the readings
    cout << "Readings (MB/s): [";
    const double* readings = pilot_get_pi_readings(wl, 0);
    size_t rounds = pilot_get_num_of_rounds(wl);
    for(size_t i = 0; i < rounds; ++i) {
        cout << readings[i];
        if (i != rounds-1) cout << ", ";
    }
    cout << "]" << endl;

    res = pilot_export(wl, CSV, result_file_name.c_str());
    if (res != 0) {
        cout << pilot_strerror(res) << endl;
        return res;
    }
    cout << "Unit readings are saved to " << result_file_name << endl;
    assert(pilot_destroy_workload(wl) == 0);
    delete g_io_buf;
    return 0;
}
