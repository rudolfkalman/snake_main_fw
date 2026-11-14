cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME main_module)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})
option(MAIN_MODULE_POWER_ON_TEST "Run the snake PCB WDT/latch power-on test" ON)
option(MAIN_MODULE_D_PIN_TEST "Toggle only latch D while keeping WDI alive" OFF)
option(MAIN_MODULE_LED1_BLINK_TEST "Toggle only LED1 using internal HSI clock" OFF)
option(MAIN_MODULE_CAN_RX_TEST "Receive CAN frames and report over USB CDC" OFF)
option(MAIN_MODULE_CAN_TX_TEST "Transmit CAN frames and report over USB CDC" OFF)
option(MAIN_MODULE_MD_TEST "Run the TB6643 motor forward/reverse test" OFF)
option(MAIN_MODULE_PWM_SERVO_TEST "Move both DS51150 PWM servos together" OFF)
option(MAIN_MODULE_STS_TEST "Power on the STS3215 and run a slow position sweep" OFF)
option(MAIN_MODULE_BATTERY_MONITOR_TEST "Read the snake PCB cell balance taps and report over USB CDC" OFF)
option(MAIN_MODULE_WDT_TIMEOUT_TEST "Stop WDI five seconds after the latch power-on command" OFF)

# Enable CMake support for ASM and C languages
enable_language(C ASM)

set(BOARD_TYPE_VALUE "${BOARD_TYPE}")
if(BOARD_TYPE STREQUAL "BOARD_TYPE_PWM")
    set(BOARD_TYPE_VALUE 1)
elseif(BOARD_TYPE STREQUAL "BOARD_TYPE_LIMIT")
    set(BOARD_TYPE_VALUE 2)
elseif(BOARD_TYPE STREQUAL "BOARD_TYPE_ABS")
    set(BOARD_TYPE_VALUE 3)
endif()

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    "Bsp/AS5600/Src/drv_as5600.c"
    "Bsp/Stdio/Src/drv_stdio.c"
    "Bsp/PID/Src/pid.c"
    "Core/Src/main_module_power.c"
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    App/Config
    Bsp/AS5600/Inc
    Bsp/I2C/Inc
    Bsp/Stdio/Inc
    Bsp/PID/Inc
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
    CAN_DEVICE_ID=${CAN_ID}
    BOARD_TYPE=${BOARD_TYPE_VALUE}
    MAIN_MODULE_POWER_ON_TEST=$<BOOL:${MAIN_MODULE_POWER_ON_TEST}>
    MAIN_MODULE_D_PIN_TEST=$<BOOL:${MAIN_MODULE_D_PIN_TEST}>
    MAIN_MODULE_LED1_BLINK_TEST=$<BOOL:${MAIN_MODULE_LED1_BLINK_TEST}>
    MAIN_MODULE_STS_TEST=$<BOOL:${MAIN_MODULE_STS_TEST}>
    MAIN_MODULE_CAN_RX_TEST=$<BOOL:${MAIN_MODULE_CAN_RX_TEST}>
    MAIN_MODULE_CAN_TX_TEST=$<BOOL:${MAIN_MODULE_CAN_TX_TEST}>
    MAIN_MODULE_MD_TEST=$<BOOL:${MAIN_MODULE_MD_TEST}>
    MAIN_MODULE_PWM_SERVO_TEST=$<BOOL:${MAIN_MODULE_PWM_SERVO_TEST}>
    MAIN_MODULE_BATTERY_MONITOR_TEST=$<BOOL:${MAIN_MODULE_BATTERY_MONITOR_TEST}>
    MAIN_MODULE_WDT_TIMEOUT_TEST=$<BOOL:${MAIN_MODULE_WDT_TIMEOUT_TEST}>
)

if(BOARD_TYPE_VALUE EQUAL 3)
    set(PID_GAIN_HEADER_NAME "gain_${CAN_ID}.h")

    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/App/Config/${PID_GAIN_HEADER_NAME}")
        message(FATAL_ERROR "Missing App/Config/${PID_GAIN_HEADER_NAME}")
    endif()

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/pid_gain.h.in"
        "${CMAKE_CURRENT_BINARY_DIR}/generated/pid_gain.h"
        @ONLY
    )

    target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
    )
endif()

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)

# リンカーに float printf の使用を強制するフラグ
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    "-Wl,-u,_printf_float"
)
