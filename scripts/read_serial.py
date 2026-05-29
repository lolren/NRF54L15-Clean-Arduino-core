#!/usr/bin/env python3
"""
read_serial.py - Read Serial output from XIAO nRF54L15 boards after pyocd reset.
Usage: python3 read_serial.py [port0] [port1]
Defaults: /dev/ttyACM0 (E91217E8), /dev/ttyACM1 (761FDE87)

Tested working: Serial output is available ~5-10s after reset.
Note: Bluefruit.begin() disables Serial on nRF54L15.
Use nrf54l15_hal (BleRadio) for Serial + BLE testing.
"""
import serial, time, subprocess, sys

PORT0 = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"
PORT1 = sys.argv[2] if len(sys.argv) > 2 else "/dev/ttyACM1"

def read_lines(port_obj, label, duration=20):
    lines = []
    end = time.time() + duration
    while time.time() < end:
        while port_obj.in_waiting:
            line = port_obj.readline().decode().strip()
            if line:
                lines.append(line)
                print(f"  {label}: {line}")
        time.sleep(0.3)
    return lines

def wait_for_line(port_obj, label, prefix, timeout=15):
    end = time.time() + timeout
    while time.time() < end:
        while port_obj.in_waiting:
            line = port_obj.readline().decode().strip()
            print(f"  {label}: {line}")
            if line.startswith(prefix):
                return line
        time.sleep(0.3)
    return None

def main():
    print(f"Opening {PORT0} and {PORT1}...")
    p0 = serial.Serial(PORT0, 115200, timeout=1)
    p1 = serial.Serial(PORT1, 115200, timeout=1)
    time.sleep(1)
    p0.reset_input_buffer()
    p1.reset_input_buffer()

    print("Resetting boards...")
    subprocess.run(['pyocd', 'reset', '-t', 'nrf54l', '-u', 'E91217E8'], capture_output=True)
    subprocess.run(['pyocd', 'reset', '-t', 'nrf54l', '-u', '761FDE87'], capture_output=True)
    
    return p0, p1

if __name__ == "__main__":
    p0, p1 = main()
