/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  UnitTest for UnitGH3038 (StampHeart)
  Wiring: GROVE (I2C) -> Port A (NessoN1: QWIIC), GROVE (INT/RST) -> Port C or Port B (pin1 = RST, pin2 = INT)
*/
#include <gtest/gtest.h>
#include <thread>
#include <M5Unified.h>
#include <esp_heap_caps.h>
#include <M5UnitUnified.hpp>
#include <googletest/test_template.hpp>
#include <googletest/test_helper.hpp>
#include <unit/unit_GH3038.hpp>

using namespace m5::unit::googletest;
using namespace m5::unit;
using namespace m5::unit::gh3038;

namespace {
// Wiring: GROVE (I2C) -> Port A (NessoN1: QWIIC, using a QWIIC-GROVE cable)
// GROVE (INT/RST) -> Port C (Core / Core2 / CoreS3 / CoreS3SE / Fire (PSRAM disabled) / Tab5)
//                    or Port B (Dial / NessoN1 GROVE)
// StampHeart INT/RST GROVE: pin1 (next to 5V) = RST, pin2 = INT
bool select_int_rst_port(m5::pin_name_t& rst, m5::pin_name_t& intr)
{
    switch (M5.getBoard()) {
        case m5::board_t::board_M5Stack:
            // Fire is also detected as M5Stack. Its Port C is shared with PSRAM, so it is usable only without PSRAM
            if (heap_caps_get_total_size(MALLOC_CAP_SPIRAM)) {
                return false;
            }
            rst  = m5::pin_name_t::port_c_pin1;
            intr = m5::pin_name_t::port_c_pin2;
            return true;
        case m5::board_t::board_M5StackCore2:
        case m5::board_t::board_M5StackCoreS3:
        case m5::board_t::board_M5StackCoreS3SE:
        case m5::board_t::board_M5Tab5:
            rst  = m5::pin_name_t::port_c_pin1;
            intr = m5::pin_name_t::port_c_pin2;
            return true;
        case m5::board_t::board_M5Dial:
        case m5::board_t::board_ArduinoNessoN1:
            rst  = m5::pin_name_t::port_b_pin1;
            intr = m5::pin_name_t::port_b_pin2;
            return true;
        default:
            return false;
    }
}
int8_t rst_pin()
{
    m5::pin_name_t rst{}, intr{};
    return select_int_rst_port(rst, intr) ? M5.getPin(rst) : -1;
}
int8_t int_pin()
{
    m5::pin_name_t rst{}, intr{};
    return select_int_rst_port(rst, intr) ? M5.getPin(intr) : -1;
}

constexpr char wiring_message[] = "Connect the StampHeart INT/RST GROVE (Port C or Port B, see select_int_rst_port())";

constexpr Mode mode_table[]         = {Mode::HR, Mode::SpO2, Mode::All};
constexpr uint8_t valid_table[]     = {0x01, 0x06, 0x07};  // Valid bits of each mode
constexpr SamplingRate rate_table[] = {SamplingRate::Rate25, SamplingRate::Rate50, SamplingRate::Rate100,
                                       SamplingRate::Rate200};
constexpr Gain gain_table[]         = {Gain::Level0, Gain::Level1, Gain::Level2, Gain::Level3,
                               Gain::Level4, Gain::Level5, Gain::Level6};
constexpr Color color_table[]       = {Color::Green, Color::Red, Color::IR};
constexpr uint8_t current_table[]   = {0, 1, 64, 128, 255};

bool wait_updated(UnitGH3038* unit, const uint32_t timeout_ms)
{
    const m5::utility::elapsed_time_t start_at = m5::utility::millis();
    do {
        unit->update();
        if (unit->updated()) {
            return true;
        }
        std::this_thread::yield();
    } while (!m5::utility::hasElapsed(start_at, timeout_ms));
    return false;
}

// Test-only subclass to reach the FIFO level not exposed by the public API
class GH3038Probe : public m5::unit::UnitGH3038 {
public:
    bool fifoUsed(uint16_t& v)
    {
        return read_fifo_level(v);
    }
};
}  // namespace

class TestGH3038 : public I2CComponentTestBase<UnitGH3038> {
protected:
    virtual bool begin() override
    {
        // NessoN1: I2C on QWIIC (Port A), as its GROVE (Port B) is used for INT/RST
        return m5::unit::wiring::addI2C(Units, *unit, unit->component_config().clock,
                                        m5::unit::wiring::NessoPort::PortA) &&
               Units.begin();
    }
    virtual UnitGH3038* get_instance() override
    {
        auto ptr = new GH3038Probe();
        if (ptr) {
            auto cfg    = ptr->config();
            cfg.rst_pin = rst_pin();
            cfg.int_pin = -1;  // Polling (the interrupt path is tested by TestGH3038Interrupt)
            ptr->config(cfg);
        }
        return ptr;
    }
};

TEST_F(TestGH3038, Begin)
{
    SCOPED_TRACE(ustr);

    EXPECT_GE(rst_pin(), 0) << wiring_message;
    EXPECT_TRUE(unit->inPeriodic());
    EXPECT_EQ(unit->mode(), Mode::All);
    EXPECT_EQ(unit->samplingRate(), SamplingRate::Rate100);
    EXPECT_FALSE(unit->isInterruptEnabled());

    const auto cfg = unit->config();
    Gain g{};
    uint8_t c{};
    EXPECT_TRUE(unit->readGain(g, Color::Green));
    EXPECT_EQ(g, cfg.green_gain);
    EXPECT_TRUE(unit->readGain(g, Color::Red));
    EXPECT_EQ(g, cfg.red_gain);
    EXPECT_TRUE(unit->readGain(g, Color::IR));
    EXPECT_EQ(g, cfg.ir_gain);
    EXPECT_TRUE(unit->readLEDCurrent(c, Color::Green));
    EXPECT_EQ(c, cfg.green_current);
    EXPECT_TRUE(unit->readLEDCurrent(c, Color::Red));
    EXPECT_EQ(c, cfg.red_current);
    EXPECT_TRUE(unit->readLEDCurrent(c, Color::IR));
    EXPECT_EQ(c, cfg.ir_current);
}

TEST_F(TestGH3038, SoftReset)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->softReset());
    EXPECT_FALSE(unit->inPeriodic());
    EXPECT_TRUE(unit->startPeriodicMeasurement());
    EXPECT_TRUE(unit->inPeriodic());
    EXPECT_TRUE(wait_updated(unit.get(), 1000));
}

TEST_F(TestGH3038, PeriodicModes)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_FALSE(unit->inPeriodic());

    for (uint8_t i = 0; i < m5::stl::size(mode_table); ++i) {
        const auto mode = mode_table[i];
        auto s          = m5::utility::formatString("Mode:%u", m5::stl::to_underlying(mode));
        SCOPED_TRACE(s);

        EXPECT_TRUE(unit->startPeriodicMeasurement(mode, SamplingRate::Rate100));
        EXPECT_TRUE(unit->inPeriodic());
        EXPECT_FALSE(unit->startPeriodicMeasurement(mode, SamplingRate::Rate100));  // Already started

        EXPECT_TRUE(wait_updated(unit.get(), 1000));
        EXPECT_GT(unit->available(), 0U);
        const auto cfg = unit->config();
        while (unit->available()) {
            const auto d = unit->oldest();
            EXPECT_EQ(d.valid, valid_table[i]);
            for (auto&& color : color_table) {
                if (d.has(color)) {
                    Gain expected{};
                    switch (color) {
                        case Color::Green:
                            expected = cfg.green_gain;
                            break;
                        case Color::Red:
                            expected = cfg.red_gain;
                            break;
                        case Color::IR:
                            expected = cfg.ir_gain;
                            break;
                    }
                    EXPECT_EQ(d.gain[color_index(color)], expected);
                } else {
                    EXPECT_EQ(d.value(color), 0);
                }
            }
            unit->discard();
        }

        EXPECT_TRUE(unit->stopPeriodicMeasurement());
        EXPECT_FALSE(unit->inPeriodic());
    }
}

TEST_F(TestGH3038, PeriodicRates)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->stopPeriodicMeasurement());

    for (auto&& rate : rate_table) {
        auto s = m5::utility::formatString("Rate:%u", m5::stl::to_underlying(rate));
        SCOPED_TRACE(s);

        EXPECT_TRUE(unit->startPeriodicMeasurement(Mode::All, rate));
        EXPECT_EQ(unit->samplingRate(), rate);
        EXPECT_TRUE(wait_updated(unit.get(), 1000));
        unit->flush();

        // Samples in 1 second: expect about the rate (+-20%, tolerance for the read timing)
        const auto hz                              = sampling_rate_to_hz(rate);
        uint32_t count                             = 0;
        const m5::utility::elapsed_time_t start_at = m5::utility::millis();
        while (!m5::utility::hasElapsed(start_at, 1000)) {
            unit->update();
            EXPECT_FALSE(unit->overflowed());
            if (unit->updated()) {
                count += unit->retrieved();
            }
            std::this_thread::yield();
        }
        EXPECT_GE(count, hz * 8 / 10);
        EXPECT_LE(count, hz * 12 / 10);

        EXPECT_TRUE(unit->stopPeriodicMeasurement());
    }
}

TEST_F(TestGH3038, StopStopsData)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(wait_updated(unit.get(), 1000));
    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    unit->flush();

    // The chip's FIFO must not grow after stop (i.e. it actually stopped sampling)
    auto probe = static_cast<GH3038Probe*>(unit.get());
    uint16_t a{};
    EXPECT_TRUE(probe->fifoUsed(a));
    m5::utility::delay(200);
    uint16_t b{};
    EXPECT_TRUE(probe->fifoUsed(b));
    EXPECT_EQ(a, b);

    unit->update(true);
    EXPECT_FALSE(unit->updated());
    EXPECT_EQ(unit->available(), 0U);
}

TEST_F(TestGH3038, GainAndCurrent)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->setAutoAdjust(false));  // Manual writes during periodic measurement need it off

    for (auto&& periodic : {true, false}) {
        auto s = m5::utility::formatString("Periodic:%u", periodic);
        SCOPED_TRACE(s);
        if (periodic) {
            if (!unit->inPeriodic()) {
                EXPECT_TRUE(unit->startPeriodicMeasurement(Mode::All, SamplingRate::Rate100));
            }
        } else {
            EXPECT_TRUE(unit->stopPeriodicMeasurement());
        }

        for (auto&& color : color_table) {
            for (auto&& gain : gain_table) {
                Gain g{};
                EXPECT_TRUE(unit->writeGain(color, gain));
                EXPECT_TRUE(unit->readGain(g, color));
                EXPECT_EQ(g, gain);
            }
            for (auto&& code : current_table) {
                uint8_t c{};
                EXPECT_TRUE(unit->writeLEDCurrent(color, code));
                EXPECT_TRUE(unit->readLEDCurrent(c, color));
                EXPECT_EQ(c, code);
            }
        }
    }

    // Invalid values
    EXPECT_FALSE(unit->writeGain(static_cast<Color>(3), Gain::Level0));
    EXPECT_FALSE(unit->writeGain(Color::Green, static_cast<Gain>(7)));
    EXPECT_FALSE(unit->writeLEDCurrent(static_cast<Color>(3), 0));
    Gain ig{};
    EXPECT_FALSE(unit->readGain(ig, static_cast<Color>(3)));
    uint8_t ic{};
    EXPECT_FALSE(unit->readLEDCurrent(ic, static_cast<Color>(3)));

    // The gain written before start is applied to the chip (readGain) and tagged in Data
    EXPECT_TRUE(unit->writeGain(Color::Red, Gain::Level2));
    EXPECT_TRUE(unit->startPeriodicMeasurement(Mode::SpO2, SamplingRate::Rate100));
    Gain g{};
    EXPECT_TRUE(unit->readGain(g, Color::Red));
    EXPECT_EQ(g, Gain::Level2);
    EXPECT_TRUE(wait_updated(unit.get(), 1000));
    EXPECT_EQ(unit->oldest().gain[color_index(Color::Red)], Gain::Level2);
}

TEST_F(TestGH3038, LEDCurrentOfSample)
{
    SCOPED_TRACE(ustr);

    // Data carries the LED current at the measurement, readPPG() converts it to the current at begin
    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_TRUE(unit->writeLEDCurrent(Color::Green, 32));
    EXPECT_TRUE(unit->startPeriodicMeasurement(Mode::HR, SamplingRate::Rate100));
    EXPECT_TRUE(wait_updated(unit.get(), 1000));

    const auto d = unit->oldest();
    EXPECT_EQ(d.led_current[color_index(Color::Green)], 32);
    m5::heart::PPGSample s{};
    EXPECT_TRUE(unit->readPPG(s));  // The first sample after the start is oldest()
    const float scale = static_cast<float>(unit->config().green_current) / 32.0f;
    EXPECT_FLOAT_EQ(s.get(m5::heart::Channel::Green), d.current(Color::Green) * scale);
}

TEST_F(TestGH3038, AutoAdjustGuard)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->autoAdjust());  // config_t default
    EXPECT_TRUE(unit->inPeriodic());

    // Refused while adjusting automatically during periodic measurement
    EXPECT_FALSE(unit->writeGain(Color::Green, Gain::Level2));
    EXPECT_FALSE(unit->writeLEDCurrent(Color::Green, 32));
    Gain g{};
    EXPECT_TRUE(unit->readGain(g, Color::Green));
    EXPECT_EQ(g, unit->config().green_gain);

    // Allowed after disabling
    EXPECT_TRUE(unit->setAutoAdjust(false));
    EXPECT_FALSE(unit->autoAdjust());
    EXPECT_TRUE(unit->writeGain(Color::Green, Gain::Level2));
    EXPECT_TRUE(unit->writeLEDCurrent(Color::Green, 32));
    uint8_t c{};
    EXPECT_TRUE(unit->readLEDCurrent(c, Color::Green));
    EXPECT_EQ(c, 32);

    // Allowed outside periodic measurement even if enabled
    EXPECT_TRUE(unit->setAutoAdjust(true));
    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_TRUE(unit->writeGain(Color::Green, Gain::Level1));
    EXPECT_TRUE(unit->readGain(g, Color::Green));
    EXPECT_EQ(g, Gain::Level1);
}

class TestGH3038NoAutoAdjust : public TestGH3038 {
protected:
    virtual UnitGH3038* get_instance() override
    {
        auto ptr = new GH3038Probe();
        if (ptr) {
            auto cfg        = ptr->config();
            cfg.rst_pin     = rst_pin();
            cfg.int_pin     = -1;
            cfg.auto_adjust = false;
            ptr->config(cfg);
        }
        return ptr;
    }
};

TEST_F(TestGH3038NoAutoAdjust, Config)
{
    SCOPED_TRACE(ustr);

    EXPECT_FALSE(unit->autoAdjust());
    EXPECT_TRUE(unit->inPeriodic());
    EXPECT_TRUE(unit->writeGain(Color::Green, Gain::Level2));
    EXPECT_TRUE(unit->writeLEDCurrent(Color::Green, 32));
}

class TestGH3038BeginConfig : public TestGH3038 {
protected:
    virtual UnitGH3038* get_instance() override
    {
        auto ptr = new GH3038Probe();
        if (ptr) {
            auto cfg          = ptr->config();
            cfg.rst_pin       = rst_pin();
            cfg.int_pin       = -1;
            cfg.mode          = Mode::HR;
            cfg.rate          = SamplingRate::Rate50;
            cfg.green_gain    = Gain::Level2;
            cfg.red_gain      = Gain::Level4;
            cfg.ir_gain       = Gain::Level5;
            cfg.green_current = 32;
            cfg.red_current   = 100;
            cfg.ir_current    = 90;
            cfg.auto_adjust   = false;  // Keep the gain and the current as configured
            ptr->config(cfg);
        }
        return ptr;
    }
};

TEST_F(TestGH3038BeginConfig, Applied)
{
    SCOPED_TRACE(ustr);
    using m5::heart::Channel;
    using m5::heart::channel_bit;

    EXPECT_TRUE(unit->inPeriodic());
    EXPECT_FALSE(unit->autoAdjust());
    EXPECT_EQ(unit->mode(), Mode::HR);
    EXPECT_EQ(unit->samplingRate(), SamplingRate::Rate50);

    // The gain and the current of every color (color_table order) are applied, also of the colors the mode does not use
    constexpr Gain expected_gain[]   = {Gain::Level2, Gain::Level4, Gain::Level5};
    constexpr uint8_t expected_cur[] = {32, 100, 90};
    for (uint8_t i = 0; i < 3; ++i) {
        Gain gain{};
        uint8_t current{};
        EXPECT_TRUE(unit->readGain(gain, color_table[i])) << i;
        EXPECT_EQ(gain, expected_gain[i]) << i;
        EXPECT_TRUE(unit->readLEDCurrent(current, color_table[i])) << i;
        EXPECT_EQ(current, expected_cur[i]) << i;
    }
    const auto spec = unit->ppgSpec();
    EXPECT_EQ(spec.channels, channel_bit(Channel::Green));
    EXPECT_FLOAT_EQ(spec.sampling_rate_hz, 50.0f);
}

class TestGH3038BeginNoPeriodic : public TestGH3038 {
protected:
    virtual UnitGH3038* get_instance() override
    {
        auto ptr = new GH3038Probe();
        if (ptr) {
            auto cfg           = ptr->config();
            cfg.rst_pin        = rst_pin();
            cfg.int_pin        = -1;
            cfg.mode           = Mode::SpO2;
            cfg.rate           = SamplingRate::Rate25;
            cfg.start_periodic = false;
            ptr->config(cfg);
        }
        return ptr;
    }
};

TEST_F(TestGH3038BeginNoPeriodic, NotStarted)
{
    SCOPED_TRACE(ustr);

    using m5::heart::Channel;
    using m5::heart::channel_bit;

    EXPECT_FALSE(unit->inPeriodic());
    EXPECT_FLOAT_EQ(unit->ppgSpec().sampling_rate_hz, 0.0f);
    // The mode and the rate are kept for startPeriodicMeasurement() without arguments
    EXPECT_EQ(unit->mode(), Mode::SpO2);
    EXPECT_EQ(unit->samplingRate(), SamplingRate::Rate25);

    EXPECT_TRUE(unit->startPeriodicMeasurement());
    EXPECT_TRUE(unit->inPeriodic());
    const auto spec = unit->ppgSpec();
    EXPECT_EQ(spec.channels, channel_bit(Channel::Red) | channel_bit(Channel::IR));
    EXPECT_FLOAT_EQ(spec.sampling_rate_hz, 25.0f);
    EXPECT_TRUE(unit->stopPeriodicMeasurement());
}

class TestGH3038Interrupt : public TestGH3038 {
protected:
    virtual UnitGH3038* get_instance() override
    {
        auto ptr = new GH3038Probe();
        if (ptr) {
            auto cfg    = ptr->config();
            cfg.rst_pin = rst_pin();
            cfg.int_pin = int_pin();
            ptr->config(cfg);
        }
        return ptr;
    }
};

TEST_F(TestGH3038Interrupt, Periodic)
{
    SCOPED_TRACE(ustr);

    EXPECT_GE(int_pin(), 0) << wiring_message;
    EXPECT_TRUE(unit->isInterruptEnabled());
    EXPECT_TRUE(unit->inPeriodic());

    EXPECT_TRUE(wait_updated(unit.get(), 1000));
    unit->flush();

    // The interrupt path must deliver about one event per waterline (10 frames at Rate100/All),
    // not one event per read as polling would give (~20 events of ~5)
    uint32_t count                             = 0;
    uint32_t events                            = 0;
    const m5::utility::elapsed_time_t start_at = m5::utility::millis();
    while (!m5::utility::hasElapsed(start_at, 1000)) {
        unit->update();
        EXPECT_FALSE(unit->overflowed());
        if (unit->updated()) {
            ++events;
            EXPECT_GE(unit->retrieved(), 8U);
            count += unit->retrieved();
        }
        std::this_thread::yield();
    }
    EXPECT_GE(count, 80U);  // Rate100
    EXPECT_LE(count, 120U);
    EXPECT_GE(events, 7U);
    EXPECT_LE(events, 13U);
}

namespace {
// Interrupt events and samples in a second
void count_events(UnitGH3038* u, uint32_t& events, uint32_t& count)
{
    events = count                             = 0;
    const m5::utility::elapsed_time_t start_at = m5::utility::millis();
    while (!m5::utility::hasElapsed(start_at, 1000)) {
        u->update();
        if (u->updated()) {
            ++events;
            count += u->retrieved();
        }
        std::this_thread::yield();
    }
}
}  // namespace

TEST_F(TestGH3038Interrupt, Move)
{
    SCOPED_TRACE(ustr);

    EXPECT_GE(int_pin(), 0) << wiring_message;
    EXPECT_TRUE(unit->isInterruptEnabled());
    EXPECT_TRUE(wait_updated(unit.get(), 1000));

    Gain gain_before{}, gain_after{};
    uint8_t current_before{}, current_after{};
    EXPECT_TRUE(unit->readGain(gain_before, Color::Green));
    EXPECT_TRUE(unit->readLEDCurrent(current_before, Color::Green));

    uint32_t events{}, count{};
    // Move construction: the interrupt and the I2C access go with the new object
    std::unique_ptr<UnitGH3038> first{new UnitGH3038(std::move(*unit))};
    EXPECT_FALSE(unit->isInterruptEnabled());
    EXPECT_TRUE(first->isInterruptEnabled());
    EXPECT_TRUE(first->inPeriodic());
    // Move again and destroy the source: its destructor must not remove the interrupt of the new object
    std::unique_ptr<UnitGH3038> second{new UnitGH3038(std::move(*first))};
    first.reset();
    EXPECT_TRUE(second->isInterruptEnabled());
    // The first call after the move reads from the chip: it must go through the new object
    EXPECT_TRUE(second->readGain(gain_after, Color::Green));
    EXPECT_TRUE(second->readLEDCurrent(current_after, Color::Green));
    EXPECT_EQ(gain_after, gain_before);
    EXPECT_EQ(current_after, current_before);
    count_events(second.get(), events, count);
    EXPECT_GE(count, 80U);  // Rate100
    EXPECT_LE(count, 120U);
    EXPECT_GE(events, 7U);  // About one event per waterline (interrupt, not polling)
    EXPECT_LE(events, 13U);
    EXPECT_TRUE(second->stopPeriodicMeasurement());
    EXPECT_TRUE(second->startPeriodicMeasurement(Mode::All, SamplingRate::Rate100));  // I2C through the new object

    // Move assignment back to the original (for the teardown)
    *unit = std::move(*second);
    second.reset();
    EXPECT_TRUE(unit->isInterruptEnabled());
    EXPECT_TRUE(wait_updated(unit.get(), 1000));
    count_events(unit.get(), events, count);
    EXPECT_GE(count, 80U);
    EXPECT_LE(count, 120U);
    EXPECT_GE(events, 7U);
    EXPECT_LE(events, 13U);
}

TEST_F(TestGH3038, PPGSpec)
{
    SCOPED_TRACE(ustr);
    using m5::heart::Channel;
    using m5::heart::channel_bit;

    // Default: All, 100 Hz
    EXPECT_TRUE(unit->inPeriodic());
    m5::heart::PPGSpec spec = unit->ppgSpec();
    EXPECT_EQ(spec.channels, channel_bit(Channel::Green) | channel_bit(Channel::Red) | channel_bit(Channel::IR));
    EXPECT_FLOAT_EQ(spec.sampling_rate_hz, 100.0f);
    EXPECT_EQ(spec.unit, m5::heart::ValueUnit::PicoAmpere);

    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_FLOAT_EQ(unit->ppgSpec().sampling_rate_hz, 0.0f);

    EXPECT_TRUE(unit->startPeriodicMeasurement(Mode::HR, SamplingRate::Rate25));
    spec = unit->ppgSpec();
    EXPECT_EQ(spec.channels, channel_bit(Channel::Green));
    EXPECT_FLOAT_EQ(spec.sampling_rate_hz, 25.0f);
    EXPECT_TRUE(unit->stopPeriodicMeasurement());

    EXPECT_TRUE(unit->startPeriodicMeasurement(Mode::SpO2, SamplingRate::Rate200));
    spec = unit->ppgSpec();
    EXPECT_EQ(spec.channels, channel_bit(Channel::Red) | channel_bit(Channel::IR));
    EXPECT_FLOAT_EQ(spec.sampling_rate_hz, 200.0f);
}

TEST_F(TestGH3038, ReadPPGIndependent)
{
    SCOPED_TRACE(ustr);
    using m5::heart::Channel;

    EXPECT_TRUE(unit->inPeriodic());
    EXPECT_TRUE(wait_updated(unit.get(), 1000));
    unit->flush();
    m5::heart::PPGSample s{};
    while (unit->readPPG(s)) {
        // Skip everything before the flush
    }

    EXPECT_TRUE(wait_updated(unit.get(), 1000));
    const auto avail = unit->available();
    EXPECT_GT(avail, 0U);
    const auto first = unit->oldest();

    uint32_t count{};
    while (unit->readPPG(s)) {
        if (count == 0) {
            EXPECT_TRUE(s.has(Channel::Green));
            EXPECT_TRUE(s.has(Channel::Red));
            EXPECT_TRUE(s.has(Channel::IR));
            EXPECT_FLOAT_EQ(s.get(Channel::Green), first.greenCurrent());
            EXPECT_FLOAT_EQ(s.get(Channel::Red), first.redCurrent());
            EXPECT_FLOAT_EQ(s.get(Channel::IR), first.irCurrent());
        }
        ++count;
    }
    EXPECT_EQ(count, avail);
    // readPPG() does not consume the stored data
    EXPECT_EQ(unit->available(), avail);
    EXPECT_EQ(unit->oldest().green(), first.green());
    // discard() / flush() do not bring back samples already read
    unit->discard();
    EXPECT_FALSE(unit->readPPG(s));
    unit->flush();
    EXPECT_FALSE(unit->readPPG(s));
}

TEST_F(TestGH3038, ReadPPGRate)
{
    SCOPED_TRACE(ustr);

    const float rate = unit->ppgSpec().sampling_rate_hz;
    EXPECT_FLOAT_EQ(rate, 100.0f);
    EXPECT_TRUE(wait_updated(unit.get(), 1000));
    m5::heart::PPGSample s{};
    while (unit->readPPG(s)) {
        // Skip the samples before the measurement window
    }

    uint32_t count{};
    const m5::utility::elapsed_time_t start_at = m5::utility::millis();
    while (!m5::utility::hasElapsed(start_at, 2000)) {
        unit->update();
        EXPECT_FALSE(unit->ppgOverflowed());
        while (unit->readPPG(s)) {
            ++count;
        }
        std::this_thread::yield();
    }
    EXPECT_NEAR(count, rate * 2.0f, rate * 2.0f * 0.1f);
}
