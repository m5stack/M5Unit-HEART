/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Native test for the pulse processor
*/
#include <gtest/gtest.h>
#include <cmath>
#include <heart/pulse_processor.hpp>
#include "ppg_synth.hpp"

using namespace m5::heart;

namespace {
constexpr uint8_t ALL_CHANNELS{0x07};

struct RunResult {
    uint32_t beats{};          // isBeat() count
    uint32_t valid_samples{};  // Samples with hasBpm()
    uint32_t samples{};
};

// Feed the generator into the processor for the seconds, scaling the values
RunResult run(ppg_synth::Generator& gen, PulseProcessor& proc, const float sec, const float scale = 1.0f)
{
    RunResult r{};
    const uint32_t n = static_cast<uint32_t>(sec * proc.samplingRate());
    for (uint32_t i = 0; i < n; ++i) {
        PPGSample s = gen.next();
        for (auto& v : s.value) {
            v *= scale;
        }
        proc.push(s);
        r.beats += proc.isBeat() ? 1 : 0;
        r.valid_samples += proc.hasBpm() ? 1 : 0;
        ++r.samples;
    }
    return r;
}

ppg_synth::Config make_config(const float rate, const float bpm)
{
    ppg_synth::Config cfg{};
    cfg.sampling_rate_hz = rate;
    cfg.bpm              = bpm;
    return cfg;
}
}  // namespace

TEST(PulseProcessor, ChannelSelection)
{
    const uint8_t green = channel_bit(Channel::Green);
    const uint8_t red   = channel_bit(Channel::Red);
    const uint8_t ir    = channel_bit(Channel::IR);

    EXPECT_EQ(PulseProcessor(100.0f, ALL_CHANNELS).heartRateChannel(), channel_index(Channel::Green));
    EXPECT_EQ(PulseProcessor(100.0f, red | ir).heartRateChannel(), channel_index(Channel::IR));
    EXPECT_EQ(PulseProcessor(100.0f, red).heartRateChannel(), channel_index(Channel::Red));
    EXPECT_EQ(PulseProcessor(100.0f, 0).heartRateChannel(), INVALID_CHANNEL);

    PulseProcessor::config_t cfg{};
    cfg.hr_channel = channel_index(Channel::IR);
    EXPECT_EQ(PulseProcessor(100.0f, ALL_CHANNELS, cfg).heartRateChannel(), channel_index(Channel::IR));
    EXPECT_EQ(PulseProcessor(100.0f, green, cfg).heartRateChannel(), INVALID_CHANNEL);  // IR is not available

    EXPECT_TRUE(PulseProcessor(100.0f, ALL_CHANNELS).isSpO2Enabled());
    EXPECT_TRUE(PulseProcessor(100.0f, red | ir).isSpO2Enabled());
    EXPECT_FALSE(PulseProcessor(100.0f, green | ir).isSpO2Enabled());
    cfg.spo2 = false;
    EXPECT_FALSE(PulseProcessor(100.0f, ALL_CHANNELS, cfg).isSpO2Enabled());
}

TEST(PulseProcessor, InvalidRate)
{
    PulseProcessor proc(0.0f, ALL_CHANNELS);
    EXPECT_FLOAT_EQ(proc.samplingRate(), 0.0f);
    EXPECT_EQ(proc.heartRateChannel(), INVALID_CHANNEL);

    ppg_synth::Generator gen(make_config(100.0f, 72.0f));
    for (int i = 0; i < 2000; ++i) {
        proc.push(gen.next());
    }
    EXPECT_FALSE(proc.hasBpm());
    EXPECT_TRUE(std::isnan(proc.bpm()));
    EXPECT_FALSE(proc.hasSpO2());
}

TEST(PulseProcessor, HeartRates)
{
    const float bpms[]  = {40.0f, 72.0f, 120.0f, 180.0f};
    const float rates[] = {25.0f, 50.0f, 100.0f, 400.0f};
    for (const auto rate : rates) {
        for (const auto bpm : bpms) {
            SCOPED_TRACE(::testing::Message() << rate << " Hz, " << bpm << " bpm");
            ppg_synth::Generator gen(make_config(rate, bpm));
            PulseProcessor proc(rate, ALL_CHANNELS);
            const RunResult r = run(gen, proc, 30.0f);
            EXPECT_TRUE(proc.hasBpm());
            EXPECT_NEAR(proc.bpm(), bpm, bpm * 0.02f);
            // Beats after the settling time (2 s) and the first two peaks
            EXPECT_NEAR(r.beats, bpm / 60.0f * 28.0f - 1.0f, 2.0f);
        }
    }
}

TEST(PulseProcessor, Startup)
{
    ppg_synth::Generator gen(make_config(100.0f, 72.0f));
    PulseProcessor proc(100.0f, ALL_CHANNELS);
    // Not valid while the filters settle (2 s)
    RunResult r = run(gen, proc, 2.0f);
    EXPECT_EQ(r.beats, 0U);
    EXPECT_EQ(r.valid_samples, 0U);
    // Valid after three beats
    r = run(gen, proc, 6.0f);
    EXPECT_TRUE(proc.hasBpm());
    EXPECT_GT(r.valid_samples, 0U);
}

TEST(PulseProcessor, RateStep)
{
    ppg_synth::Generator gen(make_config(100.0f, 70.0f));
    PulseProcessor proc(100.0f, ALL_CHANNELS);
    run(gen, proc, 15.0f);
    EXPECT_NEAR(proc.bpm(), 70.0f, 1.5f);

    gen.setBpm(110.0f);
    run(gen, proc, 8.0f);
    EXPECT_TRUE(proc.hasBpm());
    EXPECT_NEAR(proc.bpm(), 110.0f, 2.0f);

    gen.setBpm(80.0f);
    run(gen, proc, 8.0f);
    EXPECT_TRUE(proc.hasBpm());
    EXPECT_NEAR(proc.bpm(), 80.0f, 2.0f);
}

TEST(PulseProcessor, NoiseNoDoubleCount)
{
    const float bpms[] = {40.0f, 72.0f, 120.0f};
    for (const auto bpm : bpms) {
        SCOPED_TRACE(bpm);
        ppg_synth::Config cfg = make_config(100.0f, bpm);
        cfg.noise             = 0.001f;  // 5 % of the pulse amplitude
        cfg.wander            = 0.005f;
        ppg_synth::Generator gen(cfg);
        PulseProcessor proc(100.0f, ALL_CHANNELS);
        const RunResult r = run(gen, proc, 60.0f);
        EXPECT_NEAR(proc.bpm(), bpm, bpm * 0.03f);
        EXPECT_NEAR(r.beats, bpm / 60.0f * 58.0f - 1.0f, bpm / 60.0f * 58.0f * 0.05f);
    }
}

TEST(PulseProcessor, SpO2)
{
    const float ratios[] = {0.4f, 0.6f, 0.8f, 1.0f};
    for (const auto ratio : ratios) {
        SCOPED_TRACE(ratio);
        ppg_synth::Config cfg = make_config(100.0f, 72.0f);
        cfg.ratio             = ratio;
        ppg_synth::Generator gen(cfg);
        PulseProcessor proc(100.0f, ALL_CHANNELS);
        run(gen, proc, 20.0f);
        EXPECT_TRUE(proc.hasSpO2());
        EXPECT_NEAR(proc.spo2(), ratio_to_spo2(ratio), 1.0f);
    }
}

TEST(PulseProcessor, SpO2WithoutGreen)
{
    ppg_synth::Config cfg = make_config(100.0f, 72.0f);
    cfg.channels          = channel_bit(Channel::Red) | channel_bit(Channel::IR);
    cfg.ratio             = 0.6f;
    ppg_synth::Generator gen(cfg);
    PulseProcessor proc(100.0f, cfg.channels);
    EXPECT_EQ(proc.heartRateChannel(), channel_index(Channel::IR));
    run(gen, proc, 20.0f);
    EXPECT_NEAR(proc.bpm(), 72.0f, 1.5f);
    EXPECT_NEAR(proc.spo2(), 95.0f, 1.0f);
}

TEST(PulseProcessor, NoSpO2WithoutRedOrIR)
{
    ppg_synth::Config cfg = make_config(100.0f, 72.0f);
    cfg.channels          = channel_bit(Channel::Green);
    ppg_synth::Generator gen(cfg);
    PulseProcessor proc(100.0f, cfg.channels);
    run(gen, proc, 20.0f);
    EXPECT_TRUE(proc.hasBpm());
    EXPECT_FALSE(proc.hasSpO2());
    EXPECT_TRUE(std::isnan(proc.spo2()));
}

TEST(PulseProcessor, PerfusionIndex)
{
    ppg_synth::Generator gen(make_config(100.0f, 72.0f));
    PulseProcessor proc(100.0f, ALL_CHANNELS);
    EXPECT_TRUE(std::isnan(proc.perfusionIndex()));
    run(gen, proc, 10.0f);
    // Generated peak-to-peak is 2 % of DC; the band-pass keeps most of it
    EXPECT_GT(proc.perfusionIndex(), 1.2f);
    EXPECT_LT(proc.perfusionIndex(), 2.2f);
}

TEST(PulseProcessor, PerfusionIndexOfIR)
{
    // The heart rate channel is Green, but the perfusion index is of IR when IR is available
    ppg_synth::Generator gen(make_config(100.0f, 72.0f));  // Green and IR: 2 %, Red: 2 % x ratio 0.6
    PulseProcessor same(100.0f, ALL_CHANNELS);             // IR as generated (the same as Green)
    PulseProcessor weak(100.0f, ALL_CHANNELS);             // IR replaced by the weaker Red
    PulseProcessor no_ir(100.0f, channel_bit(Channel::Green) | channel_bit(Channel::Red));
    EXPECT_EQ(weak.heartRateChannel(), channel_index(Channel::Green));
    const uint32_t n = static_cast<uint32_t>(10.0f * same.samplingRate());
    for (uint32_t i = 0; i < n; ++i) {
        PPGSample s = gen.next();
        same.push(s);
        no_ir.push(s);
        s.value[channel_index(Channel::IR)] = s.value[channel_index(Channel::Red)];
        weak.push(s);
    }
    EXPECT_FALSE(std::isnan(same.perfusionIndex()));
    EXPECT_NEAR(weak.perfusionIndex(), same.perfusionIndex() * 0.6f, 0.05f);
    EXPECT_TRUE(weak.hasBpm());  // The validity uses the heart rate channel
    // Without IR: the heart rate channel (Green)
    EXPECT_NEAR(no_ir.perfusionIndex(), same.perfusionIndex(), 0.05f);
}

TEST(PulseProcessor, InvalidWithoutPulse)
{
    ppg_synth::Config cfg = make_config(100.0f, 72.0f);
    cfg.perfusion         = 0.0f;
    cfg.noise             = 0.0001f;
    ppg_synth::Generator gen(cfg);
    PulseProcessor proc(100.0f, ALL_CHANNELS);
    const RunResult r = run(gen, proc, 30.0f);
    EXPECT_EQ(r.valid_samples, 0U);
    EXPECT_FALSE(proc.hasBpm());
    EXPECT_FALSE(proc.hasSpO2());
}

TEST(PulseProcessor, PerfusionOutOfRange)
{
    // Too small (0.02 %) and too large (40 %)
    const float perfusions[] = {0.0002f, 0.4f};
    for (const auto p : perfusions) {
        SCOPED_TRACE(p);
        ppg_synth::Config cfg = make_config(100.0f, 72.0f);
        cfg.perfusion         = p;
        ppg_synth::Generator gen(cfg);
        PulseProcessor proc(100.0f, ALL_CHANNELS);
        const RunResult r = run(gen, proc, 20.0f);
        EXPECT_GT(r.beats, 10U);  // Beats are detected
        EXPECT_EQ(r.valid_samples, 0U);
        EXPECT_FALSE(proc.hasBpm());
    }
}

TEST(PulseProcessor, InvalidWhileIrregularAndRecover)
{
    ppg_synth::Generator gen(make_config(100.0f, 72.0f));
    PulseProcessor proc(100.0f, ALL_CHANNELS);
    run(gen, proc, 10.0f);
    EXPECT_TRUE(proc.hasBpm());

    // Irregular rhythm: RR cycles 0.6 / 0.9 / 1.2 s (one beat per segment)
    const float rr_table[] = {0.6f, 0.9f, 1.2f};
    uint32_t valid{}, total{};
    for (int i = 0; i < 30; ++i) {
        const float rr = rr_table[i % 3];
        gen.setBpm(60.0f / rr);
        const RunResult r = run(gen, proc, rr);
        valid += r.valid_samples;
        total += r.samples;
    }
    EXPECT_FALSE(proc.hasBpm());
    EXPECT_LT(valid, 400U);  // Only until the first inconsistent RR (about 3 s of 27 s)
    EXPECT_EQ(total, 2700U);

    // Regular again
    gen.setBpm(72.0f);
    run(gen, proc, 8.0f);
    EXPECT_TRUE(proc.hasBpm());
    EXPECT_NEAR(proc.bpm(), 72.0f, 1.5f);
}

TEST(PulseProcessor, LostPulseExpires)
{
    ppg_synth::Generator gen(make_config(100.0f, 72.0f));
    PulseProcessor proc(100.0f, ALL_CHANNELS);
    run(gen, proc, 10.0f);
    EXPECT_TRUE(proc.hasBpm());
    EXPECT_TRUE(proc.hasSpO2());

    // Finger removed: flat signal
    gen.setPerfusion(0.0f);
    run(gen, proc, 4.0f);
    EXPECT_FALSE(proc.hasBpm());
    EXPECT_FALSE(proc.hasSpO2());
    EXPECT_TRUE(std::isnan(proc.perfusionIndex()));

    // Back
    gen.setPerfusion(0.02f);
    run(gen, proc, 8.0f);
    EXPECT_TRUE(proc.hasBpm());
    EXPECT_TRUE(proc.hasSpO2());
}

TEST(PulseProcessor, ScaleInvariance)
{
    ppg_synth::Config cfg = make_config(100.0f, 84.0f);
    cfg.noise             = 0.0005f;
    cfg.ratio             = 0.7f;
    ppg_synth::Generator gen1(cfg), gen1000(cfg);
    PulseProcessor p1(100.0f, ALL_CHANNELS), p1000(100.0f, ALL_CHANNELS);
    run(gen1, p1, 20.0f, 1.0f);
    run(gen1000, p1000, 20.0f, 1000.0f);
    EXPECT_TRUE(p1.hasBpm());
    EXPECT_TRUE(p1000.hasBpm());
    EXPECT_NEAR(p1.bpm(), p1000.bpm(), 0.01f);
    EXPECT_TRUE(p1.hasSpO2());
    EXPECT_TRUE(p1000.hasSpO2());
    EXPECT_NEAR(p1.spo2(), p1000.spo2(), 0.1f);
    EXPECT_NEAR(p1.perfusionIndex(), p1000.perfusionIndex(), 0.01f);
}

TEST(PulseProcessor, MissingSamples)
{
    // Every 7th sample lacks Red
    ppg_synth::Config cfg = make_config(100.0f, 72.0f);
    cfg.drop_every        = 7;
    cfg.drop_channels     = channel_bit(Channel::Red);
    ppg_synth::Generator gen(cfg);
    PulseProcessor proc(100.0f, ALL_CHANNELS);
    run(gen, proc, 20.0f);
    EXPECT_NEAR(proc.bpm(), 72.0f, 1.5f);
    EXPECT_NEAR(proc.spo2(), 95.0f, 1.0f);

    // A sample without the heart rate channel, or with NaN, is ignored
    PPGSample s{};
    s.set(Channel::Red, 1.0f);
    s.set(Channel::IR, 1.0f);
    proc.push(s);
    EXPECT_FALSE(proc.isBeat());
    EXPECT_TRUE(proc.hasBpm());
    s.set(Channel::Green, std::nanf(""));
    proc.push(s);
    EXPECT_TRUE(proc.hasBpm());
    EXPECT_NEAR(proc.bpm(), 72.0f, 1.5f);
}

TEST(PulseProcessor, Reset)
{
    ppg_synth::Generator gen(make_config(100.0f, 72.0f));
    PulseProcessor proc(100.0f, ALL_CHANNELS);
    run(gen, proc, 10.0f);
    EXPECT_TRUE(proc.hasBpm());

    proc.reset();
    EXPECT_FALSE(proc.hasBpm());
    EXPECT_TRUE(std::isnan(proc.bpm()));
    EXPECT_FALSE(proc.hasSpO2());
    EXPECT_TRUE(std::isnan(proc.perfusionIndex()));
    EXPECT_FLOAT_EQ(proc.samplingRate(), 100.0f);
    EXPECT_EQ(proc.heartRateChannel(), channel_index(Channel::Green));

    run(gen, proc, 10.0f);
    EXPECT_TRUE(proc.hasBpm());
    EXPECT_NEAR(proc.bpm(), 72.0f, 1.5f);
}
