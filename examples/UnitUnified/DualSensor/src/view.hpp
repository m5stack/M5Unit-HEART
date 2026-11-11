/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef M5_UNIT_HEART_EXAMPLE_VIEW_HPP
#define M5_UNIT_HEART_EXAMPLE_VIEW_HPP

#include <cmath>
#include <limits>
#include "meter.hpp"

struct View {
    View(const uint32_t wid, const uint32_t hgt, bool unit) : _type{unit}
    {
        _theme_color = unit ? TFT_BLUE : TFT_YELLOW;
        _sprite.setPsram(false);
        _sprite.setColorDepth(2);
        _sprite.createSprite(wid, hgt);
        _sprite.setPaletteColor(palette_black, TFT_BLACK);
        _sprite.setPaletteColor(palette_white, TFT_WHITE);
        _sprite.setPaletteColor(palette_theme, _theme_color);
        _sprite.setPaletteColor(palette_gauge, TFT_DARKGRAY);
        _sprite.setTextColor(palette_white, palette_black);
        _sprite.setFont(&fonts::FreeSansBold9pt7b);
        _meter = new Meter(0, hgt * 2 / 3, wid, hgt / 3, palette_theme, palette_gauge, palette_black);
    }

    // Plot one sample of the waveform
    inline void push_back(const float value)
    {
        _meter->push_back(value);
    }

    // Update the values after processing (NaN if not valid)
    void update(const float bpm, const float spo2, const bool beat)
    {
        _bpm  = bpm;
        _spo2 = spo2;
        if (beat) {
            _beat = beat_frames;
        } else if (_beat) {
            --_beat;
        }
    }

    void render()
    {
        _sprite.clear(palette_black);
        _sprite.drawString(_type ? "Unit" : "Hat", 0, 0);
        _sprite.setCursor(0, 24);
        if (std::isnan(_bpm)) {
            _sprite.printf("BPM:  --.--\n");
        } else {
            _sprite.printf("BPM: %3.2f\n", _bpm);
        }
        if (std::isnan(_spo2)) {
            _sprite.printf("SpO2: --.--");
        } else {
            _sprite.printf("SpO2:%3.2f", _spo2);
        }
        _sprite.fillCircle(_sprite.width() - 12, 24 * 3, 7, _beat ? palette_theme : palette_white);

        _meter->push(&_sprite);
    }

    void clear()
    {
        _bpm  = std::numeric_limits<float>::quiet_NaN();
        _spo2 = std::numeric_limits<float>::quiet_NaN();
        _beat = 0;
        _meter->clear();
    }

    void push(LovyanGFX* target, const uint32_t x, const uint32_t y)
    {
        _sprite.pushSprite(target, x, y);
    }

    enum : uint8_t {
        palette_black = 0,
        palette_white = 1,
        palette_theme = 2,
        palette_gauge = 3,
    };
    static constexpr uint32_t beat_frames{4};  // Number of updates the beat mark stays lit
    LGFX_Sprite _sprite{};
    Meter* _meter{};
    float _bpm{std::numeric_limits<float>::quiet_NaN()};
    float _spo2{std::numeric_limits<float>::quiet_NaN()};
    uint32_t _left{}, _top{}, _beat{};
    bool _type{};
    m5gfx::rgb565_t _theme_color{};
};
#endif
