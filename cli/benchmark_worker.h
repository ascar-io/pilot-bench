/*
 * benchmark_worker.h: the adaptor for running a sequential write workload.
 * Later we will make it universal and be able to load and run workloads
 * from a dynamic library.
 *
 * Copyright (c) 2015, University of California, Santa Cruz, CA, USA.
 * Created by Yan Li <yanli@ucsc.edu, elliot.li.tech@gmail.com>,
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

#ifndef CLI_BENCHMARK_WORKER_H_
#define CLI_BENCHMARK_WORKER_H_

#include <boost/timer/timer.hpp>
#include <common.h>
#include <fcntl.h>
#include "../interface_include/libpilot.h"
#include <string>
#include <thread>
#include <vector>

#define kNumOfPI (1)
// We are using two PIs. 0 is time per I/O, 1 is throughput per I/O.
enum piid_t {
    time_pi = 0,
    //tp_pi = 1
};
extern size_t g_current_round;
extern void *g_logger;
extern size_t g_io_size;
extern char *g_io_buf;
extern std::string g_output_file_name;
extern bool g_fsync;
extern std::string g_result_file_name;

double ur_format_func(const pilot_workload_t* wl, double ur);

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
template <class Logger>
int workload_func(size_t total_work_amount,
                  pilot_malloc_func_t *lib_malloc_func,
                  size_t *num_of_work_unit,
                  double ***unit_readings,
                  double **readings) {
    using namespace pilot;
    using namespace std;
    using boost::timer::cpu_timer;
    using boost::timer::nanosecond_type;

    Logger &logger = *static_cast<Logger*>(g_logger);
    *num_of_work_unit = total_work_amount / g_io_size;
    // allocate space for storing result readings
    *unit_readings = (double**)lib_malloc_func(sizeof(double*) * kNumOfPI);
    (*unit_readings)[0] = (double*)lib_malloc_func(sizeof(double) * *num_of_work_unit);
    //(*unit_readings)[1] = (double*)lib_malloc_func(sizeof(double) * *num_of_work_unit);
    *readings = (double*)lib_malloc_func(sizeof(double) * kNumOfPI);
    std::vector<nanosecond_type> work_unit_elapsed_times(*num_of_work_unit);

    int fd = open(g_output_file_name.c_str(), O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP);
    if (fd == -1) {
        logger << "file open error: " << strerror(errno) << std::endl;
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
                logger << "I/O error: " << strerror(errno) << std::endl;
                close(fd);
                return errno;
            }
        }
        if (g_fsync) {
            do {
                res = fsync(fd);
            } while (EINTR == res);
            if (0 != res) {
                logger << "fsync error: " << strerror(errno) << std::endl;
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
        //(*unit_readings)[tp_pi][i] = ((double)g_io_size / MEGABYTE) / ((double)(work_unit_elapsed_times[i]-prev_ts) / ONE_SECOND);
        prev_ts = work_unit_elapsed_times[i];
    }

    (*readings)[time_pi] = (double)total_elapsed_time / ONE_SECOND;
    //(*readings)[tp_pi] = ((double)total_work_amount / MEGABYTE) / ((double)total_elapsed_time / ONE_SECOND);

    return 0;
}

template <typename Logger>
bool pre_workload_run_hook(pilot_workload_t* wl) {
    Logger &logger = *static_cast<Logger*>(g_logger);
    g_current_round = pilot_get_num_of_rounds(wl);
    logger << "Round " << g_current_round << " started with "
         << pilot_next_round_work_amount(wl) / pilot::MEGABYTE << " MB work amount ... " << std::endl;
    return true;
}

template <typename Logger>
bool post_workload_run_hook(pilot_workload_t* wl) {
    using namespace std;
    Logger &logger = *static_cast<Logger*>(g_logger);

    logger << "Round " << g_current_round << " finished" << endl;
    logger << "Round " << g_current_round << " Summary" << endl;
    logger << "============================" << endl;
    char *buf = pilot_text_round_summary(wl, pilot_get_num_of_rounds(wl) - 1);
    logger << buf << endl;
    pilot_free_text_dump(buf);

    logger << "Workload Summary So Far" << endl;
    logger << "============================" << endl;
    buf = pilot_text_workload_summary(wl);
    logger << buf << std::endl;
    pilot_free_text_dump(buf);

    pilot_workload_info_t *wi = pilot_workload_info(wl);
    logger << wi;  // logger decides how to interpret and display workload info
    pilot_free_workload_info(wi);

    return true;
}

/**
 * \brief A benchmark worker thread
 */
template <class Logger>
class BenchmarkWorker {
private:
    size_t      io_size_;
    size_t      length_limit_;
    size_t      init_length_;
    std::string output_file_;
    std::string result_file_;
    bool        fsync_;
    Logger     &logger_;
    pilot_workload_t *wl_;
    std::thread thread_;
    int         benchmark_err_;

    void thread_func(void) {
        int res = pilot_run_workload(wl_);
        if (res != 0) {
            logger_ << "</13>" << pilot_strerror(res) << std::endl;
            benchmark_err_ = res;
            return;
        }
        logger_ << "Benchmark finished" << std::endl << std::endl;

        res = pilot_export(wl_, CSV, g_result_file_name.c_str());
        if (res != 0) {
            logger_ << "</13>" << pilot_strerror(res) << std::endl;
            benchmark_err_ = res;
            return;
        }
        logger_ << "Benchmark results are saved to " << g_result_file_name << std::endl;
        benchmark_err_ = 0;
    }
public:
    BenchmarkWorker(size_t io_size, size_t length_limit, size_t init_length,
                    const std::string &output_file,
                    const std::string &result_file, bool fsync,
                    Logger &logger)
        : io_size_(io_size), length_limit_(length_limit), init_length_(init_length),
          output_file_(output_file), result_file_(result_file), fsync_(fsync),
          logger_(logger), benchmark_err_(0) {
        g_io_size = io_size;
        g_output_file_name = std::string(output_file);
        g_fsync = fsync;
        g_logger = static_cast<void*>(&logger);
        g_result_file_name = std::string(result_file);

        // fill g_io_buf with some pseudo-random data
        g_io_buf = new char[g_io_size];
        for (size_t i = 0; i < g_io_size; ++i)
            g_io_buf[i] = (char)i + 42;

        // Starting the actual work
        logger_ << "Running benchmark ..." << std::endl;
        wl_ = pilot_new_workload("Sequential write");
        pilot_set_num_of_pi(wl_, kNumOfPI);
        pilot_set_pi_unit_reading_format_func(wl_, 0, &ur_format_func, "MB/s");
        pilot_set_work_amount_limit(wl_, length_limit);
        pilot_set_init_work_amount(wl_, init_length);
        pilot_set_workload_func(wl_, &workload_func<Logger>);
        // Set up hooks for displaying progress information
        pilot_set_hook_func(wl_, PRE_WORKLOAD_RUN, &pre_workload_run_hook<Logger>);
        pilot_set_hook_func(wl_, POST_WORKLOAD_RUN, &post_workload_run_hook<Logger>);
    }

    void start(void) {
        thread_ = std::thread(&BenchmarkWorker::thread_func, this);
    }

    void join(void) {
        thread_.join();
    }

    ~BenchmarkWorker() {
        pilot_destroy_workload(wl_);
        delete[] g_io_buf;
    }
};

#endif /* CLI_BENCHMARK_WORKER_H_ */
