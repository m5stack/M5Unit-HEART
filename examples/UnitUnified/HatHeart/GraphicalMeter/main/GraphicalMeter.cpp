/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Graphical meter example for HatHeart
*/
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedHEART.h>
#include <Wire.h>
#include "../src/view.hpp"

namespace {
auto& lcd = M5.Display;
m5::unit::UnitUnified Units;
m5::unit::HatHeart hat;
m5::heart::PulseMonitor monitor{};

constexpr bool using_wire1{false};  // Using Wire1 if true

View* view{};

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

    // The screen shall be in landscape mode
    if (lcd.height() > lcd.width()) {
        lcd.setRotation(1);
    }

    // Setup required to use HatHEART
    pinMode(25, INPUT_PULLUP);
    pinMode(26, OUTPUT);

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

    lcd.clear(0);

    view = new View(lcd.width(), lcd.height(), false);
    view->_monitor.setSamplingRate(hat.caluculateSamplingRate());
    view->push(&lcd, 0, 0);
}

void loop()
{
    M5.update();
    Units.update();

    if (hat.updated()) {
        if (hat.overflow()) {
            M5_LOGW("OVERFLOW:%u", hat.overflow());
        }
        while (hat.available()) {
            view->push_back(hat.ir(), hat.red());
            view->update();
            hat.discard();
        }
        view->render();
        view->push(&lcd, 0, 0);
    }

    if (M5.BtnA.wasClicked()) {
        view->clear();
    }
}
