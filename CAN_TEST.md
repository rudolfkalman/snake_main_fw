# can2ethernet → main_module CAN test

Build `can2ethernet_fw` with `CAN_TEST_ENABLED=1` and `CAN_TX_TEST_ENABLED=1`. It sends Classic CAN at 500 kbit/s, standard ID `0x124`, DLC 8 every 500 ms. Payload is a little-endian 32-bit sequence followed by `A5 5A C3 3C`.

Build this firmware with `MAIN_MODULE_CAN_RX_TEST=ON`. main_module waits about 6.6 seconds for its latch/WDI startup, keeps WDI active, then receives frames and prints `CAN_RX` records to USB CDC serial. It also prints `MAIN_CAN_RX` status with RX count and CAN error state.

Connect CANH to CANH and CANL to CANL. Join isolated bus-side CANG references. Use 120 Ω termination at both physical ends of the bus.

Build and upload in each firmware directory with `./Build.sh` and `./Upload.sh`. The main firmware expects the Pico CMSIS-DAP probe; can2ethernet defaults to the CMSIS-DAP probe and also supports `DEBUG_ADAPTER=stlink`.
