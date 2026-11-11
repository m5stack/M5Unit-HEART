/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Example of using UnitHeart (MAX30100) via UnitPaHub

  Board ---> PaHub ---> ch:0 UnitHeart

  The values are printed to the serial output. When the board has a screen, the PaHub channel, the latest IR value
  and the perfusion index are also drawn.
*/
#include <algorithm>
#include <string>

#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedHEART.h>
#include <M5UnitUnifiedHUB.h>                 // UnitPaHub
#include <wiring/m5_unit_unified_wiring.hpp>  // Board-aware connection helpers (include last)

namespace {
auto& lcd = M5.Display;

m5::unit::UnitUnified Units;
m5::unit::UnitPaHub hub;
m5::unit::UnitHeart unit;
m5::heart::PulseController pulse;  // Sampling rate and channels are taken from the unit

constexpr uint8_t HEART_CHANNEL{0};  // PaHub channel the UnitHeart is connected to

constexpr int32_t line_count{3};           // PaHub channel, IR, PI
constexpr uint32_t draw_interval_ms{250};  // Drawing often delays update() and overflows the FIFO

LGFX_Sprite sprite;
bool has_lcd{};
int32_t line_height{};
uint16_t latest_ir{};
m5::utility::elapsed_time_t drawn_at{};

// The text size follows the screen, from AtomS3 (128x128) and StickC (160x80) to Tab5 (1280x720).
// Boards without a screen (Atom, NanoC6, ...) report a 1x1 dummy display, EPD is too slow to update
bool setup_sprite()
{
    if (lcd.width() <= 8 || lcd.height() <= 8 || lcd.isEPD()) {
        return false;
    }
    sprite.setFont(&fonts::Font2);
    // A line is 1.5 times the font height. The text size is limited by the width and by the height of all lines
    const int32_t base_line_height{sprite.fontHeight() * 3 / 2};
    sprite.setTextSize(
        std::max<int32_t>(1, std::min<int32_t>(lcd.width() / 160, lcd.height() / (base_line_height * line_count))));
    line_height = std::min<int32_t>(sprite.fontHeight() * 3 / 2, lcd.height() / line_count);

    // Only the text area, so that a large screen does not take a long time to update
    sprite.setPsram(false);
    sprite.setColorDepth(1);
    if (!sprite.createSprite(lcd.width(), line_height * line_count)) {
        M5_LOGW("Failed to create the sprite, nothing is drawn");
        return false;
    }
    sprite.setPaletteColor(0, TFT_DARKGREEN);
    sprite.setPaletteColor(1, TFT_WHITE);
    sprite.setTextColor(1, 0);
    sprite.setTextDatum(middle_center);
    return true;
}

void draw_lcd()
{
    // PI is shown only while the heart rate is valid
    const std::string lines[line_count] = {
        m5::utility::formatString("PaHub ch:%u", HEART_CHANNEL),
        m5::utility::formatString("IR %u", static_cast<unsigned>(latest_ir)),
        pulse.hasBpm() ? m5::utility::formatString("PI %.2f%%", pulse.perfusionIndex()) : std::string("PI --.--%"),
    };
    sprite.fillSprite(0);
    int32_t y{line_height / 2};
    for (auto&& s : lines) {
        sprite.drawString(s.c_str(), sprite.width() / 2, y);
        y += line_height;
    }
    sprite.pushSprite(&lcd, 0, (lcd.height() - sprite.height()) / 2);
}
}  // namespace

void setup()
{
    M5.begin();
    M5.setTouchButtonHeightByRatio(100);

    // The screen shall be in landscape mode
    if (lcd.height() > lcd.width()) {
        lcd.setRotation(1);
    }

    if (!hub.add(unit, HEART_CHANNEL)) {
        M5_LOGE("Failed to add children");
        m5::unit::wiring::failStop();
    }
    // Run the PaHub at the clock of the unit behind it
    auto hcfg  = hub.component_config();
    hcfg.clock = std::min(hcfg.clock, unit.component_config().clock);
    hub.component_config(hcfg);

    // Board-aware I2C for the PaHub: NessoN1 -> PortB GROVE (SoftwareI2C), NanoC6 / NanoH2 -> Ex_I2C, others -> Wire.
    // The UnitHeart is reached through the hub (added above), so only the hub is added here
    if (!m5::unit::wiring::addI2C(Units, hub) || !Units.begin()) {
        M5_LOGE("Failed to begin");
        m5::unit::wiring::failStop();
    }

    M5.Log.printf("M5UnitUnified initialized\n");
    M5.Log.printf("%s\n", Units.debugInfo().c_str());

    // Add after the periodic measurement has started (the sampling rate is 0 before that)
    if (!pulse.add(unit)) {
        M5_LOGE("Failed to add the PPG data source");
        m5::unit::wiring::failStop();
    }

    has_lcd = setup_sprite();
    if (has_lcd) {
        lcd.fillScreen(TFT_DARKGREEN);
        draw_lcd();
    }
}

void loop()
{
    M5.update();
    Units.update();

    if (unit.updated()) {
        // Process the new samples before they are discarded below
        pulse.update();

        // If overflow is occurring, the sampling rate should be reduced because the processing is not up to par
        if (unit.overflow()) {
            M5_LOGW("OVERFLOW:%u", unit.overflow());
        }
        // MAX30100 is equipped with a FIFO, so multiple data may be stored
        while (unit.available()) {
            latest_ir = unit.ir();
            M5.Log.printf(">IR:%u\n>RED:%u\n", static_cast<unsigned>(latest_ir), static_cast<unsigned>(unit.red()));
            unit.discard();  // Discard the oldest data
        }
        // Invalid values (NaN) are not printed
        if (pulse.hasBpm()) {
            M5.Log.printf(">BPM:%f\n>PI:%f\n", pulse.bpm(), pulse.perfusionIndex());
        }
        if (pulse.hasSpO2()) {
            M5.Log.printf(">SpO2:%f\n", pulse.spo2());
        }
    }

    if (has_lcd && m5::utility::hasElapsed(drawn_at, draw_interval_ms)) {
        drawn_at = m5::utility::millis();
        draw_lcd();
    }
}

#if !defined(ARDUINO)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>

#if CONFIG_FREERTOS_UNICORE
// Single-core SoCs: run loop() back-to-back, but every ~2 s yield a 5 ms slice so the IDLE task
// runs and feeds the task watchdog (default 5 s).
static inline void feedIdleTaskPeriodically(void)
{
    constexpr uint32_t FEED_INTERVAL_MS{2000};
    constexpr TickType_t FEED_SLEEP_TICKS{pdMS_TO_TICKS(5)};
    static uint32_t s_last_feed_ms{};
    const uint32_t now_ms{static_cast<uint32_t>(esp_timer_get_time() / 1000)};
    if (now_ms - s_last_feed_ms >= FEED_INTERVAL_MS) {
        s_last_feed_ms = now_ms;
        vTaskDelay(FEED_SLEEP_TICKS);
    }
}
#endif

extern "C" void app_main(void)
{
    setup();
    for (;;) {
#if CONFIG_FREERTOS_UNICORE
        feedIdleTaskPeriodically();
#endif
        loop();
    }
}
#endif
