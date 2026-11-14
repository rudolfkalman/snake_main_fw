#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
exec arm-none-eabi-gdb \
  -ex "target extended-remote localhost:3333" \
  build/power_on/main_module.elf
