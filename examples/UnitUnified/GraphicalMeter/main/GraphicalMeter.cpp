/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Graphical meter example for UnitHeart / HatHeart / StampHeart
  The core must be equipped with LCD
*/
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedHEART.h>
#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <wiring/m5_unit_unified_wiring.hpp>  // Board-aware connection helpers (include last)
#include "../src/view.hpp"

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
m5::unit::UnitHeart heart;
#elif defined(USING_HAT_HEART)
m5::unit::HatHeart heart;
#elif defined(USING_STAMP_HEART)
m5::unit::StampHeart heart;
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
// Filter for the plotted waveform: 0.3 Hz high-pass removes the DC level, 10 Hz low-pass removes the sensor noise
// while keeping the pulse shape (the processing uses its own 0.5-4 Hz band-pass)
m5::heart::BandPass plot_filter{};
// The horizontal axis is time: one plotted point is 20 ms regardless of the sampling rate
// (samples are thinned out above 50 Hz and repeated below it)
constexpr float plot_points_per_sec{50.0f};
float plot_step{1.0f};  // Plotted points per sample
float plot_acc{};       // Accumulated plotted points

// Plotted points are queued and drawn one per 20 ms, so that the waveform scrolls smoothly
// while the unit delivers the samples in batches (about every 0.1 s)
constexpr uint32_t plot_interval_ms{static_cast<uint32_t>(1000.0f / plot_points_per_sec)};
constexpr size_t plot_queue_size{64};
constexpr size_t plot_queue_catch_up{15};  // Draw extra points when more than this are waiting (0.3 s)
float plot_queue[plot_queue_size]{};
size_t plot_queue_head{}, plot_queue_count{};
m5::utility::elapsed_time_t plot_latest{};

void plot_enqueue(const float v)
{
    if (plot_queue_count == plot_queue_size) {  // Full: drop the oldest
        plot_queue_head = (plot_queue_head + 1) % plot_queue_size;
        --plot_queue_count;
    }
    plot_queue[(plot_queue_head + plot_queue_count) % plot_queue_size] = v;
    ++plot_queue_count;
}

float plot_dequeue()
{
    const float v   = plot_queue[plot_queue_head];
    plot_queue_head = (plot_queue_head + 1) % plot_queue_size;
    --plot_queue_count;
    return v;
}

View* view{};

// Sampling rate switched by a long press of BtnA (other settings are kept as far as possible)
constexpr uint32_t rate_table[] = {50, 100, 200};
constexpr size_t rate_count{sizeof(rate_table) / sizeof(rate_table[0])};
size_t rate_index{1};  // 100 Hz: the default of all units
#if defined(USING_UNIT_HEART)
constexpr char unit_name[] = "Unit";
#elif defined(USING_HAT_HEART)
constexpr char unit_name[] = "Hat";
#else
constexpr char unit_name[] = "Stamp";
#endif
char title[16]{};  // "<name> <rate>Hz"

// Restart the periodic measurement at rate_table[idx]
bool start_measurement(const size_t idx)
{
    heart.stopPeriodicMeasurement();
#if defined(USING_UNIT_HEART)
    // MAX30100: 200 Hz does not allow the 1600 us pulse width, so 800 us is used for it
    constexpr Sampling rates[] = {Sampling::Rate50, Sampling::Rate100, Sampling::Rate200};
    const auto cfg             = heart.config();
    const LEDPulse width       = (idx == 2) ? LEDPulse::Width800 : cfg.width;
    const bool high_resolution = cfg.high_resolution && width == LEDPulse::Width1600;
    return heart.startPeriodicMeasurement(cfg.mode, rates[idx], width, cfg.ir_current, high_resolution,
                                          cfg.red_current);
#elif defined(USING_HAT_HEART)
    // MAX30102: effective rate = sampling rate / FIFO average (the pulse width is kept)
    constexpr Sampling rates[]    = {Sampling::Rate200, Sampling::Rate400, Sampling::Rate200};
    constexpr FIFOSampling avgs[] = {FIFOSampling::Average4, FIFOSampling::Average4, FIFOSampling::Average1};
    const auto cfg                = heart.config();
    return heart.startPeriodicMeasurement(cfg.mode, cfg.adc_range, rates[idx], cfg.pulse_width, avgs[idx],
                                          cfg.ir_current, cfg.red_current);
#else
    constexpr SamplingRate rates[] = {SamplingRate::Rate50, SamplingRate::Rate100, SamplingRate::Rate200};
    return heart.startPeriodicMeasurement(heart.mode(), rates[idx]);
#endif
}

// Rebuild the processing and the plot for the current sampling rate of the unit
void restart_view()
{
    const float rate = heart.ppgSpec().sampling_rate_hz;
    plot_filter      = m5::heart::BandPass(0.3f, 10.0f, rate);
    plot_step        = (rate > 0.0f) ? plot_points_per_sec / rate : 1.0f;
    plot_acc         = 0.0f;
    plot_queue_head = plot_queue_count = 0;
    pulse.add(heart);
    snprintf(title, sizeof(title), "%s %luHz", unit_name, static_cast<unsigned long>(rate + 0.5f));
    view->clear();
    M5.Log.printf("Sampling rate: %.1f Hz\n", rate);
}

// Waveform to plot: the heart rate channel filtered, inverted so that the systolic peak points up
float plot_value()
{
#if defined(USING_STAMP_HEART)
    const float raw = static_cast<float>(heart.green());
#else
    const float raw = static_cast<float>(heart.ir());
#endif
    return plot_filter.process(-raw);
}

}  // namespace

void setup()
{
    M5.begin();
    M5.setTouchButtonHeightByRatio(100);

    auto board = M5.getBoard();
    // The screen shall be in landscape mode (skip for M5Tab5)
    if (board != m5::board_t::board_M5Tab5 && lcd.height() > lcd.width()) {
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

    // HatHeart: NessoN1 uses Wire1, other boards use Wire (handled by addHatI2C)
    if (!m5::unit::wiring::addHatI2C(Units, heart, 400 * 1000U) || !Units.begin()) {
        M5_LOGE("Failed to begin");
        m5::unit::wiring::failStop();
    }
#elif defined(USING_STAMP_HEART)
    {
        // Core / Core2 / CoreS3 / CoreS3SE: I2C GROVE -> Port A, INT/RST GROVE -> Port C
        auto cfg = heart.config();
        if (has_usable_port_c()) {
            cfg.rst_pin = M5.getPin(m5::pin_name_t::port_c_pin1);
            cfg.int_pin = M5.getPin(m5::pin_name_t::port_c_pin2);
        } else {
            cfg.rst_pin = fallback_rst_pin;
            cfg.int_pin = fallback_int_pin;
        }
        heart.config(cfg);
        M5.Log.printf("StampHeart INT:%d RST:%d\n", cfg.int_pin, cfg.rst_pin);
    }
    if (!m5::unit::wiring::addI2C(Units, heart, 400 * 1000U) || !Units.begin()) {
        M5_LOGE("Failed to begin");
        m5::unit::wiring::failStop();
    }
#else
    // UnitHeart: NessoN1 -> SoftwareI2C (M5HAL), NanoC6 / NanoH2 -> M5.Ex_I2C, others -> Wire
    if (!m5::unit::wiring::addI2C(Units, heart, 400 * 1000U) || !Units.begin()) {
        M5_LOGE("Failed to begin");
        m5::unit::wiring::failStop();
    }
#endif

    M5.Log.printf("M5UnitUnified initialized\n");
    M5.Log.printf("%s\n", Units.debugInfo().c_str());

    lcd.fillScreen(0);

    if (!pulse.add(heart)) {
        M5_LOGE("Failed to add the PPG data source");
        m5::unit::wiring::failStop();
    }

    // The title area is sized by the longest title
    snprintf(title, sizeof(title), "%s 200Hz", unit_name);
#if defined(USING_UNIT_HEART)
    view = new View(lcd.width(), lcd.height(), title, TFT_BLUE);
#elif defined(USING_HAT_HEART)
    view = new View(lcd.width(), lcd.height(), title, TFT_YELLOW);
#else
    view = new View(lcd.width(), lcd.height(), title, TFT_GREEN);
#endif
    restart_view();
    view->push(&lcd, 0, 0);

    M5.Log.printf("periodic:%d\n", heart.inPeriodic());
}

void loop()
{
    M5.update();
    Units.update();

    if (heart.updated()) {
        // Process the new samples before they are discarded below
        pulse.update();
        if (heart.ppgOverflowed()) {
            M5_LOGW("OVERFLOW");
        }
        while (heart.available()) {
            const float v = plot_value();  // The filter runs at the sampling rate
            plot_acc += plot_step;
            while (plot_acc >= 1.0f) {
                plot_enqueue(v);
                plot_acc -= 1.0f;
            }
            heart.discard();
        }
        view->update(pulse.bpm(), pulse.spo2(), pulse.isBeat());
    }

    // Draw one queued point per 20 ms (more if the queue is backed up)
    if (plot_queue_count && m5::utility::hasElapsed(plot_latest, plot_interval_ms)) {
        plot_latest = m5::utility::millis();
        size_t n    = 1 + ((plot_queue_count > plot_queue_catch_up) ? plot_queue_count - plot_queue_catch_up : 0);
        while (n-- && plot_queue_count) {
            view->push_back(plot_dequeue());
        }
        view->render();
        view->push(&lcd, 0, 0);
    }

    if (M5.BtnA.wasHold()) {
        // Long press: switch the sampling rate 50 -> 100 -> 200 Hz
        rate_index = (rate_index + 1) % rate_count;
        if (!start_measurement(rate_index)) {
            M5_LOGE("Failed to start %lu Hz", static_cast<unsigned long>(rate_table[rate_index]));
        }
        restart_view();
    } else if (M5.BtnA.wasClicked()) {
        restart_view();  // Restart the processing
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
