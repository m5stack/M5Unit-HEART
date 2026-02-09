/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Graphical meter example for UnitHeart / HatHeart
  The core must be equipped with LCD
*/
// #define USING_M5HAL  // When using M5HAL (UnitHeart only)

#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedHEART.h>
#include <Wire.h>
#include <M5HAL.hpp>
#include "../src/view.hpp"

// *************************************************************
// Choose one define symbol to match the unit you are using
// *************************************************************
#if !defined(USING_UNIT_HEART) && !defined(USING_HAT_HEART)
// #define USING_UNIT_HEART
//  #define USING_HAT_HEART
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
m5::unit::UnitHeart heart;
#elif defined(USING_HAT_HEART)
m5::unit::HatHeart heart;
#endif

View* view{};

#if defined(USING_HAT_HEART)
struct I2cPins {
    int sda;
    int scl;
    bool use_wire1;
};

I2cPins get_hat_i2c_pins(const m5::board_t board)
{
    switch (board) {
        case m5::board_t::board_M5StickC:
        case m5::board_t::board_M5StickCPlus:
        case m5::board_t::board_M5StickCPlus2:
            return {0, 26, true};
        case m5::board_t::board_M5StickS3:
            return {8, 0, true};
        case m5::board_t::board_M5StackCoreInk:
            return {25, 26, true};
        case m5::board_t::board_ArduinoNessoN1:
            return {6, 7, true};
        default:
            return {-1, -1, false};
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

    const auto board = M5.getBoard();

    // The screen shall be in landscape mode
    if (lcd.height() > lcd.width()) {
        lcd.setRotation(1);
    }

#if defined(USING_HAT_HEART)
    const auto pins = get_hat_i2c_pins(board);
    M5_LOGI("getHatPin: SDA:%u SCL:%u %s", pins.sda, pins.scl, pins.use_wire1 ? "Wire1" : "Wire");
    if (pins.sda < 0 || pins.scl < 0) {
        M5_LOGE("Illegal pin number");
        lcd.clear(TFT_RED);
        while (true) {
            m5::utility::delay(10000);
        }
    }

    // Setup required to use HatHEART
    pinMode(pins.scl, OUTPUT);

    TwoWire& wire = pins.use_wire1 ? Wire1 : Wire;
    wire.end();
    wire.begin(pins.sda, pins.scl, 400 * 1000U);
    if (!Units.add(heart, wire) || !Units.begin()) {
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
        // Wire is used internally, so SoftwareI2C handles the unit
        pin_num_sda = M5.getPin(m5::pin_name_t::port_b_out);
        pin_num_scl = M5.getPin(m5::pin_name_t::port_b_in);
        M5_LOGI("getPin(NessoN1): SDA:%u SCL:%u", pin_num_sda, pin_num_scl);
        m5::hal::bus::I2CBusConfig i2c_cfg;
        i2c_cfg.pin_sda = m5::hal::gpio::getPin(pin_num_sda);
        i2c_cfg.pin_scl = m5::hal::gpio::getPin(pin_num_scl);
        auto i2c_bus    = m5::hal::bus::i2c::getBus(i2c_cfg);
        M5_LOGI("Bus:%d", i2c_bus.has_value());
        if (!Units.add(heart, i2c_bus ? i2c_bus.value() : nullptr) || !Units.begin()) {
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
        Wire.begin(pin_num_sda, pin_num_scl, 400000U);
        if (!Units.add(heart, Wire) || !Units.begin()) {
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

    lcd.clear(0);

#if defined(USING_UNIT_HEART)
    view = new View(lcd.width(), lcd.height(), true);
#else
    view = new View(lcd.width(), lcd.height(), false);
#endif
    view->_monitor.setSamplingRate(heart.caluculateSamplingRate());
    view->push(&lcd, 0, 0);

    M5_LOGI("periodic:%d", heart.inPeriodic());
}

void loop()
{
    M5.update();
    Units.update();

    if (heart.updated()) {
        if (heart.overflow()) {
            M5_LOGW("OVERFLOW:%u", heart.overflow());
        }
        while (heart.available()) {
            view->push_back(heart.ir(), heart.red());
            view->update();
            heart.discard();
        }
        view->render();
        view->push(&lcd, 0, 0);
    }

    if (M5.BtnA.wasClicked()) {
        view->clear();
    }
}
