/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file spo2_estimator.cpp
  @brief SpO2 estimation from beat-synchronous ratio of ratios
*/
#include "spo2_estimator.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr size_t MIN_BEATS{3};            // Beats required for a valid value
constexpr float RATIO_MIN{0.3f};          // Plausible range of R
constexpr float RATIO_MAX{1.2f};          //
constexpr float RATIO_SPREAD_MAX{0.15f};  // Allowed max - min of the recent R
constexpr float SPO2_OFFSET{110.0f};      // Provisional: SpO2 = 110 - 25R
constexpr float SPO2_SLOPE{25.0f};        //

bool positive_finite(const float v)
{
    return std::isfinite(v) && v > 0.0f;
}
}  // namespace

namespace m5 {
namespace heart {

float ratio_to_spo2(const float r)
{
    if (std::isnan(r)) {
        return std::numeric_limits<float>::quiet_NaN();
    }
    return std::min(100.0f, std::max(0.0f, SPO2_OFFSET - SPO2_SLOPE * r));
}

bool SpO2Estimator::addBeat(const float red_ac, const float red_dc, const float ir_ac, const float ir_dc)
{
    if (!positive_finite(red_ac) || !positive_finite(red_dc) || !positive_finite(ir_ac) || !positive_finite(ir_dc)) {
        return false;
    }
    const float r = (red_ac / red_dc) / (ir_ac / ir_dc);
    if (!std::isfinite(r)) {
        return false;
    }
    _ratios.push(r);
    return true;
}

bool SpO2Estimator::isValid() const
{
    if (_ratios.size() < MIN_BEATS) {
        return false;
    }
    const float r = ratio();
    return r >= RATIO_MIN && r <= RATIO_MAX && ratioSpread() <= RATIO_SPREAD_MAX;
}

float SpO2Estimator::spo2() const
{
    return isValid() ? ratio_to_spo2(ratio()) : std::numeric_limits<float>::quiet_NaN();
}

}  // namespace heart
}  // namespace m5
