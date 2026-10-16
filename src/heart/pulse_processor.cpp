/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file pulse_processor.cpp
  @brief Heart rate, SpO2 and perfusion index from PPG samples
*/
#include "pulse_processor.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
using m5::heart::Channel;
using m5::heart::channel_bit;
using m5::heart::channel_index;

constexpr float BAND_LOW_HZ{0.5f};            // Band-pass lower cutoff
constexpr float BAND_HIGH_HZ{4.0f};           // Band-pass upper cutoff
constexpr float DC_TIME_CONSTANT_SEC{1.0f};   // DC estimation (EMA)
constexpr float SETTLE_SEC{2.0f};             // Filters settle before the beat detection starts
constexpr uint32_t MIN_CONSECUTIVE_BEATS{3};  // Valid beats in a row required for the heart rate
constexpr float PERFUSION_MIN{0.05f};         // Plausible perfusion index (%)
constexpr float PERFUSION_MAX{20.0f};         //
constexpr uint8_t PEAKS_BEFORE_INTERVAL{2};   // The first trough found after a peak starts the first interval
constexpr float BPM_TIME_CONSTANT_SEC{4.0f};  // Smoothing of the heart rate (EMA per beat)
constexpr float RATIO_TIME_CONSTANT_SEC{4.0f};  // Smoothing of the ratio of ratios for SpO2 (EMA per beat)

constexpr uint8_t RED{channel_index(Channel::Red)};
constexpr uint8_t IR{channel_index(Channel::IR)};

uint8_t select_hr_channel(const uint8_t channels, const int8_t requested)
{
    if (requested >= 0) {
        return (requested < m5::heart::NUMBER_OF_CHANNELS && (channels & (1U << requested)))
                   ? static_cast<uint8_t>(requested)
                   : m5::heart::INVALID_CHANNEL;
    }
    const Channel order[] = {Channel::Green, Channel::IR, Channel::Red};
    for (const auto ch : order) {
        if (channels & channel_bit(ch)) {
            return channel_index(ch);
        }
    }
    return m5::heart::INVALID_CHANNEL;
}

bool has_finite(const m5::heart::PPGSample& s, const uint8_t idx)
{
    return (s.valid & (1U << idx)) && std::isfinite(s.value[idx]);
}

}  // namespace

namespace m5 {
namespace heart {

void PulseProcessor::Accumulator::clear()
{
    ac_max  = -std::numeric_limits<float>::infinity();
    ac_min  = std::numeric_limits<float>::infinity();
    raw_sum = 0.0f;
    count   = 0;
}

void PulseProcessor::Accumulator::add(const float ac, const float raw)
{
    ac_max = std::max(ac_max, ac);
    ac_min = std::min(ac_min, ac);
    raw_sum += raw;
    ++count;
}

void PulseProcessor::Accumulator::merge(const Accumulator& o)
{
    if (!o.count) {
        return;
    }
    ac_max = std::max(ac_max, o.ac_max);
    ac_min = std::min(ac_min, o.ac_min);
    raw_sum += o.raw_sum;
    count += o.count;
}

float PulseProcessor::Accumulator::range() const
{
    return count ? ac_max - ac_min : std::numeric_limits<float>::quiet_NaN();
}

float PulseProcessor::Accumulator::mean() const
{
    return count ? raw_sum / count : std::numeric_limits<float>::quiet_NaN();
}

PulseProcessor::PulseProcessor(const float sampling_rate_hz, const uint8_t channels)
    : PulseProcessor(sampling_rate_hz, channels, config_t{})
{
}

PulseProcessor::PulseProcessor(const float sampling_rate_hz, const uint8_t channels, const config_t& cfg)
    : _rate{(sampling_rate_hz > 0.0f) ? sampling_rate_hz : 0.0f},
      _channels{static_cast<uint8_t>(channels & 0x07)},
      _cfg(cfg)
{
    if (_rate > 0.0f) {
        _hr           = select_hr_channel(_channels, _cfg.hr_channel);
        _pi           = (_hr != INVALID_CHANNEL && (_channels & channel_bit(Channel::IR))) ? IR : _hr;
        _spo2_enabled = _cfg.spo2 && (_hr != INVALID_CHANNEL) && (_channels & channel_bit(Channel::Red)) &&
                        (_channels & channel_bit(Channel::IR));
        _dc_alpha = 1.0f - std::exp(-1.0f / (DC_TIME_CONSTANT_SEC * _rate));
        _settle   = static_cast<uint32_t>(std::lround(SETTLE_SEC * _rate));
        for (auto& bp : _bandpass) {
            bp = BandPass(BAND_LOW_HZ, BAND_HIGH_HZ, _rate);
        }
        _detector = BeatDetector(_rate);
    }
    reset();
}

void PulseProcessor::reset()
{
    _count = 0;
    for (uint8_t c = 0; c < NUMBER_OF_CHANNELS; ++c) {
        _primed[c] = false;
        _dc[c]     = 0.0f;
        _bandpass[c].reset();
        _interval[c].clear();
        _pending[c].clear();
    }
    _detector.reset();
    _estimator.clear();
    _peaks           = 0;
    _beat            = false;
    _perfusion_index = std::numeric_limits<float>::quiet_NaN();
    _hr_perfusion    = std::numeric_limits<float>::quiet_NaN();
    _wave            = 0.0f;
    _bpm             = std::numeric_limits<float>::quiet_NaN();
    _ratio           = std::numeric_limits<float>::quiet_NaN();
}

void PulseProcessor::push(const PPGSample& s)
{
    _beat = false;
    if (_hr == INVALID_CHANNEL || !has_finite(s, _hr)) {
        return;
    }

    float ac[NUMBER_OF_CHANNELS]{};
    float raw[NUMBER_OF_CHANNELS]{};
    bool present[NUMBER_OF_CHANNELS]{};
    for (uint8_t c = 0; c < NUMBER_OF_CHANNELS; ++c) {
        if (!(_channels & (1U << c)) || !has_finite(s, c)) {
            continue;
        }
        const float x = s.value[c];
        if (!_primed[c]) {
            _primed[c] = true;
            _dc[c]     = x;
        }
        _dc[c] += _dc_alpha * (x - _dc[c]);
        // Reflective PPG: the value falls in systole, so invert to make the systolic peak positive
        ac[c]      = -_bandpass[c].process(x);
        raw[c]     = x;
        present[c] = true;
    }
    _wave = ac[_hr];

    if (++_count <= _settle) {
        return;
    }
    const uint8_t events = _detector.process(ac[_hr]);
    if (events & BeatDetector::EXPIRED) {
        _estimator.clear();
        _perfusion_index = std::numeric_limits<float>::quiet_NaN();
        _hr_perfusion    = std::numeric_limits<float>::quiet_NaN();
        _bpm             = std::numeric_limits<float>::quiet_NaN();
        _ratio           = std::numeric_limits<float>::quiet_NaN();
    }
    if (events & BeatDetector::RESEEDED) {
        _bpm = std::numeric_limits<float>::quiet_NaN();  // The rhythm changed: restart the smoothing
    }
    accumulate(events, ac, raw, _spo2_enabled && present[RED] && present[IR], present[_pi]);
}

void PulseProcessor::accumulate(const uint8_t events, const float* ac, const float* raw, const bool spo2_sample,
                                const bool pi_sample)
{
    if (events & BeatDetector::TROUGH_CANDIDATE) {
        // The trough moved: everything up to here belongs to the current interval
        for (uint8_t c = 0; c < NUMBER_OF_CHANNELS; ++c) {
            _interval[c].merge(_pending[c]);
            _pending[c].clear();
        }
    }
    _pending[_hr].add(ac[_hr], raw[_hr]);
    if (spo2_sample) {
        _pending[RED].add(ac[RED], raw[RED]);
        _pending[IR].add(ac[IR], raw[IR]);
    } else if (pi_sample && _pi != _hr) {
        _pending[_pi].add(ac[_pi], raw[_pi]);
    }

    if (events & BeatDetector::PEAK) {
        // The trough before this peak is final: _interval is one trough-to-trough beat
        finish_beat((events & BeatDetector::BEAT) != 0);
        for (uint8_t c = 0; c < NUMBER_OF_CHANNELS; ++c) {
            _interval[c] = _pending[c];
            _pending[c].clear();
        }
    }
}

void PulseProcessor::finish_beat(const bool valid)
{
    _beat = valid;
    if (_peaks < PEAKS_BEFORE_INTERVAL) {
        ++_peaks;
        return;
    }
    if (!valid) {
        return;
    }
    const float dc = _dc[_hr];
    const float pp = _interval[_hr].range();
    if (dc > 0.0f && std::isfinite(pp)) {
        _hr_perfusion = pp / dc * 100.0f;
    }
    const float pi_dc = _dc[_pi];
    const float pi_pp = _interval[_pi].range();
    if (pi_dc > 0.0f && std::isfinite(pi_pp)) {
        _perfusion_index = pi_pp / pi_dc * 100.0f;
    }
    smooth_bpm();
    if (_spo2_enabled) {
        _estimator.addBeat(_interval[RED].range(), _interval[RED].mean(), _interval[IR].range(), _interval[IR].mean());
        smooth_ratio();
    }
}

void PulseProcessor::smooth_ratio()
{
    if (!hasSpO2()) {
        _ratio = std::numeric_limits<float>::quiet_NaN();
        return;
    }
    // Smooth the median of the recent R over a few seconds as the heart rate. The first valid value starts from the
    // median as it is
    const float median = _estimator.ratio();
    if (std::isnan(_ratio)) {
        _ratio = median;
        return;
    }
    const float alpha = 1.0f - std::exp(-(60.0f / bpm()) / RATIO_TIME_CONSTANT_SEC);
    _ratio += alpha * (median - _ratio);
}

void PulseProcessor::smooth_bpm()
{
    if (!hasBpm()) {
        _bpm = std::numeric_limits<float>::quiet_NaN();
        return;
    }
    // The median of the recent RR intervals follows the beat-to-beat variation (respiration etc.), so smooth it
    // over a few seconds. The first valid value starts from the median as it is
    const float median = _detector.bpm();
    if (std::isnan(_bpm)) {
        _bpm = median;
        return;
    }
    const float alpha = 1.0f - std::exp(-(60.0f / median) / BPM_TIME_CONSTANT_SEC);
    _bpm += alpha * (median - _bpm);
}

bool PulseProcessor::hasBpm() const
{
    return _hr != INVALID_CHANNEL && _count > _settle && _detector.hasRecentBeat() &&
           _detector.consecutiveBeats() >= MIN_CONSECUTIVE_BEATS && _detector.rrCount() >= MIN_CONSECUTIVE_BEATS &&
           _hr_perfusion >= PERFUSION_MIN && _hr_perfusion <= PERFUSION_MAX;
}

float PulseProcessor::bpm() const
{
    if (!hasBpm()) {
        return std::numeric_limits<float>::quiet_NaN();
    }
    return std::isnan(_bpm) ? _detector.bpm() : _bpm;
}

bool PulseProcessor::hasSpO2() const
{
    return _spo2_enabled && hasBpm() && _estimator.isValid();
}

float PulseProcessor::spo2() const
{
    if (!hasSpO2()) {
        return std::numeric_limits<float>::quiet_NaN();
    }
    const float r = ratio();
    if (!_cfg.spo2_conversion) {
        return ratio_to_spo2(r);
    }
    const float v = _cfg.spo2_conversion(r);
    return std::isnan(v) ? v : std::min(100.0f, std::max(0.0f, v));
}

float PulseProcessor::ratio() const
{
    if (!hasSpO2()) {
        return std::numeric_limits<float>::quiet_NaN();
    }
    return std::isnan(_ratio) ? _estimator.ratio() : _ratio;
}

}  // namespace heart
}  // namespace m5
