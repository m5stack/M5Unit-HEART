/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file pulse_monitor.cpp
  @brief Calculate BPM and SpO2
*/
#include "pulse_monitor.hpp"
#include <vector>

namespace m5 {
namespace heart {

void PulseMonitor::setSamplingRate(const float samplingRate)
{
    if (samplingRate < 1.0f) {
        M5_LIB_LOGE("SamplingRate must be greater equal than 1.0f");
        return;
    }
    _sampling_rate = samplingRate;
    _max_samples   = (size_t)samplingRate * _range;

    _filterIR.setSamplingRate(5.0f, samplingRate);
    clear();
}

void PulseMonitor::clear()
{
    _dataIR.clear();
    _beat = false;
    _bpm = _SpO2 = 0.0f;

    _count  = 0;
    _avered = _aveir = _sumredrms = _sumirrms = 0;
}

void PulseMonitor::push_back(const float ir)
{
    _dataIR.push_back(_filterIR.process(ir));
    if (_dataIR.size() > _max_samples) {
        _dataIR.pop_front();
    }
}

void PulseMonitor::push_back(const float ir, const float red)
{
    push_back(ir);

    // For SpO2 (each second)
    _avered = _avered * 0.95f + red * (1.0f - 0.95f);
    _aveir  = _aveir * 0.95f + ir * (1.0f - 0.95f);
    _sumredrms += (red - _avered) * (red - _avered);
    _sumirrms += (ir - _aveir) * (ir - _aveir);
    if (++_count == (uint32_t)_sampling_rate) {
        float R    = (std::sqrt(_sumredrms) / _avered) / (std::sqrt(_sumirrms) / _aveir);
        _SpO2      = -23.3f * (R - 0.4f) + 100;
        _SpO2      = std::fmax(std::fmin(100.0f, _SpO2), 80.0f);  // clamp 80-100
        _sumredrms = _sumirrms = 0;
        _count                 = 0;
    }
}

void PulseMonitor::update()
{
    _bpm = calculate_bpm();
}

float PulseMonitor::calculate_bpm()
{
    std::vector<uint32_t> peaks;
    float threshold = 50.f;
    bool negatived{};
    for (uint32_t i = 1; i < _dataIR.size() - 1; ++i) {
        if (negatived && (_dataIR[i] > threshold) && _dataIR[i] > _dataIR[i - 1] && _dataIR[i] > _dataIR[i + 1]) {
            peaks.push_back(i);
            negatived = false;
            _beat     = (i == (_dataIR.size() - 2));  // last?
        } else if (!negatived && _dataIR[i] < 0.0f) {
            negatived = true;
        }
    }
    if (peaks.size() < 2) {
        return 0.0f;
    }

    float isum{};
    uint32_t cnt{};
    for (size_t i = 1; i < peaks.size(); ++i) {
        isum += (peaks[i] - peaks[i - 1]) / _sampling_rate;
        ++cnt;
    }
    float average_rr = isum / cnt;
    return 60.0f / average_rr;
}

}  // namespace heart
}  // namespace m5
