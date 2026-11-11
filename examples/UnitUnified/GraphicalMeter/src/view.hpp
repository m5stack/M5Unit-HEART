/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef M5_UNIT_HEART_EXAMPLE_VIEW_HPP
#define M5_UNIT_HEART_EXAMPLE_VIEW_HPP

#include <cmath>
#include <cstdio>
#include <limits>
#include <m5_utility/compatibility_feature.hpp>
#include "meter.hpp"

struct View {
    View(const uint32_t wid, const uint32_t hgt, const char* title, const m5gfx::rgb565_t theme)
        : _title{title}, _theme_color{theme}
    {
        _screen_w            = wid;
        _plot_top            = hgt * 2 / 3;
        _plot_hgt            = hgt / 3;
        const uint32_t spr_w = wid;
        const uint32_t spr_h = _plot_hgt;
        _plot_sprite.setPsram(false);
        _plot_sprite.setColorDepth(4);  // Room for the color of the second waveform
        _plot_sprite.createSprite(spr_w, spr_h);
        _plot_sprite.setPaletteColor(palette_black, TFT_BLACK);
        _plot_sprite.setPaletteColor(palette_white, TFT_WHITE);
        _plot_sprite.setPaletteColor(palette_theme, _theme_color);
        _plot_sprite.setPaletteColor(palette_gauge, TFT_DARKGRAY);
        _plot_sprite.setPaletteColor(palette_second, TFT_RED);
        _plot_sprite.setTextColor(palette_white, palette_black);
        _plot_sprite.setFont(&fonts::FreeSansBold9pt7b);
        _text_sprite.setPsram(false);
        _text_sprite.setColorDepth(2);
        _text_sprite.setPaletteColor(palette_black, TFT_BLACK);
        _text_sprite.setPaletteColor(palette_white, TFT_WHITE);
        _text_sprite.setPaletteColor(palette_theme, _theme_color);
        _text_sprite.setPaletteColor(palette_gauge, TFT_DARKGRAY);
        _text_sprite.setTextColor(palette_white, palette_black);
        _text_sprite.setFont(&fonts::FreeSansBold9pt7b);
        const int32_t line_h  = _text_sprite.fontHeight();
        const int32_t title_w = _text_sprite.textWidth(_title);
        const int32_t bpm_w   = _text_sprite.textWidth("BPM: 000.00");
        const int32_t spo2_w  = _text_sprite.textWidth("SpO2:100.00");
        _text_w               = std::max(title_w, std::max(bpm_w, spo2_w));
        _text_h               = line_h * 4;
        _text_sprite.createSprite(_text_w, _text_h);
        _text_sprite.setPaletteColor(palette_black, TFT_BLACK);
        _text_sprite.setPaletteColor(palette_white, TFT_WHITE);
        _text_sprite.setPaletteColor(palette_theme, _theme_color);
        _text_sprite.setPaletteColor(palette_gauge, TFT_DARKGRAY);
        _text_sprite.setTextColor(palette_white, palette_black);
        _text_sprite.setFont(&fonts::FreeSansBold9pt7b);
        _beat_size = 16;
        _beat_x    = _screen_w - _beat_size - 2;
        _beat_y    = (line_h * 2) + (line_h / 2) - (_beat_size / 2);
        _beat_sprite.setPsram(false);
        _beat_sprite.setColorDepth(2);
        _beat_sprite.createSprite(_beat_size, _beat_size);
        _beat_sprite.setPaletteColor(palette_black, TFT_BLACK);
        _beat_sprite.setPaletteColor(palette_white, TFT_WHITE);
        _beat_sprite.setPaletteColor(palette_theme, _theme_color);
        _beat_sprite.setPaletteColor(palette_gauge, TFT_DARKGRAY);
        _meter = new Meter(0, 0, spr_w, spr_h, palette_theme, palette_gauge, palette_black);
    }

    // Show a second waveform in red under the main one
    inline void enableSecondLine()
    {
        _meter->enableSecondLine(palette_second);
    }

    // Plot one sample of the waveform
    inline void push_back(const float value)
    {
        _meter->push_back(value);
    }
    // Plot one sample of the waveform and of the second one
    inline void push_back(const float value, const float second)
    {
        _meter->push_back(value, second);
    }

    // Update the values after processing (NaN if not valid)
    void update(const float bpm, const float spo2, const float pi, const bool beat)
    {
        _bpm = bpm;
        _pi  = pi;
        // The last valid SpO2 is kept for spo2_hold_ms while the heart rate is valid (it drops out for a few beats)
        if (!std::isnan(spo2)) {
            _spo2    = spo2;
            _spo2_at = m5::utility::millis();
        } else if (std::isnan(bpm) || m5::utility::hasElapsed(_spo2_at, spo2_hold_ms)) {
            _spo2 = std::numeric_limits<float>::quiet_NaN();
        }
        if (beat) {
            _beat = beat_frames;
        } else if (_beat) {
            --_beat;
        }
    }

    void render()
    {
        _plot_sprite.clear(palette_black);
        _meter->push(&_plot_sprite);
        _text_sprite.clear(palette_black);
        // One line per fontHeight (the line feed of printf advances more and the last line would be cut)
        const int32_t line_h = _text_sprite.fontHeight();
        char buf[24]{};
        _text_sprite.drawString(_title, 0, 0);
        if (std::isnan(_bpm)) {
            snprintf(buf, sizeof(buf), "BPM:  --");
        } else {
            snprintf(buf, sizeof(buf), "BPM: %3.0f", _bpm);
        }
        _text_sprite.drawString(buf, 0, line_h);
        if (std::isnan(_spo2)) {
            snprintf(buf, sizeof(buf), "SpO2: --");
        } else {
            snprintf(buf, sizeof(buf), "SpO2:%3.0f", _spo2);
        }
        _text_sprite.drawString(buf, 0, line_h * 2);
        if (std::isnan(_pi)) {
            snprintf(buf, sizeof(buf), "PI:   --.--");
        } else {
            snprintf(buf, sizeof(buf), "PI:   %3.2f", _pi);
        }
        _text_sprite.drawString(buf, 0, line_h * 3);
        _beat_sprite.clear(palette_black);
        _beat_sprite.fillCircle(_beat_size / 2, _beat_size / 2, 7, _beat ? palette_theme : palette_white);
    }

    void clear()
    {
        _bpm  = std::numeric_limits<float>::quiet_NaN();
        _spo2 = std::numeric_limits<float>::quiet_NaN();
        _pi   = std::numeric_limits<float>::quiet_NaN();
        _beat = 0;
        _meter->clear();
        _plot_sprite.clear(palette_black);
        _text_sprite.clear(palette_black);
        _beat_sprite.clear(palette_black);
    }

    void push(LovyanGFX* target, const uint32_t x, const uint32_t y)
    {
        _text_sprite.pushSprite(target, x, y);
        _beat_sprite.pushSprite(target, x + _beat_x, y + _beat_y);
        _plot_sprite.pushSprite(target, x, y + _plot_top);
    }

    enum : uint8_t {
        palette_black  = 0,
        palette_white  = 1,
        palette_theme  = 2,
        palette_gauge  = 3,
        palette_second = 4,  // Plot sprite only
    };
    static constexpr uint32_t beat_frames{4};                         // Number of updates the beat mark stays lit
    static constexpr m5::utility::elapsed_time_t spo2_hold_ms{6000};  // Time the last valid SpO2 is kept
    LGFX_Sprite _plot_sprite{};
    LGFX_Sprite _text_sprite{};
    LGFX_Sprite _beat_sprite{};
    Meter* _meter{};
    const char* _title{};
    float _bpm{std::numeric_limits<float>::quiet_NaN()};
    float _spo2{std::numeric_limits<float>::quiet_NaN()};
    m5::utility::elapsed_time_t _spo2_at{};  // Time of the last valid SpO2
    float _pi{std::numeric_limits<float>::quiet_NaN()};
    uint32_t _beat{};
    uint32_t _screen_w{}, _plot_top{}, _plot_hgt{};
    int32_t _text_w{}, _text_h{}, _beat_x{}, _beat_y{}, _beat_size{};
    m5gfx::rgb565_t _theme_color{};
};
#endif
