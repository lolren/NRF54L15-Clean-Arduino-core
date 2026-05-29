#!/usr/bin/env python3
"""Test Serial output on both XIAO nRF54L15 boards via pyocd reset."""
import serial, time, subprocess, sys

PORT_P = '/dev/ttyACM0'  # E91217E8 (peripheral)
PORT_C = '/dev/ttyACM1'  # 761FDE87 (central)

def read_all(port, duration=20):
    """Read all available lines for duration seconds."""
    lines = []
    deadline = time.time() + duration
    while time.time() < deadline:
        while port.in_waiting:
            lines.append(port.readline().decode().strip())
        time.sleep(0.3)
    return lines

def main():
    print(f"Opening {PORT_P} and {PORT_C}...")
    p0 = serial.Serial(PORT_P, 115200, timeout=1)
    p1 = serial.Serial(PORT_C, 115200, timeout=1)
    time.sleep(1)

    print("Resetting boards...")
    subprocess.run(['pyocd', 'reset', '-t', 'nrf54l', '-u', 'E91217E8'], capture_output=True)
    subprocess.run(['pyocd', 'reset', '-t', 'nrf54l', '-u', '761FDE87'], capture_output=True)

    peri_lines = read_all(p0, 20)
    cent_lines = read_all(p1, 20)

    print(f"\n=== Peripheral ({len(peri_lines)} lines) ===")
    for l in peri_lines:
        print(f"  {l}")

    print(f"\n=== Central ({len(cent_lines)} lines) ===")
    for l in cent_lines:
        print(f"  {l}")

    p0.close()
    p1.close()
    return peri_lines, cent_lines

if __name__ == '__main__':
    main()
