/*
 * unit_test_misc.cc
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

#include <common.h>
#include "gtest/gtest.h"
#include "pilot/libpilot.h"

using namespace pilot;
using namespace std;

TEST(MiscUnitTests, TestFindLastLines) {
    stringstream ss;
    ASSERT_STREQ("", sstream_get_last_lines(ss));
    ASSERT_STREQ("", sstream_get_last_lines(ss, 1));
    ASSERT_STREQ("", sstream_get_last_lines(ss, 3));
    ASSERT_STREQ("", sstream_get_last_lines(ss, 100));
    ss << "\n";
    ASSERT_STREQ("\n", sstream_get_last_lines(ss));
    ASSERT_STREQ("\n", sstream_get_last_lines(ss, 1));
    ASSERT_STREQ("\n", sstream_get_last_lines(ss, 3));
    ASSERT_STREQ("\n", sstream_get_last_lines(ss, 100));
    ss << "\n";
    ASSERT_STREQ("\n", sstream_get_last_lines(ss));
    ASSERT_STREQ("\n", sstream_get_last_lines(ss, 1));
    ASSERT_STREQ("\n\n", sstream_get_last_lines(ss, 3));
    ASSERT_STREQ("\n\n", sstream_get_last_lines(ss, 100));
    ss << "a";
    ASSERT_STREQ("a", sstream_get_last_lines(ss));
    ASSERT_STREQ("a", sstream_get_last_lines(ss, 1));
    ASSERT_STREQ("\n\na", sstream_get_last_lines(ss, 3));
    ASSERT_STREQ("\n\na", sstream_get_last_lines(ss, 100));
    ss.str(std::string());
    ss << "a";
    ASSERT_STREQ("a", sstream_get_last_lines(ss));
    ASSERT_STREQ("a", sstream_get_last_lines(ss, 1));
    ASSERT_STREQ("a", sstream_get_last_lines(ss, 3));
    ASSERT_STREQ("a", sstream_get_last_lines(ss, 100));
    ss.str(std::string());
    ss << "a\n";
    ASSERT_STREQ("a\n", sstream_get_last_lines(ss));
    ASSERT_STREQ("a\n", sstream_get_last_lines(ss, 1));
    ASSERT_STREQ("a\n", sstream_get_last_lines(ss, 3));
    ASSERT_STREQ("a\n", sstream_get_last_lines(ss, 100));
    ss.str(std::string());
    ss << "\na";
    ASSERT_STREQ("a", sstream_get_last_lines(ss));
    ASSERT_STREQ("a", sstream_get_last_lines(ss, 1));
    ASSERT_STREQ("\na", sstream_get_last_lines(ss, 3));
    ASSERT_STREQ("\na", sstream_get_last_lines(ss, 100));
    ss.str(std::string());
    ss << "\naa";
    ASSERT_STREQ("aa", sstream_get_last_lines(ss));
    ASSERT_STREQ("aa", sstream_get_last_lines(ss, 1));
    ASSERT_STREQ("\naa", sstream_get_last_lines(ss, 3));
    ASSERT_STREQ("\naa", sstream_get_last_lines(ss, 100));
    ss.str(std::string());
    ss << "\na\n";
    ASSERT_STREQ("a\n", sstream_get_last_lines(ss));
    ASSERT_STREQ("a\n", sstream_get_last_lines(ss, 1));
    ASSERT_STREQ("\na\n", sstream_get_last_lines(ss, 3));
    ASSERT_STREQ("\na\n", sstream_get_last_lines(ss, 100));
    ss.str(std::string());
    ss << "3:[2016-08-16 15:56:38] <debug> Reading data from unit_test_analyze_input_3col_with_malformed_header.csv" << endl;
    ASSERT_STREQ("3:[2016-08-16 15:56:38] <debug> Reading data from unit_test_analyze_input_3col_with_malformed_header.csv\n", sstream_get_last_lines(ss));
    ASSERT_STREQ("3:[2016-08-16 15:56:38] <debug> Reading data from unit_test_analyze_input_3col_with_malformed_header.csv\n", sstream_get_last_lines(ss, 1));
    ASSERT_STREQ("3:[2016-08-16 15:56:38] <debug> Reading data from unit_test_analyze_input_3col_with_malformed_header.csv\n", sstream_get_last_lines(ss, 3));
    ASSERT_STREQ("3:[2016-08-16 15:56:38] <debug> Reading data from unit_test_analyze_input_3col_with_malformed_header.csv\n", sstream_get_last_lines(ss, 100));
}

int main(int argc, char **argv) {
    PILOT_LIB_SELF_CHECK;
    // we only display fatals because errors are expected in some test cases
    pilot_set_log_level(lv_fatal);

    // this does away a gtest warning message, and we don't care about execution time
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
