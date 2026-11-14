#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
TARGET_ELF="build/power_on/main_module.elf"
if [ ! -f "$TARGET_ELF" ]; then
  echo "Run ./Build.sh first." >&2
  exit 1
fi
openocd \
  -f interface/cmsis-dap.cfg \
  -f target/stm32g4x.cfg \
  -c "adapter speed ${SWD_KHZ:-20}" \
  -c "program ${TARGET_ELF} verify reset exit"
