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
constexpr uint32_t MEASURE_TEMPERATURE_DURATION{29};  // 29ms

#if defined(ARDUINO)
#if defined(I2C_BUFFER_LENGTH)
constexpr uint32_t read_buffer_length{I2C_BUFFER_LENGTH};
#else
constexpr uint32_t read_buffer_length{32};
#endif
#else
//! @TODO for M5HAL
constexpr uint32_t read_buffer_length{32};
#endif

constexpr uint32_t sr_table[] = {50, 100, 167, 200, 400, 600, 800, 1000};

constexpr uint8_t spo2_table[] = {
    // LSB:200 MSB:1600
    0x0F, 0x0F, 0x07, 0x07, 0x03, 0x01, 0x01, 0x01,
};
constexpr uint8_t hr_table[] = {
    // LSB:200 MSB:1600
    0x0F, 0x0F, 0x07, 0x07, 0x03, 0x03, 0x03, 0x03,
};
constexpr uint8_t none_table[] = {
    // LSB:200 MSB::1600
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
constexpr const uint8_t* allowed_setting_table[] = {none_table, none_table, hr_table, spo2_table};

inline bool is_allowed_settings(const Mode mode, const Sampling rate, const LEDPulse width)
{
    return allowed_setting_table[m5::stl::to_underlying(mode)][m5::stl::to_underlying(rate)] &
           (1U << m5::stl::to_underlying(width));
}

// Calculate the interval per data
inline uint32_t caluculate_interval_time(const Sampling rate)
{
    return std::floor(1000.f / sr_table[m5::stl::to_underlying(rate)]);
}

}  // namespace

namespace m5 {
namespace unit {

namespace max30100 {

///@cond
struct ModeConfiguration {
    bool shdn() const
    {
        return value & (1U << 7);
    }
    bool reset() const
    {
        return value & (1U << 6);
    }
    bool temperature() const
    {
        return value & (1U << 3);
    }
    Mode mode() const
    {
        return static_cast<Mode>(value & 0x07);
    }
    void shdn(const bool b)
    {
        value = (value & ~(1U << 7)) | ((b ? 1 : 0) << 7);
    }
    void reset(const bool b)
    {
        value = (value & ~(1U << 6)) | ((b ? 1 : 0) << 6);
    }
    void temperature(const bool b)
    {
        value = (value & ~(1U << 3)) | ((b ? 1 : 0) << 3);
    }
    void mode(const Mode m)
    {
        value = (value & ~0x07) | (m5::stl::to_underlying(m) & 0x07);
    }
    uint8_t value{};
};

struct SpO2Configuration {
    bool resolution() const
    {
        return value & (1U << 6);
    }
    Sampling rate() const
    {
        return static_cast<Sampling>((value >> 2) & 0x07);
    }
    LEDPulse width() const
    {
        return static_cast<LEDPulse>(value & 0x03);
    }
    void resolution(const bool b)
    {
        value = (value & ~(1U << 6)) | ((b ? 1 : 0) << 6);
    }
    void rate(const Sampling rate)
    {
        value = (value & ~(0x07 << 2)) | ((m5::stl::to_underlying(rate) & 0x07) << 2);
    }
    void width(const LEDPulse width)
    {
        value = (value & ~0x03) | (m5::stl::to_underlying(width) & 0x03);
    }
    uint8_t value{};
};

struct LEDConfiguration {
    LED red() const
    {
        return static_cast<LED>((value >> 4) & 0x0F);
    }
    LED ir() const
    {
        return static_cast<LED>(value & 0x0F);
    }
    void red(const LED cc)
    {
        value = (value & ~(0x0F << 4)) | ((m5::stl::to_underlying(cc) & 0x0F) << 4);
    }
    void ir(const LED cc)
    {
        value = (value & ~0x0F) | (m5::stl::to_underlying(cc) & 0x0F);
    }
    uint8_t value{};
};
///@endcond

}  // namespace max30100

// class UnitMAX30100
const char UnitMAX30100::name[] = "UnitMAX30100";
const types::uid_t UnitMAX30100::uid{"UnitMAX30100"_mmh3};
const types::attr_t UnitMAX30100::attr{attribute::AccessI2C};

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
    if (!read_register8(READ_PART_ID, pid) || pid != partId) {
        M5_LIB_LOGE("Cannot detect MAX30100 %x", pid);
        return false;
    }

#if 0    
    // Clear interrupt status
    uint8_t it{};
    if (!read_register8(READ_INTERRUPT_STATUS, it)) {
        M5_LIB_LOGE("Failed to read INTERRUPT_STATUS");
        return false;
    }
#endif

    return _cfg.start_periodic ? startPeriodicMeasurement(_cfg.mode, _cfg.rate, _cfg.width, _cfg.ir_current,
                                                          _cfg.high_resolution, _cfg.red_current)
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
                //                _latest = at;
                _latest = m5::utility::millis();
            }
        }
    }
}

bool UnitMAX30100::start_periodic_measurement()
{
    if (inPeriodic()) {
        return false;
    }

    Sampling rate{};
    if (readSpO2SamplingRate(rate)) {
        _periodic = writeShutdownControl(false) && resetFIFO();
        if (_periodic) {
            _latest   = 0;
            _interval = caluculate_interval_time(rate);
            // M5_LIB_LOGE(">>>>R: Rate:%u IT:%u", rate, _interval);
            //              _mask     = adc_resolution_bits_table[m5::stl::to_underlying(width)];
            return true;
        }
    }
    return false;
}

bool UnitMAX30100::start_periodic_measurement(const max30100::Mode mode, const max30100::Sampling rate,
                                              const max30100::LEDPulse width, const max30100::LED ir_current,
                                              const bool resolution, const max30100::LED red_current)
{
    if (inPeriodic()) {
        return false;
    }
    return writeMode(mode) && writeSpO2Configuration(resolution, rate, width) &&
           writeLEDCurrent(ir_current, red_current) && start_periodic_measurement();
}

bool UnitMAX30100::stop_periodic_measurement()
{
    ModeConfiguration mc{};
    if (read_register8(MODE_CONFIGURATION, mc.value)) {
        mc.shdn(true);
        if (writeRegister8(MODE_CONFIGURATION, mc.value)) {
            _periodic = false;
            return true;
        }
    }
    return false;
}

bool UnitMAX30100::readMode(max30100::Mode& mode)
{
    mode = Mode::None;
    ModeConfiguration mc{};
    if (read_register8(MODE_CONFIGURATION, mc.value)) {
        mode = mc.mode();
        return true;
    }
    return false;
}

bool UnitMAX30100::writeMode(const max30100::Mode mode)
{
    if (inPeriodic()) {
        M5_LIB_LOGD("Periodic measurements are running");
        return false;
    }

    ModeConfiguration mc{};
    if (read_register8(MODE_CONFIGURATION, mc.value)) {
        mc.mode(mode);
        if (writeRegister8(MODE_CONFIGURATION, mc.value)) {
            _mode = mode;
            return true;
        }
    }
    return false;
}

bool UnitMAX30100::readShutdownControl(bool& shdn)
{
    shdn = false;
    ModeConfiguration mc{};
    if (read_register8(MODE_CONFIGURATION, mc.value)) {
        shdn = mc.shdn();
        return true;
    }
    return false;
}

bool UnitMAX30100::writeShutdownControl(const bool shdn)
{
    if (inPeriodic()) {
        M5_LIB_LOGD("Periodic measurements are running");
        return false;
    }

    ModeConfiguration mc{};
    if (read_register8(MODE_CONFIGURATION, mc.value)) {
        mc.shdn(shdn);
        return writeRegister8(MODE_CONFIGURATION, mc.value);
    }
    return false;
}

bool UnitMAX30100::readSpO2Configuration(bool& resolution, max30100::Sampling& rate, max30100::LEDPulse& width)
{
    resolution = false;
    rate       = Sampling::Rate50;
    width      = LEDPulse::Width200;

    SpO2Configuration sc{};
    if (read_register8(SPO2_CONFIGURATION, sc.value)) {
        resolution = sc.resolution();
        rate       = sc.rate();
        width      = sc.width();
        return true;
    }
    return false;
}

bool UnitMAX30100::writeSpO2Configuration(const bool resolution, const max30100::Sampling rate,
                                          const max30100::LEDPulse width)
{
    SpO2Configuration sc{};
    sc.resolution(resolution);
    sc.rate(rate);
    sc.width(width);
    return write_spo2_configuration(sc);
}

bool UnitMAX30100::write_spo2_configuration(const SpO2Configuration& sc)
{
    if (inPeriodic()) {
        M5_LIB_LOGD("Periodic measurements are running");
        return false;
    }

    if (!is_allowed_settings(_mode, sc.rate(), sc.width())) {
        M5_LIB_LOGE("Invalid combination. Mode:%u, S:%u W:%u", _mode, sc.rate(), sc.width());
        return false;
    }
    return writeRegister8(SPO2_CONFIGURATION, sc.value);
}

bool UnitMAX30100::writeSpO2SamplingRate(const max30100::Sampling rate)
{
    SpO2Configuration sc{};
    if (read_register8(SPO2_CONFIGURATION, sc.value)) {
        sc.rate(rate);
        return write_spo2_configuration(sc);
    }
    return false;
}

bool UnitMAX30100::writeSpO2HighResolution(const bool enabled)
{
    SpO2Configuration sc{};
    if (read_register8(SPO2_CONFIGURATION, sc.value)) {
        sc.resolution(enabled);
        return write_spo2_configuration(sc);
    }
    return false;
}

bool UnitMAX30100::writeSpO2LEDPulseWidth(const max30100::LEDPulse width)
{
    SpO2Configuration sc{};
    if (read_register8(SPO2_CONFIGURATION, sc.value)) {
        sc.width(width);
        return write_spo2_configuration(sc);
    }
    return false;
}

bool UnitMAX30100::readLEDCurrent(LED& ir_current, LED& red_current)
{
    ir_current = red_current = LED::Current0_0;

    LEDConfiguration lc{};
    if (read_register8(LED_CONFIGURATION, lc.value)) {
        ir_current  = lc.ir();
        red_current = lc.red();
        return true;
    }
    return false;
}

bool UnitMAX30100::writeLEDCurrent(const max30100::LED ir_current, const max30100::LED red_current)
{
    LEDConfiguration lc{};
    lc.ir(ir_current);
    lc.red(red_current);
    return writeRegister8(LED_CONFIGURATION, lc.value);
}

bool UnitMAX30100::resetFIFO()
{
    return writeRegister8(FIFO_WRITE_POINTER, 0) && writeRegister8(FIFO_OVERFLOW_COUNTER, 0) &&
           writeRegister8(FIFO_READ_POINTER, 0);
}

bool UnitMAX30100::measureTemperatureSingleshot(TemperatureData& td)
{
    ModeConfiguration mc{};
    if (read_register8(MODE_CONFIGURATION, mc.value)) {
        // Request measure
        mc.temperature(true);
        if (writeRegister8(MODE_CONFIGURATION, mc.value)) {
            auto timeout_at = m5::utility::millis() + 500;
            m5::utility::delay(MEASURE_TEMPERATURE_DURATION);  // We have to wait at least this long
            do {
                if (read_register8(MODE_CONFIGURATION, mc.value) && !mc.temperature()) {
                    return read_measurement_temperature(td);
                }
                m5::utility::delay(1);
            } while (m5::utility::millis() <= timeout_at);
            M5_LIB_LOGW("timeout");
        }
    }
    return false;
}

bool UnitMAX30100::reset()
{
    ModeConfiguration mc{};
    mc.reset(true);
    if (writeRegister8(MODE_CONFIGURATION, mc.value)) {
        auto timeout_at = m5::utility::millis() + 1000;
        do {
            if (read_register8(MODE_CONFIGURATION, mc.value) && !mc.reset()) {
                _periodic = false;
                _mode     = mc.mode();
                _retrived = _overflow = 0;
                return true;
            }
            m5::utility::delay(1);
        } while (m5::utility::millis() <= timeout_at);
    }
    return false;
}

//
bool UnitMAX30100::read_FIFO()
{
    uint8_t wptr{}, rptr{};
    _retrived = _overflow = 0;

    if (!read_register8(FIFO_WRITE_POINTER, wptr) || !read_register8(FIFO_READ_POINTER, rptr) ||
        !read_register8(FIFO_OVERFLOW_COUNTER, _overflow)) {
        M5_LIB_LOGE("Failed to read ptrs");
        return false;
    }

    uint_fast8_t readCount = _overflow        ? MAX_FIFO_DEPTH
                             : (wptr >= rptr) ? (wptr - rptr)
                                              : (wptr + MAX_FIFO_DEPTH - rptr);

    // M5_LIB_LOGD("Ptr:%u/%u OF:%u RC:%u/%u", rptr, wptr, _overflow,
    //             (wptr >= rptr) ? (wptr - rptr) : (wptr + MAX_FIFO_DEPTH - rptr), readCount);

    assert(readCount <= MAX_FIFO_DEPTH);

#if 1
    if (readCount) {
        uint8_t reg{FIFO_DATA_REGISTER};
        if (writeWithTransaction(&reg, 1) != m5::hal::error::error_t::OK) {
            return false;
        }
        uint8_t rbuf[MAX_FIFO_DEPTH * 4]{};

        int32_t left = 4 * readCount;

        while (left > 0) {
            uint32_t batch_len   = (left > read_buffer_length) ? read_buffer_length - (read_buffer_length % 4) : left;
            uint32_t batch_count = batch_len / 4;

            // M5_LIB_LOGE("    batch:%u/%u", batch_len, batch_count);

            if (readWithTransaction(rbuf, batch_len) != m5::hal::error::error_t::OK) {
                return false;
            }

            for (uint_fast8_t i = 0; i < batch_count; ++i) {
                Data d;
                // Unlike MAX30102, the length of data per session does not change even in HROnly
                memcpy(d.raw.data(), rbuf + 4 * i, 4);
                _data->push_back(d);
            }
            left -= batch_len;
        }
        _retrived = readCount;
    }

#else
    while (readCount--) {
        Data d{};
        if (!read_register(FIFO_DATA_REGISTER, d.raw.data(), d.raw.size())) {
            M5_LIB_LOGE("Failed to read");
            return false;
        }
        _data->push_back(d);
        ++_retrived;
    }
#endif
    return (_retrived != 0);
}

bool UnitMAX30100::read_measurement_temperature(max30100::TemperatureData& td)
{
    return read_register(TEMP_INTEGER, td.raw.data(), td.raw.size());
}

bool UnitMAX30100::readRevisionID(uint8_t& rev)
{
    rev = 0x00;
    return read_register8(READ_REVISION_ID, rev);
}

uint32_t UnitMAX30100::caluculateSamplingRate()
{
    Sampling rate{};
    if (readSpO2SamplingRate(rate)) {
        return 1000 / caluculate_interval_time(rate);
    }
    return 0;
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
