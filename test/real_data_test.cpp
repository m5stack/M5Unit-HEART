/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Native regression test with StampHeart real data
*/
#include <gtest/gtest.h>
#include <cmath>
#include <heart/pulse_processor.hpp>
#include "stampheart_finger_100hz.hpp"

using namespace m5::heart;

namespace {
constexpr size_t rows{sizeof(stampheart_finger_100hz) / sizeof(stampheart_finger_100hz[0])};
constexpr float expected_bpm{72.0f};
constexpr float tolerance_bpm{3.0f};
// Arbitrary scale of each channel to check that the unit of the values does not matter
constexpr float channel_scale[] = {0.43f, 1.07f, 2.9f};  // Green, Red, IR

PPGSample make_sample(const size_t i, const bool scaled)
{
    PPGSample s{};
    const Channel channels[] = {Channel::Green, Channel::Red, Channel::IR};
    for (const auto ch : channels) {
        const size_t idx = channel_index(ch);
        const float v    = static_cast<float>(stampheart_finger_100hz[i][idx]);
        s.set(ch, scaled ? v * channel_scale[idx] : v);
    }
    return s;
}
}  // namespace

TEST(RealData, StampHeartFinger)
{
    EXPECT_EQ(rows, 1511U);

    PulseProcessor proc(100.0f, 0x07);
    EXPECT_EQ(proc.heartRateChannel(), channel_index(Channel::Green));

    uint32_t beats{}, invalid_after_6s{};
    for (size_t i = 0; i < rows; ++i) {
        proc.push(make_sample(i, false));
        beats += proc.isBeat() ? 1 : 0;
        if (i >= 600 && !proc.hasBpm()) {
            ++invalid_after_6s;
        }
    }
    EXPECT_TRUE(proc.hasBpm());
    EXPECT_NEAR(proc.bpm(), expected_bpm, tolerance_bpm);
    EXPECT_NEAR(beats, 14U, 2U);
    EXPECT_EQ(invalid_after_6s, 0U);  // Stays valid once settled
    EXPECT_GT(proc.perfusionIndex(), 0.05f);
    EXPECT_LT(proc.perfusionIndex(), 20.0f);
}

TEST(RealData, UnitDoesNotMatter)
{
    PulseProcessor counts(100.0f, 0x07), scaled(100.0f, 0x07);
    for (size_t i = 0; i < rows; ++i) {
        counts.push(make_sample(i, false));
        scaled.push(make_sample(i, true));
    }
    EXPECT_TRUE(scaled.hasBpm());
    EXPECT_NEAR(scaled.bpm(), counts.bpm(), 0.01f);
    EXPECT_EQ(scaled.hasSpO2(), counts.hasSpO2());
    if (scaled.hasSpO2()) {
        EXPECT_NEAR(scaled.spo2(), counts.spo2(), 0.1f);
    }
}

TEST(RealData, OtherHeartRateChannels)
{
    const Channel channels[] = {Channel::Red, Channel::IR};
    for (const auto ch : channels) {
        SCOPED_TRACE(channel_index(ch));
        PulseProcessor::config_t cfg{};
        cfg.hr_channel = static_cast<int8_t>(channel_index(ch));
        PulseProcessor proc(100.0f, 0x07, cfg);
        for (size_t i = 0; i < rows; ++i) {
            proc.push(make_sample(i, false));
        }
        EXPECT_TRUE(proc.hasBpm());
        EXPECT_NEAR(proc.bpm(), expected_bpm, tolerance_bpm);
    }
}
