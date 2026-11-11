/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file gh3038_auto_adjust.hpp
  @brief Decision of the automatic LED current / gain adjustment of UnitGH3038
  @note Independent of the chip access
*/
#ifndef M5_UNIT_HEART_UNIT_GH3038_AUTO_ADJUST_HPP
#define M5_UNIT_HEART_UNIT_GH3038_AUTO_ADJUST_HPP

#include <cstdint>

namespace m5 {
namespace unit {
namespace gh3038 {

constexpr float ADJUST_HIGH_RATIO{0.80f};    //!< @brief Lower when the maximum exceeds this ratio of the full scale
constexpr float ADJUST_LOW_RATIO{0.25f};     //!< @brief Raise when the mean falls below this ratio of the full scale
constexpr float ADJUST_TARGET_RATIO{0.50f};  //!< @brief Target ratio of the mean after the adjustment
constexpr uint8_t ADJUST_MIN_CODE{8};        //!< @brief Lowest LED current code of the adjustment

/*!
  @struct AdjustInput
  @brief Input of decide_adjustment()
 */
struct AdjustInput {
    float mean{};            //!< Mean raw value of the window
    float max{};             //!< Maximum raw value of the window
    float full_scale{};      //!< Full scale of the raw value
    uint8_t code{};          //!< LED current code now
    uint8_t gain{};          //!< Gain level now
    uint8_t max_code{};      //!< Upper limit of the code (the value at begin)
    uint8_t max_gain{};      //!< Upper limit of the gain level (the value at begin)
    float raw_ratio_down{};  //!< Raw value ratio when the gain is lowered by one level (0 if not possible)
    float raw_ratio_up{};    //!< Raw value ratio when the gain is raised by one level (0 if not possible)
};

/*!
  @struct AdjustOutput
  @brief Output of decide_adjustment()
 */
struct AdjustOutput {
    uint8_t code{};  //!< New LED current code
    uint8_t gain{};  //!< New gain level
    //! @brief Is anything changed from the input?
    inline bool changed(const AdjustInput& in) const
    {
        return code != in.code || gain != in.gain;
    }
};

/*!
  @brief Decide the LED current code and gain level from the statistics of a window
  @param in Statistics and the current settings
  @return New settings (the same as the input if nothing to change)
  @note Nothing changes while the maximum and the mean are in range.
  The code and the gain never exceed max_code and max_gain
 */
AdjustOutput decide_adjustment(const AdjustInput& in);

}  // namespace gh3038
}  // namespace unit
}  // namespace m5
#endif
