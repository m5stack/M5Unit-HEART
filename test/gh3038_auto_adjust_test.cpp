/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Native test for the decision of the GH3038 auto adjustment
*/
#include <gtest/gtest.h>
#include <unit/gh3038_auto_adjust.hpp>

using namespace m5::unit::gh3038;

namespace {
AdjustInput make_input(const float mean, const float max, const uint8_t code, const uint8_t gain)
{
    AdjustInput in{};
    in.mean           = mean;
    in.max            = max;
    in.full_scale     = 1000.0f;
    in.code           = code;
    in.gain           = gain;
    in.max_code       = 64;
    in.max_gain       = 3;
    in.raw_ratio_down = 0.4f;
    in.raw_ratio_up   = 2.5f;
    return in;
}
}  // namespace

TEST(GH3038AutoAdjust, InRangeKeeps)
{
    const auto in  = make_input(500.0f, 700.0f, 64, 3);
    const auto out = decide_adjustment(in);
    EXPECT_FALSE(out.changed(in));
}

TEST(GH3038AutoAdjust, SaturatedLowersCurrent)
{
    const auto in  = make_input(960.0f, 990.0f, 64, 3);
    const auto out = decide_adjustment(in);
    EXPECT_EQ(out.code, 33);  // 64 * 500 / 960 = 33.3
    EXPECT_EQ(out.gain, 3);
}

TEST(GH3038AutoAdjust, BelowMinCodeLowersGain)
{
    const auto in  = make_input(990.0f, 999.0f, 10, 3);
    const auto out = decide_adjustment(in);
    EXPECT_EQ(out.gain, 2);
    EXPECT_EQ(out.code, 13);  // 10 * 500 / 990 = 5.05, / 0.4 = 12.6
}

TEST(GH3038AutoAdjust, FloorKeeps)
{
    auto in           = make_input(990.0f, 999.0f, ADJUST_MIN_CODE, 0);
    in.raw_ratio_down = 0.0f;
    const auto out    = decide_adjustment(in);
    EXPECT_FALSE(out.changed(in));
}

TEST(GH3038AutoAdjust, DarkRaisesCurrentUpToLimit)
{
    const auto in  = make_input(100.0f, 150.0f, 20, 3);
    const auto out = decide_adjustment(in);
    EXPECT_EQ(out.code, 64);  // 20 * 500 / 100 = 100, limited to max_code
    EXPECT_EQ(out.gain, 3);   // Already max_gain
}

TEST(GH3038AutoAdjust, DarkRaisesGainBelowLimit)
{
    const auto in  = make_input(200.0f, 260.0f, 40, 2);
    const auto out = decide_adjustment(in);
    EXPECT_EQ(out.gain, 3);
    EXPECT_EQ(out.code, 40);  // 40 * 500 / 200 = 100 > 64, / 2.5 = 40
}

TEST(GH3038AutoAdjust, NoFingerAtLimitsKeeps)
{
    const auto in  = make_input(0.0f, 0.0f, 64, 3);
    const auto out = decide_adjustment(in);
    EXPECT_FALSE(out.changed(in));
}

TEST(GH3038AutoAdjust, NoFingerReturnsToLimits)
{
    const auto in  = make_input(0.0f, 0.0f, 20, 2);
    const auto out = decide_adjustment(in);
    EXPECT_EQ(out.gain, 3);
    EXPECT_EQ(out.code, 64);
}

TEST(GH3038AutoAdjust, ZeroCurrentConfigKeeps)
{
    auto in        = make_input(0.0f, 0.0f, 0, 3);
    in.max_code    = 0;
    const auto out = decide_adjustment(in);
    EXPECT_FALSE(out.changed(in));
}

TEST(GH3038AutoAdjust, InvalidFullScaleKeeps)
{
    auto in        = make_input(990.0f, 999.0f, 64, 3);
    in.full_scale  = 0.0f;
    const auto out = decide_adjustment(in);
    EXPECT_FALSE(out.changed(in));
}
