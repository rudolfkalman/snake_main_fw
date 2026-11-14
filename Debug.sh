#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
exec openocd \
  -f interface/cmsis-dap.cfg \
  -f target/stm32g4x.cfg \
  -c "adapter speed 100"
