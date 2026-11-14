# snake PCB WDT / latch power-on test

`./Build.sh` builds this mode into `build/power_on/main_module.elf` and `.bin`.
`./Upload.sh` programs that ELF through the Raspberry Pi Debug Probe (CMSIS-DAP)
and resets the MCU. `./Debug.sh` starts OpenOCD; in another terminal,
`./GDB.sh` attaches GDB to it.
The CMake option `MAIN_MODULE_POWER_ON_TEST=ON` redirects main() before any
legacy MD initialization. This is a bench power-on test, not full robot firmware.

## Sequence

- HSI16 runs the MCU; HSI48 runs USB CDC. The external crystal is unused.
- PA8/PA9 (TB6643 IN1/IN2) are held LOW. Motor PWM, CAN, SPI and ADC are not started.
- An initial D=LOW / CLK pulse requests latch OFF.
- PB13 (WDI) toggles every 550 ms: falling-edge period 1100 ms.
- After six falling edges, about 6.6 seconds, PB14 (D) goes HIGH, then PB15
  (CLK) pulses HIGH for 50 ms. D has 50 ms setup and hold times. Both return LOW.
- The ON command is issued only once per boot. WDI keeps running afterward.
- If an iteration delays an edge beyond 650 ms, the firmware requests OFF,
  freezes WDI, and requires a reset to try again. There are no catch-up pulses.

The board connects U5 SET0/SET1 to 3.3 V and leaves CWD unconnected. The
TPS3430 falling-edge window is 920 ms < interval < 1360 ms at worst-case limits.
Source: https://www.ti.com/lit/ds/symlink/tps3430.pdf (section 6.6).

## Observe

- LED1 (PB11) follows WDI, changing every 550 ms.
- LED2 (PB12) lights after the ON request, not after a voltage measurement.
- USB CDC prints `MAIN_POWER ... latch=WARMUP` and then `latch=ON_SENT` once
  per second. A timing fault prints `latch=OFF_SENT` and `WDI=STOP_TIMING_FAULT`.
- WDO/CLR, Q and VSW are not fed back to the MCU. `Q/VSW=unverified` is
  intentional: measure U8 Q and VSW to confirm the hardware turned on.
- NRST must be HIGH so U7 releases the shared WDO/CLR line. If another device
  holds ESP32_CLR LOW, the latch cannot turn on. No automatic re-arm is attempted.
- NRST reset restarts the sequence and automatically requests ON again after
  the warmup. A clear-line fault without MCU reset leaves the latch OFF.

## Before programming

Disconnect the ESP32 WDI output: PB13 now drives the same net. Power-on will
energize VSW after the warmup. Resolve the reported ADC_C2 overvoltage before
reconnecting the battery; this firmware does not measure/protect the cell inputs.

```sh
cd ~/projects/decommissioning2026/main_module_fw
./Build.sh
./Upload.sh
```

USB enumeration is not required for the WDI/latch sequence. Rebuild this target
before programming; older ELF files under other build directories are unrelated.

## BOOT0 configuration on a newly installed STM32

The checked board initially ran at PC 0x1fff44xx (ROM bootloader), even though
flash verified against this ELF. FLASH_OPTR was 0xffeff8aa: nSWBOOT0 selected
the PB8/BOOT0 pin, shared with CAN RX. Configure nSWBOOT0=0 and nBOOT0=1 to
boot from user flash independently of PB8. Only those two bits should be changed.

```sh
openocd -f interface/cmsis-dap.cfg -f target/stm32g4x.cfg \
  -c 'init; halt; stm32l4x option_write 0 0x20 0x08000000 0x0c000000; stm32l4x option_load 0; shutdown'
```

Option loading resets the target and can report a disconnect/error. Reconnect
and read FLASH_OPTR at 0x40022020 to verify rather than assuming success.
On the checked board it became 0xfbeff8aa, and PB13/PB11 began toggling with
PB12 HIGH after the ON request. This does not verify the external Q/VSW voltage.
Reference: ST RM0440, boot configuration / FLASH_OPTR.
https://www.st.com/resource/en/reference_manual/dm00355726.pdf
