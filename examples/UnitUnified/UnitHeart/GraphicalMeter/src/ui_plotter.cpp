/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file ui_plotter.cpp
  @brief Plotter
 */
#include "ui_plotter.hpp"
#include <M5Unified.h>
#include <algorithm>

namespace m5 {
namespace ui {

Plotter::Plotter(LovyanGFX* parent, const size_t maxPlot, const int32_t wid, const int32_t hgt,
                 const int32_t coefficient)
    : _parent(parent), _wid{wid}, _hgt{hgt}, _coefficient(coefficient), _data(maxPlot), _autoScale{true}
{
}

Plotter::Plotter(LovyanGFX* parent, const size_t maxPlot, const int32_t minimum, const int32_t maximum,
                 const int32_t wid, const int32_t hgt, const int32_t coefficient)
    : _parent(parent),
      _min{minimum},
      _max{maximum},
      _wid{wid},
      _hgt{hgt},
      _coefficient(coefficient),
      _data(maxPlot),
      _autoScale{false}
{
}

void Plotter::push_back(const float val)
{
    push_back((int32_t)(val * _coefficient));
}

void Plotter::push_back(const int32_t val)
{
    auto v = _autoScale ? val : std::min(std::max(val, _min), _max);
    _data.push_back(v);

    if (_autoScale && _data.size() >= 2) {
        auto it = std::minmax_element(_data.cbegin(), _data.cend());
        _min    = *(it.first);
        _max    = *(it.second);
        if (_min == _max) {
            ++_max;
        }
    }
}

void Plotter::push(LovyanGFX* dst, const int32_t x, const int32_t y)
{
    dst->setClipRect(x, y, width(), height());

    // gauge
    dst->drawFastHLine(x, y, _wid, _gaugeClr);
    dst->drawFastHLine(x, y + (_hgt >> 1), _wid, _gaugeClr);
    dst->drawFastHLine(x, y + (_hgt >> 2), _wid, _gaugeClr);
    dst->drawFastHLine(x, y + (_hgt >> 2) * 3, _wid, _gaugeClr);
    dst->drawFastHLine(x, y + _hgt - 1, _wid, _gaugeClr);

    if (_data.size() >= 2) {
        auto it    = _data.cbegin();
        auto itend = --_data.cend();
        auto sz    = _data.size();
        const float range{_max > _min ? (float)_max - _min : 1.0f};
        const int32_t hh{_hgt - 1};
        int32_t left{x};

        // plot latest
        if (sz > _wid) {
            auto cnt{sz - _wid};
            while (cnt--) {
                ++it;  // Bidirectional iterator, so only ++/-- is available.
            }
        }
        if (sz < _wid) {
            left += _wid - sz;
        }

        while (it != itend && left < _wid) {
            int32_t s{*it}, e{*(++it)};
            dst->drawLine(left, y + hh - hh * (s - _min) / range, left + 1, y + hh - hh * (e - _min) / range, _lineClr);
            ++left;
        }
    }
    dst->clearClipRect();
}

}  // namespace ui
}  // namespace m5
