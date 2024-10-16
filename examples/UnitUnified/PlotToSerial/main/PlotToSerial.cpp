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
#if !defined(USING_M5HAL)
#include <Wire.h>
#endif

namespace {
auto& lcd = M5.Display;
m5::unit::UnitUnified Units;
m5::unit::UnitHEART unit;
m5::heart::PulseMonitor monitor(100.f);

}  // namespace

void setup()
{
    M5.begin();

    // Another settings
    if (0) {
        auto cfg          = unit.config();
        cfg.sampling_rate = m5::unit::max30100::Sampling::Rate200;
        cfg.pulse_width   = m5::unit::max30100::LedPulseWidth::PW200;
        cfg.ir_current    = m5::unit::max30100::CurrentControl::mA7_6;
        cfg.red_current   = m5::unit::max30100::CurrentControl::mA7_6;
        auto sr           = m5::unit::max30100::getSamplingRate(cfg.sampling_rate);
        monitor.setSamplingRate(sr);
        unit.config(cfg);
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
    if (!Units.add(unit, i2c_bus ? i2c_bus.value() : nullptr) || !Units.begin()) {
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
    if (!Units.add(unit, Wire) || !Units.begin()) {
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

void loop()
{
    M5.update();
    Units.update();
    if (unit.updated()) {
        // WARNING
        // If overflow is occurring, the sampling rate should be reduced because the processing is not up to par
        if (unit.overflow()) {
            M5_LOGW("\n>OVERFLOW:%u", unit.overflow());
        }

        bool beat{};
        // MAX30100 is equipped with a FIFO, so multiple data may be stored
        while (unit.available()) {
            monitor.push_back(unit.ir(), unit.red());  // Push back the oldest data
            //M5_LOGI("\n>MIR:%f\n>MRED:%f", monitor.latestIR(), monitor.latestRED());
            monitor.update();
            beat = monitor.isBeat();
            unit.discard();  // Discard the oldest data
        }
        M5_LOGI("\n>BPM:%f\n>SpO2:%f\n>BEAT:%u", monitor.bpm(), monitor.SpO2(), beat);
    }

    // Measure tempeature
    if (M5.BtnA.wasClicked()) {
        m5::unit::max30100::TemperatureData td{};
        if (unit.measureTemperatureSingleshot(td)) {
            M5_LOGI("\n>Temp:%f", td.celsius());
        }
    }
}
