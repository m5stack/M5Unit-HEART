/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  UnitTest for UnitMAX30100
*/
#include <gtest/gtest.h>
#include <Wire.h>
#include <M5Unified.h>
#include <M5UnitUnified.hpp>
#include <googletest/test_template.hpp>
#include <googletest/test_helper.hpp>
#include <unit/unit_MAX30100.hpp>
#include <chrono>
#include <cmath>

using namespace m5::unit::googletest;
using namespace m5::unit;
using namespace m5::unit::max30100;
using namespace m5::unit::max30100::command;

class TestMAX30100 : public I2CComponentTestBase<UnitMAX30100> {
protected:
    virtual UnitMAX30100* get_instance() override
    {
        auto ptr = new m5::unit::UnitMAX30100();
        if (ptr) {
            auto ccfg = ptr->component_config();
            ptr->component_config(ccfg);
        }
        return ptr;
    }
};

namespace {
constexpr uint32_t STORED_SIZE{6};

constexpr uint8_t spo2_table[] = {
    // LSB:200 MSB:1600
    0x0F, 0x0F, 0x07, 0x07, 0x03, 0x01, 0x01, 0x01,
};
constexpr uint8_t hr_table[] = {
    // LSB:200 MSB:1600
    0x0F, 0x0F, 0x07, 0x07, 0x03, 0x03, 0x03, 0x03,
};
constexpr uint8_t none_table[] = {
    // LSB:200 MSB:1600
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
constexpr const uint8_t* allowed_setting_table[] = {none_table, none_table, hr_table, spo2_table};

inline bool is_allowed_settings(const Mode mode, const Sampling rate, const LEDPulse width)
{
    return allowed_setting_table[m5::stl::to_underlying(mode)][m5::stl::to_underlying(rate)] &
           (1U << m5::stl::to_underlying(width));
}

constexpr Mode mode_table[] = {
    Mode::SpO2,
    Mode::HROnly,
};

constexpr Sampling sr_table[] = {
    Sampling::Rate50,  Sampling::Rate100, Sampling::Rate167, Sampling::Rate200,
    Sampling::Rate400, Sampling::Rate600, Sampling::Rate800, Sampling::Rate1000,
};

constexpr LEDPulse pw_table[] = {
    LEDPulse::Width200,
    LEDPulse::Width400,
    LEDPulse::Width800,
    LEDPulse::Width1600,
};

constexpr bool res_table[] = {true, false};

constexpr LED cur_table[] = {
    LED::Current0_0,  LED::Current4_4,  LED::Current7_6,  LED::Current11_0, LED::Current14_2, LED::Current17_4,
    LED::Current20_8, LED::Current24_0, LED::Current27_1, LED::Current30_6, LED::Current33_8, LED::Current37_0,
    LED::Current40_2, LED::Current43_6, LED::Current46_8, LED::Current50_0,
};

void test_spo2_config(UnitMAX30100* unit, const Mode mode)
{
    EXPECT_TRUE(unit->writeMode(mode));

    for (auto& res : res_table) {
        for (auto&& sr : sr_table) {
            for (auto&& pw : pw_table) {
                auto s = m5::utility::formatString("Mode:%u RES:%u Rate:%u Width:%u", mode, res, sr, pw);
                SCOPED_TRACE(s);

                bool resolution{};
                Sampling rate{};
                LEDPulse width{};

                if (is_allowed_settings(mode, sr, pw)) {
                    EXPECT_TRUE(unit->writeSpO2Configuration(res, sr, pw));

                    EXPECT_TRUE(unit->readSpO2Configuration(resolution, rate, width));
                    EXPECT_EQ(resolution, res);
                    EXPECT_EQ(rate, sr);
                    EXPECT_EQ(width, pw);
                } else {
                    EXPECT_TRUE(unit->readSpO2Configuration(resolution, rate, width));

                    EXPECT_FALSE(unit->writeSpO2Configuration(res, sr, pw));

                    bool resolution2{};
                    Sampling rate2{};
                    LEDPulse width2{};
                    EXPECT_TRUE(unit->readSpO2Configuration(resolution2, rate2, width2));
                    EXPECT_EQ(resolution2, resolution);
                    EXPECT_EQ(rate2, rate);
                    EXPECT_EQ(width2, width);
                }
            }
        }
    }
}

// Verify individual SpO2 parameter read/write APIs (roundtrip)
void test_spo2_config_each(UnitMAX30100* unit, const Mode mode)
{
    auto s = m5::utility::formatString("Mode:%u", mode);
    SCOPED_TRACE(s);

    EXPECT_TRUE(unit->writeMode(mode));
    EXPECT_TRUE(unit->writeSpO2Configuration(false, Sampling::Rate50, LEDPulse::Width200));

    // Resolution
    for (auto& res : res_table) {
        bool resolution{};
        EXPECT_TRUE(unit->writeSpO2HighResolution(res));
        EXPECT_TRUE(unit->readSpO2HighResolution(resolution));
        EXPECT_EQ(resolution, res);
    }

    // Sampling rate (allowed settings only)
    for (auto&& sr : sr_table) {
        if (is_allowed_settings(mode, sr, LEDPulse::Width200)) {
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
void collect_and_verify(U* unit, uint32_t count, bool expect_ir, bool expect_red)
{
    auto ad        = unit->template asAdapter<m5::unit::AdapterI2C>(m5::unit::Adapter::Type::I2C);
    bool is_bus    = ad && ad->implType() == m5::unit::AdapterI2C::ImplType::Bus;
    uint32_t limit = is_bus ? 3U : 1U;
    auto timeout   = std::max<uint32_t>(unit->interval() * (count + 1) * 2, 2000);
    auto result    = collect_periodic_measurements(unit, count, timeout);

    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_FALSE(unit->inPeriodic());
    EXPECT_FALSE(result.timed_out);
    EXPECT_EQ(result.update_count, count);
    EXPECT_LE(result.median(), result.expected_interval + limit);

    EXPECT_GE(unit->available(), count);
    EXPECT_FALSE(unit->empty());

    uint32_t cnt{unit->available() / 2};
    uint32_t left = unit->available() - cnt;
    uint32_t air{}, ared{};
    while (cnt-- && unit->available()) {
        air += unit->ir();
        ared += unit->red();
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

void test_periodic_spo2(UnitMAX30100* unit)
{
    // Pairwise
    // clang-format off
    constexpr std::tuple<bool, Sampling, LEDPulse> cond_table[] = {
        {false, Sampling::Rate100, LEDPulse::Width1600 },
        {true, Sampling::Rate167, LEDPulse::Width400 },
        {true, Sampling::Rate200, LEDPulse::Width200 },
        {true, Sampling::Rate100, LEDPulse::Width800 },
        {false, Sampling::Rate1000, LEDPulse::Width200 },
        {true, Sampling::Rate800, LEDPulse::Width200 },
        {true, Sampling::Rate400, LEDPulse::Width200 },
        {true, Sampling::Rate600, LEDPulse::Width200 },
        {false, Sampling::Rate50, LEDPulse::Width400 },
        {true, Sampling::Rate50, LEDPulse::Width800 },
        {false, Sampling::Rate100, LEDPulse::Width400 },
        {true, Sampling::Rate50, LEDPulse::Width1600 },
        {false, Sampling::Rate100, LEDPulse::Width200 },
        {false, Sampling::Rate200, LEDPulse::Width800 },
        {false, Sampling::Rate400, LEDPulse::Width400 },
        {false, Sampling::Rate50, LEDPulse::Width200 },
        {false, Sampling::Rate200, LEDPulse::Width400 },
        {false, Sampling::Rate167, LEDPulse::Width800 },
        {false, Sampling::Rate800, LEDPulse::Width200 },
        {false, Sampling::Rate600, LEDPulse::Width200 },
        {true, Sampling::Rate1000, LEDPulse::Width200 },
        {false, Sampling::Rate167, LEDPulse::Width200 },
    };
    // clang-format on

    for (auto&& cond : cond_table) {
        bool res{};
        Sampling rate{};
        LEDPulse width{};
        std::tie(res, rate, width) = cond;

        auto s = m5::utility::formatString("SPO2 RES:%u SR:%u WID:%u", res, rate, width);
        SCOPED_TRACE(s);

        EXPECT_TRUE(unit->startPeriodicMeasurement(Mode::SpO2, rate, width, LED::Current27_1, res, LED::Current27_1));
        collect_and_verify(unit, STORED_SIZE, true, true);
    }
}

void test_periodic_hr(UnitMAX30100* unit)
{
    // Pairwise
    // clang-format off
    constexpr std::tuple<bool, Sampling, LEDPulse> cond_table[] = {
        {true, Sampling::Rate600, LEDPulse::Width400 },
        {false, Sampling::Rate1000, LEDPulse::Width400 },
        {false, Sampling::Rate100, LEDPulse::Width1600 },
        {true, Sampling::Rate167, LEDPulse::Width400 },
        {true, Sampling::Rate200, LEDPulse::Width200 },
        {true, Sampling::Rate100, LEDPulse::Width800 },
        {false, Sampling::Rate1000, LEDPulse::Width200 },
        {true, Sampling::Rate800, LEDPulse::Width200 },
        {true, Sampling::Rate400, LEDPulse::Width200 },
        {false, Sampling::Rate600, LEDPulse::Width200 },
        {false, Sampling::Rate50, LEDPulse::Width400 },
        {true, Sampling::Rate50, LEDPulse::Width800 },
        {false, Sampling::Rate100, LEDPulse::Width400 },
        {true, Sampling::Rate50, LEDPulse::Width1600 },
        {false, Sampling::Rate100, LEDPulse::Width200 },
        {false, Sampling::Rate200, LEDPulse::Width800 },
        {false, Sampling::Rate400, LEDPulse::Width400 },
        {false, Sampling::Rate50, LEDPulse::Width200 },
        {false, Sampling::Rate800, LEDPulse::Width400 },
        {false, Sampling::Rate200, LEDPulse::Width400 },
        {false, Sampling::Rate167, LEDPulse::Width800 },
        {true, Sampling::Rate1000, LEDPulse::Width200 },
        {false, Sampling::Rate167, LEDPulse::Width200 },
    };
    // clang-format on

    for (auto&& cond : cond_table) {
        bool res{};
        Sampling rate{};
        LEDPulse width{};
        std::tie(res, rate, width) = cond;

        auto s = m5::utility::formatString("HR RES:%u SR:%u WID:%u", res, rate, width);
        SCOPED_TRACE(s);

        EXPECT_TRUE(unit->startPeriodicMeasurement(Mode::HROnly, rate, width, LED::Current27_1, res));
        collect_and_verify(unit, STORED_SIZE, true, false);
    }
}

}  // namespace

TEST_F(TestMAX30100, Mode)
{
    constexpr bool bool_table[] = {true, false};

    SCOPED_TRACE(ustr);
    EXPECT_TRUE(unit->inPeriodic());

    // Failed if in periodic
    EXPECT_TRUE(unit->inPeriodic());
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

TEST_F(TestMAX30100, SpO2Configuration)
{
    SCOPED_TRACE(ustr);

    // Failed if in periodic
    EXPECT_FALSE(unit->writeSpO2Configuration(true, Sampling::Rate50, LEDPulse::Width200));

    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_FALSE(unit->inPeriodic());

    test_spo2_config(unit.get(), Mode::SpO2);
    test_spo2_config_each(unit.get(), Mode::SpO2);

    test_spo2_config(unit.get(), Mode::HROnly);
    test_spo2_config_each(unit.get(), Mode::HROnly);
}

TEST_F(TestMAX30100, LEDCurrent)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_FALSE(unit->inPeriodic());

    // Boundary values: min, mid, max
    constexpr LED boundary[] = {LED::Current0_0, LED::Current24_0, LED::Current50_0};
    for (auto&& ir : boundary) {
        for (auto&& red : boundary) {
            auto s = m5::utility::formatString("IR:%u Red:%u", ir, red);
            SCOPED_TRACE(s);
            EXPECT_TRUE(unit->writeLEDCurrent(ir, red));

            LED ir_current{}, red_current{};
            EXPECT_TRUE(unit->readLEDCurrent(ir_current, red_current));
            EXPECT_EQ(ir_current, ir);
            EXPECT_EQ(red_current, red);
        }
    }
}

TEST_F(TestMAX30100, Temperature)
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
            // M5_LOGI("TempS>C:%f F:%f", td.celsius(), td.fahrenheit());
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

TEST_F(TestMAX30100, TemperatureDataSentinel)
{
    TemperatureData td{};

    td.raw = {0x80, 0x00};
    EXPECT_FALSE(std::isfinite(td.celsius()));
    EXPECT_FALSE(std::isfinite(td.fahrenheit()));

    td.raw = {0xFF, 0x00};
    EXPECT_FLOAT_EQ(td.celsius(), -1.0f);
    EXPECT_FLOAT_EQ(td.fahrenheit(), 30.2f);
}

TEST_F(TestMAX30100, Revision)
{
    SCOPED_TRACE(ustr);

    uint8_t rev{};
    EXPECT_TRUE(unit->readRevisionID(rev));
    EXPECT_NE(rev, 0);
    // M5_LOGI("Rev:%02X", rev);
}

TEST_F(TestMAX30100, Reset)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_FALSE(unit->inPeriodic());

    //
    EXPECT_TRUE(unit->writeMode(Mode::SpO2));
    EXPECT_TRUE(unit->writeSpO2Configuration(true, Sampling::Rate400, LEDPulse::Width400));
    EXPECT_TRUE(unit->writeLEDCurrent(LED::Current24_0, LED::Current17_4));

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

    bool resolution{};
    Sampling rate{};
    LEDPulse width{};
    EXPECT_TRUE(unit->readSpO2Configuration(resolution, rate, width));
    EXPECT_FALSE(resolution);
    EXPECT_EQ(rate, Sampling::Rate50);
    EXPECT_EQ(width, LEDPulse::Width200);

    LED ir{}, red{};
    EXPECT_TRUE(unit->readLEDCurrent(ir, red));
    EXPECT_EQ(ir, LED::Current0_0);

    EXPECT_EQ(red, LED::Current0_0);

    uint8_t rptr{0xFF}, wptr{0xFF}, cnt{0xFF};
    EXPECT_TRUE(unit->readFIFOReadPointer(rptr));
    EXPECT_TRUE(unit->readFIFOWritePointer(wptr));
    EXPECT_TRUE(unit->readFIFOOverflowCounter(cnt));
    EXPECT_EQ(rptr, 0U);
    EXPECT_EQ(wptr, 0U);
    EXPECT_EQ(cnt, 0U);
}

TEST_F(TestMAX30100, Periodic)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->inPeriodic());
    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_FALSE(unit->inPeriodic());

    // 100 sps
    EXPECT_TRUE(unit->startPeriodicMeasurement(Mode::SpO2, Sampling::Rate100, LEDPulse::Width1600, LED::Current27_1,
                                               true, LED::Current27_1));

    // Wait first updated
    auto start_at = m5::utility::millis();
    do {
        unit->update();
        std::this_thread::yield();
    } while (!unit->updated() && m5::utility::millis() - start_at <= 1000);
    EXPECT_TRUE(unit->updated());

    //    M5_LOGW("%u %u", unit->retrieved(), unit->available());

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

    // Sampling about 20 times (overflow!)
    m5::utility::delay(200);

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

TEST_F(TestMAX30100, Periodic_SPO2)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->inPeriodic());
    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_FALSE(unit->inPeriodic());

    test_periodic_spo2(unit.get());
}

TEST_F(TestMAX30100, Periodic_HR)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->inPeriodic());
    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_FALSE(unit->inPeriodic());

    test_periodic_hr(unit.get());
}
