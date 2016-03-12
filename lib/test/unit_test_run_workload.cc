/*
 * unit_test_run_workload.cc: unit tests for pilot_run_workload()
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
#include <cmath>
#include <cstring>
#include "gtest/gtest.h"
#include "libpilot.h"
#include <vector>

using namespace pilot;
using namespace std;

static int g_test_round = 0;
static const int g_num_of_pi = 1;    //! TODO: need cases with more PIs
/**
 * g_mock_readings[piid][round]
 */
static const vector<vector<double> > g_mock_readings {
    {42, 43, 44}
};
/**
 * g_mock_unit_readings[round][piid][unit_id]
 */
static const vector<vector<vector<double> > > g_mock_unit_readings {
    {{1, 5, 10, 20, 30, 40, 42, 42, 42}},
    {{2, 4,  9, 21, 22, 41, 43, 41, 40}},
    {{1, 4, 12, 25, 32, 42, 42, 43, 41}}
};
static const int g_total_rounds = g_mock_readings[0].size();

int mock_workload_func(size_t total_work_amount,
                       pilot_malloc_func_t *lib_malloc_func,
                       size_t *num_of_work_unit,
                       double ***unit_readings,
                       double **readings) {
    if (0 == g_test_round)
        assert(500/10 == total_work_amount);
    *num_of_work_unit = g_mock_unit_readings[g_test_round][0].size();

    // store unit_readings
    assert(NULL == *unit_readings);
    *unit_readings = (double**)lib_malloc_func(sizeof(double*) * g_num_of_pi);
    for (int piid = 0; piid < g_num_of_pi; ++piid) {
        size_t memsize = sizeof(double) * g_mock_unit_readings[g_test_round][piid].size();
        (*unit_readings)[piid] = (double*)lib_malloc_func(memsize);
        memcpy((*unit_readings)[piid], g_mock_unit_readings[g_test_round][piid].data(), memsize);
    }

    // store readings
    assert(NULL == *readings);
    *readings = (double*)lib_malloc_func(sizeof(double*) * g_num_of_pi);
    for (int i = 0; i < g_num_of_pi; ++i)
        (*readings)[i] = g_mock_readings[i][g_test_round];

    ++g_test_round;
    return 0;
}

bool post_workload_hook(pilot_workload_t* wl) {
    return g_test_round < g_total_rounds;
}

TEST(PilotRunWorkloadTest, RunWorkload) {
    pilot_workload_t *wl = pilot_new_workload("Test workload");
    size_t num_of_pi;
    pilot_set_log_level(lv_fatal);
    ASSERT_EQ(ERR_NOT_INIT, pilot_get_num_of_pi(wl, &num_of_pi));
    pilot_set_log_level(lv_warning);
    pilot_set_num_of_pi(wl, 1);
    ASSERT_EQ(0, pilot_get_num_of_pi(wl, &num_of_pi));
    ASSERT_EQ(size_t(1), num_of_pi);
    pilot_set_short_round_detection_threshold(wl, 0);
    pilot_set_warm_up_removal_method(wl, NO_WARM_UP_REMOVAL);

    // Limit the write to 500 MB
    pilot_set_work_amount_limit(wl, 500);
    size_t work_amount_limit;
    ASSERT_EQ(0, pilot_get_work_amount_limit(wl, &work_amount_limit));
    ASSERT_EQ(size_t(500), work_amount_limit);

    pilot_set_workload_func(wl, &mock_workload_func);
    pilot_set_hook_func(wl, POST_WORKLOAD_RUN, &post_workload_hook);

    ASSERT_EQ(ERR_STOPPED_BY_HOOK, pilot_run_workload(wl));
    ASSERT_EQ(g_total_rounds, pilot_get_num_of_rounds(wl));
    size_t num_of_work_units;
    const double *pi_unit_readings = pilot_get_pi_unit_readings(wl, 0, 0, &num_of_work_units);
    ASSERT_EQ(g_mock_unit_readings[0][0].size(), num_of_work_units);
    ASSERT_EQ(0, memcmp(g_mock_unit_readings[0][0].data(), pi_unit_readings,
                        sizeof(double) * g_mock_unit_readings[0][0].size()));

    // Test analytical result
    pilot_analytical_result_t *ar1 = pilot_analytical_result(wl);
    ASSERT_EQ(0, pilot_export(wl, "/tmp/unit_test_run_workload_export"));
    pilot_analytical_result_t *ar2 = pilot_analytical_result(wl);
    // since there's no new raw data comes in between getting ar1, exporting
    // to files, and getting ar2, their update_time should be identical.
    ASSERT_EQ(ar1->update_time, ar2->update_time);
    ASSERT_EQ(g_num_of_pi, ar1->num_of_pi);
    ASSERT_EQ(g_total_rounds, ar1->num_of_rounds);
    for (int pi = 0; pi < g_num_of_pi; ++pi) {
        ASSERT_EQ(0, memcmp(g_mock_readings[pi].data(), pilot_get_pi_readings(wl, pi),
                            sizeof(double) * g_mock_readings[pi].size()));
        ASSERT_EQ(g_total_rounds, ar1->readings_num[pi]);
        double exp_readings_sum = 0;
        for (int round = 0; round < g_total_rounds; ++round) {
            exp_readings_sum += g_mock_readings[pi][round];
        }
        ASSERT_EQ(exp_readings_sum / g_total_rounds, ar1->readings_mean[pi]);
        ASSERT_EQ(ARITHMETIC_MEAN, ar1->readings_mean_method[0]);
        double exp_readings_var = 0;
        for (int round = 0; round < g_total_rounds; ++round) {
            exp_readings_var += pow(g_mock_readings[pi][round] - ar1->readings_mean[pi], 2);
        }
        ASSERT_EQ(exp_readings_var / (g_total_rounds-1), ar1->readings_var[pi]);
        ASSERT_EQ(0, ar1->readings_autocorrelation_coefficient[pi]);
        ASSERT_EQ(1, ar1->readings_optimal_subsession_size[pi]);
        ASSERT_EQ(exp_readings_var / (g_total_rounds-1), ar1->readings_optimal_subsession_var[pi]);
        ASSERT_EQ(0, ar1->readings_optimal_subsession_autocorrelation_coefficient[pi]);
        ASSERT_DOUBLE_EQ(4.9682754235006596, ar1->readings_optimal_subsession_ci_width[pi]);
        ASSERT_EQ(5, ar1->readings_required_sample_size[pi]);

        size_t exp_ur_num = 0;
        double exp_ur_sum = 0;
        double exp_ur_var = 0;
        for (int round = 0; round < g_total_rounds; ++round) {
            exp_ur_num += g_mock_unit_readings[round][pi].size();
            for (size_t k = 0; k < g_mock_unit_readings[round][pi].size(); ++k) {
                exp_ur_sum += g_mock_unit_readings[round][pi][k];
            }
        }
        ASSERT_EQ(exp_ur_num, ar1->unit_readings_num[pi]);
        ASSERT_EQ(exp_ur_sum / exp_ur_num, ar1->unit_readings_mean[pi]);
        ASSERT_EQ(ARITHMETIC_MEAN, ar1->unit_readings_mean_method[0]);
        for (int round = 0; round < g_total_rounds; ++round) {
            for (size_t k = 0; k < g_mock_unit_readings[round][pi].size(); ++k) {
                exp_ur_var += pow(g_mock_unit_readings[round][pi][k] - ar1->unit_readings_mean[pi], 2);
            }
        }
        ASSERT_EQ(exp_ur_var / (exp_ur_num-1), ar1->unit_readings_var[pi]);
        ASSERT_DOUBLE_EQ(0.62643602129514497, ar1->unit_readings_autocorrelation_coefficient[pi]);
        ASSERT_EQ(-1, ar1->unit_readings_optimal_subsession_size[pi]);
        // we don't have enough data for calculating the required sample size
        ASSERT_EQ(-1, ar1->unit_readings_required_sample_size[pi]);
    }

    pilot_free_analytical_result(ar1);
    pilot_free_analytical_result(ar2);

    pilot_destroy_workload(wl);
}

TEST(PilotRunWorkloadTest, ChangingNumberOfReadings) {
    //! TODO: Changing number of readings after running the first round of workload is not allowed.
}

int main(int argc, char **argv) {
    PILOT_LIB_SELF_CHECK;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
