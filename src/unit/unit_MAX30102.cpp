/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file unit_MAX30102.cpp
  @brief MAX30102 Unit for M5UnitUnified
*/
#include "unit_MAX30102.hpp"
#include <M5Utility.hpp>
#include <limits>  // NaN
#include <algorithm>
#include <cassert>

using namespace m5::utility::mmh3;
using namespace m5::unit::types;
using namespace m5::unit::max30102;
using namespace m5::unit::max30102::command;

namespace {
constexpr uint8_t partId{0x15};
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

constexpr FIFOSampling fifo_sampling_table[] = {
    FIFOSampling::Average1,  FIFOSampling::Average2,  FIFOSampling::Average4,
    FIFOSampling::Average8,  FIFOSampling::Average16, FIFOSampling::Average32,
    FIFOSampling::Average32,  // duplicated
    FIFOSampling::Average32,  // duplicated
};

struct FIFOConfiguration {
    inline FIFOSampling average() const
    {
        return fifo_sampling_table[(value >> 5) & 0x07];
    }
    inline bool rollover() const
    {
        return value & (1U << 4);
    }
    inline uint8_t almostFull() const
    {
        return value & 0x0F;
    }

    inline void average(const FIFOSampling avg)
    {
        value = (value & ~(0x07 << 5)) | (m5::stl::to_underlying(avg) << 5);
    }
    inline void rollover(const bool enabled)
    {
        value = (value & ~(1U << 4)) | (enabled ? (1U << 4) : 0);
    }
    inline void almostFull(const uint8_t v)
    {
        value = (value & ~0x0F) | (v & 0x0F);
    }
    uint8_t value{};
};

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

inline bool is_allowed_settings(const Mode mode, const Sampling rate, const LEDPulse width)
{
    return allowed_setting_table[m5::stl::to_underlying(mode)][m5::stl::to_underlying(rate)] &
           (1U << m5::stl::to_underlying(width));
}

struct MultiLEDControl {
    inline Slot slotH() const
    {
        return static_cast<Slot>((value >> 4) & 0x07);
    }
    inline Slot slotL() const
    {
        return static_cast<Slot>(value & 0x07);
    }
    inline void slotH(const Slot m)
    {
        value = (value & ~(0x07 << 4)) | (m5::stl::to_underlying(m) << 4);
    }
    inline void slotL(const Slot m)
    {
        value = (value & ~0x07) | m5::stl::to_underlying(m);
    }
    uint8_t value{};
};

#if 0
bool check_valid_multi_led_mode_array(const Slot array[4])
{
    return (std::adjacent_find(array, array + 4,
                               [](const Slot a, const Slot b) { return (a == Slot::None) && (b != Slot::None); }) ==
            (array + 4)) &&
           std::all_of(array, array + 4, [](const Slot m) { return m5::stl::to_underlying(m) <= 2; });
}
#endif

constexpr uint32_t sampling_rate_table[] = {
    50, 100, 200, 400, 800, 1000, 1600, 3200,
};

constexpr uint32_t average_table[] = {
    1, 2, 4, 8, 16, 32, 32, 32,
};

constexpr uint32_t adc_resolution_bits_table[] = {
    0x007FFF,  // 15 bits
    0x00FFFF,  // 16 bits
    0x01FFFF,  // 17 bits
    0x03FFFF,  // 18 bits
};

// Calculate the interval per data
inline uint32_t caluculate_interval_time(const FIFOSampling avg, const Sampling rate)
{
    float freq = sampling_rate_table[m5::stl::to_underlying(rate)] / (float)average_table[m5::stl::to_underlying(avg)];

    // M5_LIB_LOGE(">>>>>>>>>> avg:%u %u rate:%u %u => %f %f", avg, average_table[m5::stl::to_underlying(avg)], rate,
    //             sampling_rate_table[m5::stl::to_underlying(rate)], freq, std::ceil(1000.f / freq));

    return std::floor(1000.f / freq);
}

}  // namespace

namespace m5 {
namespace unit {

namespace max30102 {

///@cond
struct ModeConfiguration {
    inline bool shdn() const
    {
        return value & (1U << 7);
    }
    inline bool reset() const
    {
        return value & (1U << 6);
    }
    inline Mode mode() const
    {
        return static_cast<Mode>(value & 0x07);
    }
    inline void shdn(const bool b)
    {
        value = (value & ~(1U << 7)) | ((b ? 1 : 0) << 7);
    }
    inline void reset(const bool b)
    {
        value = (value & ~(1U << 6)) | ((b ? 1 : 0) << 6);
    }
    inline void mode(const Mode m)
    {
        value = (value & ~0x07) | (m5::stl::to_underlying(m) & 0x07);
    }
    uint8_t value{};
};

struct SpO2Configuration {
    inline ADC range() const
    {
        return static_cast<ADC>((value >> 5) & 0x03);
    }
    inline Sampling rate() const
    {
        return static_cast<Sampling>((value >> 2) & 0x07);
    }
    inline LEDPulse width() const
    {
        return static_cast<LEDPulse>(value & 0x03);
    }
    inline void range(const ADC range)
    {
        value = (value & ~(0x03 << 5)) | ((m5::stl::to_underlying(range) & 0x03) << 5);
    }
    inline void rate(const Sampling rate)
    {
        value = (value & ~(0x07 << 2)) | ((m5::stl::to_underlying(rate) & 0x07) << 2);
    }
    inline void width(const LEDPulse width)
    {
        value = (value & ~0x03) | (m5::stl::to_underlying(width) & 0x03);
    }
    uint8_t value{};
};
///@endcond

}  // namespace max30102

// class UnitMAX30102
const char UnitMAX30102::name[] = "UnitMAX30102";
const types::uid_t UnitMAX30102::uid{"UnitMAX30102"_mmh3};
const types::attr_t UnitMAX30102::attr{attribute::AccessI2C};

bool UnitMAX30102::begin()
{
    auto ssize = stored_size();
    assert(ssize >= max30102::MAX_FIFO_DEPTH && "stored_size must be greater than MAX_FIFO_DEPTH");
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
        M5_LIB_LOGE("Cannot detect MAX30102 %x", pid);
        return false;
    }

    if (!reset()) {
        M5_LIB_LOGE("Failed to reset");
        return false;
    }

    if (!readMode(_mode) || !readMultiLEDModeControl(_slot[0], _slot[1])) {
        M5_LIB_LOGE("Failed to read settings");
        return false;
    }

    return (_cfg.start_periodic && (_cfg.mode == Mode::SpO2 || _cfg.mode == Mode::HROnly))
               ? startPeriodicMeasurement(_cfg.mode, _cfg.adc_range, _cfg.sampling_rate, _cfg.pulse_width,
                                          _cfg.fifo_sampling_average, _cfg.ir_current, _cfg.red_current)
               : true;
}

void UnitMAX30102::update(const bool force)
{
    _updated = false;
    if (inPeriodic()) {
        auto at = m5::utility::millis();
        if (force || !_latest || at >= _latest + _interval) {
            _updated = (read_FIFO() && _retrived);
            if (_updated) {
                _latest = m5::utility::millis();
            }
        }
    }
}

bool UnitMAX30102::start_periodic_measurement()
{
    if (inPeriodic()) {
        return false;
    }

    FIFOSampling avg{};
    bool rollover{};
    uint8_t almostFull{};
    max30102::ADC range{};
    Sampling rate{};
    LEDPulse width{};

    if (readFIFOConfiguration(avg, rollover, almostFull) && readSpO2Configuration(range, rate, width)) {
        _periodic = writeFIFOConfiguration(avg, true /* rollover always true */, almostFull) &&
                    writeShutdownControl(false) && resetFIFO();
        if (_periodic) {
            _latest   = 0;
            _interval = caluculate_interval_time(avg, rate);
            _mask     = adc_resolution_bits_table[m5::stl::to_underlying(width)];

            // M5_LIB_LOGI(">>> AVG:%u SR:%u => interval:%u  WID:%u => mask:%0x", avg, rate, _interval, width, _mask);
        }
    }
    return _periodic;
}

bool UnitMAX30102::start_periodic_measurement(const max30102::Mode mode, const max30102::ADC range,
                                              const max30102::Sampling rate, const max30102::LEDPulse width,
                                              const max30102::FIFOSampling avg, const uint8_t ir_current,
                                              const uint8_t red_current)
{
    if (inPeriodic()) {
        return false;
    }
    return writeMode(mode) && writeSpO2Configuration(range, rate, width) && write_fifo_sampling_average(avg) &&
           writeLEDCurrent(1, ir_current) && writeLEDCurrent(0, red_current) && start_periodic_measurement();
}

bool UnitMAX30102::stop_periodic_measurement()
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
bool UnitMAX30102::readMode(max30102::Mode& mode)
{
    mode = Mode::None;
    ModeConfiguration mc{};
    if (read_register8(MODE_CONFIGURATION, mc.value)) {
        mode = mc.mode();
        return true;
    }
    return false;
}

bool UnitMAX30102::writeMode(const max30102::Mode mode)
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

bool UnitMAX30102::readShutdownControl(bool& shdn)
{
    shdn = false;
    ModeConfiguration mc{};
    if (read_register8(MODE_CONFIGURATION, mc.value)) {
        shdn = mc.shdn();
        return true;
    }
    return false;
}

bool UnitMAX30102::writeShutdownControl(const bool shdn)
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

bool UnitMAX30102::readSpO2Configuration(max30102::ADC& range, max30102::Sampling& rate, max30102::LEDPulse& width)
{
    range = ADC::Range2048nA;
    rate  = Sampling::Rate50;
    width = LEDPulse::Width69;

    SpO2Configuration sc{};
    if (read_register8(SPO2_CONFIGURATION, sc.value)) {
        range = sc.range();
        rate  = sc.rate();
        width = sc.width();
        return true;
    }
    return false;
}

bool UnitMAX30102::write_spo2_configuration(const SpO2Configuration& sc)
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

bool UnitMAX30102::writeSpO2Configuration(const max30102::ADC range, const max30102::Sampling rate,
                                          const max30102::LEDPulse width)
{
    SpO2Configuration sc{};
    sc.range(range);
    sc.rate(rate);
    sc.width(width);
    return write_spo2_configuration(sc);
}

bool UnitMAX30102::writeSpO2ADCRange(const max30102::ADC range)
{
    SpO2Configuration sc{};
    if (read_register8(SPO2_CONFIGURATION, sc.value)) {
        sc.range(range);
        return write_spo2_configuration(sc);
    }
    return false;
}

bool UnitMAX30102::writeSpO2SamplingRate(const max30102::Sampling rate)
{
    SpO2Configuration sc{};
    if (read_register8(SPO2_CONFIGURATION, sc.value)) {
        sc.rate(rate);
        return write_spo2_configuration(sc);
    }
    return false;
}

bool UnitMAX30102::writeSpO2LEDPulseWidth(const max30102::LEDPulse width)
{
    SpO2Configuration sc{};
    if (read_register8(SPO2_CONFIGURATION, sc.value)) {
        sc.width(width);
        return write_spo2_configuration(sc);
    }
    return false;
}

bool UnitMAX30102::read_led_current(const uint8_t idx, uint8_t& raw)
{
    raw = 0;
    return (idx < 2) ? read_register8(LED_CONFIGURATION_1 + idx, raw) : false;
}

bool UnitMAX30102::read_led_current(const uint8_t idx, float& mA)
{
    mA = std::numeric_limits<float>::quiet_NaN();
    uint8_t raw{};
    if (read_led_current(idx, raw)) {
        mA = 0.2f * raw;
        return true;
    }
    return false;
}

bool UnitMAX30102::write_led_current(const uint8_t idx, const uint8_t raw)
{
    return (idx < 2) ? writeRegister8((uint8_t)(LED_CONFIGURATION_1 + idx), raw) : false;
}

bool UnitMAX30102::write_led_current(const uint8_t idx, const float mA)
{
    if (mA < 0.0f || mA > 51.0f) {
        M5_LIB_LOGE("Valid range 0.0 - 51.0 (0.2 increments) %f", mA);
        return false;
    }
    uint8_t raw = (uint8_t)(mA * 5);  // / 0.2f
    return write_led_current(idx, raw);
}

bool UnitMAX30102::readMultiLEDModeControl(max30102::Slot& slot1, max30102::Slot& slot2)
{
    slot1 = slot2 = Slot::None;
    MultiLEDControl mc{};
    if (read_register8(MULTI_LED_MODE_CONTROL_12, mc.value)) {
        slot1 = mc.slotL();
        slot2 = mc.slotH();
        return true;
    }
    return false;
}

bool UnitMAX30102::writeMultiLEDModeControl(const max30102::Slot slot1, const max30102::Slot slot2)
{
    if (inPeriodic()) {
        M5_LIB_LOGD("Periodic measurements are running");
        return false;
    }

    if (_mode != Mode::MultiLED) {
        M5_LIB_LOGW("Mode is not MultiLED");
        return false;
    }

    if (slot1 == Slot::None && slot2 != Slot::None) {
        M5_LIB_LOGE(
            "The slots contain incorrect values or are in the wrong order. The slots should be enabled in "
            "order. "
            "%u,%u",
            slot1, slot2);
        return false;
    }

    MultiLEDControl mc{};
    mc.slotL(slot1);
    mc.slotH(slot2);
    if (writeRegister8(MULTI_LED_MODE_CONTROL_12, mc.value)) {
        _slot[0] = slot1;
        _slot[1] = slot2;
        return true;
    }
    return false;
}

bool UnitMAX30102::measureTemperatureSingleshot(TemperatureData& td)
{
    // Request measure
    if (writeRegister8(TEMP_CONFIGURATION, 0x01)) {
        auto timeout_at = m5::utility::millis() + 500;     // timeout 0.5 sec
        m5::utility::delay(MEASURE_TEMPERATURE_DURATION);  // We have to wait at least this long
        do {
            uint8_t v{};
            if (read_register8(TEMP_CONFIGURATION, v) && (v == 0)) {
                return read_measurement_temperature(td);
            }
            m5::utility::delay(1);
        } while (m5::utility::millis() <= timeout_at);
        M5_LIB_LOGW("timeout");
    }
    return false;
}

bool UnitMAX30102::read_measurement_temperature(max30102::TemperatureData& td)
{
    return read_register(TEMP_INTEGER, td.raw.data(), td.raw.size());
}

bool UnitMAX30102::readFIFOConfiguration(max30102::FIFOSampling& avg, bool& rollover, uint8_t& almostFull)
{
    avg        = FIFOSampling::Average1;
    rollover   = false;
    almostFull = 0xFF;

    FIFOConfiguration fc{};
    if (read_register8(FIFO_CONFIGURATION, fc.value)) {
        avg        = fc.average();
        rollover   = fc.rollover();
        almostFull = fc.almostFull();
        return true;
    }
    return false;
}

bool UnitMAX30102::writeFIFOConfiguration(const max30102::FIFOSampling avg, const bool rollover,
                                          const uint8_t almostFull)
{
    if (inPeriodic()) {
        M5_LIB_LOGD("Periodic measurements are running");
        return false;
    }

    FIFOConfiguration fc{};
    fc.average(avg);
    fc.rollover(rollover);
    fc.almostFull(almostFull);
    return writeRegister8(FIFO_CONFIGURATION, fc.value);
}

bool UnitMAX30102::write_fifo_sampling_average(const max30102::FIFOSampling avg)
{
    FIFOConfiguration fc{};
    if (read_register8(FIFO_CONFIGURATION, fc.value)) {
        fc.average(avg);
        return writeRegister8(FIFO_CONFIGURATION, fc.value);
    }
    return false;
}

bool UnitMAX30102::reset_FIFO(const bool circling_read_ptr)
{
    if (!writeFIFOReadPointer(0) || !writeFIFOWritePointer(0) || !writeFIFOOverflowCounter(0)) {
        return false;
    }

    if (circling_read_ptr) {
        // Make the overflow counter behave normally by circling the read pointer
        uint32_t cnt{MAX_FIFO_DEPTH}, rcnt{};
        while (cnt--) {
            Data discard{};
            rcnt += read_register(FIFO_DATA_REGISTER, discard.raw.data(), discard.raw.size()) ? 1 : 0;
        }
        return (rcnt == MAX_FIFO_DEPTH);
    }
    return true;
}

bool UnitMAX30102::read_FIFO()
{
    uint8_t rptr{}, wptr{};
    _retrived = _overflow = 0;

    if (!readFIFOReadPointer(rptr) || !readFIFOWritePointer(wptr) || !readFIFOOverflowCounter(_overflow)) {
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
    uint32_t dlen = (_mode == Mode::HROnly)     ? 3
                    : (_mode == Mode::SpO2)     ? 6
                    : (_mode == Mode::MultiLED) ? 3 * ((_slot[0] != Slot::None) + (_slot[1] != Slot::None))
                                                : 0;
    if (dlen && readCount) {
        uint8_t reg{FIFO_DATA_REGISTER};
        if (writeWithTransaction(&reg, 1) != m5::hal::error::error_t::OK) {
            return false;
        }
        uint8_t rbuf[MAX_FIFO_DEPTH * 6]{};

        // M5_LIB_LOGE("blen:%u dlen:%u rc:%u len:%u/%u", read_buffer_length, dlen, readCount, dlen * readCount,
        //             MAX_FIFO_DEPTH * 6);

        int32_t left = dlen * readCount;

        while (left > 0) {
            uint32_t batch_len = (left > read_buffer_length) ? read_buffer_length - (read_buffer_length % dlen) : left;
            uint32_t batch_count = batch_len / dlen;

            // M5_LIB_LOGE("    batch:%u/%u", batch_len, batch_count);

            if (readWithTransaction(rbuf, batch_len) != m5::hal::error::error_t::OK) {
                return false;
            }

            for (uint_fast8_t i = 0; i < batch_count; ++i) {
                Data d;
                d.mask = _mask;
                switch (_mode) {
                        // IR 3 bytes
                    case Mode::HROnly:
                        memcpy(d.raw.data() + 3, rbuf + dlen * i, dlen);
                        break;
                        // Data order depends slots setting 3 or 6 bytes
                    case Mode::MultiLED:
                        memcpy(d.raw.data() + 3 * (_slot[0] == Slot::IR), rbuf + dlen * i, 3);
                        if (dlen == 6) {
                            memcpy(d.raw.data() + 3 * (_slot[1] == Slot::IR), rbuf + dlen * i + 3, 3);
                        }
                        break;
                        // SPO2 Red,IR 6 bytes
                    default:
                        memcpy(d.raw.data(), rbuf + dlen * i, dlen);
                        break;
                }
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

bool UnitMAX30102::reset()
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
                _slot[0] = _slot[1] = Slot::None;
                return true;
            }
            m5::utility::delay(1);
        } while (m5::utility::millis() <= timeout_at);
    }
    return false;
}

bool UnitMAX30102::readRevisionID(uint8_t& rev)
{
    rev = 0x00;
    return read_register8(READ_REVISION_ID, rev);
}

uint32_t UnitMAX30102::caluculateSamplingRate()
{
    FIFOSampling avg{};
    bool rollover{};
    uint8_t almostFull{};
    Sampling rate{};

    if (readFIFOConfiguration(avg, rollover, almostFull) && readSpO2SamplingRate(rate)) {
        return 1000 / caluculate_interval_time(avg, rate);
    }
    return 0;
}

// Max30102 works with stop bit false, so wrap
bool UnitMAX30102::read_register8(const uint8_t reg, uint8_t& v)
{
    return readRegister8(reg, v, 0, false /*stop*/);
}

bool UnitMAX30102::read_register(const uint8_t reg, uint8_t* buf, const size_t len)
{
    return readRegister(reg, buf, len, 0, false /*stop*/);
}

}  // namespace unit
}  // namespace m5
