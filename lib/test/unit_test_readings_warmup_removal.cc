/*
 * unit_test_readings_warm-up_removal.cc: test cases for doing warm-up removal
 * on readings (i.e., when unit readings are not available)
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

#include "common.h"
#include "gtest/gtest.h"
#include <iomanip>
#include "libpilot.h"
#include <limits>
#include <vector>

using namespace pilot;
using namespace std;

static void prepare_data(const vector<size_t> &work_amounts,
                         const vector<nanosecond_type> &setup_durations,
                         const vector<nanosecond_type> &warmup_durations,
                         const vector<nanosecond_type> &cooldown_durations,
                         double warmup_v, double sp_v, double cooldown_v,
                         vector<nanosecond_type> &total_durations) {
    // v comes in as per second, need to convert them to per nanosecond
    warmup_v /= ONE_SECOND;
    sp_v /= ONE_SECOND;
    cooldown_v /= ONE_SECOND;

    vector<nanosecond_type> warmup_work_amounts(warmup_durations.size());
    transform(warmup_durations.begin(), warmup_durations.end(), warmup_work_amounts.begin(),
              [warmup_v] (nanosecond_type c) -> size_t { return warmup_v * c; });

    vector<nanosecond_type> cooldown_work_amounts(cooldown_durations.size());
    transform(cooldown_durations.begin(), cooldown_durations.end(), cooldown_work_amounts.begin(),
              [cooldown_v] (nanosecond_type c) -> size_t { return cooldown_v * c; });

    // calculate the sustainable phase (sp) work amounts and durations
    vector<size_t> sp_work_amounts(work_amounts.size());
    vector<nanosecond_type> sp_durations(work_amounts.size());
    total_durations.resize(work_amounts.size());
    for (size_t i = 0; i < work_amounts.size(); ++i) {
        sp_work_amounts[i] = work_amounts[i] - warmup_work_amounts[i] - cooldown_work_amounts[i];
        sp_durations[i] = sp_work_amounts[i] / sp_v;
        total_durations[i] = setup_durations[i] + warmup_durations[i] + sp_durations[i] + cooldown_durations[i];
    }
}

TEST(WarmUpRemoval, NotEnoughInputData) {
    const vector<size_t> work_amounts {50};
    const vector<nanosecond_type> total_durations {ONE_SECOND};
    double v = 1, ci_width = 1;
    ASSERT_EQ(ERR_NOT_ENOUGH_DATA,
              pilot_wps_warmup_removal_dw_method_p(work_amounts.size(), work_amounts.data(),
                                              total_durations.data(), .95, numeric_limits<double>::infinity(),
                                              &v, &ci_width));
    ASSERT_TRUE(v < 0) << "v should be set to a negative number";
    ASSERT_TRUE(ci_width < 0) << "ci_width should be set to a negative number";
}

TEST(WarmUpRemoval, ReadingsWithFixedSetupAndWarmupDurationIdenticalDiffWorkAmounts) {
    /* This is the simplest case: we use synthetic data with fixed setup and
     * warm-up phase duration to test
     */
    const vector<size_t> work_amounts {50, 100, 50, 100};
    const vector<nanosecond_type> setup_durations(work_amounts.size(), ONE_SECOND);
    const vector<nanosecond_type> warmup_durations(work_amounts.size(), 2*ONE_SECOND);
    const vector<nanosecond_type> cooldown_durations(work_amounts.size(), ONE_SECOND);

    // sustainable system performance (v)
    const double v = 1.5;
    vector<nanosecond_type> total_durations(work_amounts.size());
    // During warm-up phase the system performance (v) is tripled due to the
    // effect of cache. During cool-down phase, v lowers to 0.5v.
    prepare_data(work_amounts, setup_durations, warmup_durations,
                 cooldown_durations, 3*v, v, 0.5*v, total_durations);
    double calc_v, calc_ci_width;
    ASSERT_EQ(0,
              pilot_wps_warmup_removal_dw_method_p(work_amounts.size(), work_amounts.data(),
                                              total_durations.data(), .95, numeric_limits<double>::infinity(),
                                              &calc_v, &calc_ci_width));
    ASSERT_NEAR(v, calc_v, 0.00000001);
    ASSERT_DOUBLE_EQ(0, calc_ci_width);
}

TEST(WarmUpRemoval, ReadingsWithFixedSetupAndWarmupDurationChangingDiffWorkAmounts) {
    /* In this case we use different diff work amount (wa_{i} - wa_{i-1})
     */
    const vector<size_t> work_amounts {50, 100, 50, 100, 50, 80, 50, 80, 50, 100, 50, 100};
    const vector<nanosecond_type> setup_durations(work_amounts.size(), ONE_SECOND);
    const vector<nanosecond_type> warmup_durations(work_amounts.size(), 2*ONE_SECOND);
    const vector<nanosecond_type> cooldown_durations(work_amounts.size(), ONE_SECOND);

    // sustainable system performance (v)
    const double v = 1.5;
    vector<nanosecond_type> total_durations(work_amounts.size());
    // During warm-up phase the system performance (v) is tripled due to the
    // effect of cache. During cool-down phase, v lowers to 0.5v.
    prepare_data(work_amounts, setup_durations, warmup_durations,
                 cooldown_durations, 3*v, v, 0.5*v, total_durations);
    double calc_v, calc_ci_width;
    pilot_wps_warmup_removal_dw_method_p(work_amounts.size(), work_amounts.data(),
                                    total_durations.data(), .95, numeric_limits<double>::infinity(),
                                    &calc_v, &calc_ci_width);
    ASSERT_NEAR(v, calc_v, 0.00000001);
    ASSERT_DOUBLE_EQ(0, calc_ci_width);
}
