/*
 * common.h: the common header file shared across pilot-tool
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

#ifndef PILOT_TOOL_INCLUDE_COMMON_H_
#define PILOT_TOOL_INCLUDE_COMMON_H_

#include <iostream>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>

#define debug_log   BOOST_LOG_TRIVIAL(debug)
#define info_log    BOOST_LOG_TRIVIAL(info)
#define warning_log BOOST_LOG_TRIVIAL(warning)
#define error_log   BOOST_LOG_TRIVIAL(error)
#define fatal_log   BOOST_LOG_TRIVIAL(fatal)

namespace pilot {

inline void die_if (bool condition, int error_code = 1, const char *error_msg = NULL) {
    if (condition) return;

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

} /* namespace pilot */

#endif /* PILOT_TOOL_INCLUDE_COMMON_H_ */
