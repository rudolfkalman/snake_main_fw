# Current Sensing Work Log

Date: 2026-07-17

## Target

- CAN device ID: `0x11`
- Board type: `BOARD_TYPE_PWM`
- Motor output starts stopped: `direction = 0`, `motor_power = 0`
- Test motors: Tamiya 540 and RZ735
- Test supply voltage: approximately 11 V

## Current Signal Path

- The board uses a Hall current sensor on the high-side DC link.
- PWM OFF/freewheel current circulating inside the H-bridge does not pass through
  the Hall sensor.
- The STM32 OPAMP gain is currently 8:
  `OPAMP_PGA_GAIN_8_OR_MINUS_7`.
- Sensor sensitivity used by firmware: `0.0203 V/A`.
- Current conversion:

  ```text
  current = ((adc - adc_offset) * 3.3 / 4095) / 8 / 0.0203
  ```

- The Hall sensor itself supports a much wider range. Gain 8 is intentionally
  used to improve resolution in the narrower range needed by this board.

## ADC Timing

- TIM1 is center-aligned with `ARR = 8499`.
- TIM1 CH3 compare is `8498`.
- ADC1 regular conversion trigger is `TIM1_CC3`, rising edge.
- The motor ON interval is centered around the TIM1 counter apex, so ADC samples
  near the center of PWM ON time.
- With the current 80 MHz timer clock, PWM is approximately 4.7 kHz and CC3
  compare events occur approximately 9.4 kHz.
- ADC clock prescaler is currently `ADC_CLOCK_SYNC_PCLK_DIV4`.

## Oversampling And Filtering

- STM32 ADC hardware oversampling is currently disabled:

  ```c
  hadc1.Init.OversamplingMode = DISABLE;
  ```

- Previously intended behavior was likely four conversions averaged back to a
  12-bit result:

  ```c
  hadc1.Init.OversamplingMode = ENABLE;
  hadc1.Init.Oversampling.Ratio = ADC_OVERSAMPLING_RATIO_4;
  hadc1.Init.Oversampling.RightBitShift = ADC_RIGHTBITSHIFT_2;
  hadc1.Init.Oversampling.TriggeredMode = ADC_TRIGGEREDMODE_SINGLE_TRIGGER;
  hadc1.Init.Oversampling.OversamplingStopReset =
      ADC_REGOVERSAMPLING_CONTINUED_MODE;
  ```

- No checked-in Git revision was found with hardware oversampling enabled. The
  `Current detection is possible` commit also has it disabled.
- The working tree uses a time-step-aware first-order 20 Hz low-pass filter for
  displayed current. `Raw current` remains unfiltered.
- The 20 Hz filter is for telemetry. Fast overcurrent protection should use the
  raw/oversampled center sample, not the 20 Hz result.

## USB Telemetry

Firmware output format:

```text
CUR,mcu_ms,firmware_duty,current_A,raw_current_A,adc_raw,adc_filtered,adc_offset
```

`Operator/src/duty_current_plot.py`:

- Sends the same 8-byte binary command as the C operator.
- Uses CAN ID `0x11` by default.
- Alternates configurable duty commands.
- Displays current and duty in real time.
- Drains all pending serial input each loop to avoid the previous delay of up to
  approximately 10 seconds.
- Writes an optional CSV log.

Example:

```bash
python3 Operator/src/duty_current_plot.py 0 0x11 \
  --positive 100 --negative 20 --period 10 \
  --csv Operator/src/log.csv
```

## Measurements

Files:

- `Operator/src/log.csv`
- `Operator/src/log_duty=20.csv`
- `Operator/src/log_duty=100.csv`

Two unloaded motors, Tamiya 540 and RZ735, connected simultaneously:

| Command | Firmware duty | Steady ON-center current |
| ---: | ---: | ---: |
| +20% | +19% | approximately 3.39 A |
| +100% | +100% | approximately 2.75 A |

The higher 20% sample is expected because it is ON-center current, not PWM-cycle
average battery current. At low speed the back EMF is lower, so the current
during an 11 V ON pulse can be higher.

Approximate battery current, when not braking or regenerating:

```text
20%:  3.39 A * 0.20 = approximately 0.68 A
100%: 2.75 A * 1.00 = approximately 2.75 A
```

This duty multiplication is only an estimate. It is not valid during reversal,
braking, or regeneration without accounting for H-bridge switching state.

Startup and reversal samples reached approximately 10.7 A in the logs. The
current signal returned close to the measured ADC offset when motor power was
removed.

## Intended Use

- Primary board purpose: overcurrent protection.
- PWM-center current sampling is appropriate for detecting driven-state current.
- Protection should use raw or hardware-oversampled ADC values and reject short
  switching glitches, for example by requiring two or three consecutive
  threshold violations.
- Torque will be estimated rather than directly measured. Duty, speed, bus
  voltage, motor parameters, and ON-center current can be used for the estimate.
- A single high-side sensor cannot directly observe winding current during the
  H-bridge freewheel interval.
- When two motors share one bridge, only their total current is available.

## Build And Flash Status

Build command:

```bash
./Build.sh 0x11 BOARD_TYPE_PWM
```

The 20 Hz filter firmware built successfully as:

```text
build/main_module_0x11.elf
```

The latest automated flash attempts could not connect to the STM32 over SWD.
OpenOCD detected the ST-Link and approximately 3.2 V target voltage but reported
`unable to connect to the target`. `st-flash` also reported that NRST was not
connected. Verify the firmware on the board before relying on the flash status.

## Next Check

- Restore ADC hardware oversampling with ratio 4 and right shift 2 if that is the
  intended production configuration.
- Keep the 20 Hz display filter separate from the fast protection path.
- Confirm the desired overcurrent threshold and consecutive-sample policy.
