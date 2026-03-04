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
bool is_epd_panel{};

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
    is_epd_panel = lcd.isEPD();

    const auto board_type = M5.getBoard();

    // The screen shall be in landscape mode
    if (lcd.height() > lcd.width()) {
        lcd.setRotation(1);
    }

    const auto pins = get_hat_i2c_pins(board_type);
    M5_LOGI("getHatPin: SDA:%u SCL:%u", pins.sda, pins.scl);
    if (pins.sda < 0 || pins.scl < 0) {
        M5_LOGE("Hat port not exists");
        lcd.fillScreen(TFT_RED);
        while (true) {
            m5::utility::delay(10000);
        }
    }

    // Setup required to use HatHEART
    pinMode(pins.scl, OUTPUT);

    // HatHeart settings
    bool hat_ready{};
    if (board_type == m5::board_t::board_ArduinoNessoN1) {
        // Using wire1
        Wire1.end();
        Wire1.begin(pins.sda, pins.scl, 400 * 1000U);
        hat_ready = Units.add(hat, Wire1);
    } else {
        // SoftwareI2C for Hat
        m5::hal::bus::I2CBusConfig hat_i2c_cfg;
        hat_i2c_cfg.pin_sda = m5::hal::gpio::getPin(pins.sda);
        hat_i2c_cfg.pin_scl = m5::hal::gpio::getPin(pins.scl);
        auto hat_i2c_bus    = m5::hal::bus::i2c::getBus(hat_i2c_cfg);
        hat_ready           = Units.add(hat, hat_i2c_bus ? hat_i2c_bus.value() : nullptr);
    }

    // UnitHeart settings
    auto pin_num_sda = M5.getPin(m5::pin_name_t::port_a_sda);
    auto pin_num_scl = M5.getPin(m5::pin_name_t::port_a_scl);
    bool unit_ready{};
    if (board_type == m5::board_t::board_ArduinoNessoN1) {
        // For NessoN1 GROVE
        // Wire is used internally, so SoftwareI2C handles the unit
        pin_num_sda = M5.getPin(m5::pin_name_t::port_b_out);
        pin_num_scl = M5.getPin(m5::pin_name_t::port_b_in);
        M5_LOGI("getPin(NessoN1): SDA:%u SCL:%u", pin_num_sda, pin_num_scl);
        m5::hal::bus::I2CBusConfig i2c_cfg;
        i2c_cfg.pin_sda = m5::hal::gpio::getPin(pin_num_sda);
        i2c_cfg.pin_scl = m5::hal::gpio::getPin(pin_num_scl);
        auto i2c_bus    = m5::hal::bus::i2c::getBus(i2c_cfg);
        M5_LOGI("Bus:%d", i2c_bus.has_value());
        unit_ready = Units.add(unit, i2c_bus ? i2c_bus.value() : nullptr);
    } else {
        // UnitHeart on Wire
        M5_LOGI("getPin: SDA:%u SCL:%u", pin_num_sda, pin_num_scl);
        Wire.end();
        Wire.begin(pin_num_sda, pin_num_scl, 400 * 1000U);
        unit_ready = Units.add(unit, Wire);
    }

    if (!hat_ready || !unit_ready || !Units.begin()) {
        M5_LOGE("Failed to begin %u/%u", hat_ready, unit_ready);
        M5_LOGE("%s", Units.debugInfo().c_str());
        lcd.fillScreen(TFT_RED);
        while (true) {
            m5::utility::delay(10000);
        }
    }

    M5_LOGI("M5UnitUnified has been begun");
    M5_LOGI("%s", Units.debugInfo().c_str());

    view[0] = new View(lcd.width() >> 1, lcd.height(), true);
    view[1] = new View(lcd.width() >> 1, lcd.height(), false);
    view[0]->_monitor.setSamplingRate(unit.calculateSamplingRate());
    view[1]->_monitor.setSamplingRate(hat.calculateSamplingRate());
    if (!is_epd_panel) {
        view[0]->push(&lcd, lcd.width() >> 1, 0);
        view[1]->push(&lcd, 0, 0);
    }
}

void loop()
{
    M5.update();
    Units.update();

    if (!is_epd_panel) {
        lcd.startWrite();
    }
    if (unit.updated()) {
        if (unit.overflow()) {
            M5_LOGW("OVERFLOW U:%u", unit.overflow());
        }
        bool beat{};
        while (unit.available()) {
            const auto ir  = unit.ir();
            const auto red = unit.red();
            view[0]->push_back(ir, red);
            view[0]->update();
            if (is_epd_panel) {
                M5.Log.printf(">UIR:%u\n>URED:%u\n", static_cast<uint32_t>(ir), static_cast<uint32_t>(red));
                M5.Log.printf(">UMIR:%f\n", view[0]->_monitor.latestIR());
            }
            beat |= view[0]->_monitor.isBeat();
            unit.discard();
        }
        if (is_epd_panel) {
            M5.Log.printf(">UBPM:%f\n>USpO2:%f\n>UBEAT:%u\n", view[0]->_monitor.bpm(), view[0]->_monitor.SpO2(), beat);
        }
        if (!is_epd_panel) {
            view[0]->render();
            view[0]->push(&lcd, lcd.width() >> 1, 0);
        }
    }

    if (hat.updated()) {
        if (hat.overflow()) {
            M5_LOGW("OVERFLOW H:%u", hat.overflow());
        }
        bool beat{};
        while (hat.available()) {
            const auto ir  = hat.ir();
            const auto red = hat.red();
            view[1]->push_back(ir, red);
            view[1]->update();
            if (is_epd_panel) {
                M5.Log.printf(">HIR:%u\n>HRED:%u\n", static_cast<uint32_t>(ir), static_cast<uint32_t>(red));
                M5.Log.printf(">HMIR:%f\n", view[1]->_monitor.latestIR());
            }
            beat |= view[1]->_monitor.isBeat();
            hat.discard();
        }
        if (is_epd_panel) {
            M5.Log.printf(">HBPM:%f\n>HSpO2:%f\n>HBEAT:%u\n", view[1]->_monitor.bpm(), view[1]->_monitor.SpO2(), beat);
        }
        if (!is_epd_panel) {
            view[1]->render();
            view[1]->push(&lcd, 0, 0);
        }
    }

    if (M5.BtnA.wasClicked()) {
        view[0]->clear();
        view[1]->clear();
    }
    if (!is_epd_panel) {
        lcd.endWrite();
    }
}
