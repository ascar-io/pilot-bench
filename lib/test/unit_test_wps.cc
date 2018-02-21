/*
 * unit_test_wps.cc
 * Unit tests for WPS-related functions
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

#include "gtest/gtest.h"
#include <memory>
#include "pilot/libpilot.h"

using namespace pilot;
using namespace std;

nanosecond_type const ONE_SECOND = 1000000000LL;
size_t const g_max_work_amount = 2000;

int mock_workload_func(const pilot_workload_t *wl,
                       size_t round,
                       size_t total_work_amount,
                       pilot_malloc_func_t *lib_malloc_func,
                       size_t *num_of_work_unit,
                       double ***unit_readings,
                       double **readings,
                       nanosecond_type *round_duration, void *data) {
    if (g_max_work_amount == total_work_amount) {
        // make it a valid round when the work_amount == max_work_amount
        *round_duration = 2 * ONE_SECOND;
    }
    if (5 == round)
        return 1;
    else
        return 0;
}

TEST(WPSUnitTest, CornerCases) {
    pilot_set_log_level(lv_no_show);
    shared_ptr<pilot_workload_t> wl(pilot_new_workload("WPSUnitTest"), pilot_destroy_workload);
    // WPS is not possible when init_work_amount == max_work_amount
    pilot_set_init_work_amount(wl.get(), g_max_work_amount);
    pilot_set_work_amount_limit(wl.get(), g_max_work_amount);
    ASSERT_EQ(ERR_WRONG_PARAM, pilot_set_wps_analysis(wl.get(), NULL, true, true)); // This function needs to fail
    // We work around the safety check.
    pilot_set_init_work_amount(wl.get(), g_max_work_amount / 4);
    pilot_set_workload_func(wl.get(), mock_workload_func);
    pilot_set_wps_analysis(wl.get(), NULL, true, true);
    // Set short round threshold to 1 s. Our mock_work_func will not reach 1 s
    // until work_amount == max_work_amount. This renders WPS analysis
    // impossible.
    pilot_set_short_round_detection_threshold(wl.get(), 1);
    pilot_run_workload(wl.get());
    // Make sure WPS has no result
    shared_ptr<pilot_analytical_result_t> ar(pilot_analytical_result(wl.get(), NULL), pilot_free_analytical_result);
    ASSERT_EQ(false, ar->wps_has_data);
}

int main(int argc, char **argv) {
    PILOT_LIB_SELF_CHECK;

    // this does away a gtest warning message, and we don't care about execution time
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
