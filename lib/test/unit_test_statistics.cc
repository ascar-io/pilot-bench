/*
 * unit_test_statistics.cc: unit tests for statistics routines
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
#include <fstream>
#include "gtest/gtest.h"
#include "libpilot.h"
#include <vector>

using namespace pilot;
using namespace std;

/**
 * \details These sample response time are taken from [Ferrari78], page 79.
 */
const vector<double> g_response_time{
    1.21, 1.67, 1.71, 1.53, 2.03, 2.15, 1.88, 2.02, 1.75, 1.84, 1.61, 1.35, 1.43, 1.64, 1.52, 1.44, 1.17, 1.42, 1.64, 1.86, 1.68, 1.91, 1.73, 2.18,
    2.27, 1.93, 2.19, 2.04, 1.92, 1.97, 1.65, 1.71, 1.89, 1.70, 1.62, 1.48, 1.55, 1.39, 1.45, 1.67, 1.62, 1.77, 1.88, 1.82, 1.93, 2.09, 2.24, 2.16
};

TEST(StatisticsUnitTest, CornerCases) {
    ASSERT_DEATH(pilot_subsession_auto_cov_p(g_response_time.data(), 1, 1, 0, ARITHMETIC_MEAN), "") << "Shouldn't be able to calculate covariance for one sample";
    ASSERT_DEATH(pilot_optimal_subsession_size_p(g_response_time.data(), 1, ARITHMETIC_MEAN), "") << "Shouldn't be able to calculate optimal subsession size for one sample";
}

TEST(StatisticsUnitTest, AutocorrelationCoefficient) {
    double sample_mean = pilot_subsession_mean_p(g_response_time.data(), g_response_time.size(), ARITHMETIC_MEAN);
    ASSERT_DOUBLE_EQ(1.756458333333333, sample_mean) << "Mean is wrong";

    ASSERT_DOUBLE_EQ(0.073474423758865273, pilot_subsession_var_p(g_response_time.data(), g_response_time.size(), 1, sample_mean, ARITHMETIC_MEAN)) << "Subsession mean is wrong";
    ASSERT_DOUBLE_EQ(0.046770566452423196, pilot_subsession_auto_cov_p(g_response_time.data(), g_response_time.size(), 1, sample_mean, ARITHMETIC_MEAN)) << "Coverance mean is wrong";
    ASSERT_DOUBLE_EQ(0.63655574361384437, pilot_subsession_autocorrelation_coefficient_p(g_response_time.data(), g_response_time.size(), 1, sample_mean, ARITHMETIC_MEAN)) << "Autocorrelation coefficient is wrong";

    ASSERT_DOUBLE_EQ(0.55892351761172487, pilot_subsession_autocorrelation_coefficient_p(g_response_time.data(), g_response_time.size(), 2, sample_mean, ARITHMETIC_MEAN)) << "Autocorrelation coefficient is wrong";

    ASSERT_DOUBLE_EQ(0.05264711174242424, pilot_subsession_var_p(g_response_time.data(), g_response_time.size(), 4, sample_mean, ARITHMETIC_MEAN)) << "Subsession var is wrong";
    ASSERT_DOUBLE_EQ(0.08230986644266707, pilot_subsession_autocorrelation_coefficient_p(g_response_time.data(), g_response_time.size(), 4, sample_mean, ARITHMETIC_MEAN)) << "Autocorrelation coefficient is wrong";

    ASSERT_DOUBLE_EQ(0.29157062128900485, pilot_subsession_confidence_interval_p(g_response_time.data(), g_response_time.size(), 4, .95, ARITHMETIC_MEAN));

    size_t q = 4;
    ASSERT_DOUBLE_EQ(q, pilot_optimal_subsession_size_p(g_response_time.data(), g_response_time.size(), ARITHMETIC_MEAN));

    size_t opt_sample_size;
    ASSERT_EQ(true, pilot_optimal_sample_size_p(g_response_time.data(), g_response_time.size(), sample_mean * 0.1, ARITHMETIC_MEAN, &q, &opt_sample_size, .95, .1));
    ASSERT_EQ(4, q);
    ASSERT_EQ(34, opt_sample_size);

    //! TODO Tests function pilot_est_sample_var_dist_unknown()
}

TEST(StatisticsUnitTest, HarmonicMean) {
    const vector<double> d {1.21, 1.67, 1.71, 1.53, 2.03, 2.15};
    double hm = pilot_subsession_mean_p(d.data(), d.size(), HARMONIC_MEAN);
    ASSERT_DOUBLE_EQ(1.6568334130160711, hm);
}

TEST(StatisticsUnitTest, OrdinaryLeastSquareLinearRegression1) {
    const double exp_alpha = 42;
    const double exp_v = 0.5;
    const vector<size_t> work_amount{50, 100, 150, 200, 250};
    const vector<double> error{20, -9, -18, -25, 30};
    vector<nanosecond_type> t;
    auto p_error = error.begin();
    double exp_ssr = 0;
    for_each(error.begin(), error.end(), [&exp_ssr](double e) {exp_ssr += e*e;});
    for (size_t c : work_amount) {
        t.push_back((1.0 / exp_v) * c + exp_alpha + *(p_error++));
    }
    double alpha, v, v_ci, ssr;
    pilot_wps_warmup_removal_lr_method_p(work_amount.size(),
        work_amount.data(), t.data(),
        1,  // autocorrelation_coefficient_limit
        0,  // duration threshold
        &alpha, &v, &v_ci, &ssr);
    ASSERT_NEAR(exp_ssr, ssr, 10);
    ASSERT_NEAR(44, alpha, 4);
    ASSERT_NEAR(exp_v, v, 0.1);
    ASSERT_NEAR(0.1803, v_ci, 0.01);
}

TEST(StatisticsUnitTest, OrdinaryLeastSquareLinearRegression2) {
    // We generate some mock test data
    const size_t v_wu = 500;
    const size_t v_s  = 30;
    const size_t v_td = 15;
    const nanosecond_type t_su = 10;
    const nanosecond_type t_wu = 100;
    const nanosecond_type t_td = 30;
    vector<size_t> work_amount;
    vector<nanosecond_type> round_duration;
    ofstream of("output.csv");
    of << "work amount,round_duration" << endl;
    for(nanosecond_type t_s = 0; t_s < 2000; t_s += 50) {
        work_amount.push_back(v_wu * t_wu + v_s * t_s + v_td * t_td);
        round_duration.push_back(t_su + t_wu + t_td + t_s);
        of << work_amount.back() << "," << round_duration.back() << endl;
    }
    of.close();

    double alpha, v, v_ci, ssr;
    pilot_wps_warmup_removal_lr_method_p(work_amount.size(),
        work_amount.data(), round_duration.data(),
        1,  // autocorrelation_coefficient_limit
        0,  // duration threshold
        &alpha, &v, &v_ci, &ssr);
    ASSERT_NEAR(0, ssr, .001);
    ASSERT_NEAR(-1541.7, alpha, .1);
    ASSERT_NEAR(v_s, v, .1);
    ASSERT_NEAR(0, v_ci, .001);
}

TEST(StatisticsUnitTest, OrdinaryLeastSquareLinearRegression3) {
    // Real data test A
    const vector<size_t> work_amount{429497000, 472446000, 515396000, 558346000};
    const vector<nanosecond_type> round_duration{4681140000, 5526190000, 5632120000, 5611980000};
    double alpha, v, v_ci, ssr;
    pilot_wps_warmup_removal_lr_method_p(work_amount.size(),
        work_amount.data(), round_duration.data(),
        1,  // autocorrelation_coefficient_limit
        0,  // duration threshold
        &alpha, &v, &v_ci, &ssr);
    ASSERT_NEAR(2.059332e+17, ssr, 0.001e+17);
    ASSERT_NEAR(2.0296e+9, alpha, .0001e+9);
    ASSERT_NEAR(1.0/6.7485, v, .001);
    ASSERT_DOUBLE_EQ(15.068212467990975, v_ci);
}

TEST(StatisticsUnitTest, OrdinaryLeastSquareLinearRegression4) {
    // Real data test B
    const vector<size_t> work_amount{429496729, 472446392, 515396064, 558345736, 601295408, 644245080, 687194752, 730144424, 773094096};
    const vector<nanosecond_type> round_duration{5731883327, 5235129386, 5321265550, 5860121124, 6040418744, 6513983890, 6623204911, 6828709974, 7455453108};
    double alpha, v, v_ci, ssr;
    pilot_wps_warmup_removal_lr_method_p(work_amount.size(),
        work_amount.data(), round_duration.data(),
        1,  // autocorrelation_coefficient_limit
        0,  // duration threshold
        &alpha, &v, &v_ci, &ssr);
    ASSERT_DOUBLE_EQ(5.9193307073836864e+17, ssr);
    ASSERT_NEAR(2694596476, alpha, 1);
    ASSERT_NEAR(1.0/5.7947, v, .001);
    ASSERT_DOUBLE_EQ(0.11455029540768602, v_ci);
}

TEST(StatisticsUnitTest, TestOfSignificance) {
    // Sample data from http://www.stat.yale.edu/Courses/1997-98/101/meancomp.htm.
    double mean_male = 98.105;
    double mean_female = 98.394;
    double var_male = pow(0.699, 2);
    double var_female = pow(0.743, 2);
    double ci_left, ci_right;
    size_t sample_size_male = 65;
    size_t sample_size_female = 65;
    double p = pilot_p_eq(mean_male, mean_female,
                          sample_size_male, sample_size_female,
                          var_male, var_female,
                          &ci_left, &ci_right);
    ASSERT_NEAR(0.025696808408668472, p, 0.000000001);
    ASSERT_NEAR(-0.5417739890521083, ci_left, 0.000000001);
    ASSERT_NEAR(-0.03622601094789468, ci_right, 0.000000001);
}

int main(int argc, char **argv) {
    PILOT_LIB_SELF_CHECK;
    // we only display fatals because errors are expected in some test cases
    pilot_set_log_level(lv_fatal);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
