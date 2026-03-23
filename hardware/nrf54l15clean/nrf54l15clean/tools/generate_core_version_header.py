#!/usr/bin/env python3
"""Generate CoreVersionGenerated.h from the package version string."""

import argparse
import re
from pathlib import Path


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", required=True, help="Package version string, e.g. 0.1.86")
    parser.add_argument("--out", required=True, type=Path, help="Output header path")
    return parser.parse_args()


def parse_version(version):
    match = re.fullmatch(r"\s*(\d+)\.(\d+)\.(\d+)\s*", version)
    if not match:
        raise SystemExit(
            "Unsupported version format {!r}; expected major.minor.patch".format(version)
        )
    return tuple(int(group) for group in match.groups())


def render_header(version, major, minor, patch):
    return """#ifndef NRF54L15_CLEAN_CORE_VERSION_GENERATED_H
#define NRF54L15_CLEAN_CORE_VERSION_GENERATED_H

#define ARDUINO_NRF54L15_CLEAN_VERSION_MAJOR {major}
#define ARDUINO_NRF54L15_CLEAN_VERSION_MINOR {minor}
#define ARDUINO_NRF54L15_CLEAN_VERSION_PATCH {patch}

#define ARDUINO_NRF54L15_CLEAN_VERSION_ENCODE(major, minor, patch) \\
    (((major) * 10000UL) + ((minor) * 100UL) + (patch))

#define ARDUINO_NRF54L15_CLEAN_VERSION \\
    ARDUINO_NRF54L15_CLEAN_VERSION_ENCODE( \\
        ARDUINO_NRF54L15_CLEAN_VERSION_MAJOR, \\
        ARDUINO_NRF54L15_CLEAN_VERSION_MINOR, \\
        ARDUINO_NRF54L15_CLEAN_VERSION_PATCH)

#define ARDUINO_NRF54L15_CLEAN_VERSION_STRING "{version}"

#endif  // NRF54L15_CLEAN_CORE_VERSION_GENERATED_H
""".format(version=version, major=major, minor=minor, patch=patch)


def main():
    args = parse_args()
    major, minor, patch = parse_version(args.version)
    # Path.resolve() on Python 3.5 raises FileNotFoundError when the target
    # file does not yet exist. Resolve only the parent (which must exist or be
    # created), then re-attach the filename. This is compatible with Python >= 3.4.
    parent = args.out.parent
    parent.mkdir(parents=True, exist_ok=True)
    output_path = parent.resolve() / args.out.name
    new_content = render_header(args.version, major, minor, patch)
    if output_path.exists():
        try:
            existing = output_path.read_text(encoding="utf-8")
            if existing == new_content:
                return 0
        except IOError:
            pass
    output_path.write_text(new_content, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
