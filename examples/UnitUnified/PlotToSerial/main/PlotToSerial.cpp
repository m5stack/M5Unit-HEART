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
#include <Wire.h>
#include <M5HAL.hpp>  // For NessoN1

// *************************************************************
// Choose one define symbol to match the unit you are using
// *************************************************************
#if !defined(USING_UNIT_HEART) && !defined(USING_HAT_HEART)
// #define USING_UNIT_HEART
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

struct I2cPins {
    int sda;
    int scl;
};

I2cPins get_hat_i2c_pins(const m5::board_t board)
{
    switch (board) {
        case m5::board_t::board_M5StickC:
        case m5::board_t::board_M5StickCPlus:
        case m5::board_t::board_M5StickCPlus2:
            return {0, 26};
        case m5::board_t::board_M5StickS3:
            return {8, 0};
        case m5::board_t::board_M5StackCoreInk:
            return {25, 26};
        case m5::board_t::board_ArduinoNessoN1:
            return {6, 7};
        default:
            return {-1, -1};
    }
}
#endif

}  // namespace

void setup()
{
    auto m5cfg = M5.config();
#if defined(USING_HAT_HEART)
    m5cfg.pmic_button  = false;  // Disable BtnPWR
    m5cfg.internal_imu = false;  // Disable internal IMU
    m5cfg.internal_rtc = false;  // Disable internal RTC
#endif

    M5.begin(m5cfg);
    M5.setTouchButtonHeightByRatio(100);

    // The screen shall be in landscape mode
    if (lcd.height() > lcd.width()) {
        lcd.setRotation(1);
    }

    auto board = M5.getBoard();

#if defined(USING_HAT_HEART)
    const auto pins = get_hat_i2c_pins(board);
    M5_LOGI("getHatPin: SDA:%u SCL:%u", pins.sda, pins.scl);
    if (pins.sda < 0 || pins.scl < 0) {
        M5_LOGE("Illegal pin number");
        lcd.clear(TFT_RED);
        while (true) {
            m5::utility::delay(10000);
        }
    }

    // Setup required to use HatHEART
    pinMode(pins.scl, OUTPUT);

    // Using MultiLED mode
    // In MultiLED mode, you need to set and start them yourself
    if (using_multi_led_mode) {
        auto cfg           = unit.config();
        cfg.start_periodic = false;  // Ignore auto start
        unit.config(cfg);
    }
    auto& wire = (board == m5::board_t::board_ArduinoNessoN1) ? Wire1 : Wire;
    wire.end();
    wire.begin(pins.sda, pins.scl, 400 * 1000U);
    if (!Units.add(unit, wire) || !Units.begin()) {
        M5_LOGE("Failed to begin");
        lcd.clear(TFT_RED);
        while (true) {
            m5::utility::delay(10000);
        }
    }
#else
    auto pin_num_sda = M5.getPin(m5::pin_name_t::port_a_sda);
    auto pin_num_scl = M5.getPin(m5::pin_name_t::port_a_scl);
    // For NessoN1 GROVE
    if (board == m5::board_t::board_ArduinoNessoN1) {
        // Port A of the NessoN1 is QWIIC, then use portB (GROVE)
        pin_num_sda = M5.getPin(m5::pin_name_t::port_b_out);
        pin_num_scl = M5.getPin(m5::pin_name_t::port_b_in);
        M5_LOGI("getPin(NessoN1): SDA:%u SCL:%u", pin_num_sda, pin_num_scl);
        // Wire is used internally, so SoftwareI2C handles the unit
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
    } else {
        // Using TwoWire
        M5_LOGI("getPin: SDA:%u SCL:%u", pin_num_sda, pin_num_scl);
        Wire.end();
        Wire.begin(pin_num_sda, pin_num_scl, 400 * 1000U);
        if (!Units.add(unit, Wire) || !Units.begin()) {
            M5_LOGE("Failed to begin");
            lcd.clear(TFT_RED);
            while (true) {
                m5::utility::delay(10000);
            }
        }
    }
#endif

    M5_LOGI("M5UnitUnified has been begun");
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
