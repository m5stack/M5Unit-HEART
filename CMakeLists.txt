# ESP-IDF component manifest for M5Unit-HEART.
# Upstream: https://github.com/m5stack/M5Unit-HEART
#
# UnitHeart (MAX30100) / HatHeart (MAX30102) over I2C. The public headers only
# pull in M5UnitUnified; M5Utility is used in the .cpp internals only.
file(GLOB_RECURSE SRCS "src/*.cpp")
idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "src"
    REQUIRES M5UnitUnified
    PRIV_REQUIRES M5Utility
)

# Precompiled GH3038 core for StampHeart (src/<target>/libm5gh3038core.a)
set(GH3038_CORE_LIB "${CMAKE_CURRENT_LIST_DIR}/src/${IDF_TARGET}/libm5gh3038core.a")
if(EXISTS "${GH3038_CORE_LIB}")
    add_prebuilt_library(m5gh3038core "${GH3038_CORE_LIB}")
    target_link_libraries(${COMPONENT_LIB} PRIVATE m5gh3038core)
else()
    # Not an error: UnitHeart / HatHeart can be used without the core
    message(WARNING "M5Unit-HEART: ${GH3038_CORE_LIB} is not found. StampHeart (UnitGH3038) is not available")
endif()
