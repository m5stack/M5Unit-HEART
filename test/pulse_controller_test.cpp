/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Native test for the pulse controller
*/
#include <gtest/gtest.h>
#include <cmath>
#include <deque>
#include <heart/pulse_controller.hpp>
#include "ppg_synth.hpp"

using namespace m5::heart;

namespace {
// Data source fed by the test
class MockSource : public PPGDataSource {
public:
    MockSource(const uint8_t channels, const float rate, const ValueUnit unit)
    {
        _spec.channels         = channels;
        _spec.sampling_rate_hz = rate;
        _spec.unit             = unit;
    }
    virtual PPGSpec ppgSpec() const override
    {
        return _spec;
    }
    virtual bool readPPG(PPGSample& out) override
    {
        if (_queue.empty()) {
            return false;
        }
        out = _queue.front();
        _queue.pop_front();
        return true;
    }
    virtual bool ppgOverflowed() const override
    {
        return _overflowed;
    }

    // Queue the samples of the generator for the seconds (scaled)
    void fill(ppg_synth::Generator& gen, const float sec, const float scale = 1.0f)
    {
        const uint32_t n = static_cast<uint32_t>(sec * _spec.sampling_rate_hz);
        for (uint32_t i = 0; i < n; ++i) {
            PPGSample s = gen.next();
            for (auto& v : s.value) {
                v *= scale;
            }
            s.valid &= _spec.channels;
            _queue.push_back(s);
        }
    }
    size_t queued() const
    {
        return _queue.size();
    }

    PPGSpec _spec{};
    bool _overflowed{};

private:
    std::deque<PPGSample> _queue{};
};

// Feed in chunks like a unit that is read every 100 ms
uint32_t feed(PulseController& ctrl, MockSource& src, ppg_synth::Generator& gen, const float sec,
              const float scale = 1.0f)
{
    uint32_t total{};
    for (float t = 0.0f; t < sec - 0.05f; t += 0.1f) {
        src.fill(gen, 0.1f, scale);
        total += ctrl.update();
    }
    return total;
}

ppg_synth::Config make_config(const float rate)
{
    ppg_synth::Config cfg{};
    cfg.sampling_rate_hz = rate;
    cfg.bpm              = 72.0f;
    cfg.ratio            = 0.6f;
    return cfg;
}
}  // namespace

TEST(PulseController, NoSource)
{
    PulseController ctrl{};
    EXPECT_EQ(ctrl.processor(), nullptr);
    EXPECT_EQ(ctrl.update(), 0U);
    EXPECT_FALSE(ctrl.hasBpm());
    EXPECT_TRUE(std::isnan(ctrl.bpm()));
    EXPECT_FALSE(ctrl.hasSpO2());
    EXPECT_TRUE(std::isnan(ctrl.spo2()));
    EXPECT_FALSE(ctrl.isBeat());
}

TEST(PulseController, AddRejectsInvalidRate)
{
    PulseController ctrl{};
    MockSource zero(0x07, 0.0f, ValueUnit::Count);
    EXPECT_FALSE(ctrl.add(zero));
    EXPECT_EQ(ctrl.processor(), nullptr);

    MockSource nan_rate(0x07, std::nanf(""), ValueUnit::Count);
    EXPECT_FALSE(ctrl.add(nan_rate));
}

TEST(PulseController, RateAutoConfig)
{
    MockSource src(0x07, 50.0f, ValueUnit::PicoAmpere);
    PulseController ctrl{};
    EXPECT_TRUE(ctrl.add(src));
    const PulseProcessor* proc = ctrl.processor();
    EXPECT_NE(proc, nullptr);
    if (proc) {
        EXPECT_FLOAT_EQ(proc->samplingRate(), 50.0f);
        EXPECT_EQ(proc->heartRateChannel(), channel_index(Channel::Green));
    }

    ppg_synth::Generator gen(make_config(50.0f));
    const uint32_t processed = feed(ctrl, src, gen, 20.0f);
    EXPECT_EQ(processed, 1000U);
    EXPECT_EQ(src.queued(), 0U);
    EXPECT_TRUE(ctrl.hasBpm());
    EXPECT_NEAR(ctrl.bpm(), 72.0f, 1.5f);
    EXPECT_TRUE(ctrl.hasSpO2());
    EXPECT_NEAR(ctrl.spo2(), 95.0f, 1.0f);
    EXPECT_GT(ctrl.perfusionIndex(), 0.05f);
}

TEST(PulseController, Config)
{
    PulseProcessor::config_t cfg{};
    cfg.hr_channel = channel_index(Channel::Red);
    cfg.spo2       = false;
    PulseController ctrl(cfg);
    MockSource src(0x07, 100.0f, ValueUnit::Count);
    EXPECT_TRUE(ctrl.add(src));
    EXPECT_EQ(ctrl.processor()->heartRateChannel(), channel_index(Channel::Red));
    EXPECT_FALSE(ctrl.processor()->isSpO2Enabled());
}

TEST(PulseController, IsBeatPerUpdate)
{
    MockSource src(0x07, 100.0f, ValueUnit::Count);
    PulseController ctrl{};
    EXPECT_TRUE(ctrl.add(src));
    ppg_synth::Generator gen(make_config(100.0f));

    uint32_t beats{};
    for (int i = 0; i < 200; ++i) {  // 20 s
        src.fill(gen, 0.1f);
        ctrl.update();
        beats += ctrl.isBeat() ? 1 : 0;
    }
    EXPECT_NEAR(beats, 20U, 2U);  // At most one beat in each 100 ms chunk

    // No new samples: no beat
    EXPECT_EQ(ctrl.update(), 0U);
    EXPECT_FALSE(ctrl.isBeat());
}

TEST(PulseController, OverflowResets)
{
    MockSource src(0x07, 100.0f, ValueUnit::Count);
    PulseController ctrl{};
    EXPECT_TRUE(ctrl.add(src));
    ppg_synth::Generator gen(make_config(100.0f));
    feed(ctrl, src, gen, 10.0f);
    EXPECT_TRUE(ctrl.hasBpm());

    // Overflow flag without new samples does nothing
    src._overflowed = true;
    EXPECT_EQ(ctrl.update(), 0U);
    EXPECT_TRUE(ctrl.hasBpm());

    // Overflow with new samples resets the processor
    src.fill(gen, 0.1f);
    EXPECT_EQ(ctrl.update(), 10U);
    EXPECT_FALSE(ctrl.hasBpm());
    // Calling again while the flag is still set does not reset again
    src._overflowed = false;
    feed(ctrl, src, gen, 10.0f);
    EXPECT_TRUE(ctrl.hasBpm());
    EXPECT_NEAR(ctrl.bpm(), 72.0f, 1.5f);
}

TEST(PulseController, MissingChannel)
{
    // Red and IR only: heart rate from IR, SpO2 available
    MockSource src(channel_bit(Channel::Red) | channel_bit(Channel::IR), 100.0f, ValueUnit::Count);
    PulseController ctrl{};
    EXPECT_TRUE(ctrl.add(src));
    EXPECT_EQ(ctrl.processor()->heartRateChannel(), channel_index(Channel::IR));
    ppg_synth::Generator gen(make_config(100.0f));
    feed(ctrl, src, gen, 20.0f);
    EXPECT_NEAR(ctrl.bpm(), 72.0f, 1.5f);
    EXPECT_NEAR(ctrl.spo2(), 95.0f, 1.0f);

    // Red only (MAX30102 HROnly): heart rate from Red, no SpO2
    MockSource red(channel_bit(Channel::Red), 100.0f, ValueUnit::PicoAmpere);
    PulseController ctrl_red{};
    EXPECT_TRUE(ctrl_red.add(red));
    EXPECT_EQ(ctrl_red.processor()->heartRateChannel(), channel_index(Channel::Red));
    ppg_synth::Generator gen_red(make_config(100.0f));
    feed(ctrl_red, red, gen_red, 20.0f);
    EXPECT_NEAR(ctrl_red.bpm(), 72.0f, 1.5f);
    EXPECT_FALSE(ctrl_red.hasSpO2());
}

TEST(PulseController, SpecChangeRebuilds)
{
    MockSource src(0x07, 100.0f, ValueUnit::Count);
    PulseController ctrl{};
    EXPECT_TRUE(ctrl.add(src));
    ppg_synth::Generator gen(make_config(100.0f));
    feed(ctrl, src, gen, 10.0f);
    EXPECT_TRUE(ctrl.hasBpm());

    // Rate change
    src._spec.sampling_rate_hz = 50.0f;
    EXPECT_EQ(ctrl.update(), 0U);
    EXPECT_FLOAT_EQ(ctrl.processor()->samplingRate(), 50.0f);
    EXPECT_FALSE(ctrl.hasBpm());
    ppg_synth::Generator gen50(make_config(50.0f));
    feed(ctrl, src, gen50, 10.0f);
    EXPECT_NEAR(ctrl.bpm(), 72.0f, 1.5f);

    // Channel change
    src._spec.channels = channel_bit(Channel::Green);
    ctrl.update();
    EXPECT_EQ(ctrl.processor()->channels(), channel_bit(Channel::Green));
    EXPECT_FALSE(ctrl.processor()->isSpO2Enabled());

    // Stopped (rate 0): samples are drained but not processed
    src.fill(gen50, 1.0f);
    src._spec.sampling_rate_hz = 0.0f;
    EXPECT_EQ(ctrl.update(), 50U);
    EXPECT_FLOAT_EQ(ctrl.processor()->samplingRate(), 0.0f);
    EXPECT_FALSE(ctrl.hasBpm());
}

TEST(PulseController, UnitDoesNotMatter)
{
    MockSource count_src(0x07, 100.0f, ValueUnit::Count);
    MockSource pico_src(0x07, 100.0f, ValueUnit::PicoAmpere);
    PulseController count_ctrl{}, pico_ctrl{};
    EXPECT_TRUE(count_ctrl.add(count_src));
    EXPECT_TRUE(pico_ctrl.add(pico_src));
    ppg_synth::Generator gen1(make_config(100.0f)), gen2(make_config(100.0f));
    feed(count_ctrl, count_src, gen1, 20.0f, 1.0f);
    feed(pico_ctrl, pico_src, gen2, 20.0f, 1000.0f);
    EXPECT_TRUE(count_ctrl.hasBpm());
    EXPECT_NEAR(count_ctrl.bpm(), pico_ctrl.bpm(), 0.01f);
    EXPECT_NEAR(count_ctrl.spo2(), pico_ctrl.spo2(), 0.1f);
}
