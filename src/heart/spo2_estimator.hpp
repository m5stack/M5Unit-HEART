/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file spo2_estimator.hpp
  @brief SpO2 estimation from beat-synchronous ratio of ratios
*/
#ifndef M5_UNIT_HEART_HEART_SPO2_ESTIMATOR_HPP
#define M5_UNIT_HEART_HEART_SPO2_ESTIMATOR_HPP

#include <cstdint>

#include "recent_values.hpp"

namespace m5 {
namespace heart {

/*!
  @brief Provisional conversion from the ratio of ratios to SpO2 (%)
  @param r Ratio of ratios
  @return 110 - 25R clamped to 0...100 (NaN if r is NaN)
  @warning Not calibrated for any sensor
 */
float ratio_to_spo2(const float r);

/*!
  @brief Conversion from the ratio of ratios to SpO2 (%)
  @details Return NaN for an R that is out of the calibrated range. The result is clamped to 0...100
 */
using SpO2Conversion = float (*)(const float r);

//! @brief Default of the allowed spread (max - min) of the recent ratios of ratios
constexpr float SPO2_RATIO_SPREAD_DEFAULT{0.25f};

/*!
  @class SpO2Estimator
  @brief Keeps the ratio of ratios (R) of recent beats and converts the median to SpO2
 */
class SpO2Estimator {
public:
    SpO2Estimator() = default;
    /*!
      @brief Constructor
      @param ratio_spread_max Allowed spread (max - min) of the recent R for a valid value
     */
    explicit SpO2Estimator(const float ratio_spread_max) : _ratio_spread_max{ratio_spread_max}
    {
    }

    //! @brief Remove all beats
    inline void clear()
    {
        _ratios.clear();
    }
    /*!
      @brief Add a beat
      @param red_ac Peak-to-peak of the red pulsatile component in the beat
      @param red_dc Mean of the red raw value in the beat
      @param ir_ac Peak-to-peak of the IR pulsatile component in the beat
      @param ir_dc Mean of the IR raw value in the beat
      @return True if accepted (all values must be positive and finite)
     */
    bool addBeat(const float red_ac, const float red_dc, const float ir_ac, const float ir_dc);

    //! @brief Number of beats kept
    inline size_t count() const
    {
        return _ratios.size();
    }
    //! @brief Median of the recent R (NaN if none)
    inline float ratio() const
    {
        return _ratios.median();
    }
    //! @brief Maximum minus minimum of the recent R (NaN if none)
    inline float ratioSpread() const
    {
        return _ratios.spread();
    }
    //! @brief Allowed spread (max - min) of the recent R for a valid value
    inline float ratioSpreadMax() const
    {
        return _ratio_spread_max;
    }
    //! @brief Are the recent R consistent enough?
    bool isValid() const;
    //! @brief SpO2 (%), NaN if not valid
    float spo2() const;

private:
    RecentValues<5> _ratios{};
    float _ratio_spread_max{SPO2_RATIO_SPREAD_DEFAULT};
};

}  // namespace heart
}  // namespace m5
#endif
