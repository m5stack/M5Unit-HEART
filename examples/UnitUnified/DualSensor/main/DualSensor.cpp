/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Example of using M5UnitUnified to connect both UnitHeart and HatHeart
*/
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedHEART.h>
#include <driver/gpio.h>
#include <wiring/m5_unit_unified_wiring.hpp>  // Board-aware connection helpers (include last)
#include "../src/view.hpp"

namespace {
auto& lcd = M5.Display;
m5::unit::UnitUnified Units;
m5::unit::UnitHeart unit;
m5::unit::HatHeart hat;

View* view[2]{};
bool is_epd_panel{};

}  // namespace

using namespace m5::unit::max30102;

void setup()
{
    M5.begin();
    M5.setTouchButtonHeightByRatio(100);
    is_epd_panel = lcd.isEPD();

    // The screen shall be in landscape mode
    if (lcd.height() > lcd.width()) {
        lcd.setRotation(1);
    }

    const auto hat_pins = m5::unit::wiring::hatI2CPins();
    if (hat_pins.sda < 0 || hat_pins.scl < 0) {
        M5_LOGE("Hat port not exists");
        m5::unit::wiring::failStop();
    }

    // Setup required to use HatHEART: drive SCL as output before bus init (ESP-IDF native GPIO)
    gpio_set_direction(static_cast<gpio_num_t>(hat_pins.scl), GPIO_MODE_OUTPUT);

    // HatHeart bus. The Unit occupies the board's default I2C (GROVE), so the Hat needs a separate bus:
    //  - NessoN1: Hat has its own port -> addHatI2C (Arduino: Wire1, ESP-IDF native: LP I2C on C6).
    //  - Other boards: no spare hardware I2C, so bit-bang SoftwareI2C on the Hat pins via M5HAL.
    // (addHatI2C is avoided for the non-NessoN1 case because it would reuse Wire = the Unit's bus.)
    bool hat_ready{};
    if (hat_pins.useWire1) {
        hat_ready = m5::unit::wiring::addHatI2C(Units, hat, 400 * 1000U);
    } else {
        hat_ready = m5::unit::wiring::i2cSoftware(Units, hat, hat_pins.sda, hat_pins.scl);
    }

    // UnitHeart on the board's default I2C: NessoN1 -> SoftwareI2C (GROVE port_b), others -> Wire (port_a)
    const bool unit_ready = m5::unit::wiring::addI2C(Units, unit, 400 * 1000U);

    if (!hat_ready || !unit_ready || !Units.begin()) {
        M5_LOGE("Failed to begin %u/%u", hat_ready, unit_ready);
        M5_LOGE("%s", Units.debugInfo().c_str());
        m5::unit::wiring::failStop();
    }

    M5_LOGI("M5UnitUnified initialized");
    M5_LOGI("%s", Units.debugInfo().c_str());

    view[0] = new View(lcd.width() >> 1, lcd.height(), true);
    view[1] = new View(lcd.width() >> 1, lcd.height(), false);
    view[0]->_monitor.setSamplingRate(unit.calculateSamplingRate());
    view[1]->_monitor.setSamplingRate(hat.calculateSamplingRate());
    if (!is_epd_panel) {
        view[0]->push(&lcd, lcd.width() >> 1, 0);
        view[1]->push(&lcd, 0, 0);
    }
}

void loop()
{
    M5.update();
    Units.update();

    if (!is_epd_panel) {
        lcd.startWrite();
    }
    if (unit.updated()) {
        if (unit.overflow()) {
            M5_LOGW("OVERFLOW U:%u", unit.overflow());
        }
        bool beat{};
        while (unit.available()) {
            const auto ir  = unit.ir();
            const auto red = unit.red();
            view[0]->push_back(ir, red);
            view[0]->update();
            if (is_epd_panel) {
                M5.Log.printf(">UIR:%u\n>URED:%u\n", static_cast<uint32_t>(ir), static_cast<uint32_t>(red));
                M5.Log.printf(">UMIR:%f\n", view[0]->_monitor.latestIR());
            }
            beat |= view[0]->_monitor.isBeat();
            unit.discard();
        }
        if (is_epd_panel) {
            M5.Log.printf(">UBPM:%f\n>USpO2:%f\n>UBEAT:%u\n", view[0]->_monitor.bpm(), view[0]->_monitor.SpO2(), beat);
        }
        if (!is_epd_panel) {
            view[0]->render();
            view[0]->push(&lcd, lcd.width() >> 1, 0);
        }
    }

    if (hat.updated()) {
        if (hat.overflow()) {
            M5_LOGW("OVERFLOW H:%u", hat.overflow());
        }
        bool beat{};
        while (hat.available()) {
            const auto ir  = hat.ir();
            const auto red = hat.red();
            view[1]->push_back(ir, red);
            view[1]->update();
            if (is_epd_panel) {
                M5.Log.printf(">HIR:%u\n>HRED:%u\n", static_cast<uint32_t>(ir), static_cast<uint32_t>(red));
                M5.Log.printf(">HMIR:%f\n", view[1]->_monitor.latestIR());
            }
            beat |= view[1]->_monitor.isBeat();
            hat.discard();
        }
        if (is_epd_panel) {
            M5.Log.printf(">HBPM:%f\n>HSpO2:%f\n>HBEAT:%u\n", view[1]->_monitor.bpm(), view[1]->_monitor.SpO2(), beat);
        }
        if (!is_epd_panel) {
            view[1]->render();
            view[1]->push(&lcd, 0, 0);
        }
    }

    if (M5.BtnA.wasClicked()) {
        view[0]->clear();
        view[1]->clear();
    }
    if (!is_epd_panel) {
        lcd.endWrite();
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
    constexpr uint32_t FEED_INTERVAL_MS   = 2000;
    constexpr TickType_t FEED_SLEEP_TICKS = pdMS_TO_TICKS(5);
    static uint32_t s_next_feed_ms        = 0;
    const uint32_t now_ms                 = static_cast<uint32_t>(esp_timer_get_time() / 1000);
    if (now_ms >= s_next_feed_ms) {
        s_next_feed_ms = now_ms + FEED_INTERVAL_MS;
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
