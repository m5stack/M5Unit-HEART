/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file M5UnitUnifiedHEART.hpp
  @brief Main header of M5UnitUnifiedHEART

  @mainpage M5Unit-HEART
  Library for UnitHEART using M5UnitUnified.
*/
#ifndef M5_UNIT_HEART_HPP
#define M5_UNIT_HEART_HPP

#include "unit/unit_MAX30100.hpp"
#include "utility/heart_rate.hpp"

/*!
  @namespace m5
  @brief Top level namespace of M5stack
 */
namespace m5 {

/*!
  @namespace unit
  @brief Unit-related namespace
 */
namespace unit {

using UnitHEART = m5::unit::UnitMAX30100;

}  // namespace unit
}  // namespace m5

#endif
