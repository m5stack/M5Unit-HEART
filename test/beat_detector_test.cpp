/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Native test for the beat detector
*/
#include <gtest/gtest.h>
#include <cmath>
#include <heart/beat_detector.hpp>

using namespace m5::heart;

namespace {
constexpr float pi{3.14159265f};
constexpr float rate{100.0f};

struct Counts {
    uint32_t peaks{}, beats{}, expired{};
};

// Feed a sine of the heart rate for the seconds
Counts feed_sine(BeatDetector& bd, const float bpm, const float sec, float& phase)
{
    Counts c{};
    const uint32_t n = static_cast<uint32_t>(sec * rate);
    for (uint32_t i = 0; i < n; ++i) {
        const uint8_t ev = bd.process(std::sin(2.0f * pi * phase));
        c.peaks += (ev & BeatDetector::PEAK) ? 1 : 0;
        c.beats += (ev & BeatDetector::BEAT) ? 1 : 0;
        c.expired += (ev & BeatDetector::EXPIRED) ? 1 : 0;
        phase += bpm / 60.0f / rate;
        if (phase >= 1.0f) {
            phase -= 1.0f;
        }
    }
    return c;
}
}  // namespace

TEST(BeatDetector, Inactive)
{
    BeatDetector bd{};
    EXPECT_EQ(bd.process(1.0f), 0U);
    EXPECT_TRUE(std::isnan(bd.bpm()));
}

TEST(BeatDetector, SteadyRate)
{
    BeatDetector bd(rate);
    float phase{};
    const Counts c = feed_sine(bd, 72.0f, 10.0f, phase);
    EXPECT_NEAR(c.peaks, 12U, 1U);
    EXPECT_NEAR(c.beats, 11U, 1U);  // The first peak has no RR
    EXPECT_EQ(c.expired, 0U);
    EXPECT_EQ(bd.rrCount(), 5U);
    EXPECT_GE(bd.consecutiveBeats(), 3U);
    EXPECT_TRUE(bd.hasRecentBeat());
    EXPECT_NEAR(bd.bpm(), 72.0f, 1.0f);
}

TEST(BeatDetector, TroughAndPeakEvents)
{
    BeatDetector bd(rate);
    float phase{0.5f};
    uint32_t trough_at{}, peak_at{};
    for (uint32_t i = 0; i < 200; ++i) {
        const uint8_t ev = bd.process(std::sin(2.0f * pi * phase));
        if (ev & BeatDetector::TROUGH_CANDIDATE) {
            trough_at = i;
        }
        if (ev & BeatDetector::PEAK) {
            peak_at = i;
            break;
        }
        phase += 1.0f / rate;  // 60 bpm
    }
    // Starting at phase 0.5: trough at phase 0.75 (i = 25), peak at phase 1.25 confirmed below half height
    EXPECT_EQ(trough_at, 25U);
    EXPECT_GT(peak_at, 75U);
    EXPECT_LT(peak_at, 100U);
}

TEST(BeatDetector, Refractory)
{
    // Two humps 0.15 s apart in each beat: the second one is within the refractory period
    BeatDetector bd(rate);
    uint32_t peaks{};
    for (uint32_t i = 0; i < 1000; ++i) {
        const float t  = std::fmod(i / rate, 1.0f);  // 60 bpm
        const float a  = (t - 0.20f) / 0.03f;
        const float b  = (t - 0.35f) / 0.03f;
        const float ac = std::exp(-a * a) + 0.9f * std::exp(-b * b) - 0.3f;
        peaks += (bd.process(ac) & BeatDetector::PEAK) ? 1 : 0;
    }
    EXPECT_EQ(peaks, 10U);
    EXPECT_NEAR(bd.bpm(), 60.0f, 1.0f);
}

TEST(BeatDetector, RRRange)
{
    // 25 bpm: RR 2.4 s is longer than 2.0 s
    BeatDetector bd(rate);
    float phase{};
    const Counts c = feed_sine(bd, 25.0f, 20.0f, phase);
    EXPECT_GE(c.peaks, 7U);
    EXPECT_EQ(c.beats, 0U);
    EXPECT_EQ(bd.rrCount(), 0U);
}

TEST(BeatDetector, RateChangeReseeds)
{
    BeatDetector bd(rate);
    float phase{};
    feed_sine(bd, 60.0f, 10.0f, phase);
    EXPECT_NEAR(bd.bpm(), 60.0f, 1.0f);

    // 60 -> 120 bpm is outside +-30 %: three consistent rejections restart the history
    feed_sine(bd, 120.0f, 5.0f, phase);
    EXPECT_NEAR(bd.bpm(), 120.0f, 2.0f);
    EXPECT_GE(bd.consecutiveBeats(), 3U);
}

TEST(BeatDetector, Expire)
{
    BeatDetector bd(rate);
    float phase{};
    feed_sine(bd, 72.0f, 10.0f, phase);
    EXPECT_TRUE(bd.hasRecentBeat());

    uint32_t expired{};
    for (uint32_t i = 0; i < 400; ++i) {
        expired += (bd.process(0.0f) & BeatDetector::EXPIRED) ? 1 : 0;
    }
    EXPECT_EQ(expired, 1U);
    EXPECT_FALSE(bd.hasRecentBeat());
    EXPECT_EQ(bd.rrCount(), 0U);
    EXPECT_EQ(bd.consecutiveBeats(), 0U);
    EXPECT_TRUE(std::isnan(bd.bpm()));
}

TEST(BeatDetector, Reset)
{
    BeatDetector bd(rate);
    float phase{};
    feed_sine(bd, 72.0f, 10.0f, phase);
    bd.reset();
    EXPECT_EQ(bd.rrCount(), 0U);
    EXPECT_FALSE(bd.hasRecentBeat());
    EXPECT_TRUE(std::isnan(bd.bpm()));
}
