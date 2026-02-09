/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef M5_UNIT_HEART_EXAMPLE_VIEW_HPP
#define M5_UNIT_HEART_EXAMPLE_VIEW_HPP

#include "meter.hpp"

struct View {
    View(const uint32_t wid, const uint32_t hgt, bool unit) : _type{unit}
    {
        _sprite.createSprite(wid, hgt);
        _sprite.setFont(&fonts::FreeSansBold9pt7b);
        _meter = new Meter(0, hgt * 2 / 3, wid, hgt / 3, unit ? TFT_BLUE : TFT_YELLOW);
    }

    inline void push_back(const float ir, const float red)
    {
        _monitor.push_back(ir, red);
    }

    void update()
    {
        if (_beat) {
            --_beat;
        }
        _meter->push_back(_monitor.latestIR());
        _monitor.update();
        _beat += _monitor.isBeat() * 8;
    }

    void render()
    {
        _sprite.clear();
        _sprite.drawString(_type ? "Unit" : "Hat", 0, 0);
        _sprite.setCursor(0, 24);
        _sprite.printf("BPM: %3.2f\nSpO2:%3.2f", _monitor.bpm(), _monitor.SpO2());
        _sprite.fillCircle(_sprite.width() - 12, 24 * 3, 7, _beat ? TFT_RED : TFT_WHITE);

        _meter->push(&_sprite, 0, _sprite.height() >> 1);
    }

    void clear()
    {
        _monitor.clear();
        _meter->clear();
    }

    void push(LovyanGFX* target, const uint32_t x, const uint32_t y)
    {
        _sprite.pushSprite(target, x, y);
    }

    LGFX_Sprite _sprite{};
    m5::heart::PulseMonitor _monitor{100, 2};
    Meter* _meter{};
    uint32_t _left{}, _top{}, _beat{};
    bool _type{};
};
#endif
