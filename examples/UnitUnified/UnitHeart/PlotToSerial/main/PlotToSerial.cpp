/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Example using M5UnitUnified for UnitHeart
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
m5::unit::UnitHeart unit;
m5::heart::PulseMonitor monitor;

}  // namespace

using namespace m5::unit::max30100;

void setup()
{
    M5.begin();

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

    monitor.setSamplingRate(unit.caluculateSamplingRate());

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
            M5_LOGW("OVERFLOW:%u", unit.overflow());
        }

        bool beat{};
        // MAX30100 is equipped with a FIFO, so multiple data may be stored
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

    // Measure tempeature
    if (M5.BtnA.wasClicked()) {
        TemperatureData td{};
        if (unit.measureTemperatureSingleshot(td)) {
            M5.Log.printf(">Temp:%f\n", td.celsius());
        }
    }
}
