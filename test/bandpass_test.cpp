/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Native test for the band-pass filter
*/
#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <heart/filter/bandpass.hpp>

using namespace m5::heart;

namespace {
// Steady-state gain of the 0.5-4 Hz band-pass for a sine on a large DC level
float measure_gain(const float freq_hz, const float rate_hz)
{
    const float pi{3.14159265f};
    const float dc{1.0e6f};
    const float amplitude{1.0e3f};
    const uint32_t total  = static_cast<uint32_t>(60.0f * rate_hz);
    const uint32_t settle = static_cast<uint32_t>(40.0f * rate_hz);

    BandPass bp(0.5f, 4.0f, rate_hz);
    float peak{};
    for (uint32_t i = 0; i < total; ++i) {
        const float x = dc + amplitude * std::sin(2.0f * pi * freq_hz * i / rate_hz);
        const float y = bp.process(x);
        if (i >= settle) {
            peak = std::max(peak, std::fabs(y));
        }
    }
    return peak / amplitude;
}
}  // namespace

TEST(BandPass, Attenuation100Hz)
{
    EXPECT_LT(measure_gain(0.2f, 100.0f), 0.2f);   // Below the band (theory: 0.16)
    EXPECT_GT(measure_gain(1.2f, 100.0f), 0.95f);  // In the band (theory: 0.98)
    EXPECT_LT(measure_gain(1.2f, 100.0f), 1.02f);
    EXPECT_LT(measure_gain(8.0f, 100.0f), 0.3f);  // Above the band (theory: 0.24)
}

TEST(BandPass, AttenuationOtherRates)
{
    const float rates[] = {25.0f, 50.0f, 200.0f, 400.0f, 1000.0f};
    for (const auto rate : rates) {
        SCOPED_TRACE(rate);
        EXPECT_LT(measure_gain(0.2f, rate), 0.2f);
        EXPECT_GT(measure_gain(1.2f, rate), 0.95f);
        EXPECT_LT(measure_gain(1.2f, rate), 1.02f);
        EXPECT_LT(measure_gain(8.0f, rate), 0.3f);
    }
}

TEST(BandPass, NoStartupStep)
{
    BandPass bp(0.5f, 4.0f, 100.0f);
    for (int i = 0; i < 100; ++i) {
        EXPECT_FLOAT_EQ(bp.process(4.0e6f), 0.0f);
    }
    bp.reset();
    EXPECT_FLOAT_EQ(bp.process(-2.0e6f), 0.0f);  // Primed again with the new level
}

TEST(BandPass, PassThrough)
{
    // Cutoff at or above Nyquist: the stage is disabled
    Biquad lp(Biquad::Type::LowPass, 4.0f, 8.0f);
    EXPECT_EQ(lp.type(), Biquad::Type::PassThrough);
    EXPECT_FLOAT_EQ(lp.process(123.0f), 123.0f);

    Biquad none(Biquad::Type::HighPass, 0.5f, 0.0f);
    EXPECT_EQ(none.type(), Biquad::Type::PassThrough);

    BandPass def{};
    EXPECT_FLOAT_EQ(def.process(5.0f), 5.0f);
}
