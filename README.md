# M5Unit - HEART

## Overview

Library for UnitHeart, HatHeart and StampHeart using [M5UnitUnified](https://github.com/m5stack/M5UnitUnified).  
M5UnitUnified is a library for unified handling of various M5 units products.

### SKU:U029
Unit Heart is a blood oxygen and heart rate sensor. Integrated MAX30100, it offers a complete pulse oximeter and heart rate sensor system solution. This is a non-invasive blood oxygen and heart rate sensor, integrating two infrared light-emitting diodes and a photodetector. The detection principle is to use infrared LEDs to illuminate and detect the proportion of red blood cells carrying oxygen versus those not carrying oxygen, thereby obtaining the oxygen content.

### SKU:U118
Hat Heart is a blood oxygen and heart rate sensor. It integrates MAX30102, providing a complete pulse oximeter and heart rate sensor system solution. This is a non-invasive blood oxygen and heart rate sensor. Its detection principle is based on infrared LED light illumination, which detects the ratio of oxygenated to deoxygenated red blood cells to determine blood oxygen levels. The sensor uses an I2C communication interface and integrates infrared LEDs, photodetectors, optical components, and low-noise electronics, with some ambient light suppression for more accurate measurements.

### SKU:TBD (StampHeart)
TBD


## Related Link

- [Unit Heart & Datasheet](https://docs.m5stack.com/en/unit/heart)
- [Hat Heart & Datasheet](https://docs.m5stack.com/en/hat/hat_heart_rate)
- [Stamp Heart & Datasheet](https://docs.m5stack.com/en/products/sku/TBD)

## Required Libraries:

- [M5UnitUnified](https://github.com/m5stack/M5UnitUnified)
- [M5Utility](https://github.com/m5stack/M5Utility)
- [M5HAL](https://github.com/m5stack/M5HAL)

## License

- [M5Unit-HEART - MIT](LICENSE)
- [Precompiled GH3038 core (src/\<chip\>/libm5gh3038core.a) - M5Stack proprietary](LICENSE-precompiled.md)


## Heart rate / SpO2 processing
`m5::heart::PulseController` calculates the heart rate, SpO2 and perfusion index from any of the units above.
The sampling rate and the LED channels are taken from the unit, so the same code works for UnitHeart, HatHeart and StampHeart.

```cpp
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedHEART.h>

m5::unit::UnitUnified Units;
m5::unit::StampHeart unit;  // or UnitHeart / HatHeart
m5::heart::PulseController pulse;

void setup()
{
    M5.begin();
    // Connect the unit (see the examples for each board), then
    // Units.add(unit, Wire); Units.begin();
    pulse.add(unit);  // After the periodic measurement has started
}

void loop()
{
    M5.update();
    Units.update();
    if (unit.updated()) {
        pulse.update();  // Process the new samples before discarding them
        while (unit.available()) {
            unit.discard();
        }
        if (pulse.hasBpm()) {
            M5.Log.printf("BPM:%f\n", pulse.bpm());
        }
        if (pulse.hasSpO2()) {
            M5.Log.printf("SpO2:%f\n", pulse.spo2());
        }
    }
}
```

- The heart rate is detected on Green (StampHeart) or IR / Red with a 0.5-4 Hz band-pass filter and an adaptive peak detector.
- The values are NaN while they are not valid (no finger, weak or irregular pulse). Check `hasBpm()` / `hasSpO2()` before using them.
- To process recorded samples, use `m5::heart::PulseProcessor` and push the samples one by one.

> **WARNING:** SpO2 is calculated with a provisional conversion (110 - 25R) that is not calibrated for these sensors.  
> This library is not a medical device. Do not use the values for diagnosis or treatment.

### Migration from PulseMonitor
`m5::heart::PulseMonitor`, `EMA` and `Filter` have been removed. Use `m5::heart::PulseController` instead.

| PulseMonitor | PulseController |
|---|---|
| `setSamplingRate(unit.calculateSamplingRate())` | `add(unit)` |
| `push_back(ir, red)` and `update()` for each sample | `update()` once after `unit.updated()` (before `discard()`) |
| `bpm()` / `SpO2()` | `bpm()` / `spo2()` (NaN while not valid) |
| `isBeat()` | `isBeat()` |
| `latestIR()` | `pulseWave()` |

## Examples
See also [examples/UnitUnified](examples/UnitUnified)

- PlotToSerial: Outputs the raw values, heart rate, SpO2 and perfusion index for a serial plotter
- GraphicalMeter: Shows the pulse waveform, heart rate and SpO2 on the LCD. BtnA click restarts the processing, long press switches the sampling rate (50 / 100 / 200 Hz)
- DualSensor: Uses UnitHeart and HatHeart at the same time

### For ArduinoIDE settings
You must choose a define symbol for the unit you will use.
(Rewrite source or specify with compile options)

- PlotToSerial / GraphicalMeter
```cpp
// *************************************************************
// Choose one define symbol to match the unit you are using
// *************************************************************
#if !defined(USING_UNIT_HEART) && !defined(USING_HAT_HEART) && !defined(USING_STAMP_HEART)
// For UnitHeart (U029)
// #define USING_UNIT_HEART
// For HatHeart (U118)
// #define USING_HAT_HEART
// For StampHeart
// #define USING_STAMP_HEART
#endif
```

### For ESP-IDF settings

> **NOTE:** The ESP-IDF native build (`idf.py`) targets ESP-IDF **5.1 or later** (5.x and 6.x).  
> HatHeart on NessoN1 uses its HatPort, which is the ESP32-C6 LP I2C. This requires ESP-IDF **5.3 or later** for the ESP-IDF native build, and Arduino-ESP32 **3.2 or later** for Arduino.

On ESP-IDF native builds (`idf.py`), the unit is selected via Kconfig instead of editing the source `#define`. PlotToSerial / GraphicalMeter expose the same choice through `main/Kconfig.projbuild` (which sources `examples/UnitUnified/common/Kconfig.variant`), and `examples/UnitUnified/common/variant.cmake` maps the chosen `CONFIG_EXAMPLE_USING_*` to the source-level `USING_*` macro shared with the Arduino build.

Pick the variant with `menuconfig`:

```sh
cd examples/UnitUnified/PlotToSerial    # or GraphicalMeter
idf.py set-target esp32s3               # or esp32 / esp32c6 / esp32p4 / ...
idf.py menuconfig
# -> M5Unit-HEART example -> Target unit -> choose ONE:
#       UnitHeart (MAX30100, I2C / GROVE)
#       HatHeart (MAX30102, Hat / internal I2C)
#       StampHeart (GH3038, I2C / GROVE + INT/RST)
idf.py build flash monitor
```

DualSensor drives both UnitHeart and HatHeart at once, so it has no unit selection — just build it directly:

```sh
cd examples/UnitUnified/DualSensor
idf.py set-target esp32s3 build flash monitor
```

## Doxygen document
[GitHub Pages](https://m5stack.github.io/M5Unit-HEART/)

If you want to generate documents on your local machine, execute the following command

```
bash docs/doxy.sh
```

It will output it under docs/html  
If you want to output Git commit hashes to html, do it for the git cloned folder.

### Required
- [Doxygen](https://www.doxygen.nl/)
- [Git](https://git-scm.com/) (Output commit hash to html)
