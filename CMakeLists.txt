cmake_minimum_required(VERSION 3.10)
project(light_sensor LANGUAGES C)
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
foreach(_drv IN LISTS SROBOTIS_PERIPHERALS_LIGHT_SENSOR_ENABLED_DRIVERS)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/${_drv}.c")
        list(APPEND LIGHT_SENSOR_DRIVER_SOURCES "src/drivers/${_drv}.c")
    else()
        message(FATAL_ERROR "light_sensor: enabled driver '${_drv}' not found at src/drivers/${_drv}.c")
    endif()
endforeach()
# Always include generic I2C driver
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/drv_i2c_generic.c")
    list(APPEND LIGHT_SENSOR_DRIVER_SOURCES "src/drivers/drv_i2c_generic.c")
endif()

# Always include W1160 driver
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/drv_i2c_w1160/drv_i2c_w1160.c")
    list(APPEND LIGHT_SENSOR_DRIVER_SOURCES "src/drivers/drv_i2c_w1160/drv_i2c_w1160.c")
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
option(BUILD_TESTS "Build test programs" OFF)
if(BUILD_TESTS)
    add_executable(test_light_sensor_i2c test/test_light_sensor_i2c.c)
    target_link_libraries(test_light_sensor_i2c light_sensor)
    set_target_properties(test_light_sensor_i2c PROPERTIES BUILD_RPATH "$ORIGIN/../lib")
    install(TARGETS test_light_sensor_i2c RUNTIME DESTINATION bin)
endif()
