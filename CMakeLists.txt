#
# Copyright 2026
# Project: Industrial Worker Safety Monitoring and Emergency Alert System
# Target Board: NXP FRDM-RW612 (RW612ETA2I, Cortex-M33)
#
# Built as an MCUXpresso SDK "freestanding" project: the SDK supplies the startup file, linker
# script, clocks, device headers and drivers. Configure with one of the presets in
# CMakePresets.json (they pass SdkRootDirPath, board and CONFIG_TOOLCHAIN). See docs/BUILD.md.
#
cmake_minimum_required(VERSION 3.22.0)

include(${SdkRootDirPath}/cmake/extension/mcux.cmake)

# Freestanding project: no PROJECT_BOARD_PORT_PATH (pin_mux.c / hardware_init.c live in source/board_port)
project(FRDM_RW612_WorkerSafety LANGUAGES C CXX ASM)

include(${SdkRootDirPath}/CMakeLists.txt)

mcux_add_source(
    SOURCES source/main.c
            source/timebase.c
            source/fault_handler.c
            source/board_port/hardware_init.c
            source/board_port/pin_mux.c
            source/alerts/alert_manager.c
            source/alerts/button_sos.c
            source/alerts/buzzer.c
            source/alerts/leds.c
            source/edge_logic/safety_decision.c
            source/sensors/sensor_gas.c
            source/sensors/sensor_mpu6050.c
            source/sensors/sensor_temp.c
            source/wireless/alert_reporter.c
            source/wireless/wifi_manager.c
)

mcux_add_include(
    INCLUDES source
             source/board_port
             source/alerts
             source/edge_logic
             source/sensors
             source/wireless
)

mcux_convert_binary(BINARY ${APPLICATION_BINARY_DIR}/${MCUX_SDK_PROJECT_NAME}.bin)
