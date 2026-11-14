#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
cmake -S . -B build/power_on -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DCAN_ID=0x100 -DBOARD_TYPE=0 \
  -DMAIN_MODULE_POWER_ON_TEST=OFF -DMAIN_MODULE_D_PIN_TEST=OFF \
  -DMAIN_MODULE_LED1_BLINK_TEST=OFF -DMAIN_MODULE_STS_TEST=OFF \
  -DMAIN_MODULE_CAN_RX_TEST=ON -DMAIN_MODULE_CAN_TX_TEST=OFF -DMAIN_MODULE_MD_TEST=OFF \
  -DMAIN_MODULE_PWM_SERVO_TEST=OFF \
  -DMAIN_MODULE_BATTERY_MONITOR_TEST=OFF -DMAIN_MODULE_WDT_TIMEOUT_TEST=OFF
cmake --build build/power_on
arm-none-eabi-objcopy -O binary build/power_on/main_module.elf build/power_on/main_module.bin
