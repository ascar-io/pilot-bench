/*
 * unit_test_readings_warm-up_removal.cc: test cases for doing warm-up removal
 * on readings (i.e., when unit readings are not available)
 *
 * Copyright (c) 2017-2019 Yan Li <yanli@tuneup.ai>. All rights reserved.
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

#include "common.h"
#include "gtest/gtest.h"
#include <iomanip>
#include "pilot/libpilot.h"
#include <limits>
#include <vector>

using namespace pilot;
using namespace std;

/**
 * Calculate the total duration of rounds given speeds and durations of various non-stable phases
 * @param work_amounts
 * @param setup_durations
 * @param warmup_durations
 * @param cooldown_durations
 * @param warmup_v
 * @param sp_v
 * @param cooldown_v
 * @param total_durations
 */
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
    double alpha, v, ci_width;
    ASSERT_EQ(ERR_NOT_ENOUGH_DATA,
              pilot_wps_warmup_removal_lr_method_p(work_amounts.size(), work_amounts.data(),
                                              total_durations.data(), 1, 0,
                                              &alpha, &v, &ci_width));
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
    double calc_alpha, calc_v, calc_ci_width;
    ASSERT_EQ(0,
              pilot_wps_warmup_removal_lr_method_p(work_amounts.size(), work_amounts.data(),
                                              total_durations.data(), 1, 0,
                                              &calc_alpha, &calc_v, &calc_ci_width));
    ASSERT_NEAR(v, calc_v, 0.00000001);
    ASSERT_NEAR(0, calc_ci_width, 1e-5);
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
    double calc_alpha, calc_v, calc_ci_width;
    pilot_wps_warmup_removal_lr_method_p(work_amounts.size(), work_amounts.data(),
                                    total_durations.data(), 1, 0,
                                    &calc_alpha, &calc_v, &calc_ci_width);
    ASSERT_NEAR(v, calc_v, 0.00000001);
    ASSERT_NEAR(0, calc_ci_width, 1e-5);
}
