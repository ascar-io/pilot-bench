/*
 * unit_test_session_duration.cc: unit tests for calculating the optimal
 * session duration
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

#include <algorithm>
#include <cstring>
#include "gtest/gtest.h"
#include "libpilot.h"
#include <vector>
#include "workload.hpp"

using namespace pilot;
using namespace std;

static size_t g_test_round = 0;
static size_t g_work_amount_limit;
static const int g_num_of_pi = 1;
static vector<int> *g_expected_work_amount_per_round = NULL;
static vector<int> *g_required_sample_size_per_round = NULL;

int mock_workload_func(size_t work_amount,
                       pilot_malloc_func_t *lib_malloc_func,
                       size_t *num_of_work_unit,
                       double ***unit_readings,
                       double **readings) {
    assert(g_work_amount_limit >= work_amount);
    *num_of_work_unit = work_amount;

    // store unit_readings
    *unit_readings = (double**)lib_malloc_func(sizeof(double*) * g_num_of_pi);
    for (int piid = 0; piid < g_num_of_pi; ++piid) {
        size_t memsize = sizeof(double) * work_amount;
        (*unit_readings)[piid] = (double*)lib_malloc_func(memsize);
        // the readings here are irrelevant so we just fill in some number
        for (size_t i = 0; i < work_amount; ++i)
            (*unit_readings)[piid][i] = 42.42;
    }

    // store readings
    assert(NULL == *readings);
    *readings = (double*)lib_malloc_func(sizeof(double*) * g_num_of_pi);
    for (int i = 0; i < g_num_of_pi; ++i)
        (*readings)[i] = 42.42;

    return 0;
}

ssize_t mock_calc_required_ur_func(const pilot_workload_t* wl, int piid) {
    if (g_test_round >= g_required_sample_size_per_round->size()) {
        cerr << __func__ << "(): more rounds than expected" << endl;
        abort();
    }
    return (*g_required_sample_size_per_round)[g_test_round++];
}

void test_opt_session_duration(size_t work_amount_limit,
                               vector<int> expected_work_amount_per_round,
                               vector<int> required_sample_size_per_round) {
    pilot_workload_t *wl = pilot_new_workload("Test workload");
    pilot_set_short_workload_check(wl, false);
    pilot_set_workload_func(wl, &mock_workload_func);
    pilot_set_work_amount_limit(wl, work_amount_limit);
    pilot_set_num_of_pi(wl, 1);
    pilot_set_calc_required_unit_readings_func(wl, &mock_calc_required_ur_func);
    pilot_set_wps_analysis(wl, false);
    pilot_set_short_round_detection_threshold(wl, 0);
    pilot_set_warm_up_removal_method(wl, NO_WARM_UP_REMOVAL);

    g_expected_work_amount_per_round = &expected_work_amount_per_round;
    g_required_sample_size_per_round = &required_sample_size_per_round;
    g_work_amount_limit = work_amount_limit;
    g_test_round = 0;
    int res = pilot_run_workload(wl);
    if (0 != res) {
        cerr << "pilot_run_workload() returned " << res << endl;
        abort();
    }
    if (g_test_round != g_required_sample_size_per_round->size()) {
        cerr << __func__ << "(): not enough rounds" << endl;
        abort();
    }
    pilot_destroy_workload(wl);
}

TEST(PilotRunWorkloadTest, CalculatingOptimalSessionDuration) {
    // Tests if pilot_run_workload() can correctly assign work_amount for each
    // round. In the following case, the work_amount_limit is set to 50. The
    // first round's work amount is expected to be
    // expected_work_amount_per_round[0]=5 (1/10 of work_amount_limit). Our
    // mock workload func would return 5 samples (sample to work amount ratio
    // is 1:1 here.) The test case uses the calc_readings_required_func hook to tell
    // pilot_run_workload() that a total of 15 samples are required (as in
    // required_sample_size_per_round[0]). Now we have 5 samples so 10 more samples
    // are required. We expect the next round to have 10*(1+.2)=12 samples (as in
    // expected_work_amount_per_round[1], .2 is the ratio of safety buffer).
    pilot_set_log_level(lv_warning);
    test_opt_session_duration(
            50,          // work_amount_limit
            { 5, 12},    // expected_work_amount_per_round
            {15, 15});   // required_sample_size_per_round
    //! TODO: a test case whose average num of work units per work amount != 1
    //! TODO: a test case for multiple PIs that each of which has a different work_unit_per_amount ratio
}

TEST(PilotRunWorkloadTest, TestCalcNextRoundWorkAmountFromWPS) {
    pilot_workload_t *wl = pilot_new_workload("Test workload");
    size_t wa_limit = 1000;
    size_t wa_slice_size = wa_limit / kWPSInitSlices;
    pilot_set_work_amount_limit(wl, wa_limit);
    ASSERT_EQ(wa_slice_size, calc_next_round_work_amount_from_wps(wl));
    // let's add some mock data
    wl->round_work_amounts_.push_back(365);
    wl->round_durations_.push_back(1);
    wl->rounds_++;
    ASSERT_EQ((365 / wa_slice_size + 1) * wa_slice_size, calc_next_round_work_amount_from_wps(wl));
    wl->round_work_amounts_.push_back(999);
    wl->round_durations_.push_back(1);
    wl->rounds_++;
    wa_slice_size /= 2;
    ASSERT_EQ(wa_slice_size, calc_next_round_work_amount_from_wps(wl));
    wl->round_work_amounts_.push_back(wa_slice_size);
    wl->round_durations_.push_back(1);
    wl->rounds_++;
    ASSERT_EQ(wa_slice_size * 2, calc_next_round_work_amount_from_wps(wl));
    pilot_destroy_workload(wl);
}

int main(int argc, char **argv) {
    PILOT_LIB_SELF_CHECK;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
