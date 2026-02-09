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
        _theme_color         = unit ? TFT_BLUE : TFT_YELLOW;
        _screen_w            = wid;
        _screen_h            = hgt;
        _plot_top            = hgt * 2 / 3;
        _plot_hgt            = hgt / 3;
        const uint32_t spr_w = wid;
        const uint32_t spr_h = _plot_hgt;
        _plot_sprite.setPsram(false);
        _plot_sprite.setColorDepth(2);
        _plot_sprite.createSprite(spr_w, spr_h);
        _plot_sprite.setPaletteColor(palette_black, TFT_BLACK);
        _plot_sprite.setPaletteColor(palette_white, TFT_WHITE);
        _plot_sprite.setPaletteColor(palette_theme, _theme_color);
        _plot_sprite.setPaletteColor(palette_gauge, TFT_DARKGRAY);
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
        const int32_t title_w = _text_sprite.textWidth("Unit");
        const int32_t bpm_w   = _text_sprite.textWidth("BPM: 000.00");
        const int32_t spo2_w  = _text_sprite.textWidth("SpO2:100.00");
        _text_w               = std::max(title_w, std::max(bpm_w, spo2_w));
        _text_h               = line_h * 3;
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
        _plot_sprite.clear(palette_black);
        _meter->push(&_plot_sprite, 0, 0);
        _text_sprite.clear(palette_black);
        _text_sprite.drawString(_type ? "Unit" : "Hat", 0, 0);
        _text_sprite.setCursor(0, _text_sprite.fontHeight());
        _text_sprite.printf("BPM: %3.2f\nSpO2:%3.2f", _monitor.bpm(), _monitor.SpO2());
        _beat_sprite.clear(palette_black);
        _beat_sprite.fillCircle(_beat_size / 2, _beat_size / 2, 7, _beat ? palette_theme : palette_white);
    }

    void clear()
    {
        _monitor.clear();
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
        palette_black = 0,
        palette_white = 1,
        palette_theme = 2,
        palette_gauge = 3,
    };
    LGFX_Sprite _plot_sprite{};
    LGFX_Sprite _text_sprite{};
    LGFX_Sprite _beat_sprite{};
    m5::heart::PulseMonitor _monitor{100, 2};
    Meter* _meter{};
    uint32_t _left{}, _top{}, _beat{};
    uint32_t _screen_w{}, _screen_h{}, _plot_top{}, _plot_hgt{};
    int32_t _text_w{}, _text_h{}, _beat_x{}, _beat_y{}, _beat_size{};
    bool _type{};
    m5gfx::rgb565_t _theme_color{};
};
#endif
