/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file unit_GH3038.cpp
  @brief GH3038 Unit for M5UnitUnified
*/
#include "unit_GH3038.hpp"

#include <algorithm>
#include <new>

#include <M5Utility.hpp>
#include <driver/gpio.h>
#include <esp_attr.h>
#include <esp_err.h>

#include "gh3038_auto_adjust.hpp"

using namespace m5::utility::mmh3;
using namespace m5::unit::types;
using namespace m5::unit::gh3038;

namespace {
#if defined(ARDUINO) && defined(I2C_BUFFER_LENGTH)
constexpr uint32_t read_buffer_length{I2C_BUFFER_LENGTH};
#else
constexpr uint32_t read_buffer_length{32};
#endif

bool is_valid(const Mode mode, const SamplingRate rate)
{
    return m5::stl::to_underlying(mode) <= m5::stl::to_underlying(Mode::All) &&
           m5::stl::to_underlying(rate) <= m5::stl::to_underlying(SamplingRate::Rate200);
}

uint32_t samples_per_read(const SamplingRate rate)
{
    return std::max<uint32_t>(1U, sampling_rate_to_hz(rate) / 10U);  // About every 100 ms
}

elapsed_time_t interval_ms(const SamplingRate rate)
{
    return std::max<uint32_t>(1U, samples_per_read(rate) * 1000U / sampling_rate_to_hz(rate) / 2U);
}

// Channels (bits of m5::heart::Channel) of the mode
uint8_t channels_of(const Mode mode)
{
    // Color and m5::heart::Channel share the order (Green, Red, IR)
    switch (mode) {
        case Mode::HR:
            return 1U << color_index(Color::Green);
        case Mode::SpO2:
            return (1U << color_index(Color::Red)) | (1U << color_index(Color::IR));
        default:
            return (1U << color_index(Color::Green)) | (1U << color_index(Color::Red)) | (1U << color_index(Color::IR));
    }
}

constexpr uint8_t max_gain_level{static_cast<uint8_t>(Gain::Level6)};

// Raw value ratio when the gain level changes from `from` to `to` (the same photodiode current)
float raw_ratio(const uint8_t from, const uint8_t to)
{
    constexpr int32_t probe{1 << 20};
    const float a = m5_gh3038_raw_to_pA(probe, from);
    const float b = m5_gh3038_raw_to_pA(probe, to);
    return (b > 0.0f) ? a / b : 0.0f;
}

// Samples skipped after a change (about 0.2 s)
uint16_t settle_samples(const SamplingRate rate)
{
    return static_cast<uint16_t>(sampling_rate_to_hz(rate) / 5U);
}
}  // namespace

namespace m5 {
namespace unit {

// class UnitGH3038
const char UnitGH3038::name[] = "UnitGH3038";
const types::uid_t UnitGH3038::uid{"UnitGH3038"_mmh3};
const types::attr_t UnitGH3038::attr{attribute::AccessI2C};

UnitGH3038::~UnitGH3038()
{
    remove_interrupt();
}

bool UnitGH3038::begin()
{
    remove_interrupt();

    if (m5_gh3038_api_version() != M5_GH3038_CORE_API_VERSION || m5_gh3038_state_size() != sizeof(m5_gh3038_state_t)) {
        M5_LIB_LOGE("GH3038 core mismatch (API:%u/%u size:%u/%u)", (unsigned)m5_gh3038_api_version(),
                    (unsigned)M5_GH3038_CORE_API_VERSION, (unsigned)m5_gh3038_state_size(),
                    (unsigned)sizeof(m5_gh3038_state_t));
        return false;
    }

    const auto ssize = stored_size();
    if (ssize != _data->capacity()) {
        _data.reset(new m5::container::CircularBuffer<Data>(ssize));
        if (!_data) {
            M5_LIB_LOGE("Failed to allocate");
            return false;
        }
    }

    if ((_cfg.rst_pin >= 0 && !GPIO_IS_VALID_OUTPUT_GPIO(_cfg.rst_pin)) ||
        (_cfg.int_pin >= 0 && !GPIO_IS_VALID_GPIO(_cfg.int_pin)) || !is_valid(_cfg.mode, _cfg.rate)) {
        M5_LIB_LOGE("Invalid configuration");
        return false;
    }
    _mode = _cfg.mode;
    _rate = _cfg.rate;

    if (_cfg.rst_pin >= 0 && !setup_rst_pin()) {
        return false;
    }

    m5_gh3038_io_t io{};
    io.user     = this;
    io.write    = &UnitGH3038::io_write;
    io.read     = &UnitGH3038::io_read;
    io.delay_us = &UnitGH3038::io_delay_us;
    io.set_rst  = (_cfg.rst_pin >= 0) ? &UnitGH3038::io_set_rst : nullptr;
    io.max_read = (read_buffer_length / 4U) * 4U;  // Read the FIFO in whole words
    _periodic   = false;                           // The core state (running) is initialized here
    int r       = m5_gh3038_init(&_core, &io);
    _io_owner   = (r == M5_GH3038_OK) ? this : nullptr;

    const Gain gains[NUMBER_OF_COLORS]       = {_cfg.green_gain, _cfg.red_gain, _cfg.ir_gain};
    const uint8_t currents[NUMBER_OF_COLORS] = {_cfg.green_current, _cfg.red_current, _cfg.ir_current};
    for (uint8_t c = 0; r == M5_GH3038_OK && c < NUMBER_OF_COLORS; ++c) {
        r = m5_gh3038_set_gain(core(), c, m5::stl::to_underlying(gains[c]));
        if (r == M5_GH3038_OK) {
            r = m5_gh3038_set_led_current(core(), c, currents[c]);
        }
    }
    if (r != M5_GH3038_OK) {
        M5_LIB_LOGE("Invalid configuration (%d)", r);
        return false;
    }
    for (uint8_t c = 0; c < NUMBER_OF_COLORS; ++c) {
        _led_code[c] = _ref_code[c] = currents[c];
        _gain_level[c] = _max_gain[c] = m5::stl::to_underlying(gains[c]);
    }
    _auto_adjust = _cfg.auto_adjust;

    r = m5_gh3038_reset(core(), _cfg.rst_pin >= 0);
    if (r != M5_GH3038_OK) {
        M5_LIB_LOGE("GH3038 is not ready (%d). Check the I2C and RST wiring", r);
        return false;
    }
    if (_cfg.int_pin >= 0 && !setup_interrupt()) {
        M5_LIB_LOGW("Failed to setup the INT interrupt, fall back to polling");
    }
    return _cfg.start_periodic ? startPeriodicMeasurement(_cfg.mode, _cfg.rate) : true;
}

void UnitGH3038::update(const bool force)
{
    _updated = false;
    if (!inPeriodic()) {
        return;
    }
    if (!force && _latest && !m5::utility::hasElapsed(_latest, _interval)) {
        return;
    }
    if (!force && _int && !interrupt_asserted()) {
        return;
    }
    if (_int) {
        _int->pending = false;  // Clear before reading the chip
    }
    _updated = read_fifo();
    if (_updated && _auto_adjust) {
        auto_adjust();
    }
    if (_updated) {
        _latest = m5::utility::millis();
    }
}

bool UnitGH3038::start_periodic_measurement()
{
    return start_periodic_measurement(_mode, _rate);
}

bool UnitGH3038::start_periodic_measurement(const Mode mode, const SamplingRate rate)
{
    if (inPeriodic()) {
        return false;
    }
    if (!is_valid(mode, rate)) {
        M5_LIB_LOGE("Invalid mode or rate");
        return false;
    }
    const int r = m5_gh3038_start(core(), m5::stl::to_underlying(mode), m5::stl::to_underlying(rate));
    if (r != M5_GH3038_OK) {
        M5_LIB_LOGE("Failed to start (%d)", r);
        return false;
    }
    _mode = mode;
    _rate = rate;
    _data->clear();
    _ppg_pushed   = 0;
    _ppg_consumed = 0;
    if (_int) {
        _int->pending = false;
    }
    _latest   = 0;
    _interval = interval_ms(rate);
    reset_adjust_window();
    _periodic = true;
    return true;
}

bool UnitGH3038::stop_periodic_measurement()
{
    const int r = m5_gh3038_stop(core());
    if (r != M5_GH3038_OK) {
        M5_LIB_LOGE("Failed to stop (%d)", r);
        return false;
    }
    _periodic = false;
    return true;
}

bool UnitGH3038::readGain(Gain& gain, const Color color)
{
    uint8_t level{};
    if (m5_gh3038_get_gain(&_core, color_index(color), &level) != M5_GH3038_OK) {
        return false;
    }
    gain = static_cast<Gain>(level);
    return true;
}

bool UnitGH3038::writeGain(const Color color, const Gain gain)
{
    if (_auto_adjust && inPeriodic()) {
        M5_LIB_LOGW("Auto adjustment is enabled. Call setAutoAdjust(false) first");
        return false;
    }
    const int r = m5_gh3038_set_gain(core(), color_index(color), m5::stl::to_underlying(gain));
    if (r == M5_GH3038_ERR_ARG) {
        M5_LIB_LOGE("Invalid color or gain");
    }
    if (r != M5_GH3038_OK) {
        return false;
    }
    _gain_level[color_index(color)] = m5::stl::to_underlying(gain);
    return true;
}

bool UnitGH3038::readLEDCurrent(uint8_t& code, const Color color)
{
    return m5_gh3038_get_led_current(&_core, color_index(color), &code) == M5_GH3038_OK;
}

bool UnitGH3038::writeLEDCurrent(const Color color, const uint8_t code)
{
    if (_auto_adjust && inPeriodic()) {
        M5_LIB_LOGW("Auto adjustment is enabled. Call setAutoAdjust(false) first");
        return false;
    }
    const int r = m5_gh3038_set_led_current(core(), color_index(color), code);
    if (r == M5_GH3038_ERR_ARG) {
        M5_LIB_LOGE("Invalid color");
    }
    if (r != M5_GH3038_OK) {
        return false;
    }
    _led_code[color_index(color)] = code;
    return true;
}

bool UnitGH3038::setAutoAdjust(const bool enable)
{
    if (enable && !_auto_adjust) {
        reset_adjust_window();
    }
    _auto_adjust = enable;
    return true;
}

void UnitGH3038::reset_adjust_window()
{
    for (auto&& w : _window) {
        w      = adjust_window_t{};
        w.skip = settle_samples(_rate);
    }
}

void UnitGH3038::accumulate_adjust(const Data& d)
{
    for (uint8_t c = 0; c < NUMBER_OF_COLORS; ++c) {
        if (!(d.valid & (1U << c))) {
            continue;
        }
        auto& w = _window[c];
        if (w.skip) {
            --w.skip;
            continue;
        }
        const int32_t v = d.raw[c];
        w.max           = w.count ? std::max(w.max, v) : v;
        w.sum += v;
        ++w.count;
    }
}

void UnitGH3038::auto_adjust()
{
    const uint16_t window  = static_cast<uint16_t>(sampling_rate_to_hz(_rate));  // 1 s
    const float full_scale = static_cast<float>(m5_gh3038_raw_full_scale());
    for (uint8_t c = 0; c < NUMBER_OF_COLORS; ++c) {
        auto& w = _window[c];
        if (w.count < window) {
            continue;
        }
        AdjustInput in{};
        in.mean           = static_cast<float>(w.sum) / static_cast<float>(w.count);
        in.max            = static_cast<float>(w.max);
        in.full_scale     = full_scale;
        in.code           = _led_code[c];
        in.gain           = _gain_level[c];
        in.max_code       = _ref_code[c];
        in.max_gain       = _max_gain[c];
        in.raw_ratio_down = (in.gain > 0) ? raw_ratio(in.gain, in.gain - 1U) : 0.0f;
        in.raw_ratio_up   = (in.gain < max_gain_level) ? raw_ratio(in.gain, in.gain + 1U) : 0.0f;
        w                 = adjust_window_t{};

        const auto out = decide_adjustment(in);
        if (!out.changed(in)) {
            continue;
        }
        // Called right after the FIFO is read out, so the new settings apply to the following samples only
        if (out.gain != in.gain && m5_gh3038_set_gain(core(), c, out.gain) == M5_GH3038_OK) {
            _gain_level[c] = out.gain;
        }
        if (out.code != in.code && m5_gh3038_set_led_current(core(), c, out.code) == M5_GH3038_OK) {
            _led_code[c] = out.code;
        }
        w.skip = settle_samples(_rate);
        M5_LIB_LOGD("Auto adjust color:%u code:%u->%u gain:%u->%u", c, in.code, _led_code[c], in.gain, _gain_level[c]);
    }
}

bool UnitGH3038::softReset()
{
    _periodic   = false;
    const int r = m5_gh3038_reset(core(), false);
    if (r != M5_GH3038_OK) {
        M5_LIB_LOGE("Failed to reset (%d)", r);
        return false;
    }
    _data->clear();
    return true;
}

bool UnitGH3038::read_fifo()
{
    uint16_t count{};
    bool overflowed{};
    const int r = m5_gh3038_read(core(), &UnitGH3038::on_sample, this, &count, &overflowed);
    _retrieved  = count;
    _overflowed = overflowed;
    if (overflowed) {
        M5_LIB_LOGW("FIFO overflow");
    }
    if (r != M5_GH3038_OK) {
        M5_LIB_LOGE("Failed to read the FIFO (%d)", r);
        return false;
    }
    return count != 0;
}

bool UnitGH3038::read_fifo_level(uint16_t& words)
{
    return m5_gh3038_fifo_level(core(), &words) == M5_GH3038_OK;
}

m5::heart::PPGSpec UnitGH3038::ppgSpec() const
{
    m5::heart::PPGSpec spec{};
    spec.channels         = channels_of(_mode);
    spec.sampling_rate_hz = inPeriodic() ? static_cast<float>(sampling_rate_to_hz(_rate)) : 0.0f;
    spec.unit             = m5::heart::ValueUnit::PicoAmpere;
    return spec;
}

bool UnitGH3038::readPPG(m5::heart::PPGSample& out)
{
    if (!_data || _data->empty() || _ppg_pushed == _ppg_consumed) {
        return false;
    }
    uint32_t lag = _ppg_pushed - _ppg_consumed;
    if (lag > _data->size()) {
        // Overwritten or discarded before being read: continue from the oldest kept
        lag           = _data->size();
        _ppg_consumed = _ppg_pushed - lag;
    }
    const auto d = _data->at(_data->size() - lag);
    if (!d) {
        return false;
    }
    ++_ppg_consumed;

    out                  = m5::heart::PPGSample{};
    const Color colors[] = {Color::Green, Color::Red, Color::IR};
    for (const auto c : colors) {
        if (d->has(c)) {
            const uint8_t i = color_index(c);
            float v         = d->current(c);
            if (d->led_current[i] && _ref_code[i] && d->led_current[i] != _ref_code[i]) {
                v *= static_cast<float>(_ref_code[i]) / static_cast<float>(d->led_current[i]);
            }
            out.set(static_cast<m5::heart::Channel>(i), v);
        }
    }
    return true;
}

bool UnitGH3038::setup_rst_pin()
{
    const auto pin = static_cast<gpio_num_t>(_cfg.rst_pin);
    gpio_reset_pin(pin);
    if (gpio_set_direction(pin, GPIO_MODE_OUTPUT) != ESP_OK) {
        M5_LIB_LOGE("Failed to set RST pin %d as output", _cfg.rst_pin);
        return false;
    }
    gpio_set_level(pin, 1);
    return true;
}

bool UnitGH3038::setup_interrupt()
{
    const auto pin = static_cast<gpio_num_t>(_cfg.int_pin);
    gpio_config_t io{};
    io.pin_bit_mask = 1ULL << _cfg.int_pin;
    io.mode         = GPIO_MODE_INPUT;
    io.pull_up_en   = GPIO_PULLUP_DISABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type    = GPIO_INTR_DISABLE;
    if (gpio_config(&io) != ESP_OK) {
        M5_LIB_LOGE("gpio_config failed for pin %d", _cfg.int_pin);
        return false;
    }
    // ESP_ERR_INVALID_STATE means the ISR service is already installed, which is fine
    const esp_err_t svc = gpio_install_isr_service(0);
    if (svc != ESP_OK && svc != ESP_ERR_INVALID_STATE) {
        M5_LIB_LOGE("gpio_install_isr_service failed: %d", svc);
        return false;
    }
    auto p = new (std::nothrow) interrupt_t{};
    if (!p) {
        M5_LIB_LOGE("Failed to allocate");
        return false;
    }
    // The idle level is detected at setup (the interrupt status was cleared by the reset),
    // and the edge toward the active level is used
    p->pin        = _cfg.int_pin;
    p->idle_level = gpio_get_level(pin);
    if (gpio_set_intr_type(pin, p->idle_level ? GPIO_INTR_NEGEDGE : GPIO_INTR_POSEDGE) != ESP_OK ||
        gpio_isr_handler_add(pin, &UnitGH3038::isr_handler, p) != ESP_OK) {
        gpio_set_intr_type(pin, GPIO_INTR_DISABLE);
        delete p;
        M5_LIB_LOGE("Failed to setup the interrupt for pin %d", _cfg.int_pin);
        return false;
    }
    _int.reset(p);
    return true;
}

void UnitGH3038::remove_interrupt()
{
    _int.reset();
}

void UnitGH3038::interrupt_release_t::operator()(interrupt_t* p) const
{
    const auto pin = static_cast<gpio_num_t>(p->pin);
    gpio_isr_handler_remove(pin);  // Before the memory the ISR writes is freed
    gpio_set_intr_type(pin, GPIO_INTR_DISABLE);
    delete p;
}

bool UnitGH3038::interrupt_asserted() const
{
    // Also check the level in case an edge was missed
    return _int->pending || gpio_get_level(static_cast<gpio_num_t>(_int->pin)) != _int->idle_level;
}

void IRAM_ATTR UnitGH3038::isr_handler(void* arg)
{
    static_cast<interrupt_t*>(arg)->pending = true;
}

// The core calls back through io.user: point it to this object again after the unit is moved
m5_gh3038_state_t* UnitGH3038::core()
{
    if (_io_owner != this && m5_gh3038_set_io_user(&_core, this) == M5_GH3038_OK) {
        _io_owner = this;
    }
    return &_core;
}

bool UnitGH3038::io_write(void* user, const uint16_t reg, const uint8_t* data, const uint32_t len)
{
    return static_cast<UnitGH3038*>(user)->writeRegister(reg, data, len);
}

bool UnitGH3038::io_read(void* user, const uint16_t reg, uint8_t* rx, const uint32_t len)
{
    // Repeated start after the register address (same as the former readRegister16BE(..., false))
    return static_cast<UnitGH3038*>(user)->readRegister(reg, rx, len, 0, false);
}

void UnitGH3038::io_delay_us(void* /*user*/, const uint32_t us)
{
    if (us >= 1000U) {
        m5::utility::delay(us / 1000U);  // Yields to other tasks
    }
    if (us % 1000U) {
        m5::utility::delayMicroseconds(us % 1000U);
    }
}

bool UnitGH3038::io_set_rst(void* user, const int level)
{
    const auto self = static_cast<const UnitGH3038*>(user);
    return gpio_set_level(static_cast<gpio_num_t>(self->_cfg.rst_pin), level ? 1 : 0) == ESP_OK;
}

void UnitGH3038::on_sample(void* user, const m5_gh3038_sample_t* sample)
{
    auto self = static_cast<UnitGH3038*>(user);
    Data d{};
    for (uint8_t i = 0; i < NUMBER_OF_COLORS; ++i) {
        d.raw[i]         = sample->raw[i];
        d.gain[i]        = static_cast<Gain>(sample->gain[i]);
        d.led_current[i] = self->_led_code[i];
    }
    d.valid = sample->valid;
    if (self->_auto_adjust) {
        self->accumulate_adjust(d);
    }
    self->_data->push_back(d);
    ++self->_ppg_pushed;
}

}  // namespace unit
}  // namespace m5
