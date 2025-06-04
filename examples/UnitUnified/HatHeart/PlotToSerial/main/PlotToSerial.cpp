/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Example using M5UnitUnified for HatHeart
*/
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedHEART.h>
#include <Wire.h>

namespace {
auto& lcd = M5.Display;
m5::unit::UnitUnified Units;
m5::unit::HatHeart hat;
m5::heart::PulseMonitor monitor{};

constexpr bool using_multi_led_mode{false};  // Using multiLED mode if true
constexpr bool using_wire1{false};           // Using Wire1 if true

}  // namespace

using namespace m5::unit::max30102;

void setup()
{
    // Configuration if using Wire1
    auto m5cfg = M5.config();
    if (using_wire1) {
        m5cfg.pmic_button  = false;  // Disable BtnPWR
        m5cfg.internal_imu = false;  // Disable internal IMU
        m5cfg.internal_rtc = false;  // Disable internal RTC
    }
    M5.begin(m5cfg);

    auto board = M5.getBoard();
    if (board != m5::board_t::board_M5StickCPlus && board != m5::board_t::board_M5StickCPlus2) {
        M5_LOGE("HatHeart for StickCPlus/CPlus2");
        lcd.clear(TFT_RED);
        while (true) {
            m5::utility::delay(10000);
        }
    }

    // Setup required to use HatHEART
    pinMode(25, INPUT_PULLUP);
    pinMode(26, OUTPUT);

    // Using MultiLED mode
    // In MultiLED mode, you need to set and start them yourself
    if (using_multi_led_mode) {
        auto cfg           = hat.config();
        cfg.start_periodic = false;  // Ignore auto start
        hat.config(cfg);
    }

    // Using TwoWire
    if (using_wire1) {
        Wire1.end();
        Wire1.begin(0, 26, 400 * 1000U);
        if (!Units.add(hat, Wire1) || !Units.begin()) {
            M5_LOGE("Failed to begin");
            lcd.clear(TFT_RED);
            while (true) {
                m5::utility::delay(10000);
            }
        }
    } else {
        Wire.end();
        Wire.begin(0, 26, 400 * 1000U);
        if (!Units.add(hat, Wire) || !Units.begin()) {
            M5_LOGE("Failed to begin");
            lcd.clear(TFT_RED);
            while (true) {
                m5::utility::delay(10000);
            }
        }
    }

    M5_LOGI("M5UnitUnified has been begun");
    M5_LOGI("%s", Units.debugInfo().c_str());

    // In MultiLED mode, you need to set and start them yourself
    if (using_multi_led_mode) {
        hat.writeMode(Mode::MultiLED);
        hat.writeSpO2Configuration(ADC::Range4096nA, Sampling::Rate400, LEDPulse::Width411);
        hat.writeFIFOConfiguration(FIFOSampling::Average4, true, 15);
        // hat.writeMultiLEDModeControl(Slot::Red, Slot::IR); // (A)
        hat.writeMultiLEDModeControl(Slot::IR, Slot::Red);  // (B)
        hat.writeLEDCurrent(0, 0x1F);                       // Red if (A), IR  if (B)
        hat.writeLEDCurrent(1, 0x1F);                       // IR  if (A), Red if (B)
        hat.startPeriodicMeasurement();
    }
    lcd.clear(TFT_DARKGREEN);

    monitor.setSamplingRate(hat.caluculateSamplingRate());
}

void loop()
{
    M5.update();
    Units.update();

    if (hat.updated()) {
        // WARNING
        // If overflow is occurring, the sampling rate should be reduced because the processing is not up to par
        if (hat.overflow()) {
            M5_LOGW("OVERFLOW:%u", hat.overflow());
        }

        bool beat{};
        // MAX30102 is equipped with a FIFO, so multiple data may be stored
        while (hat.available()) {
            M5.Log.printf(">IR:%u\n>RED:%u\n", hat.ir(), hat.red());
            monitor.push_back(hat.ir(), hat.red());  // Push back the oldest data
            M5.Log.printf(">MIR:%f\n", monitor.latestIR());
            monitor.update();
            beat |= monitor.isBeat();
            hat.discard();  // Discard the oldest data
        }
        M5.Log.printf(">BPM:%f\n>SpO2:%f\n>BEAT:%u\n", monitor.bpm(), monitor.SpO2(), beat);
    }

    // Measure tempeature
    if (M5.BtnA.wasClicked()) {
        TemperatureData td{};
        if (hat.measureTemperatureSingleshot(td)) {
            M5.Log.printf(">Temp:%f\n", td.celsius());
        }
    }
}
