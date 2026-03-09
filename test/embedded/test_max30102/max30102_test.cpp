/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  UnitTest for UnitMAX30102
*/
#include <gtest/gtest.h>
#include <Wire.h>
#include <M5Unified.h>
#include <M5UnitUnified.hpp>
#include <googletest/test_template.hpp>
#include <googletest/test_helper.hpp>
#include <unit/unit_MAX30102.hpp>
#include <chrono>
#include <cmath>
#include <esp_random.h>

using namespace m5::unit::googletest;
using namespace m5::unit;
using namespace m5::unit::max30102;
using namespace m5::unit::max30102::command;

namespace hat {
struct I2cPins {
    int sda;
    int scl;
};

I2cPins get_hat_i2c_pins(const m5::board_t board)
{
    switch (board) {
        case m5::board_t::board_M5StickC:
        case m5::board_t::board_M5StickCPlus:
        case m5::board_t::board_M5StickCPlus2:
            return {0, 26};
        case m5::board_t::board_M5StickS3:
            return {8, 0};
        case m5::board_t::board_M5StackCoreInk:
            return {25, 26};
        case m5::board_t::board_ArduinoNessoN1:
            return {6, 7};
        default:
            return {-1, -1};
    }
}

}  // namespace hat

class TestMAX30102 : public I2CComponentTestBase<UnitMAX30102> {
protected:
    virtual bool begin() override
    {
        auto board      = M5.getBoard();
        const auto pins = hat::get_hat_i2c_pins(board);
        // NessoN1: Wire is used by M5Unified In_I2C; use Wire1 for Hat port
        auto& wire = (board == m5::board_t::board_ArduinoNessoN1) ? Wire1 : Wire;
        pinMode(pins.scl, OUTPUT);
        wire.end();
        wire.begin(pins.sda, pins.scl, unit->component_config().clock);
        return Units.add(*unit, wire) && Units.begin();
    }

    virtual UnitMAX30102* get_instance() override
    {
        auto ptr = new m5::unit::UnitMAX30102();
        if (ptr) {
            auto ccfg = ptr->component_config();
            ptr->component_config(ccfg);
        }
        return ptr;
    }
};

namespace {
// esp_random() used instead of std::default_random_engine

constexpr uint32_t STORED_SIZE{4};

// Same as unit_MAX30102.cpp
constexpr uint8_t spo2_table[] = {
    // LSB:50 MSB:3200
    0x0F, 0x0F, 0x0F, 0x0F, 0x07, 0x03, 0x01, 0x00,
};
constexpr uint8_t hr_table[] = {
    // LSB:50 MSB:3200
    0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x07, 0x01,
};
constexpr uint8_t none_table[] = {
    // LSB:50 MSB:3200
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

constexpr const uint8_t* allowed_setting_table[] = {
    none_table, none_table, hr_table, spo2_table, none_table, none_table, none_table, spo2_table,
};

constexpr uint32_t adc_resolution_bits_table[] = {
    0x007FFF,  // 15 bits
    0x00FFFF,  // 16 bits
    0x01FFFF,  // 17 bits
    0x03FFFF,  // 18 bits
};

bool is_allowed_settings(const Mode mode, const Sampling rate, const LEDPulse pw)
{
    return allowed_setting_table[m5::stl::to_underlying(mode)][m5::stl::to_underlying(rate)] &
           (1U << m5::stl::to_underlying(pw));
}

constexpr Mode mode_table[] = {
    Mode::SpO2,
    Mode::HROnly,
    Mode::MultiLED,
};

constexpr ADC range_table[] = {
    ADC::Range2048nA,
    ADC::Range4096nA,
    ADC::Range8192nA,
    ADC::Range16384nA,
};

constexpr Sampling sr_table[] = {
    Sampling::Rate50,  Sampling::Rate100,  Sampling::Rate200,  Sampling::Rate400,
    Sampling::Rate800, Sampling::Rate1000, Sampling::Rate1600, Sampling::Rate3200,
};

constexpr LEDPulse pw_table[] = {
    LEDPulse::Width69,
    LEDPulse::Width118,
    LEDPulse::Width215,
    LEDPulse::Width411,
};

constexpr FIFOSampling fs_table[] = {
    FIFOSampling::Average1, FIFOSampling::Average2,  FIFOSampling::Average4,
    FIFOSampling::Average8, FIFOSampling::Average16, FIFOSampling::Average32,
};

constexpr std::array<Slot, 2> slots_table[] = {
    //
    {Slot::None, Slot::None},
    //
    {Slot::IR, Slot::None},
    {Slot::Red, Slot::None},
    //
    {Slot::IR, Slot::IR},
    {Slot::IR, Slot::Red},
    {Slot::Red, Slot::IR},
    {Slot::Red, Slot::Red},
};

constexpr std::array<Slot, 2> invalid_slots_table[] = {
    {Slot::None, Slot::IR},
    {Slot::None, Slot::Red},
};

void test_spo2_config(UnitMAX30102* unit, const Mode mode)
{
    EXPECT_TRUE(unit->writeMode(mode));

    for (auto& rg : range_table) {
        for (auto&& sr : sr_table) {
            for (auto&& pw : pw_table) {
                auto s = m5::utility::formatString("Mode:%u RNG:%u Rate:%u Width:%u", mode, rg, sr, pw);
                SCOPED_TRACE(s);

                ADC range{};
                Sampling rate{};
                LEDPulse width{};

                if (is_allowed_settings(mode, sr, pw)) {
                    EXPECT_TRUE(unit->writeSpO2Configuration(rg, sr, pw));

                    EXPECT_TRUE(unit->readSpO2Configuration(range, rate, width));
                    EXPECT_EQ(range, rg);
                    EXPECT_EQ(rate, sr);
                    EXPECT_EQ(width, pw);
                } else {
                    EXPECT_TRUE(unit->readSpO2Configuration(range, rate, width));

                    EXPECT_FALSE(unit->writeSpO2Configuration(rg, sr, pw));

                    ADC range2{};
                    Sampling rate2{};
                    LEDPulse width2{};
                    EXPECT_TRUE(unit->readSpO2Configuration(range2, rate2, width2));
                    EXPECT_EQ(range2, range);
                    EXPECT_EQ(rate2, rate);
                    EXPECT_EQ(width2, width);
                }
            }
        }
    }
}

// Verify individual SpO2 parameter read/write APIs (roundtrip)
void test_spo2_config_each(UnitMAX30102* unit, const Mode mode)
{
    auto s = m5::utility::formatString("Mode:%u", mode);
    SCOPED_TRACE(s);

    EXPECT_TRUE(unit->writeMode(mode));
    EXPECT_TRUE(unit->writeSpO2Configuration(ADC::Range2048nA, Sampling::Rate50, LEDPulse::Width69));

    // ADC range
    for (auto& rg : range_table) {
        ADC range{};
        EXPECT_TRUE(unit->writeSpO2ADCRange(rg));
        EXPECT_TRUE(unit->readSpO2ADCRange(range));
        EXPECT_EQ(range, rg);
    }

    // Sampling rate (allowed settings only)
    for (auto&& sr : sr_table) {
        if (is_allowed_settings(mode, sr, LEDPulse::Width69)) {
            Sampling rate{};
            EXPECT_TRUE(unit->writeSpO2SamplingRate(sr));
            EXPECT_TRUE(unit->readSpO2SamplingRate(rate));
            EXPECT_EQ(rate, sr);
        }
    }
    EXPECT_TRUE(unit->writeSpO2SamplingRate(Sampling::Rate50));

    // LED pulse width (allowed settings only)
    for (auto&& pw : pw_table) {
        if (is_allowed_settings(mode, Sampling::Rate50, pw)) {
            LEDPulse width{};
            EXPECT_TRUE(unit->writeSpO2LEDPulseWidth(pw));
            EXPECT_TRUE(unit->readSpO2LEDPulseWidth(width));
            EXPECT_EQ(width, pw);
        }
    }
}

template <class U>
void collect_and_verify(U* unit, uint32_t count, bool expect_ir, bool expect_red, uint32_t mask = 0)
{
    auto timeout = std::max<uint32_t>(unit->interval() * (count + 1) * 2, 2000);
    auto result  = collect_periodic_measurements(unit, count, timeout);

    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_FALSE(unit->inPeriodic());
    EXPECT_FALSE(result.timed_out);

    EXPECT_GE(unit->available(), count);
    EXPECT_FALSE(unit->empty());

    uint32_t cnt{unit->available() / 2};
    uint32_t left = unit->available() - cnt;
    uint32_t air{}, ared{};
    while (cnt-- && unit->available()) {
        air += unit->ir();
        ared += unit->red();
        if (mask) {
            EXPECT_LE(unit->ir(), mask);
            EXPECT_LE(unit->red(), mask);
        }
        EXPECT_EQ(unit->oldest().ir(), unit->ir());
        EXPECT_EQ(unit->oldest().red(), unit->red());
        EXPECT_FALSE(unit->empty());
        unit->discard();
    }

    if (expect_ir) {
        EXPECT_NE(air, 0);
    } else {
        EXPECT_EQ(air, 0);
    }
    if (expect_red) {
        EXPECT_NE(ared, 0);
    } else {
        EXPECT_EQ(ared, 0);
    }

    EXPECT_EQ(unit->available(), left);
    EXPECT_FALSE(unit->empty());
    EXPECT_FALSE(unit->full());

    unit->flush();
    EXPECT_EQ(unit->available(), 0);
    EXPECT_TRUE(unit->empty());
    EXPECT_FALSE(unit->full());

    EXPECT_EQ(unit->ir(), 0);
    EXPECT_EQ(unit->red(), 0);
}

void test_periodic_spo2(UnitMAX30102* unit)
{
    // Pairwise
    constexpr std::tuple<ADC, Sampling, LEDPulse, FIFOSampling> cond_table[] = {
        {ADC::Range2048nA, Sampling::Rate50, LEDPulse::Width118, FIFOSampling::Average1},
        {ADC::Range8192nA, Sampling::Rate100, LEDPulse::Width215, FIFOSampling::Average16},
        {ADC::Range4096nA, Sampling::Rate100, LEDPulse::Width411, FIFOSampling::Average4},
        {ADC::Range4096nA, Sampling::Rate200, LEDPulse::Width215, FIFOSampling::Average8},
        {ADC::Range8192nA, Sampling::Rate1000, LEDPulse::Width69, FIFOSampling::Average8},
        {ADC::Range4096nA, Sampling::Rate800, LEDPulse::Width69, FIFOSampling::Average1},
        {ADC::Range16384nA, Sampling::Rate400, LEDPulse::Width118, FIFOSampling::Average2},
        {ADC::Range2048nA, Sampling::Rate200, LEDPulse::Width69, FIFOSampling::Average16},
        {ADC::Range16384nA, Sampling::Rate800, LEDPulse::Width215, FIFOSampling::Average32},
        {ADC::Range16384nA, Sampling::Rate1600, LEDPulse::Width69, FIFOSampling::Average1},
        {ADC::Range8192nA, Sampling::Rate200, LEDPulse::Width411, FIFOSampling::Average1},
        {ADC::Range2048nA, Sampling::Rate50, LEDPulse::Width411, FIFOSampling::Average2},
        {ADC::Range8192nA, Sampling::Rate800, LEDPulse::Width118, FIFOSampling::Average4},
        {ADC::Range2048nA, Sampling::Rate1600, LEDPulse::Width69, FIFOSampling::Average4},
        {ADC::Range4096nA, Sampling::Rate1000, LEDPulse::Width118, FIFOSampling::Average32},
        {ADC::Range4096nA, Sampling::Rate50, LEDPulse::Width215, FIFOSampling::Average4},
        {ADC::Range8192nA, Sampling::Rate400, LEDPulse::Width69, FIFOSampling::Average32},
        {ADC::Range2048nA, Sampling::Rate800, LEDPulse::Width215, FIFOSampling::Average8},
        {ADC::Range16384nA, Sampling::Rate100, LEDPulse::Width411, FIFOSampling::Average1},
        {ADC::Range16384nA, Sampling::Rate1000, LEDPulse::Width118, FIFOSampling::Average16},
        {ADC::Range8192nA, Sampling::Rate200, LEDPulse::Width69, FIFOSampling::Average2},
        {ADC::Range16384nA, Sampling::Rate1000, LEDPulse::Width69, FIFOSampling::Average4},
        {ADC::Range4096nA, Sampling::Rate400, LEDPulse::Width215, FIFOSampling::Average1},
        {ADC::Range4096nA, Sampling::Rate800, LEDPulse::Width215, FIFOSampling::Average2},
        {ADC::Range2048nA, Sampling::Rate100, LEDPulse::Width118, FIFOSampling::Average8},
        {ADC::Range16384nA, Sampling::Rate50, LEDPulse::Width69, FIFOSampling::Average32},
        {ADC::Range8192nA, Sampling::Rate50, LEDPulse::Width411, FIFOSampling::Average16},
        {ADC::Range8192nA, Sampling::Rate1600, LEDPulse::Width69, FIFOSampling::Average8},
        {ADC::Range2048nA, Sampling::Rate400, LEDPulse::Width411, FIFOSampling::Average8},
        {ADC::Range4096nA, Sampling::Rate1600, LEDPulse::Width69, FIFOSampling::Average2},
        {ADC::Range2048nA, Sampling::Rate1000, LEDPulse::Width69, FIFOSampling::Average1},
        {ADC::Range2048nA, Sampling::Rate100, LEDPulse::Width69, FIFOSampling::Average32},
        {ADC::Range4096nA, Sampling::Rate800, LEDPulse::Width69, FIFOSampling::Average16},
        {ADC::Range2048nA, Sampling::Rate1000, LEDPulse::Width69, FIFOSampling::Average2},
        {ADC::Range16384nA, Sampling::Rate200, LEDPulse::Width411, FIFOSampling::Average32},
        {ADC::Range2048nA, Sampling::Rate1600, LEDPulse::Width69, FIFOSampling::Average16},
        {ADC::Range2048nA, Sampling::Rate100, LEDPulse::Width69, FIFOSampling::Average2},
        {ADC::Range2048nA, Sampling::Rate1600, LEDPulse::Width69, FIFOSampling::Average32},
        {ADC::Range16384nA, Sampling::Rate50, LEDPulse::Width69, FIFOSampling::Average8},
        {ADC::Range2048nA, Sampling::Rate400, LEDPulse::Width69, FIFOSampling::Average4},
        {ADC::Range2048nA, Sampling::Rate400, LEDPulse::Width69, FIFOSampling::Average16},
        {ADC::Range2048nA, Sampling::Rate200, LEDPulse::Width118, FIFOSampling::Average4},
    };

    for (auto&& cond : cond_table) {
        ADC range{};
        Sampling rate{};
        LEDPulse width{};
        FIFOSampling avg{};
        std::tie(range, rate, width, avg) = cond;

        auto s = m5::utility::formatString("SPO2 RNG:%u SR:%u WID:%u AVG:%u", range, rate, width, avg);
        SCOPED_TRACE(s);

        uint32_t mask = adc_resolution_bits_table[m5::stl::to_underlying(width)];
        EXPECT_TRUE(unit->startPeriodicMeasurement(Mode::SpO2, range, rate, width, avg, 0x1f, 0x1f));
        collect_and_verify(unit, STORED_SIZE, true, true, mask);
    }
}

void test_periodic_hr(UnitMAX30102* unit)
{
    // Pairwise
    constexpr std::tuple<ADC, Sampling, LEDPulse, FIFOSampling> cond_table[] = {
        {ADC::Range8192nA, Sampling::Rate3200, LEDPulse::Width69, FIFOSampling::Average4},
        {ADC::Range2048nA, Sampling::Rate50, LEDPulse::Width118, FIFOSampling::Average1},
        {ADC::Range8192nA, Sampling::Rate100, LEDPulse::Width215, FIFOSampling::Average16},
        {ADC::Range4096nA, Sampling::Rate100, LEDPulse::Width411, FIFOSampling::Average4},
        {ADC::Range4096nA, Sampling::Rate200, LEDPulse::Width215, FIFOSampling::Average8},
        {ADC::Range8192nA, Sampling::Rate1000, LEDPulse::Width118, FIFOSampling::Average8},
        {ADC::Range4096nA, Sampling::Rate1000, LEDPulse::Width69, FIFOSampling::Average1},
        {ADC::Range16384nA, Sampling::Rate400, LEDPulse::Width118, FIFOSampling::Average2},
        {ADC::Range2048nA, Sampling::Rate200, LEDPulse::Width69, FIFOSampling::Average16},
        {ADC::Range8192nA, Sampling::Rate800, LEDPulse::Width411, FIFOSampling::Average32},
        {ADC::Range16384nA, Sampling::Rate1000, LEDPulse::Width215, FIFOSampling::Average32},
        {ADC::Range2048nA, Sampling::Rate1600, LEDPulse::Width215, FIFOSampling::Average1},
        {ADC::Range16384nA, Sampling::Rate200, LEDPulse::Width411, FIFOSampling::Average1},
        {ADC::Range2048nA, Sampling::Rate50, LEDPulse::Width411, FIFOSampling::Average2},
        {ADC::Range8192nA, Sampling::Rate400, LEDPulse::Width215, FIFOSampling::Average1},
        {ADC::Range8192nA, Sampling::Rate200, LEDPulse::Width118, FIFOSampling::Average32},
        {ADC::Range4096nA, Sampling::Rate1600, LEDPulse::Width118, FIFOSampling::Average4},
        {ADC::Range4096nA, Sampling::Rate3200, LEDPulse::Width69, FIFOSampling::Average32},
        {ADC::Range4096nA, Sampling::Rate50, LEDPulse::Width215, FIFOSampling::Average4},
        {ADC::Range2048nA, Sampling::Rate800, LEDPulse::Width118, FIFOSampling::Average4},
        {ADC::Range4096nA, Sampling::Rate800, LEDPulse::Width69, FIFOSampling::Average2},
        {ADC::Range16384nA, Sampling::Rate800, LEDPulse::Width69, FIFOSampling::Average8},
        {ADC::Range16384nA, Sampling::Rate3200, LEDPulse::Width69, FIFOSampling::Average1},
        {ADC::Range2048nA, Sampling::Rate3200, LEDPulse::Width69, FIFOSampling::Average8},
        {ADC::Range16384nA, Sampling::Rate100, LEDPulse::Width118, FIFOSampling::Average1},
        {ADC::Range4096nA, Sampling::Rate1000, LEDPulse::Width411, FIFOSampling::Average16},
        {ADC::Range8192nA, Sampling::Rate200, LEDPulse::Width215, FIFOSampling::Average2},
        {ADC::Range16384nA, Sampling::Rate1000, LEDPulse::Width69, FIFOSampling::Average4},
        {ADC::Range16384nA, Sampling::Rate1600, LEDPulse::Width118, FIFOSampling::Average16},
        {ADC::Range2048nA, Sampling::Rate800, LEDPulse::Width215, FIFOSampling::Average1},
        {ADC::Range2048nA, Sampling::Rate400, LEDPulse::Width69, FIFOSampling::Average32},
        {ADC::Range4096nA, Sampling::Rate400, LEDPulse::Width411, FIFOSampling::Average8},
        {ADC::Range2048nA, Sampling::Rate100, LEDPulse::Width69, FIFOSampling::Average32},
        {ADC::Range16384nA, Sampling::Rate50, LEDPulse::Width69, FIFOSampling::Average32},
        {ADC::Range8192nA, Sampling::Rate50, LEDPulse::Width69, FIFOSampling::Average16},
        {ADC::Range8192nA, Sampling::Rate1600, LEDPulse::Width69, FIFOSampling::Average8},
        {ADC::Range2048nA, Sampling::Rate1600, LEDPulse::Width69, FIFOSampling::Average2},
        {ADC::Range2048nA, Sampling::Rate800, LEDPulse::Width69, FIFOSampling::Average16},
        {ADC::Range2048nA, Sampling::Rate1000, LEDPulse::Width69, FIFOSampling::Average2},
        {ADC::Range2048nA, Sampling::Rate100, LEDPulse::Width69, FIFOSampling::Average2},
        {ADC::Range2048nA, Sampling::Rate1600, LEDPulse::Width69, FIFOSampling::Average32},
        {ADC::Range2048nA, Sampling::Rate3200, LEDPulse::Width69, FIFOSampling::Average16},
        {ADC::Range2048nA, Sampling::Rate400, LEDPulse::Width69, FIFOSampling::Average4},
        {ADC::Range2048nA, Sampling::Rate100, LEDPulse::Width69, FIFOSampling::Average8},
        {ADC::Range2048nA, Sampling::Rate3200, LEDPulse::Width69, FIFOSampling::Average2},
        {ADC::Range2048nA, Sampling::Rate50, LEDPulse::Width69, FIFOSampling::Average8},
        {ADC::Range2048nA, Sampling::Rate400, LEDPulse::Width69, FIFOSampling::Average16},
        {ADC::Range2048nA, Sampling::Rate200, LEDPulse::Width69, FIFOSampling::Average4},
    };

    for (auto&& cond : cond_table) {
        ADC range{};
        Sampling rate{};
        LEDPulse width{};
        FIFOSampling avg{};
        std::tie(range, rate, width, avg) = cond;

        auto s = m5::utility::formatString("HR RNG:%u SR:%u WID:%u AVG:%u", range, rate, width, avg);
        SCOPED_TRACE(s);

        uint32_t mask = adc_resolution_bits_table[m5::stl::to_underlying(width)];
        EXPECT_TRUE(unit->startPeriodicMeasurement(Mode::HROnly, range, rate, width, avg, 0x1f, 0x1f));
        collect_and_verify(unit, STORED_SIZE, true, false, mask);
    }
}

void test_periodic_multi(UnitMAX30102* unit)
{
    constexpr std::tuple<Slot, Slot> cond_table[] = {
        {Slot::IR, Slot::Red},
        {Slot::Red, Slot::Red},
        {Slot::IR, Slot::None},
        {Slot::Red, Slot::None},
    };

    for (auto&& cond : cond_table) {
        Slot slot1{}, slot2{};
        std::tie(slot1, slot2) = cond;

        auto s = m5::utility::formatString("Multi %u/%u", slot1, slot2);
        SCOPED_TRACE(s);

        EXPECT_TRUE(unit->writeMultiLEDModeControl(slot1, slot2));
        EXPECT_TRUE(unit->startPeriodicMeasurement());
        bool has_ir  = (slot1 == Slot::IR || slot2 == Slot::IR);
        bool has_red = (slot1 == Slot::Red || slot2 == Slot::Red);
        collect_and_verify(unit, STORED_SIZE, has_ir, has_red, 0x3FFFF);
    }
}

}  // namespace

TEST_F(TestMAX30102, Mode)
{
    constexpr bool bool_table[] = {true, false};

    SCOPED_TRACE(ustr);
    EXPECT_TRUE(unit->inPeriodic());

    // Failed if in periodic
    for (auto&& m : mode_table) {
        EXPECT_FALSE(unit->writeMode(m));
    }
    for (auto&& shdn : bool_table) {
        EXPECT_FALSE(unit->writeShutdownControl(shdn));
    }

    //
    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_FALSE(unit->inPeriodic());

    // Mode
    for (auto&& m : mode_table) {
        EXPECT_TRUE(unit->writeMode(m));
        Mode m2{};
        EXPECT_TRUE(unit->readMode(m2));
        EXPECT_EQ(m2, m);
    }

    // SHDN
    for (auto&& shdn : bool_table) {
        EXPECT_TRUE(unit->writeShutdownControl(shdn));
        bool shdn2{};
        EXPECT_TRUE(unit->readShutdownControl(shdn2));
        EXPECT_EQ(shdn2, shdn);
    }
}

TEST_F(TestMAX30102, SpO2Configuration)
{
    SCOPED_TRACE(ustr);

    // Failed if in periodic
    EXPECT_FALSE(unit->writeSpO2Configuration(ADC::Range2048nA, Sampling::Rate50, LEDPulse::Width69));

    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_FALSE(unit->inPeriodic());

    test_spo2_config(unit.get(), Mode::SpO2);
    test_spo2_config_each(unit.get(), Mode::SpO2);

    test_spo2_config(unit.get(), Mode::HROnly);
    test_spo2_config_each(unit.get(), Mode::HROnly);

    test_spo2_config(unit.get(), Mode::MultiLED);
    test_spo2_config_each(unit.get(), Mode::MultiLED);
}

TEST_F(TestMAX30102, LEDCurrent)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_FALSE(unit->inPeriodic());

    // Boundary values: min, near-min, mid, near-max, max
    constexpr uint8_t boundary[] = {0, 1, 127, 128, 254, 255};
    for (auto cur : boundary) {
        auto s = m5::utility::formatString("cur:%u", cur);
        SCOPED_TRACE(s);

        EXPECT_TRUE(unit->writeLEDCurrent(0, cur));
        EXPECT_TRUE(unit->writeLEDCurrent(1, cur));

        uint8_t raw{};
        EXPECT_TRUE(unit->readLEDCurrent(raw, 0));
        EXPECT_EQ(raw, cur);
        EXPECT_TRUE(unit->readLEDCurrent(raw, 1));
        EXPECT_EQ(raw, cur);

        float mA = (255 - cur) * 0.2f;
        EXPECT_TRUE(unit->writeLEDCurrent(0, mA));
        EXPECT_TRUE(unit->writeLEDCurrent(1, mA));

        float f{};
        EXPECT_TRUE(unit->readLEDCurrent(f, 0));
        EXPECT_FLOAT_EQ(f, mA);
        EXPECT_TRUE(unit->readLEDCurrent(f, 1));
        EXPECT_FLOAT_EQ(f, mA);
    }

    // Out-of-range (float)
    EXPECT_FALSE(unit->writeLEDCurrent(0, -0.01f));
    EXPECT_FALSE(unit->writeLEDCurrent(1, -0.01f));

    EXPECT_FALSE(unit->writeLEDCurrent(0, 51.01f));
    EXPECT_FALSE(unit->writeLEDCurrent(1, 51.01f));
}

TEST_F(TestMAX30102, MultiLEDMode)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_FALSE(unit->inPeriodic());

    // MultLED mode
    EXPECT_TRUE(unit->writeMode(Mode::MultiLED));

    // valid
    Slot slot1{}, slot2{};
    for (auto&& slots : slots_table) {
        auto s = m5::utility::formatString("0:%u 1:%u", slots[0], slots[1]);
        SCOPED_TRACE(s);

        EXPECT_TRUE(unit->writeMultiLEDModeControl(slots[0], slots[1]));

        EXPECT_TRUE(unit->readMultiLEDModeControl(slot1, slot2));
        EXPECT_EQ(slot1, slots[0]);
        EXPECT_EQ(slot2, slots[1]);
    }

    // invalid
    for (auto&& slots : invalid_slots_table) {
        auto s = m5::utility::formatString("0:%u 1:%u", slots[0], slots[1]);
        SCOPED_TRACE(s);

        EXPECT_FALSE(unit->writeMultiLEDModeControl(slots[0], slots[1]));

        Slot s1{}, s2{};
        EXPECT_TRUE(unit->readMultiLEDModeControl(s1, s2));
        EXPECT_EQ(s1, slot1);
        EXPECT_EQ(s2, slot2);
    }

    // All invalid (slots can be set for MultiLED mode only).
    constexpr Mode m_table[] = {
        Mode::SpO2,
        Mode::HROnly,
    };
    for (auto&& mode : m_table) {
        auto s = m5::utility::formatString("mode:%u", mode);
        SCOPED_TRACE(s);

        EXPECT_TRUE(unit->writeMode(mode));

        for (auto&& slots : slots_table) {
            auto s = m5::utility::formatString("0:%u 1:%u", slots[0], slots[1]);
            SCOPED_TRACE(s);

            EXPECT_FALSE(unit->writeMultiLEDModeControl(slots[0], slots[1]));
            Slot s1{}, s2{};
            EXPECT_TRUE(unit->readMultiLEDModeControl(s1, s2));
            EXPECT_EQ(s1, slot1);
            EXPECT_EQ(s2, slot2);
        }
        for (auto&& slots : invalid_slots_table) {
            auto s = m5::utility::formatString("0:%u 1:%u", slots[0], slots[1]);
            SCOPED_TRACE(s);

            EXPECT_FALSE(unit->writeMultiLEDModeControl(slots[0], slots[1]));
            Slot s1{}, s2{};
            EXPECT_TRUE(unit->readMultiLEDModeControl(s1, s2));
            EXPECT_EQ(s1, slot1);
            EXPECT_EQ(s2, slot2);
        }
    }
}

TEST_F(TestMAX30102, FIFOConfiguration)
{
    SCOPED_TRACE(ustr);

    // Failed if in periodic
    EXPECT_FALSE(unit->writeFIFOConfiguration(FIFOSampling::Average1, true, 15));

    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_FALSE(unit->inPeriodic());

    for (auto&& fs : fs_table) {
        bool ro    = esp_random() % 2;
        uint8_t af = esp_random() % 0x0F;
        auto s     = m5::utility::formatString("FS:%u RO:%u AF:%u", fs, ro, af);
        SCOPED_TRACE(s);
        EXPECT_TRUE(unit->writeFIFOConfiguration(fs, ro, af));

        FIFOSampling avg{};
        bool rollover{};
        uint8_t almostFull{};
        EXPECT_TRUE(unit->readFIFOConfiguration(avg, rollover, almostFull));
        EXPECT_EQ(avg, fs);
        EXPECT_EQ(rollover, ro);
        EXPECT_EQ(almostFull, af);
    }
}

TEST_F(TestMAX30102, Temperature)
{
    SCOPED_TRACE(ustr);

    for (auto&& m : mode_table) {
        auto s = m5::utility::formatString("Mode:%u", m);
        SCOPED_TRACE(s);

        EXPECT_TRUE(unit->stopPeriodicMeasurement());  // to Power-save mode
        EXPECT_FALSE(unit->inPeriodic());
        EXPECT_TRUE(unit->writeMode(m));

        TemperatureData td{};
        uint32_t cnt{4};

        // Does not work in power-save mode
        while (cnt--) {
            EXPECT_FALSE(unit->measureTemperatureSingleshot(td));
            EXPECT_FALSE(std::isfinite(td.celsius()));
            EXPECT_FALSE(std::isfinite(td.fahrenheit()));
        }

        // Work in not power-save mode
        EXPECT_TRUE(unit->writeShutdownControl(false));
        cnt = 4;
        while (cnt--) {
            EXPECT_TRUE(unit->measureTemperatureSingleshot(td));
            EXPECT_TRUE(std::isfinite(td.celsius()));
            EXPECT_TRUE(std::isfinite(td.fahrenheit()));
            // M5_LOGI("TempS>C:%f F:%f", td.celsius(), td.fahrenheit());
        }

        // Measurement is possible during periodic measurements
        EXPECT_TRUE(unit->startPeriodicMeasurement());
        EXPECT_TRUE(unit->inPeriodic());
        cnt = 4;
        while (cnt--) {
            EXPECT_TRUE(unit->measureTemperatureSingleshot(td));
            EXPECT_TRUE(std::isfinite(td.celsius()));
            EXPECT_TRUE(std::isfinite(td.fahrenheit()));
            // M5_LOGI("TempS>C:%f F:%f", td.celsius(), td.fahrenheit());
        }
    }
}

TEST_F(TestMAX30102, Revision)
{
    SCOPED_TRACE(ustr);

    uint8_t rev{};
    EXPECT_TRUE(unit->readRevisionID(rev));
    EXPECT_NE(rev, 0);
}

TEST_F(TestMAX30102, Reset)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_FALSE(unit->inPeriodic());

    //
    EXPECT_TRUE(unit->writeMode(Mode::MultiLED));
    EXPECT_TRUE(unit->writeSpO2Configuration(ADC::Range16384nA, Sampling::Rate400, LEDPulse::Width411));
    EXPECT_TRUE(unit->writeLEDCurrent(0, 255));
    EXPECT_TRUE(unit->writeLEDCurrent(1, 255));
    EXPECT_TRUE(unit->writeMultiLEDModeControl(Slot::IR, Slot::Red));
    EXPECT_TRUE(unit->writeFIFOConfiguration(FIFOSampling::Average16, true, 8));

    EXPECT_TRUE(unit->writeFIFOReadPointer(1));
    EXPECT_TRUE(unit->writeFIFOWritePointer(1));
    EXPECT_TRUE(unit->writeFIFOOverflowCounter(1));

    EXPECT_TRUE(unit->startPeriodicMeasurement());
    EXPECT_TRUE(unit->inPeriodic());

    //
    EXPECT_TRUE(unit->reset());

    //
    Mode mode{};
    EXPECT_TRUE(unit->readMode(mode));
    EXPECT_EQ(mode, Mode::None);

    ADC range{};
    Sampling rate{};
    LEDPulse width{};
    EXPECT_TRUE(unit->readSpO2Configuration(range, rate, width));
    EXPECT_EQ(range, ADC::Range2048nA);
    EXPECT_EQ(rate, Sampling::Rate50);
    EXPECT_EQ(width, LEDPulse::Width69);

    uint8_t led1{}, led2{};
    EXPECT_TRUE(unit->readLEDCurrent(led1, 0));
    EXPECT_TRUE(unit->readLEDCurrent(led2, 1));
    EXPECT_EQ(led1, 0);
    EXPECT_EQ(led2, 0);

    Slot slot1{}, slot2{};
    EXPECT_TRUE(unit->readMultiLEDModeControl(slot1, slot2));
    EXPECT_EQ(slot1, Slot::None);
    EXPECT_EQ(slot2, Slot::None);

    FIFOSampling avg{};
    bool rollover{};
    uint8_t almostFull{};
    EXPECT_TRUE(unit->readFIFOConfiguration(avg, rollover, almostFull));
    EXPECT_EQ(avg, FIFOSampling::Average1);
    EXPECT_FALSE(rollover);
    EXPECT_EQ(almostFull, 15);  // In the datasheet the POR state is 0, but...

    uint8_t rptr{0xFF}, wptr{0xFF}, cnt{0xFF};
    EXPECT_TRUE(unit->readFIFOReadPointer(rptr));
    EXPECT_TRUE(unit->readFIFOWritePointer(wptr));
    EXPECT_TRUE(unit->readFIFOOverflowCounter(cnt));
    EXPECT_EQ(rptr, 0U);
    EXPECT_EQ(wptr, 0U);
    EXPECT_EQ(cnt, 0U);
}

TEST_F(TestMAX30102, Periodic)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->inPeriodic());
    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_FALSE(unit->inPeriodic());

    // 100 sps
    EXPECT_TRUE(unit->startPeriodicMeasurement(Mode::SpO2, ADC::Range4096nA, Sampling::Rate100, LEDPulse::Width411,
                                               FIFOSampling::Average1, 0x1F, 0x1F));

    // Wait first updated
    auto start_at = m5::utility::millis();
    do {
        unit->update();
        std::this_thread::yield();
    } while (!unit->updated() && m5::utility::millis() - start_at <= 1000);
    EXPECT_TRUE(unit->updated());

    EXPECT_FALSE(unit->full());
    EXPECT_FALSE(unit->empty());
    EXPECT_GT(unit->available(), 0U);

    while (unit->available()) {
        EXPECT_EQ(unit->ir(), unit->oldest().ir());
        EXPECT_EQ(unit->red(), unit->oldest().red());
        unit->discard();
    }

    // Sampling about 10 times (not overflow)
    m5::utility::delay(100);

    unit->update();
    EXPECT_TRUE(unit->updated());

    EXPECT_GE(unit->available(), 10U);
    auto retrieved = unit->retrieved();
    EXPECT_GT(retrieved, 0U);
    EXPECT_FALSE(unit->full());
    EXPECT_FALSE(unit->empty());

    EXPECT_NE(unit->ir(), 0U);
    EXPECT_NE(unit->red(), 0U);
    EXPECT_EQ(unit->ir(), unit->oldest().ir());
    EXPECT_EQ(unit->red(), unit->oldest().red());
    unit->flush();

    EXPECT_EQ(unit->available(), 0U);
    EXPECT_EQ(unit->retrieved(), retrieved);  // Not clear on flush
    EXPECT_FALSE(unit->full());
    EXPECT_TRUE(unit->empty());

    // Sampling about 40 times (overflow!)
    m5::utility::delay(400);

    unit->update();
    EXPECT_TRUE(unit->updated());

    EXPECT_EQ(unit->available(), MAX_FIFO_DEPTH);
    EXPECT_EQ(unit->retrieved(), MAX_FIFO_DEPTH);
    EXPECT_TRUE(unit->full());
    EXPECT_FALSE(unit->empty());
    EXPECT_GT(unit->overflow(), 0U);

    while (unit->available()) {
        // M5_LOGI("IR:%u RED:%u", unit->ir(), unit->red());
        EXPECT_NE(unit->ir(), 0U);
        EXPECT_NE(unit->red(), 0U);
        EXPECT_EQ(unit->ir(), unit->oldest().ir());
        EXPECT_EQ(unit->red(), unit->oldest().red());
        unit->discard();
    }
}

TEST_F(TestMAX30102, Periodic_SPO2)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->inPeriodic());
    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_FALSE(unit->inPeriodic());

    test_periodic_spo2(unit.get());
}

TEST_F(TestMAX30102, Periodic_HR)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->inPeriodic());
    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_FALSE(unit->inPeriodic());

    test_periodic_hr(unit.get());
}

TEST_F(TestMAX30102, Periodic_MultiLED)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->inPeriodic());
    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_FALSE(unit->inPeriodic());

    EXPECT_TRUE(unit->writeMode(Mode::MultiLED));
    EXPECT_TRUE(unit->writeSpO2Configuration(ADC::Range4096nA, Sampling::Rate400, LEDPulse::Width411));
    EXPECT_TRUE(unit->writeFIFOConfiguration(FIFOSampling::Average4, true, 15));
    EXPECT_TRUE(unit->writeLEDCurrent(0, 0x40));
    EXPECT_TRUE(unit->writeLEDCurrent(1, 0x1F));

    test_periodic_multi(unit.get());
}
