# Dependencies (M5Unit-HEART, M5UnitUnified, M5Unified, ...) are pulled in via
# main/idf_component.yml and auto-added as requirements by the component manager.
# The UI plotter lives in the shared example src/ dir and is compiled in alongside the sketch.
idf_component_register(
    SRCS "GraphicalMeter.cpp" "../src/ui_plotter.cpp"
    INCLUDE_DIRS "." "../src")

# The unit variant is selected via Kconfig (main/Kconfig.projbuild -> common/Kconfig.variant)
# with `idf.py menuconfig` or sdkconfig.defaults (e.g. CONFIG_EXAMPLE_USING_HAT_HEART=y). The
# CONFIG_* -> USING_* mapping is shared by all UnitUnified examples. Include it after the register
# call (it needs ${COMPONENT_LIB}).
include("${CMAKE_CURRENT_LIST_DIR}/../../common/variant.cmake")
