# Dependencies (M5Unit-HEART, M5UnitUnified, M5Unified, ...) are pulled in via
# main/idf_component.yml and auto-added as requirements by the component manager.
# The UI plotter lives in the shared example src/ dir and is compiled in alongside the sketch.
# DualSensor always uses both UnitHeart and HatHeart, so there is no Kconfig USING_* variant.
idf_component_register(
    SRCS "DualSensor.cpp" "../src/ui_plotter.cpp"
    INCLUDE_DIRS "." "../src")
