#!/usr/bin/env python3
"""Summarize BleChannelSoundingVprServicePowerProbe output."""

from __future__ import annotations

import argparse
import re
import statistics
import sys
from pathlib import Path


LINE_RE = re.compile(
    r"power_probe ok=(?P<ok>[01])\s+svc=(?P<svc>\d+\.\d+)\s+opmask=0x(?P<opmask>[0-9A-Fa-f]+)\s+"
    r"phase_ms=(?P<idle>\d+)\/(?P<conn>\d+)\/(?P<cs>\d+)\/(?P<final>\d+)\s+"
    r"total_ms=(?P<total>\d+)\s+cs_gap_ms=(?P<gap>\d+)\s+cs_runs=(?P<runs>\d+)\s+proc=(?P<proc>\d+)\s+"
    r"last_q4=(?P<q4>\d+)\s+last_nominal_dist_m=(?P<dist>\d+\.\d+)\s+"
    r"last_sub=(?P<lsub>\d+)\/(?P<psub>\d+)\s+last_steps=(?P<lsteps>\d+)\/(?P<psteps>\d+)\s+"
    r"final=(?P<fconn>[01])\/(?P<fbind>[01])\/(?P<frun>[01])\/(?P<fconf>[01])\/(?P<fen>[01])#(?P<reason>[0-9A-Fa-f]+)"
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Summarize BleChannelSoundingVprServicePowerProbe serial logs."
    )
    parser.add_argument("--log", required=True, help="Captured serial log file")
    return parser.parse_args()


def mean_or_zero(values: list[float]) -> float:
    return statistics.mean(values) if values else 0.0


def main() -> int:
    args = parse_args()
    log_path = Path(args.log)
    if not log_path.is_file():
        print(f"log not found: {log_path}", file=sys.stderr)
        return 2

    rows: list[dict[str, str]] = []
    with log_path.open("r", encoding="utf-8", errors="replace") as handle:
        for raw in handle:
            match = LINE_RE.search(raw.strip())
            if match:
                rows.append(match.groupdict())

    if not rows:
        print("no power_probe summary lines found", file=sys.stderr)
        return 1

    idle = [float(row["idle"]) for row in rows]
    conn = [float(row["conn"]) for row in rows]
    cs = [float(row["cs"]) for row in rows]
    final = [float(row["final"]) for row in rows]
    total = [float(row["total"]) for row in rows]
    runs = [float(row["runs"]) for row in rows]
    proc = [float(row["proc"]) for row in rows]
    dist = [float(row["dist"]) for row in rows]

    latest = rows[-1]
    print(f"summaries={len(rows)}")
    print(f"latest_ok={latest['ok']}")
    print(f"service_version={latest['svc']}")
    print(f"opmask=0x{latest['opmask']}")
    print(f"idle_mean_ms={mean_or_zero(idle):.1f}")
    print(f"connected_mean_ms={mean_or_zero(conn):.1f}")
    print(f"cs_mean_ms={mean_or_zero(cs):.1f}")
    print(f"final_idle_mean_ms={mean_or_zero(final):.1f}")
    print(f"total_mean_ms={mean_or_zero(total):.1f}")
    print(f"cs_gap_ms={latest['gap']}")
    print(f"cs_runs_mean={mean_or_zero(runs):.2f}")
    print(f"completed_proc_mean={mean_or_zero(proc):.2f}")
    print(f"last_nominal_dist_mean_m={mean_or_zero(dist):.4f}")
    print(f"latest_last_sub={latest['lsub']}/{latest['psub']}")
    print(f"latest_last_steps={latest['lsteps']}/{latest['psteps']}")
    print(
        "latest_final="
        f"{latest['fconn']}/{latest['fbind']}/{latest['frun']}/"
        f"{latest['fconf']}/{latest['fen']}#{latest['reason']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
