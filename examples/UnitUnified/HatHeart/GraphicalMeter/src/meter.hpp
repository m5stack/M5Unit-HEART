/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef M5_UNIT_HEART_EXAMPLE_METER_HPP
#define M5_UNIT_HEART_EXAMPLE_METER_HPP

#include <M5GFX.h>
#include "ui_plotter.hpp"
#include <m5_utility/log/library_log.hpp>

class Meter {
public:
    Meter(const uint32_t left, const uint32_t top, const uint32_t wid, const uint32_t hgt, m5gfx::rgb565_t tcolor)
        : _left(left), _top(top), _theme_color(tcolor)
    {
        _plotter = new m5::ui::Plotter(nullptr, wid, wid, hgt);
        _plotter->setGaugeTextDatum(textdatum_t::top_right);
        _plotter->setLineColor(_theme_color);
    }

    inline void push_back(const float value)
    {
        _plotter->push_back(value);
    }

    inline void push(LovyanGFX* target, const uint32_t x, const uint32_t y)
    {
        _plotter->push(target, _left, _top);
    }

    inline void clear()
    {
        _plotter->clear();
    }

private:
    uint32_t _left{}, _top{};
    m5::ui::Plotter* _plotter{};
    m5gfx::rgb565_t _theme_color{};
};
#endif
