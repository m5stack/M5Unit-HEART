/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Example using M5UnitUnified for UnitHeart / HatHeart / StampHeart
*/
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
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
// RST must be driven high: StampHeart pulls RST low, so the chip does not respond if RST is not wired
// StampHeart is intended to be wired directly to a Stamp host (e.g. StampC5 / StampP4)
// Set these to the GPIOs you wired (used on the boards that select_int_rst_port() does not handle)
// INT -1: polling, RST -1: not driven (the chip stays in reset, set the GPIO you wired)
constexpr int8_t fallback_int_pin{-1};
constexpr int8_t fallback_rst_pin{-1};

// Port for the INT/RST GROVE. The I2C GROVE goes to Port A (NessoN1: QWIIC, using a QWIIC-GROVE cable)
// Port C: Core / Core2 / CoreS3 / CoreS3SE / Fire (PSRAM disabled) / Tab5, Port B: Dial / NessoN1 (GROVE)
bool select_int_rst_port(m5::pin_name_t& rst, m5::pin_name_t& intr)
{
    switch (M5.getBoard()) {
        case m5::board_t::board_M5Stack:
            // Fire is also detected as M5Stack. Its Port C is shared with PSRAM, so it is usable only without PSRAM
            if (heap_caps_get_total_size(MALLOC_CAP_SPIRAM)) {
                return false;
            }
            rst  = m5::pin_name_t::port_c_pin1;
            intr = m5::pin_name_t::port_c_pin2;
            return true;
        case m5::board_t::board_M5StackCore2:
        case m5::board_t::board_M5StackCoreS3:
        case m5::board_t::board_M5StackCoreS3SE:
        case m5::board_t::board_M5Tab5:
            rst  = m5::pin_name_t::port_c_pin1;
            intr = m5::pin_name_t::port_c_pin2;
            return true;
        case m5::board_t::board_M5Dial:
        case m5::board_t::board_ArduinoNessoN1:
            rst  = m5::pin_name_t::port_b_pin1;
            intr = m5::pin_name_t::port_b_pin2;
            return true;
        default:
            return false;
    }
}
#endif

#if defined(USING_STAMP_HEART)
// SpO2 conversion for StampHeart: the provisional slope (110 - 25R) with the offset fitted against pulse oximeters
// at normal SpO2 (94-98 %). Not verified at lower SpO2
float stamp_heart_spo2(const float r)
{
    return 112.0f - 25.0f * r;
}
#endif

m5::heart::PulseProcessor::config_t make_pulse_config()
{
    // Settings to adjust (the values other than spo2_conversion are the defaults)
    m5::heart::PulseProcessor::config_t cfg{};
#if defined(USING_STAMP_HEART)
    // SpO2 from the ratio of ratios R (see stamp_heart_spo2 above)
    cfg.spo2_conversion = stamp_heart_spo2;
#endif
    // Heart rate smoothing at rest (s). Larger: more stable, slower to follow / Smaller: faster, fluctuates more
    cfg.bpm_time_constant = 5.0f;
    // Heart rate smoothing while it is changing (s). Larger: slower to follow / Smaller: faster, irregular beats
    // move the value more
    cfg.bpm_fast_time_constant = 1.0f;
    // Difference (bpm) from the recent beats regarded as changing. Larger: more stable, slower to follow / Smaller:
    // faster, less stable
    cfg.bpm_fast_difference = 4.0f;
    // SpO2 smoothing (s). Larger: more stable, slower to follow / Smaller: faster, fluctuates more
    cfg.spo2_time_constant = 4.0f;
    // Allowed spread of R of the recent beats for a valid SpO2. Larger: SpO2 is shown more often, less reliable /
    // Smaller: more reliable, becomes invalid more often
    cfg.spo2_ratio_spread = 0.25f;
    return cfg;
}
// Heart rate / SpO2 processing (sampling rate and channels are taken from the unit)
m5::heart::PulseController pulse{make_pulse_config()};

#if defined(USING_HAT_HEART)
constexpr bool using_multi_led_mode{false};  // Using multiLED mode if true
#endif

// The latest raw values and PI are also shown on the LCD to check the sensor (not on no-LCD or EPD devices)
// (BPM / SpO2 are shown by GraphicalMeter)
LGFX_Sprite sprite;
bool has_lcd{};
constexpr uint32_t draw_interval_ms{250};  // Drawing often delays update() and overflows the FIFO
m5::utility::elapsed_time_t drawn_at{};
#if defined(USING_STAMP_HEART)
int32_t latest_raw[3]{};  // Green, Red, IR
#else
uint32_t latest_raw[2]{};  // IR, Red
#endif
#if !defined(USING_STAMP_HEART)
float temperature{std::numeric_limits<float>::quiet_NaN()};  // Latest measured temperature (Celsius)
#endif

void draw_lcd()
{
    sprite.fillSprite(0);
    sprite.setTextDatum(middle_center);

    // PI is shown only while the heart rate is valid
    const std::string lines[] = {
#if defined(USING_STAMP_HEART)
        m5::utility::formatString("G %ld", static_cast<long>(latest_raw[0])),
        m5::utility::formatString("R %ld", static_cast<long>(latest_raw[1])),
        m5::utility::formatString("IR %ld", static_cast<long>(latest_raw[2])),
#else
        m5::utility::formatString("IR %u", static_cast<unsigned>(latest_raw[0])),
        m5::utility::formatString("R %u", static_cast<unsigned>(latest_raw[1])),
#endif
        pulse.hasBpm() ? m5::utility::formatString("PI %.2f%%", pulse.perfusionIndex()) : std::string("PI --.--%"),
#if !defined(USING_STAMP_HEART)
        std::isnan(temperature) ? std::string("Temp --.-C") : m5::utility::formatString("Temp %.1fC", temperature),
#endif
    };
    constexpr int32_t count = sizeof(lines) / sizeof(lines[0]);
    const int32_t lh        = sprite.fontHeight() * 3 / 2;
    int32_t y               = sprite.height() / 2 - lh * (count - 1) / 2;
    for (auto&& s : lines) {
        sprite.drawString(s.c_str(), sprite.width() / 2, y);
        y += lh;
    }
    sprite.pushSprite(&lcd, 0, 0);
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
        auto cfg = unit.config();
        m5::pin_name_t rst_pin_name{}, int_pin_name{};
        if (select_int_rst_port(rst_pin_name, int_pin_name)) {
            cfg.rst_pin = M5.getPin(rst_pin_name);
            cfg.int_pin = M5.getPin(int_pin_name);
        } else {
            cfg.rst_pin = fallback_rst_pin;
            cfg.int_pin = fallback_int_pin;
        }
        unit.config(cfg);
        M5.Log.printf("StampHeart INT:%d RST:%d\n", cfg.int_pin, cfg.rst_pin);
    }
    // NessoN1: I2C on QWIIC (Port A), as its GROVE (Port B) is used for INT/RST
    if (!m5::unit::wiring::addI2C(Units, unit, 0, m5::unit::wiring::NessoPort::PortA) || !Units.begin()) {
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

    // Display setup (skip for no-LCD or EPD devices)
    has_lcd = lcd.width() > 0 && lcd.height() > 0 && !lcd.isEPD();
    if (has_lcd) {
        sprite.setPsram(false);
        sprite.setColorDepth(1);
        has_lcd = sprite.createSprite(lcd.width(), lcd.height());
    }
    if (has_lcd) {
        sprite.setPaletteColor(0, TFT_DARKGREEN);
        sprite.setPaletteColor(1, TFT_WHITE);
        sprite.setFont(&fonts::Font2);
        sprite.setTextSize(std::max<int32_t>(1, lcd.width() / 160));
        sprite.setTextColor(1, 0);
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

#if defined(USING_STAMP_HEART)
        // Overflow means update() is not called often enough for the sampling rate
        if (unit.overflowed()) {
            M5_LOGW("OVERFLOW");
        }
        while (unit.available()) {
            latest_raw[0] = unit.green();
            latest_raw[1] = unit.red();
            latest_raw[2] = unit.ir();
            M5.Log.printf(">GREEN:%ld\n>RED:%ld\n>IR:%ld\n", static_cast<long>(latest_raw[0]),
                          static_cast<long>(latest_raw[1]), static_cast<long>(latest_raw[2]));
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
            latest_raw[0] = unit.ir();
            latest_raw[1] = unit.red();
            M5.Log.printf(">IR:%u\n>RED:%u\n", static_cast<unsigned>(latest_raw[0]),
                          static_cast<unsigned>(latest_raw[1]));
            unit.discard();  // Discard the oldest data
        }
#endif
        // Invalid values (NaN) are not printed
        if (pulse.hasBpm()) {
            M5.Log.printf(">BPM:%f\n>PI:%f\n", pulse.bpm(), pulse.perfusionIndex());
        }
        if (pulse.hasSpO2()) {
            M5.Log.printf(">SpO2:%f\n>R:%f\n", pulse.spo2(), pulse.ratio());
        }
        M5.Log.printf(">BEAT:%u\n", pulse.isBeat());
    }

#if !defined(USING_STAMP_HEART)
    // Measure temperature
    if (M5.BtnA.wasClicked()) {
        TemperatureData td{};
        if (unit.measureTemperatureSingleshot(td)) {
            temperature = td.celsius();
            M5.Log.printf(">Temp:%f\n", temperature);
        }
    }
#endif

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
