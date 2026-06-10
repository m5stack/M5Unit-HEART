/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Example using M5UnitUnified for UnitHeart / HatHeart
*/
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedHEART.h>
#include <driver/gpio.h>
#include <wiring/m5_unit_unified_wiring.hpp>  // Board-aware connection helpers (include last)

// *************************************************************
// Choose one define symbol to match the unit you are using
// *************************************************************
#if !defined(USING_UNIT_HEART) && !defined(USING_HAT_HEART)
// For UnitHeart (U029)
// #define USING_UNIT_HEART
// For HatHeart (U118)
// #define USING_HAT_HEART
#endif

#if defined(USING_UNIT_HEART)
#pragma message "Using UnitHeart"
using namespace m5::unit::max30100;
#elif defined(USING_HAT_HEART)
#pragma message "Using HatHeart"
using namespace m5::unit::max30102;
#else
#error Please choose unit!
#endif

namespace {
auto& lcd = M5.Display;
m5::unit::UnitUnified Units;
#if defined(USING_UNIT_HEART)
m5::unit::UnitHeart unit;
#elif defined(USING_HAT_HEART)
m5::unit::HatHeart unit;
#endif

m5::heart::PulseMonitor monitor;

#if defined(USING_HAT_HEART)
constexpr bool using_multi_led_mode{false};  // Using multiLED mode if true
#endif

}  // namespace

void setup()
{
    M5.begin();
    M5.setTouchButtonHeightByRatio(100);

    // The screen shall be in landscape mode
    if (lcd.height() > lcd.width()) {
        lcd.setRotation(1);
    }

#if defined(USING_HAT_HEART)
    const auto hat_pins = m5::unit::wiring::hatI2CPins();
    if (hat_pins.sda < 0 || hat_pins.scl < 0) {
        M5_LOGE("Hat port not exists");
        m5::unit::wiring::failStop();
    }

    // Setup required to use HatHEART: drive SCL as output before bus init (ESP-IDF native GPIO)
    gpio_set_direction(static_cast<gpio_num_t>(hat_pins.scl), GPIO_MODE_OUTPUT);

    // Using MultiLED mode
    // In MultiLED mode, you need to set and start them yourself
    if (using_multi_led_mode) {
        auto cfg           = unit.config();
        cfg.start_periodic = false;  // Ignore auto start
        unit.config(cfg);
    }

    // HatHeart: NessoN1 uses Wire1, other boards use Wire (handled by addHatI2C)
    if (!m5::unit::wiring::addHatI2C(Units, unit, 400 * 1000U) || !Units.begin()) {
        M5_LOGE("Failed to begin");
        m5::unit::wiring::failStop();
    }
#else
    // UnitHeart: NessoN1 -> SoftwareI2C (M5HAL), NanoC6 / NanoH2 -> M5.Ex_I2C, others -> Wire
    if (!m5::unit::wiring::addI2C(Units, unit, 400 * 1000U) || !Units.begin()) {
        M5_LOGE("Failed to begin");
        m5::unit::wiring::failStop();
    }
#endif

    M5_LOGI("M5UnitUnified initialized");
    M5_LOGI("%s", Units.debugInfo().c_str());

#if defined(USING_HAT_HEART)
    // In MultiLED mode, you need to set and start them yourself
    if (using_multi_led_mode) {
        M5_LOGI("MultiLED mode");
        unit.writeMode(Mode::MultiLED);
        unit.writeSpO2Configuration(ADC::Range4096nA, Sampling::Rate400, LEDPulse::Width411);
        unit.writeFIFOConfiguration(FIFOSampling::Average4, true, 15);
        // unit.writeMultiLEDModeControl(Slot::Red, Slot::IR); // (A)
        unit.writeMultiLEDModeControl(Slot::IR, Slot::Red);  // (B)
        unit.writeLEDCurrent(0, 0x1F);                       // Red if (A), IR  if (B)
        unit.writeLEDCurrent(1, 0x1F);                       // IR  if (A), Red if (B)
        unit.startPeriodicMeasurement();
    }
#endif

    monitor.setSamplingRate(unit.calculateSamplingRate());
    lcd.fillScreen(TFT_DARKGREEN);
}

void loop()
{
    M5.update();
    Units.update();

    if (unit.updated()) {
        // WARNING
        // If overflow is occurring, the sampling rate should be reduced because the processing is not up to par
        if (unit.overflow()) {
            M5_LOGW("OVERFLOW:%u", unit.overflow());
        }

        bool beat{};
        // MAX30100/02 is equipped with a FIFO, so multiple data may be stored
        while (unit.available()) {
            M5.Log.printf(">IR:%u\n>RED:%u\n", unit.ir(), unit.red());
            monitor.push_back(unit.ir(), unit.red());  // Push back the oldest data
            M5.Log.printf(">MIR:%f\n", monitor.latestIR());
            monitor.update();
            beat |= monitor.isBeat();
            unit.discard();  // Discard the oldest data
        }
        M5.Log.printf(">BPM:%f\n>SpO2:%f\n>BEAT:%u\n", monitor.bpm(), monitor.SpO2(), beat);
    }

    // Measure temperature
    if (M5.BtnA.wasClicked()) {
        TemperatureData td{};
        if (unit.measureTemperatureSingleshot(td)) {
            M5.Log.printf(">Temp:%f\n", td.celsius());
        }
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
