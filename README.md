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
- [pcregrep](https://formulae.brew.sh/formula/pcre2)
- [Git](https://git-scm.com/) (Output commit hash to html)
