/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Native test for the synthetic PPG generator (test helper)
*/
#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include "ppg_synth.hpp"

using namespace m5::heart;

namespace {
struct Level {
    float min{1e30f}, max{-1e30f}, sum{};
    uint32_t count{};
    void add(const float v)
    {
        min = std::min(min, v);
        max = std::max(max, v);
        sum += v;
        ++count;
    }
    float mean() const
    {
        return count ? sum / count : 0.0f;
    }
};
}  // namespace

TEST(PPGSynth, Levels)
{
    ppg_synth::Config cfg{};
    cfg.sampling_rate_hz = 100.0f;
    cfg.bpm              = 60.0f;
    cfg.perfusion        = 0.02f;
    cfg.ratio            = 0.5f;
    ppg_synth::Generator gen(cfg);

    Level level[3]{};
    for (int i = 0; i < 500; ++i) {  // 5 beats
        const PPGSample s = gen.next();
        EXPECT_EQ(s.valid, 0x07U);
        for (uint8_t c = 0; c < 3; ++c) {
            level[c].add(s.value[c]);
        }
    }
    // Peak-to-peak / DC: Green and IR = perfusion, Red = ratio x perfusion
    EXPECT_NEAR((level[0].max - level[0].min) / cfg.dc[0], 0.02f, 0.001f);
    EXPECT_NEAR((level[1].max - level[1].min) / cfg.dc[1], 0.01f, 0.0005f);
    EXPECT_NEAR((level[2].max - level[2].min) / cfg.dc[2], 0.02f, 0.001f);
    EXPECT_FLOAT_EQ(level[2].max, cfg.dc[2]);  // Reflective: the value falls from DC
    EXPECT_NEAR(gen.time(), 5.0f, 1e-4f);
}

TEST(PPGSynth, Period)
{
    ppg_synth::Config cfg{};
    cfg.sampling_rate_hz = 100.0f;
    cfg.bpm              = 75.0f;  // 0.8 s
    ppg_synth::Generator gen(cfg);

    // Minimum of IR (systole) repeats every 80 samples
    int last_min{-1}, period{};
    float prev2{}, prev1{};
    for (int i = 0; i < 400; ++i) {
        const float v = gen.next().value[2];
        if (i >= 2 && prev1 < prev2 && prev1 <= v) {
            if (last_min >= 0 && (i - 1 - last_min) > 40) {
                period = i - 1 - last_min;
            }
            if (last_min < 0 || (i - 1 - last_min) > 40) {
                last_min = i - 1;
            }
        }
        prev2 = prev1;
        prev1 = v;
    }
    EXPECT_EQ(period, 80);
}

TEST(PPGSynth, ChannelsAndDrop)
{
    ppg_synth::Config cfg{};
    cfg.channels      = channel_bit(Channel::Red) | channel_bit(Channel::IR);
    cfg.drop_every    = 4;
    cfg.drop_channels = channel_bit(Channel::Red);
    ppg_synth::Generator gen(cfg);
    for (int i = 1; i <= 8; ++i) {
        const PPGSample s = gen.next();
        EXPECT_FALSE(s.has(Channel::Green));
        EXPECT_TRUE(s.has(Channel::IR));
        EXPECT_EQ(s.has(Channel::Red), (i % 4) != 0);
    }
}

TEST(PPGSynth, NoiseIsDeterministic)
{
    ppg_synth::Config cfg{};
    cfg.noise = 0.001f;
    ppg_synth::Generator a(cfg), b(cfg);
    cfg.seed = 2;
    ppg_synth::Generator c(cfg);
    bool differs{};
    for (int i = 0; i < 100; ++i) {
        const float va = a.next().value[0];
        EXPECT_FLOAT_EQ(va, b.next().value[0]);
        differs |= (va != c.next().value[0]);
    }
    EXPECT_TRUE(differs);
}
