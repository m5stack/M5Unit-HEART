/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file gh3038_auto_adjust.cpp
  @brief Decision of the automatic LED current / gain adjustment of UnitGH3038
*/
#include "gh3038_auto_adjust.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
uint8_t to_code(const float v, const uint8_t lo, const uint8_t hi)
{
    if (!(v > static_cast<float>(lo))) {  // Also NaN
        return lo;
    }
    if (v >= static_cast<float>(hi)) {
        return hi;
    }
    return static_cast<uint8_t>(std::lround(v));
}
}  // namespace

namespace m5 {
namespace unit {
namespace gh3038 {

AdjustOutput decide_adjustment(const AdjustInput& in)
{
    AdjustOutput out{};
    out.code = in.code;
    out.gain = in.gain;
    if (!(in.full_scale > 0.0f)) {
        return out;
    }
    const float high   = in.full_scale * ADJUST_HIGH_RATIO;
    const float low    = in.full_scale * ADJUST_LOW_RATIO;
    const float target = in.full_scale * ADJUST_TARGET_RATIO;
    const uint8_t lo   = std::min(ADJUST_MIN_CODE, in.max_code);

    float desired{};
    if (in.max > high) {
        const float level = (in.mean > 0.0f) ? in.mean : in.max;
        desired           = static_cast<float>(in.code) * target / level;
        if (desired < static_cast<float>(lo) && in.gain > 0 && in.raw_ratio_down > 0.0f) {
            --out.gain;
            desired /= in.raw_ratio_down;
        }
    } else if (in.mean < low) {
        // No finger gives a mean near 0: return to the limits
        desired =
            (in.mean > 0.0f) ? static_cast<float>(in.code) * target / in.mean : std::numeric_limits<float>::infinity();
        if (desired > static_cast<float>(in.max_code) && in.gain < in.max_gain && in.raw_ratio_up > 0.0f) {
            ++out.gain;
            desired /= in.raw_ratio_up;
        }
    } else {
        return out;
    }
    out.code = to_code(desired, lo, in.max_code);
    return out;
}

}  // namespace gh3038
}  // namespace unit
}  // namespace m5
