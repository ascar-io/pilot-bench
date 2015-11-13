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
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include "../interface_include/libpilot.h"

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

// We are using two PIs. 0 is time per I/O, 1 is throughput per I/O.
enum piid_t {
    time_pi = 0,
    tp_pi = 1
};

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
int workload_func(size_t total_work_amount,
                  pilot_malloc_func_t *lib_malloc_func,
                  size_t *num_of_work_unit,
                  double ***unit_readings,
                  double **readings) {
    *num_of_work_unit = total_work_amount / g_io_size;
    // allocate space for storing result readings
    *unit_readings = (double**)lib_malloc_func(sizeof(double*) * 2);
    (*unit_readings)[0] = (double*)lib_malloc_func(sizeof(double) * *num_of_work_unit);
    (*unit_readings)[1] = (double*)lib_malloc_func(sizeof(double) * *num_of_work_unit);
    *readings = (double*)lib_malloc_func(sizeof(double) * 2);
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
        (*unit_readings)[time_pi][i] = (double)(work_unit_elapsed_times[i]-prev_ts) / ONE_SECOND;
        (*unit_readings)[tp_pi][i] = ((double)g_io_size / MEGABYTE) / ((double)(work_unit_elapsed_times[i]-prev_ts) / ONE_SECOND);
        prev_ts = work_unit_elapsed_times[i];
    }

    (*readings)[time_pi] = (double)total_elapsed_time / ONE_SECOND;
    (*readings)[tp_pi] = ((double)total_work_amount / MEGABYTE) / ((double)total_elapsed_time / ONE_SECOND);

    return 0;
}

int main(int argc, char **argv) {
    // Parsing the command line arguments
    po::options_description desc("Generates a non-zero sequential write I/O workload and demonstrates how to use libpilot");
    // Don't use po::value<>()->required() here or --help wouldn't work
    desc.add_options()
            ("help", "produce help message")
            ("fsync,f", "call fsync() after each I/O request")
            ("io-size,s", po::value<size_t>(), "the size of I/O operations (default to 1 MB)")
            ("length-limit,l", po::value<size_t>(), "the max. length of the workload in bytes (default to 100*1024*1024); "
                    "the workload will not write beyond this limit")
            ("init-length,i", po::value<size_t>(), "the initial length of workload in bytes (default to 1/5 of limitsize); "
                    "the workload will start from this length and be gradually repeated or increased until the desired "
                    "confidence level is reached")
            ("output,o", po::value<string>(), "set output file name, can be a device. WARNING: the file will be overwritten if it exists.")
            ("result,r", po::value<string>(), "set result file name, (default to seq-write.csv)")
            ("warm-up-io,w", po::value<size_t>(), "the number of I/O operations that will be removed from the beginning as the warm-up phase (default to 1/5 of total IO ops)")
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
        if (vm["io-size"].as<size_t>() <= 0) {
            cout << "I/O size must be larger than 0" << endl;
            return 1;
        }
        g_io_size = vm["io-size"].as<size_t>();
    }
    string result_file_name("seq-write.csv");
    if (vm.count("result")) {
        result_file_name = vm["result"].as<string>();
    }

    // fill g_io_buf with some pseudo-random data
    g_io_buf = new char[g_io_size];
    for (size_t i = 0; i < g_io_size; ++i)
        g_io_buf[i] = (char)i + 42;
    cout << "I/O size is set to ";
    if (g_io_size >= MEGABYTE) {
        cout << g_io_size / MEGABYTE << " MB" << endl;
    } else {
        cout << g_io_size << " bytes" << endl;
    }

    if (vm.count("length-limit")) {
        if (vm["limit"].as<size_t>() <= 0) {
            cout << "I/O limit must be larger than 0" << endl;
            return 1;
        }
        io_limit = vm["limit"].as<size_t>();
    }
    cout << "I/O limit is set to ";
    if (io_limit >= MEGABYTE) {
        cout << io_limit / MEGABYTE << " MB"<< endl;
    } else {
        cout << io_limit << " bytes" << endl;
    }

    size_t init_length;
    if (vm.count("init-length"))
        init_length = vm["init-length"].as<size_t>();
    else
        init_length = io_limit / 5;

    size_t warmupio = io_limit / g_io_size / 5;
    if (vm.count("warm-up-io")) {
        warmupio = vm["warmupio"].as<size_t>();
        cout << "warmupio set to " << warmupio << endl;
    } else {
        cout << "warmupio set to 1/5 of total IO ops" << endl;
    }

    // Starting the actual work
    cout << "Running benchmark ..." << endl;
    pilot_workload_t *wl = pilot_new_workload("Sequential write");
    pilot_set_num_of_pi(wl, 2);
    pilot_set_work_amount_limit(wl, io_limit);
    pilot_set_init_work_amount(wl, init_length);
    pilot_set_workload_func(wl, &workload_func);

    int res = pilot_run_workload(wl);
    if (res != 0) {
        cout << pilot_strerror(res) << endl;
        return res;
    }
    cout << "Benchmark finished" << endl << endl;

    // analyze the readings
    cout << setprecision(4);
    size_t num_of_work_units;
    const double* time_readings = pilot_get_pi_unit_readings(wl, time_pi, 0, &num_of_work_units) + warmupio;
    num_of_work_units -= warmupio;
    double sm = pilot_subsession_mean(time_readings, num_of_work_units);
    cout << "Time per I/O analysis" << endl;
    cout << "=====================" << endl;
    cout << "removed " << warmupio << " IO ops from beginning as warm-up phase" << endl;
    cout << "sample mean: " << sm << endl;
    double var = pilot_subsession_var(time_readings, num_of_work_units, 1, sm);
    cout << "variance (q=1): " << var << endl;
    cout << "variance (q=1) % of sample mean: " << var * 100 / sm << "%" << endl;
    double ac = pilot_subsession_autocorrelation_coefficient(time_readings, num_of_work_units, 1, sm);
    cout << "autocorrelation coefficient (q=1): " << ac << endl;
    size_t q = pilot_optimal_subsession_size(time_readings, num_of_work_units);
    cout << "optimal subsession size (q): " << q << endl;
    var = pilot_subsession_var(time_readings, num_of_work_units, q, sm);
    cout << "variance (q=" << q << "): " << var << endl;
    cout << "variance (q=" << q << ") % of sample mean: " << var * 100 / sm << "%" << endl;
    size_t min_ss = pilot_optimal_length(time_readings, num_of_work_units, sm*0.05);
    cout << "minimum sample size according to t-distribution (using the q above): " << min_ss << endl;
    size_t cur_ss = num_of_work_units / q;
    cout << "current sample size: " << cur_ss << endl;

    double ci = pilot_subsession_confidence_interval(time_readings, num_of_work_units, q, .95);
    if (cur_ss >= min_ss && cur_ss >= 200) {
        cout << "We have a large enough sample size in this benchmark." << endl;
    } else {
        if (cur_ss < 200)
            cout << "sample size MIGHT NOT be large enough (>200 is recommended)" << endl;
        else
            cout << "sample size is not large enough to achieve a confidence interval of 0.05 * sample_mean." << endl;
    }
    cout << "confidence interval is " << ci*100/sm << "% of sample_mean" << endl;
    cout << "calculated throughput is "
         << double(io_limit) / double(num_of_work_units) / MEGABYTE / sm
         << " +- " << ci << " MB/s with 95% confidence" << endl;

    cout << "dumb throughput (io_limit / total_elapsed_time) (MB/s): [";
    const double* tp_readings = pilot_get_pi_readings(wl, tp_pi);
    size_t rounds = pilot_get_num_of_rounds(wl);
    for(size_t i = 0; i < rounds; ++i) {
        cout << tp_readings[i];
        if (i != rounds-1) cout << ", ";
    }
    cout << "]" << endl;

    res = pilot_export(wl, CSV, result_file_name.c_str());
    if (res != 0) {
        cout << pilot_strerror(res) << endl;
        return res;
    }
    cout << "Benchmark results are saved to " << result_file_name << endl;
    assert(pilot_destroy_workload(wl) == 0);
    delete g_io_buf;
    return 0;
}
