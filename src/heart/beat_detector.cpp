/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file beat_detector.cpp
  @brief Beat detection on the pulsatile PPG component
*/
#include "beat_detector.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr float PEAK_THRESHOLD_RATIO{0.5f};  // Peak must reach this ratio of the envelope
constexpr float ENVELOPE_DECAY_SEC{2.0f};    // Time constant of the envelope decay
constexpr float REFRACTORY_SEC{0.27f};       // Minimum time between peaks
constexpr float RR_MIN_SEC{0.27f};           // 220 bpm
constexpr float RR_MAX_SEC{2.0f};            // 30 bpm
constexpr float RR_TOLERANCE{0.3f};          // Allowed deviation from the median RR
constexpr float BEAT_TIMEOUT_SEC{3.0f};      // History is discarded without a valid beat for this time
constexpr uint8_t RESEED_COUNT{3};           // Consecutive consistent rejections that restart the history

bool within_tolerance(const float v, const float center)
{
    return std::fabs(v - center) <= RR_TOLERANCE * center;
}

float median3(const float* v)
{
    return std::max(std::min(v[0], v[1]), std::min(std::max(v[0], v[1]), v[2]));
}

}  // namespace

namespace m5 {
namespace heart {

constexpr uint8_t BeatDetector::TROUGH_CANDIDATE;
constexpr uint8_t BeatDetector::PEAK;
constexpr uint8_t BeatDetector::BEAT;
constexpr uint8_t BeatDetector::EXPIRED;

BeatDetector::BeatDetector(const float sampling_rate_hz)
{
    if (sampling_rate_hz > 0.0f) {
        _rate       = sampling_rate_hz;
        _refractory = static_cast<uint32_t>(std::lround(REFRACTORY_SEC * sampling_rate_hz));
        _decay      = std::exp(-1.0f / (ENVELOPE_DECAY_SEC * sampling_rate_hz));
        _timeout    = static_cast<uint32_t>(std::lround(BEAT_TIMEOUT_SEC * sampling_rate_hz));
    }
    reset();
}

void BeatDetector::reset()
{
    _index            = 0;
    _previous         = 0.0f;
    _envelope         = 0.0f;
    _trough           = std::numeric_limits<float>::infinity();
    _above            = false;
    _candidate        = 0.0f;
    _candidate_before = 0.0f;
    _candidate_after  = 0.0f;
    _need_after       = false;
    _candidate_index  = 0;
    _has_peak         = false;
    _peak_index       = 0;
    _peak_offset      = 0.0f;
    _has_beat         = false;
    _beat_index       = 0;
    _consecutive      = 0;
    _rr.clear();
    _rejected_count = 0;
}

uint8_t BeatDetector::process(const float ac)
{
    if (!(_rate > 0.0f)) {
        return 0;
    }
    uint8_t events{};
    const uint32_t idx = _index++;

    // Envelope follows the peak height and decays with the time constant
    _envelope *= _decay;
    if (ac > _envelope) {
        _envelope = ac;
    }

    // Trough: the lowest value since the latest peak
    if (ac < _trough) {
        _trough = ac;
        events |= TROUGH_CANDIDATE;
    }

    const float threshold = _envelope * PEAK_THRESHOLD_RATIO;
    if (!_above) {
        if (ac > 0.0f && ac >= threshold && (!_has_peak || idx - _peak_index >= _refractory)) {
            _above            = true;
            _candidate        = ac;
            _candidate_before = _previous;
            _candidate_index  = idx;
            _need_after       = true;
        }
    } else {
        if (ac > _candidate) {
            _candidate        = ac;
            _candidate_before = _previous;
            _candidate_index  = idx;
            _need_after       = true;
        } else if (_need_after) {
            _candidate_after = ac;
            _need_after      = false;
        }
        if (ac < threshold) {
            // Fell below the threshold: the highest point of the excursion is the peak
            _above             = false;
            const float offset = candidate_offset();
            events |= PEAK;
            if (_has_peak && accept_rr((_candidate_index - _peak_index + offset - _peak_offset) / _rate)) {
                events |= BEAT;
                _has_beat   = true;
                _beat_index = _candidate_index;
            }
            _has_peak    = true;
            _peak_index  = _candidate_index;
            _peak_offset = offset;
            _trough      = std::numeric_limits<float>::infinity();
        }
    }
    _previous = ac;

    if (_has_beat && idx - _beat_index > _timeout) {
        _has_beat       = false;
        _consecutive    = 0;
        _rejected_count = 0;
        _rr.clear();
        events |= EXPIRED;
    }
    return events;
}

float BeatDetector::candidate_offset() const
{
    // Parabola through the peak sample and its neighbors gives the sub-sample position (-0.5...0.5)
    const float denom = _candidate_before - 2.0f * _candidate + _candidate_after;
    if (_need_after || !(denom < 0.0f)) {
        return 0.0f;
    }
    const float offset = 0.5f * (_candidate_before - _candidate_after) / denom;
    return std::max(-0.5f, std::min(0.5f, offset));
}

float BeatDetector::bpm() const
{
    return _rr.size() ? 60.0f / _rr.median() : std::numeric_limits<float>::quiet_NaN();
}

bool BeatDetector::accept_rr(const float rr)
{
    if (rr < RR_MIN_SEC || rr > RR_MAX_SEC) {
        _consecutive = 0;
        return false;
    }
    if (_rr.size() && !within_tolerance(rr, _rr.median())) {
        return reject_rr(rr);
    }
    _rr.push(rr);
    _rejected_count = 0;
    ++_consecutive;
    return true;
}

bool BeatDetector::reject_rr(const float rr)
{
    _consecutive = 0;
    if (_rejected_count == RESEED_COUNT) {
        _rejected[0] = _rejected[1];
        _rejected[1] = _rejected[2];
        --_rejected_count;
    }
    _rejected[_rejected_count++] = rr;
    if (_rejected_count < RESEED_COUNT) {
        return false;
    }
    // Consecutive rejections that agree with each other: the rhythm has changed
    const float med = median3(_rejected);
    for (uint8_t i = 0; i < RESEED_COUNT; ++i) {
        if (!within_tolerance(_rejected[i], med)) {
            return false;
        }
    }
    _rr.clear();
    for (uint8_t i = 0; i < RESEED_COUNT; ++i) {
        _rr.push(_rejected[i]);
    }
    _rejected_count = 0;
    _consecutive    = RESEED_COUNT;
    return true;
}

}  // namespace heart
}  // namespace m5
