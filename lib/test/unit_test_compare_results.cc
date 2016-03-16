/*
 * unit_test_compare_results.cc: unit tests for the comparing results functions
 *
 * Copyright (c) 2016, University of California, Santa Cruz, CA, USA.
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

#include "gtest/gtest.h"
#include "libpilot.h"

using namespace pilot;
using namespace std;

char *g_test_data_file;

TEST(PilotUnitCompareResults, LoadBaselineFromFile) {
    pilot_workload_t *wl = pilot_new_workload("Test compare results");
    double bl_mean, bl_var;
    size_t bl_sample_size;
    ASSERT_EQ(ERR_NOT_INIT, pilot_get_baseline(wl, 0, READING_TYPE, &bl_mean, &bl_sample_size, &bl_var));
    ASSERT_EQ(ERR_NOT_INIT, pilot_get_baseline(wl, 0, UNIT_READING_TYPE, &bl_mean, &bl_sample_size, &bl_var));

    ASSERT_EQ(0, pilot_load_baseline_file(wl, g_test_data_file));
    // check that the num_of_pi is set correctly
    size_t num_of_pi;
    ASSERT_EQ(0, pilot_get_num_of_pi(wl, &num_of_pi));
    ASSERT_EQ(5, num_of_pi);
    // PI 0 has both readings and unit readings
    ASSERT_EQ(0, pilot_get_baseline(wl, 0, READING_TYPE, &bl_mean, &bl_sample_size, &bl_var));
    ASSERT_EQ(42, bl_sample_size);
    ASSERT_DOUBLE_EQ(666.66, bl_mean);
    ASSERT_DOUBLE_EQ(42.42, bl_var);
    ASSERT_EQ(0, pilot_get_baseline(wl, 0, UNIT_READING_TYPE, &bl_mean, &bl_sample_size, &bl_var));
    ASSERT_EQ(12116, bl_sample_size);
    ASSERT_DOUBLE_EQ(0.00141797, bl_mean);
    ASSERT_DOUBLE_EQ(1.13E-06, bl_var);
    // PI 1 has no readings and only unit readings
    ASSERT_EQ(ERR_NOT_INIT, pilot_get_baseline(wl, 1, READING_TYPE, &bl_mean, &bl_sample_size, &bl_var));
    ASSERT_EQ(0, pilot_get_baseline(wl, 1, UNIT_READING_TYPE, &bl_mean, &bl_sample_size, &bl_var));
    ASSERT_EQ(12117, bl_sample_size);
    ASSERT_DOUBLE_EQ(0.00141797, bl_mean);
    ASSERT_DOUBLE_EQ(1.13E-06, bl_var);
    // PI 2 has its readings_num set to 0
    ASSERT_EQ(ERR_NOT_INIT, pilot_get_baseline(wl, 2, READING_TYPE, &bl_mean, &bl_sample_size, &bl_var));
    ASSERT_EQ(0, pilot_get_baseline(wl, 2, UNIT_READING_TYPE, &bl_mean, &bl_sample_size, &bl_var));
    ASSERT_EQ(12118, bl_sample_size);
    ASSERT_DOUBLE_EQ(0.00141796, bl_mean);
    ASSERT_DOUBLE_EQ(1.13E-06, bl_var);
    // PI 3 has only readings and no unit readings
    ASSERT_EQ(0, pilot_get_baseline(wl, 3, READING_TYPE, &bl_mean, &bl_sample_size, &bl_var));
    ASSERT_EQ(43, bl_sample_size);
    ASSERT_DOUBLE_EQ(666.66, bl_mean);
    ASSERT_DOUBLE_EQ(42.42, bl_var);
    ASSERT_EQ(ERR_NOT_INIT, pilot_get_baseline(wl, 3, UNIT_READING_TYPE, &bl_mean, &bl_sample_size, &bl_var));
    // PI 4 has only readings and unit_readings_num set to 0
    ASSERT_EQ(0, pilot_get_baseline(wl, 4, READING_TYPE, &bl_mean, &bl_sample_size, &bl_var));
    ASSERT_EQ(44, bl_sample_size);
    ASSERT_DOUBLE_EQ(666.66, bl_mean);
    ASSERT_DOUBLE_EQ(42.42, bl_var);
    ASSERT_EQ(ERR_NOT_INIT, pilot_get_baseline(wl, 4, UNIT_READING_TYPE, &bl_mean, &bl_sample_size, &bl_var));
}

int main(int argc, char **argv) {
    if (argc < 2) {
        cout << "Usage: " << argv[0] << " test_data_file" << endl;
        return 2;
    }
    g_test_data_file = argv[1];

    PILOT_LIB_SELF_CHECK;
    pilot_set_log_level(lv_fatal);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
