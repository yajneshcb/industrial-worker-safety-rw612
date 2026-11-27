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
            source/wireless/dashboard_transport.c
            source/wireless/ca_bundle.c
)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}
    SOURCES examples/wifi_examples/wifi_mqtt/lwippools.h
            examples/wifi_examples/wifi_mqtt/wifi_config/wifi_config.h
            examples/_boards/frdmrw612/FreeRTOSConfigBoard.h
)

mcux_add_include(
    INCLUDES source
             source/board_port
             source/alerts
             source/edge_logic
             source/sensors
             source/wireless
)

mcux_add_include(
    BASE_PATH ${SdkRootDirPath}
    INCLUDES examples/wifi_examples/wifi_mqtt
             examples/wifi_examples/wifi_mqtt/wifi_config
             examples/_boards/frdmrw612/wifi_examples/common
)

mcux_add_macro(CC "-DUSE_RTOS=1 -D_POSIX_SOURCE -DCONFIG_MONOLITHIC_WIFI=1 -DCONFIG_SOC_SERIES_RW6XX_REVISION_A2=1")

# Wi-Fi occupies additional SRAM on RW612. Use NXP's Wi-Fi memory layout.
mcux_remove_armgcc_linker_script(
    BASE_PATH ${SdkRootDirPath}
    TARGETS debug release flash_debug flash_release Debug Release
    LINKER devices/Wireless/RW/RW612/gcc/RW612_flash.ld
)
mcux_add_armgcc_linker_script(
    BASE_PATH ${SdkRootDirPath}
    TARGETS debug release flash_debug flash_release Debug Release
    LINKER examples/_boards/frdmrw612/wifi_examples/common/linker/RW610_flash.ld
)

# The RW612 device defaults include the RAM linker script. This application is
# linked for flash execution, so remove the RAM script before adding flash.
mcux_remove_armgcc_linker_script(
    BASE_PATH ${SdkRootDirPath}
    TARGETS debug release flash_debug flash_release Debug Release
    LINKER devices/Wireless/RW/RW612/gcc/RW612_ram.ld
)

mcux_convert_binary(BINARY ${APPLICATION_BINARY_DIR}/${MCUX_SDK_PROJECT_NAME}.bin)

# mbedTLS uses newlib calloc/free; RW610_flash.ld defaults __heap_size__ to 1 KB (0x0400).
target_link_options(${MCUX_SDK_PROJECT_NAME} PRIVATE "-Wl,--defsym=__heap_size__=0x10000")
