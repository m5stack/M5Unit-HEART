/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Example using M5UnitUnified for UnitHeart / HatHeart / StampHeart
*/
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedHEART.h>
#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <wiring/m5_unit_unified_wiring.hpp>  // Board-aware connection helpers (include last)

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

#if defined(USING_UNIT_HEART)
#pragma message "Using UnitHeart"
using namespace m5::unit::max30100;
#elif defined(USING_HAT_HEART)
#pragma message "Using HatHeart"
using namespace m5::unit::max30102;
#elif defined(USING_STAMP_HEART)
#pragma message "Using StampHeart"
using namespace m5::unit::gh3038;
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
#elif defined(USING_STAMP_HEART)
m5::unit::StampHeart unit;
// StampHeart INT/RST GROVE: pin1 (next to 5V) = RST, pin2 = INT
// StampHeart is intended to be wired directly to a Stamp host (e.g. StampC5 / StampP4)
// Set these to the GPIOs you wired (used except on Core / Core2 / CoreS3 / CoreS3SE)
// -1: INT unused (polling), RST not driven
constexpr int8_t fallback_int_pin{-1};
constexpr int8_t fallback_rst_pin{-1};

// Core series whose Port C can be used for the INT/RST GROVE
bool has_usable_port_c()
{
    switch (M5.getBoard()) {
        case m5::board_t::board_M5Stack:
            // Fire is also detected as M5Stack, but its Port C is shared with PSRAM
            return heap_caps_get_total_size(MALLOC_CAP_SPIRAM) == 0;
        case m5::board_t::board_M5StackCore2:
        case m5::board_t::board_M5StackCoreS3:
        case m5::board_t::board_M5StackCoreS3SE:
            return true;
        default:
            return false;
    }
}
#endif

// Heart rate / SpO2 processing (sampling rate and channels are taken from the unit)
m5::heart::PulseController pulse;

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
    if (!m5::unit::wiring::addHatI2C(Units, unit) || !Units.begin()) {
        M5_LOGE("Failed to begin");
        m5::unit::wiring::failStop();
    }
#elif defined(USING_STAMP_HEART)
    {
        // Core / Core2 / CoreS3 / CoreS3SE: I2C GROVE -> Port A, INT/RST GROVE -> Port C
        auto cfg = unit.config();
        if (has_usable_port_c()) {
            cfg.rst_pin = M5.getPin(m5::pin_name_t::port_c_pin1);
            cfg.int_pin = M5.getPin(m5::pin_name_t::port_c_pin2);
        } else {
            cfg.rst_pin = fallback_rst_pin;
            cfg.int_pin = fallback_int_pin;
        }
        unit.config(cfg);
        M5.Log.printf("StampHeart INT:%d RST:%d\n", cfg.int_pin, cfg.rst_pin);
    }
    if (!m5::unit::wiring::addI2C(Units, unit) || !Units.begin()) {
        M5_LOGE("Failed to begin");
        m5::unit::wiring::failStop();
    }
#else
    // UnitHeart: NessoN1 -> SoftwareI2C (M5HAL), NanoC6 / NanoH2 -> M5.Ex_I2C, others -> Wire
    if (!m5::unit::wiring::addI2C(Units, unit) || !Units.begin()) {
        M5_LOGE("Failed to begin");
        m5::unit::wiring::failStop();
    }
#endif

    M5.Log.printf("M5UnitUnified initialized\n");
    M5.Log.printf("%s\n", Units.debugInfo().c_str());

#if defined(USING_HAT_HEART)
    // In MultiLED mode, you need to set and start them yourself
    if (using_multi_led_mode) {
        M5.Log.printf("MultiLED mode\n");
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

    // Add after the periodic measurement has started (the sampling rate is 0 before that)
    if (!pulse.add(unit)) {
        M5_LOGE("Failed to add the PPG data source");
        m5::unit::wiring::failStop();
    }
    lcd.fillScreen(TFT_DARKGREEN);
}

void loop()
{
    M5.update();
    Units.update();

    if (unit.updated()) {
        // Process the new samples before they are discarded below
        pulse.update();

#if defined(USING_STAMP_HEART)
        // Overflow means update() is not called often enough for the sampling rate
        if (unit.overflowed()) {
            M5_LOGW("OVERFLOW");
        }
        while (unit.available()) {
            M5.Log.printf(">GREEN:%ld\n>RED:%ld\n>IR:%ld\n", static_cast<long>(unit.green()),
                          static_cast<long>(unit.red()), static_cast<long>(unit.ir()));
            unit.discard();
        }
#else
        // WARNING
        // If overflow is occurring, the sampling rate should be reduced because the processing is not up to par
        if (unit.overflow()) {
            M5_LOGW("OVERFLOW:%u", unit.overflow());
        }
        // MAX30100/02 is equipped with a FIFO, so multiple data may be stored
        while (unit.available()) {
            M5.Log.printf(">IR:%u\n>RED:%u\n", unit.ir(), unit.red());
            unit.discard();  // Discard the oldest data
        }
#endif
        // Invalid values (NaN) are not printed
        if (pulse.hasBpm()) {
            M5.Log.printf(">BPM:%f\n>PI:%f\n", pulse.bpm(), pulse.perfusionIndex());
        }
        if (pulse.hasSpO2()) {
            M5.Log.printf(">SpO2:%f\n", pulse.spo2());  // Provisional (not calibrated)
        }
        M5.Log.printf(">BEAT:%u\n", pulse.isBeat());
    }

#if !defined(USING_STAMP_HEART)
    // Measure temperature
    if (M5.BtnA.wasClicked()) {
        TemperatureData td{};
        if (unit.measureTemperatureSingleshot(td)) {
            M5.Log.printf(">Temp:%f\n", td.celsius());
        }
    }
#endif
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
