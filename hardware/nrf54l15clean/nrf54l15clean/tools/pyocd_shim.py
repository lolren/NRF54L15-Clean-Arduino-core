#!/usr/bin/env python3
from __future__ import annotations

import runpy
import sys
from pathlib import Path


def main() -> int:
    tool_root = Path(__file__).resolve().parent
    site_dir = tool_root / "runtime" / "pyocd-site"
    if not site_dir.is_dir():
        print(
            f"pyocd runtime not installed under {site_dir}. "
            "Run setup/install_linux_host_deps.sh --python first.",
            file=sys.stderr,
        )
        return 2

    sys.path.insert(0, str(site_dir))
    runpy.run_module("pyocd.__main__", run_name="__main__")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
