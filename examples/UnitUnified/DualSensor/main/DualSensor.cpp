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
#include "../src/view.hpp"

namespace {
auto& lcd = M5.Display;
m5::unit::UnitUnified Units;
m5::unit::UnitHeart unit;
m5::unit::HatHeart hat;

View* view[2]{};

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

    auto board = M5.getBoard();
    if (board != m5::board_t::board_M5StickCPlus && board != m5::board_t::board_M5StickCPlus2) {
        M5_LOGE("Example for StickCPlus/CPlus2");
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
    pinMode(25, INPUT_PULLUP);
    pinMode(26, OUTPUT);

    // Wire settings
    auto pin_num_sda = M5.getPin(m5::pin_name_t::port_a_sda);
    auto pin_num_scl = M5.getPin(m5::pin_name_t::port_a_scl);
    Wire.end();
    Wire.begin(pin_num_sda, pin_num_scl, 400 * 1000U);

    Wire1.end();
    Wire1.begin(0, 26, 400 * 1000U);

    // UnitHeart connected to GROOVE with Wire
    // HatHeart connected to PIN sockect with Wire1
    if (!Units.add(unit, Wire) || !Units.add(hat, Wire1) || !Units.begin()) {
        M5_LOGE("Failed to begin");
        lcd.clear(TFT_RED);
        while (true) {
            m5::utility::delay(10000);
        }
    }

    M5_LOGI("M5UnitUnified has been begun");
    M5_LOGI("%s", Units.debugInfo().c_str());

    lcd.startWrite();

    view[0] = new View(lcd.width() >> 1, lcd.height(), true);
    view[1] = new View(lcd.width() >> 1, lcd.height(), false);
    view[0]->_monitor.setSamplingRate(unit.caluculateSamplingRate());
    view[1]->_monitor.setSamplingRate(hat.caluculateSamplingRate());
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
