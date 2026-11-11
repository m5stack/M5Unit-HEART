/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Graphical meter example for UnitHeart / HatHeart / StampHeart
*/
// *************************************************************
// Choose one define symbol to match the unit you are using
// *************************************************************
#if !defined(USING_UNIT_HEART) && !defined(USING_HAT_HEART) && !defined(USING_STAMP_HEART)
// For UnitHeart (U029)
// #define USING_UNIT_HEART
// For HatHeart (U118)
// #define USING_HAT_HEART
// For StampHeart
// #define USING_STAMP_HEART
#endif
#include "main/GraphicalMeter.cpp"
