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

// Uncomment to print the raw values and the results to Serial (always printed on EPD devices)
// #define USING_SERIAL_LOG

namespace {
auto& lcd = M5.Display;
m5::unit::UnitUnified Units;
m5::unit::UnitHeart unit;
m5::unit::HatHeart hat;

// Heart rate / SpO2 processing for each unit (sampling rate and channels are taken from the unit)
m5::heart::PulseController pulse_unit;
m5::heart::PulseController pulse_hat;

View* view[2]{};
bool is_epd_panel{};
bool serial_log{};

// Waveform plot of one unit
// - Filter: 0.3 Hz high-pass removes the DC level, 10 Hz low-pass removes the sensor noise while keeping the pulse
//   shape (the processing uses its own 0.5-4 Hz band-pass)
// - The horizontal axis is time: one plotted point is 20 ms regardless of the sampling rate
// - Points are queued and drawn one per 20 ms, so that the waveform scrolls smoothly while the unit delivers the
//   samples in batches
constexpr float plot_points_per_sec{50.0f};
constexpr uint32_t plot_interval_ms{static_cast<uint32_t>(1000.0f / plot_points_per_sec)};
constexpr size_t plot_queue_size{64};
constexpr size_t plot_queue_catch_up{15};  // Draw extra points when more than this are waiting (0.3 s)

struct Plot {
    m5::heart::BandPass filter{};
    float step{1.0f};  // Plotted points per sample
    float acc{};       // Accumulated plotted points
    float queue[plot_queue_size]{};
    size_t head{}, count{};

    void restart(const float rate)
    {
        filter = m5::heart::BandPass(0.3f, 10.0f, rate);
        step   = (rate > 0.0f) ? plot_points_per_sec / rate : 1.0f;
        acc    = 0.0f;
        head = count = 0;
    }
    // Add a raw sample (inverted so that the systolic peak points up)
    void add(const float raw)
    {
        const float v = filter.process(-raw);  // The filter runs at the sampling rate
        acc += step;
        while (acc >= 1.0f) {
            if (count == plot_queue_size) {  // Full: drop the oldest
                head = (head + 1) % plot_queue_size;
                --count;
            }
            queue[(head + count) % plot_queue_size] = v;
            ++count;
            acc -= 1.0f;
        }
    }
    // Move the points to draw in this frame to the view
    bool draw(View* v)
    {
        if (!count) {
            return false;
        }
        size_t n = 1 + ((count > plot_queue_catch_up) ? count - plot_queue_catch_up : 0);
        while (n-- && count) {
            v->push_back(queue[head]);
            head = (head + 1) % plot_queue_size;
            --count;
        }
        return true;
    }
};

Plot plot[2]{};
m5::utility::elapsed_time_t plot_latest{};

void restart()
{
    pulse_unit.add(unit);
    pulse_hat.add(hat);
    plot[0].restart(unit.ppgSpec().sampling_rate_hz);
    plot[1].restart(hat.ppgSpec().sampling_rate_hz);
    view[0]->clear();
    view[1]->clear();
}

}  // namespace

using namespace m5::unit::max30102;

void setup()
{
    M5.begin();
    M5.setTouchButtonHeightByRatio(100);
    is_epd_panel = lcd.isEPD();
#if defined(USING_SERIAL_LOG)
    serial_log = true;
#else
    serial_log = is_epd_panel;
#endif

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
        hat_ready = m5::unit::wiring::addHatI2C(Units, hat);
    } else {
        hat_ready = m5::unit::wiring::i2cSoftware(Units, hat, hat_pins.sda, hat_pins.scl);
    }

    // UnitHeart on the board's default I2C: NessoN1 -> SoftwareI2C (GROVE port_b), others -> Wire (port_a)
    const bool unit_ready = m5::unit::wiring::addI2C(Units, unit);

    if (!hat_ready || !unit_ready || !Units.begin()) {
        M5_LOGE("Failed to begin %u/%u", hat_ready, unit_ready);
        M5_LOGE("%s", Units.debugInfo().c_str());
        m5::unit::wiring::failStop();
    }

    M5.Log.printf("M5UnitUnified initialized\n");
    M5.Log.printf("%s\n", Units.debugInfo().c_str());

    if (!pulse_unit.add(unit) || !pulse_hat.add(hat)) {
        M5_LOGE("Failed to add the PPG data sources");
        m5::unit::wiring::failStop();
    }

    view[0] = new View(lcd.width() >> 1, lcd.height(), true);
    view[1] = new View(lcd.width() >> 1, lcd.height(), false);
    restart();
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
        // Process the new samples before they are discarded below
        pulse_unit.update();
        if (unit.overflow()) {
            M5_LOGW("OVERFLOW U:%u", unit.overflow());
        }
        while (unit.available()) {
            const auto ir  = unit.ir();
            const auto red = unit.red();
            plot[0].add(static_cast<float>(ir));
            if (serial_log) {
                M5.Log.printf(">UIR:%u\n>URED:%u\n", static_cast<uint32_t>(ir), static_cast<uint32_t>(red));
            }
            unit.discard();
        }
        // PI is shown only while the heart rate is valid
        view[0]->update(pulse_unit.bpm(), pulse_unit.spo2(),
                        pulse_unit.hasBpm() ? pulse_unit.perfusionIndex() : std::numeric_limits<float>::quiet_NaN(),
                        pulse_unit.isBeat());
        if (serial_log) {
            M5.Log.printf(">UBPM:%f\n>USpO2:%f\n>UPI:%f\n>UBEAT:%u\n", pulse_unit.bpm(), pulse_unit.spo2(),
                          pulse_unit.perfusionIndex(), pulse_unit.isBeat());
        }
    }

    if (hat.updated()) {
        // Process the new samples before they are discarded below
        pulse_hat.update();
        if (hat.overflow()) {
            M5_LOGW("OVERFLOW H:%u", hat.overflow());
        }
        while (hat.available()) {
            const auto ir  = hat.ir();
            const auto red = hat.red();
            plot[1].add(static_cast<float>(ir));
            if (serial_log) {
                M5.Log.printf(">HIR:%u\n>HRED:%u\n", static_cast<uint32_t>(ir), static_cast<uint32_t>(red));
            }
            hat.discard();
        }
        view[1]->update(pulse_hat.bpm(), pulse_hat.spo2(),
                        pulse_hat.hasBpm() ? pulse_hat.perfusionIndex() : std::numeric_limits<float>::quiet_NaN(),
                        pulse_hat.isBeat());
        if (serial_log) {
            M5.Log.printf(">HBPM:%f\n>HSpO2:%f\n>HPI:%f\n>HBEAT:%u\n", pulse_hat.bpm(), pulse_hat.spo2(),
                          pulse_hat.perfusionIndex(), pulse_hat.isBeat());
        }
    }

    // Draw one queued point of each unit per 20 ms (more if the queue is backed up)
    if (!is_epd_panel && m5::utility::hasElapsed(plot_latest, plot_interval_ms)) {
        plot_latest = m5::utility::millis();
        if (plot[0].draw(view[0])) {
            view[0]->render();
            view[0]->push(&lcd, lcd.width() >> 1, 0);
        }
        if (plot[1].draw(view[1])) {
            view[1]->render();
            view[1]->push(&lcd, 0, 0);
        }
    }

    if (M5.BtnA.wasClicked()) {
        restart();  // Restart the processing
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
