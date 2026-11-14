#!/bin/bash

# Robot A
echo "Building Robot A firmware..."
./Build.sh 0x11 BOARD_TYPE_PWM
./Build.sh 0x12 BOARD_TYPE_PWM
./Build.sh 0x13 BOARD_TYPE_PWM
./Build.sh 0x14 BOARD_TYPE_PWM
./Build.sh 0x15 BOARD_TYPE_LIMIT
./Build.sh 0x16 BOARD_TYPE_LIMIT
./Build.sh 0x17 BOARD_TYPE_LIMIT

# Robot B
echo "Building Robot B firmware..."
./Build.sh 0x21 BOARD_TYPE_PWM
./Build.sh 0x22 BOARD_TYPE_PWM
./Build.sh 0x23 BOARD_TYPE_PWM
./Build.sh 0x24 BOARD_TYPE_PWM
./Build.sh 0x25 BOARD_TYPE_ABS
./Build.sh 0x26 BOARD_TYPE_ABS

echo "All builds completed."
