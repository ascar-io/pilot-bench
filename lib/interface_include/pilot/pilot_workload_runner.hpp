/*
 * pilot_workload_runner.hpp: a runner that runs the workload in a
 * separate thread. It is designed to be used with a UI logger.
 *
 * Copyright (c) 2017-2018 Yan Li <yanli@tuneup.ai>. All rights reserved.
 * The Pilot tool and library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License version 2.1 (not any other version) as published by the Free
 * Software Foundation.
 *
 * The Pilot tool and library is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program in file lgpl-2.1.txt; if not, see
 * https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 *
 * Commit 033228934e11b3f86fb0a4932b54b2aeea5c803c and before were
 * released with the following license:
 * Copyright (c) 2015, 2016, University of California, Santa Cruz, CA, USA.
 * Created by Yan Li <yanli@tuneup.ai>,
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

#ifndef LIB_INTERFACE_INCLUDE_PILOT_WORKLOAD_RUNNER_HPP_
#define LIB_INTERFACE_INCLUDE_PILOT_WORKLOAD_RUNNER_HPP_

#include <boost/timer/timer.hpp>
#include <common.h>
#include <fcntl.h>
#include "../interface_include/pilot/libpilot.h"
#include <string>
#include <thread>
#include <vector>

namespace pilot {

/**
 * \brief A workload runner that runs the workload in a separate thread
 */
template <class Logger>
class WorkloadRunner {
private:
    Logger           &logger_;
    pilot_workload_t *wl_;
    std::thread       thread_;
    int               benchmark_err_;

    void thread_func(void) {
        //! TODO: give me a higher thread execution priority
        // Starting the actual work
        logger_ << "Running benchmark ..." << std::endl;
        int res = pilot_run_workload(wl_);
        if (res != 0) {
            logger_ << "</13>" << pilot_strerror(res) << std::endl;
            benchmark_err_ = res;
            return;
        }
        logger_ << "Benchmark finished" << std::endl << std::endl;
        benchmark_err_ = 0;
    }
public:
    WorkloadRunner(pilot_workload_t *wl, Logger &logger)
        : logger_(logger), wl_(wl), benchmark_err_(0) {
    }

    void start(void) {
        thread_ = std::thread(&WorkloadRunner::thread_func, this);
    }

    void join(void) {
        thread_.join();
    }

    int get_workload_result() const {
        return benchmark_err_;
    }

    ~WorkloadRunner() {
    }
};

} // namespace pilot

#endif /* LIB_INTERFACE_INCLUDE_PILOT_WORKLOAD_RUNNER_HPP_ */
