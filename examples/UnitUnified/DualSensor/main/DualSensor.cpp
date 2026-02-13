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
#include <Wire.h>
#include <M5HAL.hpp>
#include "../src/view.hpp"

namespace {
auto& lcd = M5.Display;
m5::unit::UnitUnified Units;
m5::unit::UnitHeart unit;
m5::unit::HatHeart hat;

View* view[2]{};

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

}  // namespace

using namespace m5::unit::max30102;

void setup()
{
    // Configuration for using Wire1
    auto m5cfg         = M5.config();
    m5cfg.pmic_button  = false;  // Disable BtnPWR
    m5cfg.internal_imu = false;  // Disable internal IMU
    m5cfg.internal_rtc = false;  // Disable internal RTC
    M5.begin(m5cfg);
    M5.setTouchButtonHeightByRatio(100);

    const auto board_type = M5.getBoard();

    const auto pins = get_hat_i2c_pins(board_type);
    M5_LOGI("getHatPin: SDA:%u SCL:%u", pins.sda, pins.scl);
    if (pins.sda < 0 || pins.scl < 0) {
        M5_LOGE("HatHeart requires Wire1-capable boards");
        lcd.clear(TFT_RED);
        while (true) {
            m5::utility::delay(10000);
        }
    }

    // The screen shall be in landscape mode
    if (lcd.height() > lcd.width()) {
        lcd.setRotation(1);
    }

    // Setup required to use HatHEART
    pinMode(pins.scl, OUTPUT);

    // HatHeart on Wire1
    Wire1.end();
    Wire1.begin(pins.sda, pins.scl, 400 * 1000U);

    // UnitHeart wire settings
    auto pin_num_sda = M5.getPin(m5::pin_name_t::port_a_sda);
    auto pin_num_scl = M5.getPin(m5::pin_name_t::port_a_scl);
    // For NessoN1 GROVE
    if (board_type == m5::board_t::board_ArduinoNessoN1) {
        // Wire is used internally, so SoftwareI2C handles the unit
        pin_num_sda = M5.getPin(m5::pin_name_t::port_b_out);
        pin_num_scl = M5.getPin(m5::pin_name_t::port_b_in);
        M5_LOGI("getPin(NessoN1): SDA:%u SCL:%u", pin_num_sda, pin_num_scl);
        m5::hal::bus::I2CBusConfig i2c_cfg;
        i2c_cfg.pin_sda = m5::hal::gpio::getPin(pin_num_sda);
        i2c_cfg.pin_scl = m5::hal::gpio::getPin(pin_num_scl);
        auto i2c_bus    = m5::hal::bus::i2c::getBus(i2c_cfg);
        M5_LOGI("Bus:%d", i2c_bus.has_value());
        if (!Units.add(unit, i2c_bus ? i2c_bus.value() : nullptr) || !Units.add(hat, Wire1) || !Units.begin()) {
            M5_LOGE("Failed to begin");
            lcd.clear(TFT_RED);
            while (true) {
                m5::utility::delay(10000);
            }
        }
    } else {
        // UnitHeart on Wire
        M5_LOGI("getPin: SDA:%u SCL:%u", pin_num_sda, pin_num_scl);
        Wire.end();
        Wire.begin(pin_num_sda, pin_num_scl, 400 * 1000U);
        if (!Units.add(unit, Wire) || !Units.add(hat, Wire1) || !Units.begin()) {
            M5_LOGE("Failed to begin");
            lcd.clear(TFT_RED);
            while (true) {
                m5::utility::delay(10000);
            }
        }
    }

    M5_LOGI("M5UnitUnified has been begun");
    M5_LOGI("%s", Units.debugInfo().c_str());

    lcd.startWrite();

    view[0] = new View(lcd.width() >> 1, lcd.height(), true);
    view[1] = new View(lcd.width() >> 1, lcd.height(), false);
    view[0]->_monitor.setSamplingRate(unit.calculateSamplingRate());
    view[1]->_monitor.setSamplingRate(hat.calculateSamplingRate());
    view[0]->push(&lcd, lcd.width() >> 1, 0);
    view[1]->push(&lcd, 0, 0);
}

void loop()
{
    M5.update();
    Units.update();

    if (unit.updated()) {
        if (unit.overflow()) {
            M5_LOGW("OVERFLOW U:%u", unit.overflow());
        }
        while (unit.available()) {
            view[0]->push_back(unit.ir(), unit.red());
            view[0]->update();
            unit.discard();
        }
        view[0]->render();
        view[0]->push(&lcd, lcd.width() >> 1, 0);
    }

    if (hat.updated()) {
        if (hat.overflow()) {
            M5_LOGW("OVERFLOW H:%u", hat.overflow());
        }
        while (hat.available()) {
            view[1]->push_back(hat.ir(), hat.red());
            view[1]->update();
            hat.discard();
        }
        view[1]->render();
        view[1]->push(&lcd, 0, 0);
    }

    if (M5.BtnA.wasClicked()) {
        view[0]->clear();
        view[1]->clear();
    }
}
