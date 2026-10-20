/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Native test for the SpO2 estimator
*/
#include <gtest/gtest.h>
#include <cmath>
#include <heart/spo2_estimator.hpp>

using namespace m5::heart;

TEST(SpO2Estimator, RatioToSpO2)
{
    EXPECT_FLOAT_EQ(ratio_to_spo2(0.4f), 100.0f);
    EXPECT_FLOAT_EQ(ratio_to_spo2(0.6f), 95.0f);
    EXPECT_FLOAT_EQ(ratio_to_spo2(1.0f), 85.0f);
    EXPECT_FLOAT_EQ(ratio_to_spo2(0.2f), 100.0f);  // Clamped
    EXPECT_FLOAT_EQ(ratio_to_spo2(5.0f), 0.0f);    // Clamped
    EXPECT_TRUE(std::isnan(ratio_to_spo2(std::nanf(""))));
}

TEST(SpO2Estimator, Ratio)
{
    const float ratios[] = {0.4f, 0.6f, 0.8f, 1.0f};
    for (const auto r : ratios) {
        SCOPED_TRACE(r);
        SpO2Estimator est{};
        for (int i = 0; i < 5; ++i) {
            // Red AC/DC = r * IR AC/DC, different DC levels and scales
            EXPECT_TRUE(est.addBeat(r * 0.02f * 3.0e5f, 3.0e5f, 0.02f * 8.0e6f, 8.0e6f));
        }
        EXPECT_NEAR(est.ratio(), r, 1e-4f);
        EXPECT_TRUE(est.isValid());
        EXPECT_NEAR(est.spo2(), ratio_to_spo2(r), 0.01f);
    }
}

TEST(SpO2Estimator, Median)
{
    SpO2Estimator est{};
    const float ratios[] = {0.60f, 0.62f, 0.95f, 0.61f, 0.63f};
    for (const auto r : ratios) {
        EXPECT_TRUE(est.addBeat(r, 1.0f, 1.0f, 1.0f));
    }
    EXPECT_NEAR(est.ratio(), 0.62f, 1e-5f);
    EXPECT_NEAR(est.ratioSpread(), 0.35f, 1e-5f);
    EXPECT_FALSE(est.isValid());  // Spread > 0.15
    EXPECT_TRUE(std::isnan(est.spo2()));

    // The outlier leaves the window of 5 beats
    EXPECT_TRUE(est.addBeat(0.62f, 1.0f, 1.0f, 1.0f));
    EXPECT_TRUE(est.addBeat(0.61f, 1.0f, 1.0f, 1.0f));
    EXPECT_TRUE(est.addBeat(0.60f, 1.0f, 1.0f, 1.0f));
    EXPECT_TRUE(est.isValid());
    EXPECT_NEAR(est.spo2(), ratio_to_spo2(0.61f), 0.01f);
}

TEST(SpO2Estimator, Validity)
{
    SpO2Estimator est{};
    EXPECT_TRUE(std::isnan(est.ratio()));
    EXPECT_TRUE(est.addBeat(0.5f, 1.0f, 1.0f, 1.0f));
    EXPECT_TRUE(est.addBeat(0.5f, 1.0f, 1.0f, 1.0f));
    EXPECT_FALSE(est.isValid());  // 2 beats
    EXPECT_TRUE(est.addBeat(0.5f, 1.0f, 1.0f, 1.0f));
    EXPECT_TRUE(est.isValid());

    // R out of 0.3...1.2
    SpO2Estimator low{}, high{};
    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(low.addBeat(0.2f, 1.0f, 1.0f, 1.0f));
        EXPECT_TRUE(high.addBeat(1.5f, 1.0f, 1.0f, 1.0f));
    }
    EXPECT_FALSE(low.isValid());
    EXPECT_FALSE(high.isValid());

    // Rejected inputs
    EXPECT_FALSE(est.addBeat(0.0f, 1.0f, 1.0f, 1.0f));
    EXPECT_FALSE(est.addBeat(0.5f, -1.0f, 1.0f, 1.0f));
    EXPECT_FALSE(est.addBeat(0.5f, 1.0f, std::nanf(""), 1.0f));
    EXPECT_EQ(est.count(), 3U);

    est.clear();
    EXPECT_EQ(est.count(), 0U);
    EXPECT_FALSE(est.isValid());
}

TEST(SpO2Estimator, RatioSpread)
{
    // Spread 0.2: valid by default (0.25), invalid if only 0.15 is allowed
    SpO2Estimator def{}, narrow{0.15f};
    EXPECT_FLOAT_EQ(def.ratioSpreadMax(), SPO2_RATIO_SPREAD_DEFAULT);
    EXPECT_FLOAT_EQ(narrow.ratioSpreadMax(), 0.15f);
    for (const float r : {0.5f, 0.6f, 0.7f}) {
        EXPECT_TRUE(def.addBeat(r, 1.0f, 1.0f, 1.0f));
        EXPECT_TRUE(narrow.addBeat(r, 1.0f, 1.0f, 1.0f));
    }
    EXPECT_NEAR(def.ratioSpread(), 0.2f, 1e-5f);
    EXPECT_TRUE(def.isValid());
    EXPECT_FALSE(narrow.isValid());
    EXPECT_NEAR(def.spo2(), ratio_to_spo2(0.6f), 0.01f);

    // Kept after clear()
    narrow.clear();
    EXPECT_FLOAT_EQ(narrow.ratioSpreadMax(), 0.15f);
}
