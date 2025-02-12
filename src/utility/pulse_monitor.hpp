/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file pulse_monitor.hpp
  @brief Calculate BPM and SpO2
*/
#ifndef M5_UNIT_HEART_UTILITY_PULSE_MONITOR_HPP
#define M5_UNIT_HEART_UTILITY_PULSE_MONITOR_HPP

#include <limits>
#include <cmath>
#include <cassert>
#include <deque>
#include <m5_utility/log/library_log.hpp>

namespace m5 {
/*!
  @namepsace heart
  @brief Unit-HEART releated
 */
namespace heart {

/*!
  @class EMA
  @brief Exponential Moving Average
 */
class EMA {
public:
    explicit EMA(float factor) : _alpha(factor)
    {
    }

    inline void clear()
    {
        _ema_value = std::numeric_limits<float>::quiet_NaN();
    }

    inline float update(float new_value)
    {
        if (!std::isnan(_ema_value)) {
            _ema_value = _alpha * new_value + (1.0f - _alpha) * _ema_value;
        } else {
            _ema_value = new_value;
        }
        return _ema_value;
    }

private:
    float _alpha{}, _ema_value{std::numeric_limits<float>::quiet_NaN()};
};

/*!
  @class Filter
  @brief Apply a high-pass filter and invert polarity
 */
class Filter {
public:
    Filter(const float cutoff, const float sampling_rate)
    {
        setSamplingRate(cutoff, sampling_rate);
    }

    void setSamplingRate(const float cutoff, const float sampling_rate)
    {
        _cutoff       = cutoff;
        _samplingRate = sampling_rate;
        _prevIn = _prevOut = 0.0f;
        auto dt            = 1.0f / _samplingRate;
        auto RC            = 1.0f / (2.0f * M_PI * _cutoff);
        _alpha             = RC / (RC + dt);
        _ema.clear();
    }

    float process(const float value)
    {
        float out = _ema.update(_alpha * (_prevOut + value - _prevIn));
        _prevIn   = value;
        _prevOut  = out;
        return -out;
    }

private:
    EMA _ema{0.95f};
    float _cutoff{}, _samplingRate{};
    float _prevIn{}, _prevOut{};
    float _alpha{};
};

/*!
  @class PulseMonitor
  @brief Calculate BPM and SpO2, and detect the pulse beat
 */
class PulseMonitor {
public:
    /*!
      @brief Costructor
      @param samplingRate sampling rate
      @param sec Seconds of data to be stored
     */
    explicit PulseMonitor(const uint32_t samplingRate = 100, const uint32_t sec = 5)
        : _range{sec},
          _sampling_rate{(float)samplingRate},
          _max_samples{(size_t)samplingRate * sec},
          _filterIR(5.0f, samplingRate)
    {
        assert(sec >= 1 && "sec must be greater or eaual than 1");
        assert(samplingRate >= 1.0f && "SamplingRate must be greater or equal than 1.0f");
    }

    //! @brief Detect beat?
    bool isBeat() const
    {
        return _beat;
    }
    //! @brief Gets the BPM
    float bpm() const
    {
        return _bpm;
    }
    /*!
      @brief Gets the SpO2
      @warning IR and RED must be pushed back
    */
    float SpO2() const
    {
        return _SpO2;
    }

    /*!
      @brief Set the sampling rate
      @param samplingRate sampling rate
      @note clear stored data
     */
    void setSamplingRate(const float samplingRate);

    /*!
      @brief Push back IR
      @param ir IR data
     */
    void push_back(const float ir);
    /*!
      @brief Push back IR and RED
      @param ir IR data
      @param red RED data
      @note Calclate SpO2
     */
    void push_back(const float ir, const float red);

    /*!
      @brief Update status
      @note Calclate BPM
     */
    void update();

    //! @brief Clear inner data
    void clear();

    // Filterd ir value
    float latestIR() const
    {
        return !_dataIR.empty() ? _dataIR.back() : std::numeric_limits<float>::quiet_NaN();
    }

protected:
    float calculate_bpm();

private:
    uint32_t _range{};  // Sec.
    float _sampling_rate{};
    size_t _max_samples{};

    Filter _filterIR{0.5f, 100.f};
    std::deque<float> _dataIR;

    bool _beat{};
    float _bpm{};
    float _SpO2{};

    uint32_t _count{};
    float _avered{}, _aveir{};
    float _sumredrms{}, _sumirrms{};
};

}  // namespace heart
}  // namespace m5
#endif
