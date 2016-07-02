/*
 * unit_test_unit_readings_iter.cc: unit tests for the unit readings
 * access iterator
 *
 * Copyright (c) 2015, 2016, University of California, Santa Cruz, CA, USA.
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

#include <algorithm>
#include <cstring>
#include "gtest/gtest.h"
#include "pilot/libpilot.h"
#include <vector>

using namespace pilot;
using namespace std;

void assert_eq(const vector<double> &exp, pilot_pi_unit_readings_iter_t *iter,
               const string &error_msg) {
    auto exp_iter = exp.cbegin();
    while (pilot_pi_unit_readings_iter_valid(iter)) {
        if (exp_iter == exp.cend()) {
            cerr << error_msg << endl
                 << "unit reading iter has more data than expected, the iter currently points to "
                 << pilot_pi_unit_readings_iter_get_val(iter) << endl;
            abort();
        }
        if (*exp_iter != pilot_pi_unit_readings_iter_get_val(iter)) {
            cerr << error_msg << endl
                 << "unit reading iter returned wrong data, exp: "
                 << *exp_iter << ", actual: "
                 << pilot_pi_unit_readings_iter_get_val(iter) << endl;
            abort();
        }
        pilot_pi_unit_readings_iter_next(iter);
        ++exp_iter;
    }
    if (exp_iter != exp.cend()) {
        cerr << error_msg << endl
             << "unit reading iter has fewer readings than expected, next unit reading should be "
             << *exp_iter << endl;
        abort();
    }
}

TEST(PilotUnitReadingsIterTest, ImportingAndIterating) {
    const int total_rounds = 2;
    const int num_of_pi = 1;    //! TODO: need cases with more PIs
    const int unit_readings_per_round = 9;
    // mock_unit_readings[round][piid][unit_id]
    const double _mock_ur_data[total_rounds][num_of_pi][unit_readings_per_round] = {
        {{1, 5, 10, 20, 30, 40, 42, 43, 41}},
        {{2, 4, 11, 19, 31, 39, 41, 42, 43}}
    };
    // awkwardly convert multi-dimentional array to double**
    const double *mock_ur_rounds[total_rounds][num_of_pi] = {
        {_mock_ur_data[0][0]}, {_mock_ur_data[1][0]}
    };

    // mock_readings[round][piid]
    const vector<vector<double> > mock_readings {
        {42}, {41}
    };

    pilot_workload_t *wl = pilot_new_workload("Test Importing and Iterating");
    pilot_set_work_amount_limit(wl, 4200);
    pilot_set_num_of_pi(wl, 1);
    // remove the first one tenth of unit readings
    pilot_set_warm_up_removal_method(wl, FIXED_PERCENTAGE);
    // disable short round detection
    pilot_set_short_round_detection_threshold(wl, 0);

    // test empty iterator
    pilot_pi_unit_readings_iter_t *iter;
    iter = pilot_pi_unit_readings_iter_new(wl, 0);
    ASSERT_FALSE(pilot_pi_unit_readings_iter_valid(iter));
    pilot_pi_unit_readings_iter_destroy(iter);
    ASSERT_EQ(size_t(0), pilot_get_total_num_of_unit_readings(wl, 0));

    // now let's import some test data
    for (int round = 0; round < total_rounds; ++round)
        pilot_import_benchmark_results(wl, round, unit_readings_per_round, 0,
                                       mock_readings[round].data(),
                                       unit_readings_per_round,
                                       mock_ur_rounds[round]);

    ASSERT_EQ(size_t((unit_readings_per_round - 1) * 2), pilot_get_total_num_of_unit_readings(wl, 0));
    iter = pilot_pi_unit_readings_iter_new(wl, 0);
    assert_eq({5, 10, 20, 30, 40, 42, 43, 41, 4, 11, 19, 31, 39, 41, 42, 43}, iter, "first iter wrong");
    pilot_pi_unit_readings_iter_destroy(iter);

    // replace the first round with a list with more unit readings
    const double _new_data_a[] = {1, 5, 10, 20, 30, 40, 42, 43, 41, 42, 43};
    // awkwardly convert multi-dimentional array to double**
    const double *new_data_a[] = {_new_data_a};
    pilot_import_benchmark_results(wl, 0, sizeof(new_data_a)/sizeof(double), 0,
                                   (const double[]){42.0},
                                   sizeof(_new_data_a)/sizeof(double),
                                   new_data_a);
    ASSERT_EQ(size_t((11 - 1) + (unit_readings_per_round - 1)), pilot_get_total_num_of_unit_readings(wl, 0));
    iter = pilot_pi_unit_readings_iter_new(wl, 0);
    assert_eq({5, 10, 20, 30, 40, 42, 43, 41, 42, 43, 4, 11, 19, 31, 39, 41, 42, 43}, iter, "second iter wrong");
    pilot_pi_unit_readings_iter_destroy(iter);

    // replace the first round with a list with fewer unit readings
    const double _new_data_b[] = {1, 5, 10, 20, 30, 40, 42};
    // awkwardly convert multi-dimentional array to double**
    const double *new_data_b[] = {_new_data_b};
    pilot_import_benchmark_results(wl, 0, sizeof(new_data_b)/sizeof(double), 0,
                                   (const double[]){42.0},
                                   sizeof(_new_data_b)/sizeof(double),
                                   new_data_b);
    ASSERT_EQ(size_t(14), pilot_get_total_num_of_unit_readings(wl, 0));
    iter = pilot_pi_unit_readings_iter_new(wl, 0);
    assert_eq({5, 10, 20, 30, 40, 42, 4, 11, 19, 31, 39, 41, 42, 43}, iter, "third iter wrong");
    pilot_pi_unit_readings_iter_destroy(iter);

    // replace the first round with empty unit_readings
    pilot_import_benchmark_results(wl, 0, 42, 0,
                                   (const double[]){42.0},
                                   0,
                                   NULL);
    ASSERT_EQ(size_t(8), pilot_get_total_num_of_unit_readings(wl, 0));
    iter = pilot_pi_unit_readings_iter_new(wl, 0);
    assert_eq({4, 11, 19, 31, 39, 41, 42, 43}, iter, "fourth iter wrong");
    pilot_pi_unit_readings_iter_destroy(iter);

    pilot_destroy_workload(wl);
}

int main(int argc, char **argv) {
    PILOT_LIB_SELF_CHECK;
    pilot_set_log_level(lv_fatal);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
