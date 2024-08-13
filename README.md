# M5Unit - HEART

## Overview

Library for UnitHEART using M5UnitUnified.  

The M5UnitUnified version of the library is located under [src/unit](src/unit).  
M5UnitUnfied has a unified API and can control multiple units via PaHub, etc.

### SKU:U029

HEART is built using the MAX30100 chipset.

MAX30100 is a complete pulse oximetry and heart-rate sensor system solution designed for the demanding requirements of wearable devices.

The MAX30100 provides very small total solution size without sacrificing optical or electrical performance. Minimal external hardware components are needed for integration into a wearable device.


## Related Link

- [Unit HEART & Datasheet](https://docs.m5stack.com/ja/unit/heart)

## Required Libraries:

- [M5UnitUnified](https://github.com/m5stack/M5UnitUnified)
- [M5Utility](https://github.com/m5stack/M5Utility)
- [M5HAL](https://github.com/m5stack/M5HAL)

## License

- [M5Unit-HEART- MIT](LICENSE)


## Examples
See also [examples/UnitUnified](examples/UnitUnified)

## Doxygen document
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
