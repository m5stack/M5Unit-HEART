/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file gh3038_types.hpp
  @brief Types for GH3038
*/
#ifndef M5_UNIT_HEART_GH3038_TYPES_HPP
#define M5_UNIT_HEART_GH3038_TYPES_HPP

#include <cstdint>
#include <limits>

#include "gh3038_core.h"

namespace m5 {
namespace unit {
/*!
  @namespace gh3038
  @brief For GH3038
 */
namespace gh3038 {

/*!
  @enum Mode
  @brief Measurement mode (LEDs driven in the time slots)
 */
enum class Mode : uint8_t {
    HR,    //!< Green (1 slot)
    SpO2,  //!< Red and IR (2 slots)
    All,   //!< Green, Red and IR (3 slots)
};

/*!
  @enum Gain
  @brief TIA gain level (a higher level gives a larger value for the same light)
 */
enum class Gain : uint8_t {
    Level0,  //!< Lowest
    Level1,
    Level2,
    Level3,
    Level4,
    Level5,
    Level6,  //!< Highest
};

/*!
  @enum SamplingRate
  @brief Sampling rate of each slot
 */
enum class SamplingRate : uint8_t {
    Rate25,   //!< 25 Hz
    Rate50,   //!< 50 Hz
    Rate100,  //!< 100 Hz
    Rate200,  //!< 200 Hz
};

/*!
  @enum Color
  @brief LED color
 */
enum class Color : uint8_t {
    Green,  //!< Green
    Red,    //!< Red
    IR,     //!< Infrared
};

constexpr uint8_t NUMBER_OF_COLORS{3};  //!< @brief Number of LED colors

//! @brief Array index of the color
inline uint8_t color_index(const Color c)
{
    return static_cast<uint8_t>(c);
}

/*!
  @brief Sampling rate (Hz)
  @note Out-of-range values are clamped to the last entry
 */
inline uint32_t sampling_rate_to_hz(const SamplingRate r)
{
    static constexpr uint32_t table[] = {25, 50, 100, 200};
    static constexpr uint8_t count{sizeof(table) / sizeof(table[0])};
    const uint8_t idx{static_cast<uint8_t>(r)};
    return table[idx < count ? idx : count - 1];
}

/*!
  @struct Data
  @brief Measurement data group (one sample of each driven LED)
 */
struct Data {
    int32_t raw[NUMBER_OF_COLORS]{};          //!< Signed ADC value [Green, Red, IR]
    Gain gain[NUMBER_OF_COLORS]{};            //!< TIA gain level at the measurement
    uint8_t led_current[NUMBER_OF_COLORS]{};  //!< LED current code at the measurement
    uint8_t valid{};                          //!< Valid bits (1 << color_index)

    //! @brief Has the value of the color?
    inline bool has(const Color c) const
    {
        return valid & (1U << color_index(c));
    }
    //! @brief Raw value of the color (0 if not measured)
    inline int32_t value(const Color c) const
    {
        return raw[color_index(c)];
    }
    //! @brief Photodiode current of the color (pA), NaN if not measured
    inline float current(const Color c) const
    {
        return has(c) ? m5_gh3038_raw_to_pA(value(c), static_cast<uint8_t>(gain[color_index(c)]))
                      : std::numeric_limits<float>::quiet_NaN();
    }

    ///@name Accessors
    ///@{
    inline int32_t green() const
    {
        return value(Color::Green);
    }
    inline int32_t red() const
    {
        return value(Color::Red);
    }
    inline int32_t ir() const
    {
        return value(Color::IR);
    }
    inline bool hasGreen() const
    {
        return has(Color::Green);
    }
    inline bool hasRed() const
    {
        return has(Color::Red);
    }
    inline bool hasIR() const
    {
        return has(Color::IR);
    }
    inline float greenCurrent() const
    {
        return current(Color::Green);
    }
    inline float redCurrent() const
    {
        return current(Color::Red);
    }
    inline float irCurrent() const
    {
        return current(Color::IR);
    }
    ///@}
};

}  // namespace gh3038
}  // namespace unit
}  // namespace m5
#endif
