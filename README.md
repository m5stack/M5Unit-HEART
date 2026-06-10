# M5Unit - HEART

## Overview

Library for UnitHEART using [M5UnitUnified](https://github.com/m5stack/M5UnitUnified).  
M5UnitUnified is a library for unified handling of various M5 units products.

### SKU:U029
Unit Heart is a blood oxygen and heart rate sensor. Integrated MAX30100, it offers a complete pulse oximeter and heart rate sensor system solution. This is a non-invasive blood oxygen and heart rate sensor, integrating two infrared light-emitting diodes and a photodetector. The detection principle is to use infrared LEDs to illuminate and detect the proportion of red blood cells carrying oxygen versus those not carrying oxygen, thereby obtaining the oxygen content.

### SKU:U118
Hat Heart is a blood oxygen and heart rate sensor. It integrates MAX30102, providing a complete pulse oximeter and heart rate sensor system solution. This is a non-invasive blood oxygen and heart rate sensor. Its detection principle is based on infrared LED light illumination, which detects the ratio of oxygenated to deoxygenated red blood cells to determine blood oxygen levels. The sensor uses an I2C communication interface and integrates infrared LEDs, photodetectors, optical components, and low-noise electronics, with some ambient light suppression for more accurate measurements.


## Related Link
See also examples using conventional methods here.

- [Unit Heart & Datasheet](https://docs.m5stack.com/en/unit/heart)
- [Hat Heart & Datasheet](https://docs.m5stack.com/en/hat/hat_heart_rate)

## Required Libraries:

- [M5UnitUnified](https://github.com/m5stack/M5UnitUnified)
- [M5Utility](https://github.com/m5stack/M5Utility)
- [M5HAL](https://github.com/m5stack/M5HAL)

## License

- [M5Unit-HEART - MIT](LICENSE)


## Examples
See also [examples/UnitUnified](examples/UnitUnified)

### For ArduinoIDE settings
You must choose a define symbol for the unit you will use.
(Rewrite source or specify with compile options)

- PlotToSerial / GraphicalMeter
```cpp
// *************************************************************
// Choose one define symbol to match the unit you are using
// *************************************************************
#if !defined(USING_UNIT_HEART) && !defined(USING_HAT_HEART)
// For UnitHeart (U029)
// #define USING_UNIT_HEART
// For HatHeart (U118)
// #define USING_HAT_HEART
#endif
```

### For ESP-IDF settings

> **NOTE:** The library and examples target ESP-IDF **5.x** (>=5.0).  
> `M5Unified` / `M5GFX` do not yet support ESP-IDF 6.x; stay on the latest 5.x release until upstream support lands.

On ESP-IDF native builds (`idf.py`), the unit is selected via Kconfig instead of editing the source `#define`. PlotToSerial / GraphicalMeter expose the same choice through `main/Kconfig.projbuild` (which sources `examples/UnitUnified/common/Kconfig.variant`), and `examples/UnitUnified/common/variant.cmake` maps the chosen `CONFIG_EXAMPLE_USING_*` to the source-level `USING_*` macro shared with the Arduino build.

Pick the variant with `menuconfig`:

```sh
cd examples/UnitUnified/PlotToSerial    # or GraphicalMeter
idf.py set-target esp32s3               # or esp32 / esp32c6 / esp32p4 / ...
idf.py menuconfig
# -> M5Unit-HEART example -> Target unit -> choose ONE:
#       UnitHeart (MAX30100, I2C / GROVE)
#       HatHeart (MAX30102, Hat / internal I2C)
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
