#!/usr/bin/env python3
"""Parse LowPowerTelemetryDutyMetrics serial logs and summarize duty metrics."""

from __future__ import annotations

import argparse
import re
import statistics
import sys
from pathlib import Path


LINE_RE = re.compile(
    r"window\s+samples=(?P<samples>\d+)\s+fail=(?P<fail>\d+)\s+"
    r"active=(?P<active>\d+\.\d)%\s+sleep=(?P<sleep>\d+\.\d)%\s+"
    r"A0=(?P<a0>-?\d+)mV\s+VBAT=(?P<vbat>-?\d+)mV"
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Summarize LowPowerTelemetryDutyMetrics output."
    )
    parser.add_argument("--log", required=True, help="Path to captured serial log file")
    return parser.parse_args()


def mean_or_zero(values: list[float]) -> float:
    return statistics.mean(values) if values else 0.0


def main() -> int:
    args = parse_args()
    log_path = Path(args.log)
    if not log_path.exists():
        print(f"log not found: {log_path}", file=sys.stderr)
        return 2

    active_values: list[float] = []
    sleep_values: list[float] = []
    sample_values: list[int] = []
    fail_values: list[int] = []
    a0_values: list[int] = []
    vbat_values: list[int] = []

    with log_path.open("r", encoding="utf-8", errors="replace") as handle:
        for raw in handle:
            match = LINE_RE.search(raw.strip())
            if not match:
                continue
            sample_values.append(int(match.group("samples")))
            fail_values.append(int(match.group("fail")))
            active_values.append(float(match.group("active")))
            sleep_values.append(float(match.group("sleep")))
            a0_values.append(int(match.group("a0")))
            vbat_values.append(int(match.group("vbat")))

    if not active_values:
        print("no telemetry windows found in log", file=sys.stderr)
        return 1

    windows = len(active_values)
    print(f"windows={windows}")
    print(f"active_mean_pct={mean_or_zero(active_values):.2f}")
    print(f"active_min_pct={min(active_values):.2f}")
    print(f"active_max_pct={max(active_values):.2f}")
    print(f"sleep_mean_pct={mean_or_zero(sleep_values):.2f}")
    print(f"samples_mean={mean_or_zero([float(v) for v in sample_values]):.2f}")
    print(f"fail_mean={mean_or_zero([float(v) for v in fail_values]):.2f}")
    print(f"a0_mean_mv={mean_or_zero([float(v) for v in a0_values]):.1f}")
    print(f"vbat_mean_mv={mean_or_zero([float(v) for v in vbat_values]):.1f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
