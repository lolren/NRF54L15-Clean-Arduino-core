#!/usr/bin/env python3
"""
Test OOB (Out-of-Band) BLE pairing between two XIAO nRF54L15 boards.

Prerequisites:
  - Both boards flashed with OOB-enabled BlePair sketches
  - /dev/ttyACM0 = E91217E8 (peripheral), /dev/ttyACM1 = 761FDE87 (central)

Usage: python3 scripts/test_oob.py
"""
import serial, time, subprocess, sys

PORT0 = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"
PORT1 = sys.argv[2] if len(sys.argv) > 2 else "/dev/ttyACM1"

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

    peri_oob = cent_oob = None

    # Phase 1: Collect OOB data
    print("Collecting OOB data...")
    for i in range(30):
        while p0.in_waiting:
            l = p0.readline().decode().strip()
            print(f"  P: {l}")
            if l.startswith("OOB r="): peri_oob = l
        while p1.in_waiting:
            l = p1.readline().decode().strip()
            print(f"  C: {l}")
            if l.startswith("OOB r="): cent_oob = l
        time.sleep(0.3)

    if not peri_oob or not cent_oob:
        print("FAIL: Missing OOB data!")
        p0.close(); p1.close()
        return False

    # Phase 2: Exchange OOB
    print(f"\nExchanging OOB data...")
    time.sleep(1)
    p1.write((peri_oob + "\n").encode())  # Send peri OOB to central
    p0.write((cent_oob + "\n").encode())   # Send cent OOB to peripheral

    # Phase 3: Watch for pairing
    print("Watching for pairing...")
    peri_enc = cent_enc = False
    for i in range(60):
        while p0.in_waiting:
            l = p0.readline().decode().strip()
            print(f"  P: {l}")
            if "encryption=ON" in l or "enc=1" in l: peri_enc = True
        while p1.in_waiting:
            l = p1.readline().decode().strip()
            print(f"  C: {l}")
            if "encryption=ON" in l or "enc=1" in l: cent_enc = True
        time.sleep(1)

    p0.close(); p1.close()

    success = peri_enc and cent_enc
    print(f"\nResult: {'SUCCESS - OOB pairing established!' if success else 'PENDING - check Serial logs'}")
    return success

if __name__ == "__main__":
    main()
