import serial
import time
import sys

if len(sys.argv) > 1:
    port_index = sys.argv[1]
else:
    port_index = input("Enter ACM port index (n for /dev/ttyACM<n>): ")

port_path = f"/dev/ttyACM{port_index}"
print(f"Opening {port_path}...")

try:
    ser = serial.Serial(port_path, 12000000)
except Exception as e:
    print(f"Error opening serial port: {e}")
    sys.exit(1)

while True:
    try:
        cmd = input("Enter [ID(hex) Duty State Float] (q to quit): ")
        if cmd.lower() == 'q':
            break
        ser.write((cmd + "\n").encode())
    except EOFError:
        break
    except KeyboardInterrupt:
        break
