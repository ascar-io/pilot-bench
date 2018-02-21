/*
 * common.h: the common header file shared across pilot-tool
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

#ifndef PILOT_TOOL_INCLUDE_COMMON_H_
#define PILOT_TOOL_INCLUDE_COMMON_H_

#ifdef __arm__
  #pragma pack(4)
#endif

#include <boost/log/trivial.hpp>
#include <boost/timer/timer.hpp>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#define trace_log   BOOST_LOG_TRIVIAL(trace)
#define debug_log   BOOST_LOG_TRIVIAL(debug)
#define info_log    BOOST_LOG_TRIVIAL(info)
#define warning_log BOOST_LOG_TRIVIAL(warning)
#define error_log   BOOST_LOG_TRIVIAL(error)
#define fatal_log   BOOST_LOG_TRIVIAL(fatal)

// extra indirection so other macros can be used as parameter
// http://stackoverflow.com/questions/6713420/c-convert-integer-to-string-at-compile-time#comment7949445_6713658
#define _stringify(x) #x
#define stringify(x) _stringify(x)

#if defined(__clang__)
#  define CC_VERSION "clang-" __clang_version__
#elif defined(__GNUC__)
# if defined(__GNUC_PATCHLEVEL__)
#  define CC_VERSION "gcc-" stringify(__GNUC__) "." stringify(__GNUC_MINOR__) "." stringify(__GNUC_PATCHLEVEL__)
# else
#  define CC_VERSION "gcc-" stringify(__GNUC__) "." stringify(__GNUC_MINOR__)
# endif
#endif

#define SHOULD_NOT_REACH_HERE { fatal_log << __func__ << "():" << stringify(__LINE__) \
    << " Error: shouldn't reach here"; }

namespace pilot {

// all consts go here, they should be named either k_something, or ALL_UPPERCASE
boost::timer::nanosecond_type const ONE_SECOND = 1000000000LL;
size_t const MEGABYTE = 1024*1024;

inline void die_if (bool condition, int error_code = 1, const char *error_msg = NULL) {
    if (!condition) return;

    fatal_log << error_msg;
    exit(error_code);
}

inline void die_if (bool condition, int error_code = 1, const std::string & error_msg = "") {
    if (!condition) return;

    fatal_log << error_msg;
    exit(error_code);
}

/**
 * \brief ASSERT_VALID_POINTER(p) checks that p is a valid pointer and aborts if it is NULL
 * \details This macro is used to check critical input parameters.
 */
#define ASSERT_VALID_POINTER(p) { if (NULL == (p)) { \
        fatal_log << "function " << __func__ << "() called with " #p " as a NULL pointer"; \
        abort(); \
    }}

/**
 * Printing a pair
 * @param o the ostream object
 * @param a the pair for printing
 * @return
 */
template <typename FirstT, typename SecondT>
std::ostream& operator<<(std::ostream &o, const std::pair<FirstT, SecondT> &a) {
    o << "<" << a.first << ", " << a.second << ">";
    return o;
}

/**
 * Printing a vector
 * @param o the ostream object
 * @param a the vector for printing
 * @return
 */
template <typename T>
std::ostream& operator<<(std::ostream &o, const std::vector<T> &a) {
    o << "[";
    if (a.size() > 0) {
        // we cannot use copy() and ostream_iterator here because they insist
        // that operator<<() should be in std.
        for (size_t i = 0; i < a.size() - 1; ++i) {
            o << a[i] << ", ";
        }
        o << a.back();
    }
    o << "]";
    return o;
}

inline std::string sstream_get_last_lines(const std::stringstream& ss, size_t n = 1) {
    const std::string &s = ss.str();
    if (0 == s.size()) {
        return std::string("");
    }
    if (1 == s.size()) {
        return s;
    }
    size_t loc;
    if ('\n' != s.back()) {
        loc = s.size() - 1;
    } else {
        loc = s.size() - 2;
    }
    for (; n != 0; --n) {
        // TOOD: need to handle \r here too for Windows later
        loc = s.rfind('\n', loc);
        if (std::string::npos == loc) {
            return s;
        }
        if (0 == loc) {
            if (1 == n) {
                return std::string(s.data() + 1);
            } else {
                return s;
            }
        }
        --loc;
    }
    return std::string(s.data() + loc + 2);
}

} /* namespace pilot */

#endif /* PILOT_TOOL_INCLUDE_COMMON_H_ */
