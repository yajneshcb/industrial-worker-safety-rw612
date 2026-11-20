#
# Copyright 2026
# Project: Industrial Worker Safety Monitoring and Emergency Alert System
# Target Board: NXP FRDM-RW612 (Processor: RW612ETA2I, Cortex-M33)
#

cmake_minimum_required(VERSION 3.15)

# Target bare-metal ARM Cortex-M33
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m33)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(FRDM_RW612_WorkerSafety C CXX ASM)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)

# Target processor definition
add_definitions(
    -DCPU_RW612ETA2I
    -DFRDM_RW612
    -DMCUXPRESSO_SDK
)

# Project include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/board
    ${CMAKE_CURRENT_SOURCE_DIR}/source
    ${CMAKE_CURRENT_SOURCE_DIR}/source/sensors
    ${CMAKE_CURRENT_SOURCE_DIR}/source/alerts
    ${CMAKE_CURRENT_SOURCE_DIR}/source/edge_logic
    ${CMAKE_CURRENT_SOURCE_DIR}/source/wireless
    ${CMAKE_CURRENT_SOURCE_DIR}/device
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers
)

# Source files
file(GLOB_RECURSE BOARD_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/board/*.c")
file(GLOB_RECURSE SOURCE_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.c")
file(GLOB_RECURSE DEVICE_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/device/*.c")
file(GLOB_RECURSE DRIVER_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/drivers/*.c")

set(ALL_SRCS
    ${BOARD_SRCS}
    ${SOURCE_SRCS}
    ${DEVICE_SRCS}
    ${DRIVER_SRCS}
)

# Target executable definition
add_executable(FRDM_RW612_WorkerSafety.elf ${ALL_SRCS})

# Target compiler flags for Cortex-M33
target_compile_options(FRDM_RW612_WorkerSafety.elf PRIVATE
    -mcpu=cortex-m33
    -mthumb
    -mfpu=fpv5-sp-d16
    -mfloat-abi=hard
    -Wall
    -fno-common
    -ffunction-sections
    -fdata-sections
)

# Target linker flags for bare-metal Cortex-M33
target_link_options(FRDM_RW612_WorkerSafety.elf PRIVATE
    -mcpu=cortex-m33
    -mthumb
    -mfpu=fpv5-sp-d16
    -mfloat-abi=hard
    --specs=nano.specs
    --specs=nosys.specs
    -Wl,--gc-sections
)

# Link standard math library for sqrtf / motion algorithms
target_link_libraries(FRDM_RW612_WorkerSafety.elf PRIVATE m)

