#!/usr/bin/env python3
import os
import sys

SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
sys.path = [
    path for path in sys.path
    if os.path.realpath(path or os.getcwd()) != SCRIPT_DIR
]
os.environ.setdefault("MPLCONFIGDIR", "/tmp/matplotlib-g431-md-fw")
os.makedirs(os.environ["MPLCONFIGDIR"], exist_ok=True)

import argparse
import csv
import struct
import time
from collections import deque
from pathlib import Path

import matplotlib.pyplot as plt
import serial


def parse_port(port_or_index: str) -> str:
    if port_or_index.isdigit():
        return f"/dev/ttyACM{port_or_index}"
    return port_or_index


def send_duty(ser: serial.Serial, can_id: int, duty: int) -> None:
    packet = struct.pack(">Ii", can_id, int(duty))
    ser.write(packet)
    ser.flush()


def parse_current_line(line: str):
    fields = line.strip().split(",")
    if len(fields) < 4 or fields[0] != "CUR":
        return None
    try:
        sample = {
            "mcu_ms": int(fields[1]),
            "fw_duty": int(fields[2]),
            "current": float(fields[3]),
        }
        if len(fields) >= 5:
            sample["raw_current"] = float(fields[4])
        if len(fields) >= 6:
            sample["adc_raw"] = int(fields[5])
        if len(fields) >= 7:
            sample["adc_filtered"] = float(fields[6])
        if len(fields) >= 8:
            sample["adc_offset"] = float(fields[7])
        return sample
    except ValueError:
        return None


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Send operator.c-compatible duty packets and plot current telemetry."
    )
    parser.add_argument(
        "port",
        help="Serial port path or ACM index, e.g. /dev/ttyACM0 or 0.",
    )
    parser.add_argument(
        "can_id",
        nargs="?",
        default=0x11,
        type=lambda value: int(value, 0),
        help="Target CAN ID, e.g. 0x11. Default: 0x11.",
    )
    parser.add_argument("--baud", type=int, default=12000000)
    parser.add_argument(
        "--period",
        "--interval",
        type=float,
        default=1.0,
        help="Duty switch period [s].",
    )
    parser.add_argument("--positive", type=int, default=20)
    parser.add_argument("--negative", type=int, default=-20)
    parser.add_argument(
        "--duration",
        type=float,
        default=0.0,
        help="Measurement duration [s]. 0 means run until Ctrl-C.",
    )
    parser.add_argument(
        "--window",
        type=float,
        default=20.0,
        help="Plot time window [s].",
    )
    parser.add_argument(
        "--csv",
        type=Path,
        help="Optional CSV log path.",
    )
    args = parser.parse_args()

    port = parse_port(args.port)
    duties = [args.positive, args.negative]

    csv_file = None
    csv_writer = None
    if args.csv:
        csv_file = args.csv.open("w", newline="")
        csv_writer = csv.writer(csv_file)
        csv_writer.writerow([
            "host_time_s",
            "mcu_ms",
            "command_duty",
            "firmware_duty",
            "current_A",
            "raw_current_A",
            "adc_raw",
            "adc_filtered",
            "adc_offset",
        ])

    times = deque()
    currents = deque()
    raw_currents = deque()
    command_duties = deque()
    firmware_duties = deque()

    plt.ion()
    fig, ax_current = plt.subplots()
    ax_duty = ax_current.twinx()
    current_line, = ax_current.plot([], [], label="Current [A]", color="tab:blue")
    raw_current_line, = ax_current.plot([], [], "--", label="Raw current [A]", color="tab:cyan", alpha=0.6)
    command_line, = ax_duty.step([], [], where="post", label="Command duty [%]", color="tab:orange")
    firmware_line, = ax_duty.step([], [], where="post", label="Firmware duty [%]", color="tab:green", alpha=0.65)
    ax_current.set_xlabel("Time [s]")
    ax_current.set_ylabel("Current [A]")
    ax_duty.set_ylabel("Duty [%]")
    ax_duty.set_ylim(-110, 110)
    ax_current.grid(True)
    lines = [current_line, raw_current_line, command_line, firmware_line]
    ax_current.legend(lines, [line.get_label() for line in lines], loc="upper right")
    plt.show(block=False)

    start = time.monotonic()
    next_send = start
    duty_index = 0
    command_duty = duties[0]
    last_draw = start
    ser = None
    rx_buffer = bytearray()

    try:
        ser = serial.Serial(port, args.baud, timeout=0.005, write_timeout=1.0)
        ser.reset_input_buffer()
        send_duty(ser, args.can_id, command_duty)
        print(f"sent duty {command_duty:+d}% to 0x{args.can_id:X}")
        next_send = time.monotonic() + args.period

        while True:
            now = time.monotonic()
            elapsed = now - start
            if args.duration > 0.0 and elapsed >= args.duration:
                break

            if now >= next_send:
                duty_index = (duty_index + 1) % len(duties)
                command_duty = duties[duty_index]
                send_duty(ser, args.can_id, command_duty)
                print(f"sent duty {command_duty:+d}% to 0x{args.can_id:X}")
                next_send += args.period

            read_size = max(1, ser.in_waiting)
            raw_chunk = ser.read(read_size)
            if raw_chunk:
                rx_buffer.extend(raw_chunk)

            while True:
                newline_index = rx_buffer.find(b"\n")
                if newline_index < 0:
                    break

                raw_line = bytes(rx_buffer[:newline_index])
                del rx_buffer[:newline_index + 1]
                line = raw_line.decode("ascii", errors="ignore").strip()
                sample = parse_current_line(line)
                if sample is not None:
                    sample_time = time.monotonic() - start
                    times.append(sample_time)
                    currents.append(sample["current"])
                    raw_currents.append(sample.get("raw_current", sample["current"]))
                    command_duties.append(command_duty)
                    firmware_duties.append(sample["fw_duty"])

                    if csv_writer:
                        csv_writer.writerow([
                            f"{sample_time:.3f}",
                            sample["mcu_ms"],
                            command_duty,
                            sample["fw_duty"],
                            f"{sample['current']:.3f}",
                            f"{sample.get('raw_current', sample['current']):.3f}",
                            sample.get("adc_raw", ""),
                            sample.get("adc_filtered", ""),
                            sample.get("adc_offset", ""),
                        ])
                elif line:
                    print(line)

            while times and (elapsed - times[0]) > args.window:
                times.popleft()
                currents.popleft()
                raw_currents.popleft()
                command_duties.popleft()
                firmware_duties.popleft()

            if now - last_draw >= 0.05:
                current_line.set_data(times, currents)
                raw_current_line.set_data(times, raw_currents)
                command_line.set_data(times, command_duties)
                firmware_line.set_data(times, firmware_duties)

                if times:
                    ax_current.set_xlim(max(0.0, times[-1] - args.window), max(args.window, times[-1]))
                all_currents = list(currents) + list(raw_currents)
                if all_currents:
                    min_current = min(all_currents)
                    max_current = max(all_currents)
                    margin = max(0.5, (max_current - min_current) * 0.2)
                    ax_current.set_ylim(min_current - margin, max_current + margin)

                fig.canvas.draw_idle()
                plt.pause(0.001)
                last_draw = now

    except KeyboardInterrupt:
        print("stopping")
    finally:
        if ser is not None and ser.is_open:
            send_duty(ser, args.can_id, 0)
            print(f"sent duty +0% to 0x{args.can_id:X}")
            ser.close()
        if csv_file:
            csv_file.close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
