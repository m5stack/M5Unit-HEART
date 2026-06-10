# Map the Kconfig "Target unit" choice (see common/Kconfig.variant) to the source-level USING_*
# macro that both the Arduino and ESP-IDF builds use. Include this from an example's
# main/CMakeLists.txt *after* idf_component_register() (it needs ${COMPONENT_LIB}).
# Default (nothing selected): UnitHeart (MAX30100, I2C).
if(CONFIG_EXAMPLE_USING_HAT_HEART)
    set(M5UNIT_VARIANT USING_HAT_HEART)
else()
    set(M5UNIT_VARIANT USING_UNIT_HEART)
endif()
target_compile_definitions(${COMPONENT_LIB} PRIVATE ${M5UNIT_VARIANT})
