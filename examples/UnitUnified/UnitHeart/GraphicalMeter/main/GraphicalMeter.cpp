/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Graphical meter example for UnitHeart
  The core must be equipped with LCD
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
View* view{};

}  // namespace

using namespace m5::unit::max30102;

void setup()
{
    M5.begin();

    // The screen shall be in landscape mode
    if (lcd.height() > lcd.width()) {
        lcd.setRotation(1);
    }

    auto pin_num_sda = M5.getPin(m5::pin_name_t::port_a_sda);
    auto pin_num_scl = M5.getPin(m5::pin_name_t::port_a_scl);
    M5_LOGI("getPin: SDA:%u SCL:%u", pin_num_sda, pin_num_scl);

    Wire.end();
    Wire.begin(pin_num_sda, pin_num_scl, 400000U);
    if (!Units.add(unit, Wire) || !Units.begin()) {
        M5_LOGE("Failed to begin");
        lcd.clear(TFT_RED);
        while (true) {
            m5::utility::delay(10000);
        }
    }

    M5_LOGI("M5UnitUnified has been begun");
    M5_LOGI("%s", Units.debugInfo().c_str());

    lcd.clear(0);

    view = new View(lcd.width(), lcd.height());
    view->_monitor.setSamplingRate(unit.caluculateSamplingRate());
    view->push(&lcd, 0, 0);
}

void loop()
{
    M5.update();
    auto touch = M5.Touch.getDetail();

    Units.update();

    if (unit.updated()) {
        if (unit.overflow()) {
            M5_LOGW("OVERFLOW:%u", unit.overflow());
        }
        while (unit.available()) {
            view->push_back(unit.ir(), unit.red());
            view->update();
            unit.discard();
        }
        view->render();
        view->push(&lcd, 0, 0);
    }

    if (M5.BtnA.wasClicked() || touch.wasClicked()) {
        view->clear();
    }
}
