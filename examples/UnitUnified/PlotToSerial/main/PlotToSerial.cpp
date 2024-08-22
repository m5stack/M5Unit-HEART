/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Example using M5UnitUnified for UnitHEART
*/

// #define USING_M5HAL  // When using M5HAL

#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedHEART.h>
#include <utility/heart_rate.hpp>
#if !defined(USING_M5HAL)
#include <Wire.h>
#endif

namespace {
auto& lcd = M5.Display;
m5::unit::UnitUnified Units;
m5::unit::UnitHEART unitHeart;
m5::max30100::HeartRate heartRate(100);

uint32_t getSamplingRate(const m5::unit::max30100::Sampling rate) {
    static constexpr uint32_t table[] = {50, 100, 167, 200, 400, 600, 800, 1000};
    return table[m5::stl::to_underlying(rate)];
}

}  // namespace

void setup() {
    M5.begin();

    // Another settings
    if (0) {
        auto cfg         = unitHeart.config();
        cfg.samplingRate = m5::unit::max30100::Sampling::Rate400;
        cfg.pulseWidth   = m5::unit::max30100::LedPulseWidth::PW400;
        cfg.irCurrent    = m5::unit::max30100::CurrentControl::mA7_6;
        cfg.redCurrent   = m5::unit::max30100::CurrentControl::mA7_6;
        heartRate.setSamplingRate(getSamplingRate(cfg.samplingRate));
        heartRate.setThreshold(25.0f);  // depends on ir/redCurrent
        unitHeart.config(cfg);
    }

    auto pin_num_sda = M5.getPin(m5::pin_name_t::port_a_sda);
    auto pin_num_scl = M5.getPin(m5::pin_name_t::port_a_scl);
    M5_LOGI("getPin: SDA:%u SCL:%u", pin_num_sda, pin_num_scl);

#if defined(USING_M5HAL)
#pragma message "Using M5HAL"
    // Using M5HAL
    m5::hal::bus::I2CBusConfig i2c_cfg;
    i2c_cfg.pin_sda = m5::hal::gpio::getPin(pin_num_sda);
    i2c_cfg.pin_scl = m5::hal::gpio::getPin(pin_num_scl);
    auto i2c_bus    = m5::hal::bus::i2c::getBus(i2c_cfg);
    M5_LOGI("Bus:%d", i2c_bus.has_value());
    if (!Units.add(unitHeart, i2c_bus ? i2c_bus.value() : nullptr) || !Units.begin()) {
        M5_LOGE("Failed to begin");
        lcd.clear(TFT_RED);
        while (true) {
            m5::utility::delay(10000);
        }
    }
#else
#pragma message "Using Wire"
    // Using TwoWire
    Wire.begin(pin_num_sda, pin_num_scl, 400000U);
    if (!Units.add(unitHeart, Wire) || !Units.begin()) {
        M5_LOGE("Failed to begin");
        lcd.clear(TFT_RED);
        while (true) {
            m5::utility::delay(10000);
        }
    }
#endif

    M5_LOGI("M5UnitUnified has been begun");
    M5_LOGI("%s", Units.debugInfo().c_str());

    lcd.clear(TFT_DARKGREEN);
}

void loop() {
    M5.update();
    Units.update();
    if (unitHeart.updated()) {
        while (unitHeart.available()) {
            // M5_LOGI("\n>IR:%u\n>RED:%u", unitHeart.ir(),
            // unitHeart.red());
            bool beat = heartRate.push_back((float)unitHeart.ir(), (float)unitHeart.red());
            if (beat) {
                M5_LOGI("Beat!");
            }

            unitHeart.discard();
        }
        auto bpm = heartRate.calculate();
        M5_LOGW("\n>HRR:%f\n>SpO2:%f", bpm, heartRate.SpO2());
    }

    // buffer clear and measure tempeature
    if (M5.BtnA.wasClicked()) {
        heartRate.clear();
        m5::unit::max30100::TemperatureData td{};
        if (unitHeart.measureTemperatureSingleshot(td)) {
            M5_LOGI("\n>Temp:%f", td.celsius());
        }
    }
}
