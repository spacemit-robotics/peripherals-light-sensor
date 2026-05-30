cmake_minimum_required(VERSION 3.10)
project(light_sensor LANGUAGES C)
if(NOT DEFINED SROBOTIS_EXPORT_SET)
    set(SROBOTIS_EXPORT_SET "ComponentsTargets" CACHE STRING "Export set name used by exported targets")
endif()
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/drivers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/drv_i2c_w1160
)
# Core
set(LIGHT_SENSOR_CORE_SOURCES
    src/light_sensor_core.c
)
# Drivers (selected by list variable)
set(LIGHT_SENSOR_DRIVER_SOURCES "")
function(light_sensor_append_driver_source driver_name)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/${driver_name}.c")
        set(_driver_source "src/drivers/${driver_name}.c")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/${driver_name}/${driver_name}.c")
        set(_driver_source "src/drivers/${driver_name}/${driver_name}.c")
    else()
        message(FATAL_ERROR
            "light_sensor: enabled driver '${driver_name}' not found. Tried:\n"
            "  - src/drivers/${driver_name}.c\n"
            "  - src/drivers/${driver_name}/${driver_name}.c"
        )
    endif()

    list(FIND LIGHT_SENSOR_DRIVER_SOURCES "${_driver_source}" _driver_idx)
    if(_driver_idx EQUAL -1)
        list(APPEND LIGHT_SENSOR_DRIVER_SOURCES "${_driver_source}")
        set(LIGHT_SENSOR_DRIVER_SOURCES "${LIGHT_SENSOR_DRIVER_SOURCES}" PARENT_SCOPE)
    endif()
endfunction()

foreach(_drv IN LISTS SROBOTIS_PERIPHERALS_LIGHT_SENSOR_ENABLED_DRIVERS)
    light_sensor_append_driver_source("${_drv}")
endforeach()
# Always include generic I2C driver
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/drv_i2c_generic.c")
    light_sensor_append_driver_source("drv_i2c_generic")
endif()

# Always include W1160 driver
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/drv_i2c_w1160/drv_i2c_w1160.c")
    light_sensor_append_driver_source("drv_i2c_w1160")
endif()

set(LIGHT_SENSOR_SOURCES
    ${LIGHT_SENSOR_CORE_SOURCES}
    ${LIGHT_SENSOR_DRIVER_SOURCES}
)
add_library(light_sensor STATIC ${LIGHT_SENSOR_SOURCES})
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(light_sensor PRIVATE
        -Wall
        -Wextra
    )
endif()

# ---- Link STK ALPS static library for W1160 ----
set(STK_ALPS_STATIC_LIB "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/drv_i2c_w1160/libstk_alps_lib.a"
    CACHE FILEPATH "Path to libstk_alps_lib.a")
if(EXISTS "${STK_ALPS_STATIC_LIB}")
    target_link_libraries(light_sensor PRIVATE ${STK_ALPS_STATIC_LIB})
else()
    message(WARNING "light_sensor: STK ALPS library not found at ${STK_ALPS_STATIC_LIB}")
endif()

# Install rules
install(TARGETS light_sensor
    EXPORT ${SROBOTIS_EXPORT_SET}
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
install(FILES include/light_sensor.h
    DESTINATION include
)
if(EXISTS "${STK_ALPS_STATIC_LIB}")
    install(FILES "${STK_ALPS_STATIC_LIB}"
        DESTINATION lib
    )
endif()
option(BUILD_TESTS "Build example programs" OFF)
if(BUILD_TESTS)
    add_executable(test_light_sensor_i2c example/test_light_sensor_i2c.c)
    target_link_libraries(test_light_sensor_i2c light_sensor)
    set_target_properties(test_light_sensor_i2c PROPERTIES BUILD_RPATH "$ORIGIN/../lib")
    install(TARGETS test_light_sensor_i2c RUNTIME DESTINATION bin)
endif()
