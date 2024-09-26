/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file unit_MAX30100.cpp
  @brief MAX30100 Unit for M5UnitUnified
*/
#include "unit_MAX30100.hpp"
#include <M5Utility.hpp>
#include <limits>  // NaN
#include <cassert>

using namespace m5::utility::mmh3;
using namespace m5::unit::types;
using namespace m5::unit::max30100;
using namespace m5::unit::max30100::command;

namespace {
constexpr uint8_t partId{0x11};
// Sampling rate to interval
constexpr elapsed_time_t interval_table[] = {
    1000 / 50, 1000 / 100, 1000 / 167, 1000 / 200, 1000 / 400, 1000 / 600, 1000 / 800, 1000 / 1000,
};

constexpr uint32_t MEASURE_TEMPERATURE_DURATION{29};  // 29ms

constexpr uint8_t allowed_spo2_table[] = {
    // LSB:200 MSB:1600
    0x0F, 0x0F, 0x07, 0x07, 0x03, 0x01, 0x01, 0x01,
};
constexpr uint8_t allowed_hr_table[] = {
    // LSB:200 MSB:1600
    0x0F, 0x0F, 0x07, 0x07, 0x03, 0x03, 0x03, 0x03,
};
bool is_allowed_settings(const Mode mode, const Sample rate, const LedPulseWidth pw)
{
    return (mode != Mode::None)
               ? (mode == Mode::SPO2 ? allowed_spo2_table : allowed_hr_table)[m5::stl::to_underlying(rate)] &
                     (1U << m5::stl::to_underlying(pw))
               : false;
}

}  // namespace

namespace m5 {
namespace unit {

namespace max30100 {
uint16_t Data::ir() const
{
    return m5::types::big_uint16_t(raw[0], raw[1]).get();
}
uint16_t Data::red() const
{
    return m5::types::big_uint16_t(raw[2], raw[3]).get();
}

float TemperatureData::celsius() const
{
    return (int8_t)raw[0] + raw[1] * 0.0625f;
}

float TemperatureData::fahrenheit() const
{
    return celsius() * 9.0f / 5.0f + 32.f;
}

}  // namespace max30100

// class UnitMAX30100
const char UnitMAX30100::name[] = "UnitMAX30100";
const types::uid_t UnitMAX30100::uid{"UnitMAX30100"_mmh3};
const types::uid_t UnitMAX30100::attr{0};

bool UnitMAX30100::begin()
{
    auto ssize = stored_size();
    assert(ssize >= max30100::MAX_FIFO_DEPTH && "stored_size must be greater than MAX_FIFO_DEPT");
    if (ssize != _data->capacity()) {
        _data.reset(new m5::container::CircularBuffer<Data>(ssize));
        if (!_data) {
            M5_LIB_LOGE("Failed to allocate");
            return false;
        }
    }

    // Check PartID
    uint8_t pid{};
    if (!read_register8(PART_ID, pid) || pid != partId) {
        M5_LIB_LOGE("Cannot detect MAX30100 %x", pid);
        return false;
    }

    // Clear interrupt status
    uint8_t it{};
    if (!read_register8(READ_INTERRUPT_STATUS, it)) {
        M5_LIB_LOGE("Failed to read INTERRUPT_STATUS");
        return false;
    }

    return _cfg.start_periodic ? startPeriodicMeasurement(_cfg.mode, _cfg.sample_rate, _cfg.pulse_width,
                                                          _cfg.ir_current, _cfg.high_resolution, _cfg.red_current)
                               : true;
}

void UnitMAX30100::update(const bool force)
{
    _updated = false;
    if (inPeriodic()) {
        auto at = m5::utility::millis();
        if (force || !_latest || at >= _latest + _interval) {
            _updated = read_FIFO();
            if (_updated) {
                _latest = at;
            }
        }
    }
}

bool UnitMAX30100::start_periodic_measurement()
{
    if (inPeriodic()) {
        return false;
    }

    ModeConfiguration mc{};
    if (read_mode_configration(mc.value)) {
        bool done{true};
        if (mc.shdn()) {
            mc.shdn(false);  // power save OFF
            done = write_mode_configration(mc.value);
        }
        if (done && resetFIFO()) {
            _latest   = 0;
            _periodic = true;
            return true;
        }
    }
    return false;
}

bool UnitMAX30100::start_periodic_measurement(const max30100::Mode mode, const max30100::Sample sample_rate,
                                              const max30100::LedPulseWidth pulse_width,
                                              const max30100::CurrentControl ir_current, const bool high_resolution,
                                              const max30100::CurrentControl red_current)
{
    if (inPeriodic()) {
        return false;
    }

    SpO2Configuration sc{};
    sc.sampleRate(sample_rate);
    sc.ledPulseWidth(pulse_width);
    sc.highResolution(high_resolution);

    return writeMode(mode) && writeSpO2Configuration(sc) && writeLedCurrent(ir_current, red_current) &&
           start_periodic_measurement();
}

bool UnitMAX30100::stop_periodic_measurement()
{
    ModeConfiguration mc{};
    if (read_mode_configration(mc.value)) {
        mc.shdn(true);  // power save ON
        if (write_mode_configration(mc.value)) {
            _periodic = false;
            return true;
        }
    }
    return false;
}

bool UnitMAX30100::readModeConfiguration(max30100::ModeConfiguration& mc)
{
    return read_mode_configration(mc.value);
}

bool UnitMAX30100::writeModeConfiguration(const max30100::ModeConfiguration mc)
{
    return write_mode_configration(mc.value);
}

bool UnitMAX30100::writeMode(const max30100::Mode m)
{
    max30100::ModeConfiguration mc{};
    if (read_mode_configration(mc.value)) {
        mc.mode(m);
        return write_mode_configration(mc.value);
    }
    return false;
}

bool UnitMAX30100::reset()
{
    max30100::ModeConfiguration mc{};
    mc.reset(true);
    if (write_mode_configration(mc.value)) {
        auto start_at{m5::utility::millis()};
        // timeout 1 sec
        do {
            //  Is reset sequence completed?
            if (read_mode_configration(mc.value) && !mc.reset()) {
                return true;
            }
            m5::utility::delay(1);
        } while (m5::utility::millis() - start_at <= 1000);
    }
    return false;
}

bool UnitMAX30100::readSpO2Configuration(max30100::SpO2Configuration& sc)
{
    return read_spo2_configration(sc.value);
}

bool UnitMAX30100::writeSpO2Configuration(const max30100::SpO2Configuration sc)
{
    return write_spo2_configration(sc.value);
}

bool UnitMAX30100::writeSampleRate(const max30100::Sample rate)
{
    max30100::SpO2Configuration sc{};
    if (read_spo2_configration(sc.value)) {
        sc.sampleRate(rate);
        return write_spo2_configration(sc.value);
    }
    return false;
}

bool UnitMAX30100::writeLedPulseWidth(const max30100::LedPulseWidth width)
{
    max30100::SpO2Configuration sc{};
    if (read_spo2_configration(sc.value)) {
        sc.ledPulseWidth(width);
        return write_spo2_configration(sc.value);
    }
    return false;
}

bool UnitMAX30100::readLedConfiguration(max30100::LedConfiguration& lc)
{
    return read_led_configration(lc.value);
}

bool UnitMAX30100::writeLedConfiguration(const max30100::LedConfiguration lc)
{
    return write_led_configration(lc.value);
}

bool UnitMAX30100::writeLedCurrent(const max30100::CurrentControl ir, const max30100::CurrentControl red)
{
    max30100::LedConfiguration lc{};
    lc.ir(ir);
    lc.red(red);
    return write_led_configration(lc.value);
}

bool UnitMAX30100::resetFIFO()
{
    return writeRegister8(FIFO_WRITE_POINTER, 0) && writeRegister8(FIFO_OVERFLOW_COUNTER, 0) &&
           writeRegister8(FIFO_READ_POINTER, 0);
}

bool UnitMAX30100::measureTemperatureSingleshot(TemperatureData& td)
{
    max30100::ModeConfiguration mc{};
    if (read_mode_configration(mc.value)) {
        mc.temperature(true);
        if (write_mode_configration(mc.value)) {
            auto timeout_at = m5::utility::millis() + MEASURE_TEMPERATURE_DURATION * 2;
            bool done{};
            do {
                m5::utility::delay(MEASURE_TEMPERATURE_DURATION);
                done = read_mode_configration(mc.value) && !mc.temperature();
            } while (!done && m5::utility::millis() <= timeout_at);
            return done && read_measurement_temperature(td);
        }
    }
    return false;
}

//
bool UnitMAX30100::read_FIFO()
{
    uint8_t wptr{}, rptr{};
    if (!read_register8(FIFO_WRITE_POINTER, wptr) || !read_register8(FIFO_READ_POINTER, rptr) ||
        !read_register8(FIFO_OVERFLOW_COUNTER, _overflow)) {
        M5_LIB_LOGE("Failed to read ptrs");
        return false;
    }

    uint_fast8_t readCount = _overflow ? max30100::MAX_FIFO_DEPTH : (wptr - rptr) & (max30100::MAX_FIFO_DEPTH - 1);

    // M5_LIB_LOGI(">>cnt:%u of:%u", readCount, _overflow);

    _retrived = 0;

    if (readCount) {
        Data d{};
        for (uint_fast8_t i = 0; i < readCount; ++i) {
            // //Note that FIFO_DATA_REGISTER cannot be burst read.
            if (!read_register(FIFO_DATA_REGISTER, d.raw.data(), d.raw.size())) {
                // Recover the reading position
                M5_LIB_LOGE("Failed to read");
                if (!writeRegister8(FIFO_READ_POINTER, rptr + i)) {
                    M5_LIB_LOGE("Failed to recover");
                }
                return false;
            }
            _data->push_back(d);
            ++_retrived;
        }
        return (_retrived != 0);
    }
    return false;
}

bool UnitMAX30100::read_measurement_temperature(max30100::TemperatureData& td)
{
    return read_register(TEMP_INTEGER, td.raw.data(), td.raw.size());
}

bool UnitMAX30100::read_mode_configration(uint8_t& c)
{
    return read_register8(MODE_CONFIGURATION, c);
}

bool UnitMAX30100::write_mode_configration(const uint8_t c)
{
    if (writeRegister8(MODE_CONFIGURATION, c)) {
        max30100::ModeConfiguration mc;
        mc.value = c;
        _mode    = mc.mode();
        return true;
    }
    return false;
}

bool UnitMAX30100::enable_power_save(const bool enabled)
{
    max30100::ModeConfiguration mc{};
    if (read_mode_configration(mc.value)) {
        mc.shdn(enabled);
        return write_mode_configration(mc.value);
    }
    return false;
}

bool UnitMAX30100::read_spo2_configration(uint8_t& c)
{
    return read_register8(SPO2_CONFIGURATION, c);
}

bool UnitMAX30100::write_spo2_configration(const uint8_t c)
{
    // The write is considered successful even if the value is not
    // allowed in the configuration. However, the value is not
    // actually set, so check it.
    uint8_t chk{};
    max30100::SpO2Configuration sc;
    sc.value = c;

    if (!is_allowed_settings(_mode, sc.sampleRate(), sc.ledPulseWidth())) {
        M5_LIB_LOGW("Invalid combination %u:%u:%u", _mode, sc.sampleRate(), sc.ledPulseWidth());
        return false;
    }

    if (writeRegister8(SPO2_CONFIGURATION, c) && read_spo2_configration(chk) && (chk == c)) {
        _interval = interval_table[m5::stl::to_underlying(sc.sampleRate())];
        return true;
    }
    return false;
}

bool UnitMAX30100::enable_high_resolution(const bool enabled)
{
    max30100::SpO2Configuration sc{};
    if (read_spo2_configration(sc.value)) {
        sc.highResolution(enabled);
        return write_spo2_configration(sc.value);
    }
    return false;
}

bool UnitMAX30100::read_led_configration(uint8_t& c)
{
    return read_register8(LED_CONFIGURATION, c);
}
bool UnitMAX30100::write_led_configration(const uint8_t c)
{
    return writeRegister8(LED_CONFIGURATION, c);
}

// Max30100 works with stop bit false, so wrap
bool UnitMAX30100::read_register8(const uint8_t reg, uint8_t& v)
{
    return readRegister8(reg, v, 0, false /*stop*/);
}

bool UnitMAX30100::read_register(const uint8_t reg, uint8_t* buf, const size_t len)
{
    return readRegister(reg, buf, len, 0, false /*stop*/);
}

}  // namespace unit
}  // namespace m5
