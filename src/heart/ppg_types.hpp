/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file ppg_types.hpp
  @brief Common types for PPG (photoplethysmogram) processing
*/
#ifndef M5_UNIT_HEART_HEART_PPG_TYPES_HPP
#define M5_UNIT_HEART_HEART_PPG_TYPES_HPP

#include <cstdint>

namespace m5 {
/*!
  @namespace heart
  @brief Chip-independent heart rate / SpO2 processing
 */
namespace heart {

/*!
  @enum Channel
  @brief LED color of a PPG channel
  @note The value is also the index of PPGSample::value
 */
enum class Channel : uint8_t {
    Green,  //!< Green
    Red,    //!< Red
    IR,     //!< Infrared
};

constexpr uint8_t NUMBER_OF_CHANNELS{3};  //!< @brief Number of channels
constexpr uint8_t INVALID_CHANNEL{0xFF};  //!< @brief No channel

//! @brief Index of the channel
constexpr uint8_t channel_index(const Channel ch)
{
    return static_cast<uint8_t>(ch);
}

//! @brief Bit of the channel in a channel mask
constexpr uint8_t channel_bit(const Channel ch)
{
    return static_cast<uint8_t>(1U << static_cast<uint8_t>(ch));
}

/*!
  @enum ValueUnit
  @brief Unit of PPGSample::value
  @note The processing does not depend on the unit (ratios and relative values only)
 */
enum class ValueUnit : uint8_t {
    Count,       //!< ADC count
    PicoAmpere,  //!< Photodiode current (pA)
};

/*!
  @struct PPGSample
  @brief One sample of each channel
 */
struct PPGSample {
    float value[NUMBER_OF_CHANNELS]{};  //!< Value of each channel (index: Channel)
    uint8_t valid{};                    //!< Channels holding a value (channel_bit)

    //! @brief Has the value of the channel?
    inline bool has(const Channel ch) const
    {
        return (valid & channel_bit(ch)) != 0;
    }
    //! @brief Value of the channel (not checked by valid)
    inline float get(const Channel ch) const
    {
        return value[channel_index(ch)];
    }
    //! @brief Set the value of the channel and mark it as valid
    inline void set(const Channel ch, const float v)
    {
        value[channel_index(ch)] = v;
        valid |= channel_bit(ch);
    }
};

/*!
  @struct PPGSpec
  @brief What a PPG data source provides
 */
struct PPGSpec {
    uint8_t channels{};                //!< Available channels (channel_bit)
    float sampling_rate_hz{};          //!< Sampling rate (Hz), 0 if not measuring
    ValueUnit unit{ValueUnit::Count};  //!< Unit of the values
};

}  // namespace heart
}  // namespace m5
#endif
