# M5Unit - HEART

## Overview

Library for UnitHEART [M5UnitUnified](https://github.com/m5stack/M5UnitUnified).  
M5UnitUnified is a library for unified handling of various M5 units products.

### SKU:U029

HEART is built using the MAX30100 chipset.

MAX30100 is a complete pulse oximetry and heart-rate sensor system solution designed for the demanding requirements of wearable devices.

The MAX30100 provides very small total solution size without sacrificing optical or electrical performance. Minimal external hardware components are needed for integration into a wearable device.

### SKU:U118

HEART RATE HAT is a blood oxygen heart rate sensor. Integrate MAX30102 to provide a complete pulse oximeter and heart rate sensor system solution. This is a non-pluggable blood oxygen heart rate sensor. The sensor uses I2C communication interface, internally integrates infrared light-emitting diodes, photo-detectors, optical components and low-noise electronic equipment. A certain amount of ambient light suppression function can make the measurement results more accurate. .


## Related Link
See also examples using conventional methods here.

- [Unit Heart & Datasheet](https://docs.m5stack.com/ja/unit/heart)
- [Hat Heart & Datasheet](https://docs.m5stack.com/en/hat/hat_heart_rate)

## Required Libraries:

- [M5UnitUnified](https://github.com/m5stack/M5UnitUnified)
- [M5Utility](https://github.com/m5stack/M5Utility)
- [M5HAL](https://github.com/m5stack/M5HAL)

## License

- [M5Unit-HEART- MIT](LICENSE)


## Examples
See also [examples/UnitUnified](examples/UnitUnified)

## Doxygen document
[GitHub Pages](https://m5stack.github.io/M5Unit-HEART/)

If you want to generate documents on your local machine, execute the following command

```
bash docs/doxy.sh
```

It will output it under docs/html  
If you want to output Git commit hashes to html, do it for the git cloned folder.

### Required
- [Doxyegn](https://www.doxygen.nl/)
- [pcregrep](https://formulae.brew.sh/formula/pcre2)
- [Git](https://git-scm.com/) (Output commit hash to html)
