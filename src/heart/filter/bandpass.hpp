/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file bandpass.hpp
  @brief Second-order Butterworth filters for PPG
*/
#ifndef M5_UNIT_HEART_HEART_FILTER_BANDPASS_HPP
#define M5_UNIT_HEART_HEART_FILTER_BANDPASS_HPP

#include <cmath>
#include <cstdint>

namespace m5 {
namespace heart {

/*!
  @class Biquad
  @brief Second-order Butterworth low-pass or high-pass filter (bilinear transform)
  @details Direct form I. The numerator is computed from sample differences (high-pass) or sums (low-pass)
  so that a large DC level does not lose precision in float.
 */
class Biquad {
public:
    //! @brief Filter type
    enum class Type : uint8_t {
        PassThrough,  //!< Output equals input
        LowPass,      //!< Low-pass
        HighPass,     //!< High-pass
    };

    //! @brief Pass-through filter
    Biquad() = default;

    /*!
      @brief Constructor
      @param type Filter type
      @param cutoff_hz Cutoff frequency (Hz)
      @param sampling_rate_hz Sampling rate (Hz)
      @note It becomes a pass-through filter if the cutoff is not below the Nyquist frequency or the rate is not
      positive
     */
    Biquad(const Type type, const float cutoff_hz, const float sampling_rate_hz)
    {
        if (type == Type::PassThrough || !(sampling_rate_hz > 0.0f) || !(cutoff_hz > 0.0f) ||
            cutoff_hz >= sampling_rate_hz * 0.5f) {
            return;
        }
        const double pi{3.14159265358979323846};
        const double w0{2.0 * pi * cutoff_hz / sampling_rate_hz};
        const double cs{std::cos(w0)};
        const double alpha{std::sin(w0) / std::sqrt(2.0)};  // Q = 1/sqrt(2)
        const double a0{1.0 + alpha};
        _type = type;
        _b0   = static_cast<float>(((type == Type::LowPass) ? (1.0 - cs) : (1.0 + cs)) * 0.5 / a0);
        _a1   = static_cast<float>(-2.0 * cs / a0);
        _a2   = static_cast<float>((1.0 - alpha) / a0);
    }

    //! @brief Filter type
    inline Type type() const
    {
        return _type;
    }

    /*!
      @brief Set the state as if the input had been constant
      @param x Constant input
     */
    void reset(const float x)
    {
        _x1 = _x2 = x;
        _y1 = _y2 = (_type == Type::HighPass) ? 0.0f : x;
    }

    /*!
      @brief Process a sample
      @param x Input
      @return Output
     */
    float process(const float x)
    {
        float v{};
        switch (_type) {
            case Type::LowPass:
                v = _b0 * ((x + _x1) + (_x1 + _x2));
                break;
            case Type::HighPass:
                v = _b0 * ((x - _x1) - (_x1 - _x2));
                break;
            default:
                return x;
        }
        const float y = v - _a1 * _y1 - _a2 * _y2;
        _x2           = _x1;
        _x1           = x;
        _y2           = _y1;
        _y1           = y;
        return y;
    }

private:
    Type _type{Type::PassThrough};
    float _b0{}, _a1{}, _a2{};
    float _x1{}, _x2{}, _y1{}, _y2{};
};

/*!
  @class BandPass
  @brief Band-pass filter (second-order Butterworth high-pass and low-pass in series)
  @details The first processed sample primes the state, so a large DC level does not cause a start-up step.
 */
class BandPass {
public:
    //! @brief Pass-through filter
    BandPass() = default;

    /*!
      @brief Constructor
      @param low_hz Lower cutoff (Hz)
      @param high_hz Upper cutoff (Hz)
      @param sampling_rate_hz Sampling rate (Hz)
     */
    BandPass(const float low_hz, const float high_hz, const float sampling_rate_hz)
        : _hp(Biquad::Type::HighPass, low_hz, sampling_rate_hz), _lp(Biquad::Type::LowPass, high_hz, sampling_rate_hz)
    {
    }

    //! @brief The next process() primes the state again
    inline void reset()
    {
        _primed = false;
    }

    /*!
      @brief Process a sample
      @param x Input
      @return Output
     */
    float process(const float x)
    {
        if (!_primed) {
            _primed = true;
            _hp.reset(x);
            _lp.reset(_hp.type() == Biquad::Type::HighPass ? 0.0f : x);
        }
        return _lp.process(_hp.process(x));
    }

private:
    Biquad _hp{}, _lp{};
    bool _primed{};
};

}  // namespace heart
}  // namespace m5
#endif
