/*
 * pilot-cli.h
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

#ifndef CLI_PILOT_CLI_H_
#define CLI_PILOT_CLI_H_

#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/format.hpp>
#include <common.h>
#include <config.h>
#include <iostream>
#include <pilot/libpilot.h>
#include <sstream>
#include <stdexcept>
#include <vector>

#define GREETING_MSG "Pilot " stringify(PILOT_VERSION_MAJOR) "." \
    stringify(PILOT_VERSION_MINOR) " (compiled by " CC_VERSION " on " __DATE__ ")"

int handle_analyze(int argc, const char** argv);
int handle_run_program(int argc, const char** argv);
int handle_detect_changepoint_edm(int argc, const char** argv);

inline std::string get_timestamp(void) {
    using namespace boost::posix_time;
    ptime now = second_clock::universal_time();

    static std::locale loc(std::cout.getloc(),
                      new time_facet("%Y%m%d_%H%M%S"));

    std::stringstream ss;
    ss.imbue(loc);
    ss << now;
    return ss.str();
}

template <typename ResultType>
std::vector<ResultType> extract_csv_fields(const std::string &csvstr,
                                           const std::vector<int> &columns) {
    using namespace std;
    using namespace boost;
    vector<string> pidata_strs;
    // must have \r here to support files generated on Windows
    boost::split(pidata_strs, csvstr, boost::is_any_of(" \r\n\t,"));
    vector<ResultType> r(pidata_strs.size());
    for (int i = 0; i < (int)columns.size(); ++i) {
        int col = columns[i];
        if (col >= static_cast<int>(pidata_strs.size())) {
            throw runtime_error("Malformed line");
        }
        r[i] = lexical_cast<ResultType>(pidata_strs[col]);
    }
    return r;
}

inline void print_read_the_doc_info(void) {
    std::cerr << "To understand the math behind Pilot or read tutorials, please read the" << std::endl;
    std::cerr << "documentation at https://docs.ascar.io/" << std::endl;
}

#endif /* CLI_PILOT_CLI_H_ */
