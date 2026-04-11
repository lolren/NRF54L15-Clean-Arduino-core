#!/usr/bin/env python3
"""Build Arduino Board Manager release artifacts for XIAO nRF54L15 boards."""

from __future__ import annotations

import argparse
import hashlib
import json
import tarfile
import zipfile
from pathlib import Path


HOST_TOOL_NAME = "nrf54l15hosttools"
HOST_TOOL_VERSION = "1.0.0"
HOST_TOOL_HOSTS = (
    ("x86_64-pc-linux-gnu", ".tar.bz2"),
    ("aarch64-linux-gnu", ".tar.bz2"),
    ("i686-mingw32", ".zip"),
    ("x86_64-apple-darwin", ".tar.bz2"),
    ("arm64-apple-darwin", ".tar.bz2"),
)


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        while True:
            chunk = f.read(1024 * 1024)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


def normalize_mode(path: Path) -> int:
    return 0o755 if path.is_dir() or (path.stat().st_mode & 0o111) else 0o644


def build_archive(source_dir: Path, archive_path: Path, archive_root: str) -> None:
    if archive_path.exists():
        archive_path.unlink()

    if archive_path.suffix == ".zip":
        with zipfile.ZipFile(archive_path, mode="w", compression=zipfile.ZIP_DEFLATED) as zf:
            entries = [source_dir, *sorted(source_dir.rglob("*"))]
            for child in entries:
                rel = "" if child == source_dir else child.relative_to(source_dir).as_posix()
                arcname = archive_root if not rel else f"{archive_root}/{rel}"
                info = zipfile.ZipInfo(arcname + ("/" if child.is_dir() else ""))
                info.date_time = (1980, 1, 1, 0, 0, 0)
                info.create_system = 3
                info.external_attr = normalize_mode(child) << 16
                if child.is_dir():
                    zf.writestr(info, "")
                else:
                    zf.writestr(info, child.read_bytes())
        return

    def normalize_tarinfo(ti: tarfile.TarInfo) -> tarfile.TarInfo:
        ti.uid = 0
        ti.gid = 0
        ti.uname = ""
        ti.gname = ""
        ti.mtime = 0
        ti.mode = normalize_mode(Path(ti.name))
        return ti

    def add_path(tar: tarfile.TarFile, fs_path: Path, arcname: str) -> None:
        tarinfo = tar.gettarinfo(str(fs_path), arcname=arcname)
        tarinfo.uid = 0
        tarinfo.gid = 0
        tarinfo.uname = ""
        tarinfo.gname = ""
        tarinfo.mtime = 0
        tarinfo.mode = normalize_mode(fs_path)
        if fs_path.is_file():
            with fs_path.open("rb") as f:
                tar.addfile(tarinfo, f)
            return
        tar.addfile(tarinfo)

    with tarfile.open(archive_path, mode="w:bz2") as tar:
        add_path(tar, source_dir, archive_root)
        for child in sorted(source_dir.rglob("*")):
            rel = child.relative_to(source_dir).as_posix()
            add_path(tar, child, f"{archive_root}/{rel}")


def version_sort_key(version: str) -> tuple:
    parts = []
    for token in version.split("."):
        if token.isdigit():
            parts.append((0, int(token)))
        else:
            parts.append((1, token))
    return tuple(parts)


def parse_version(version: str) -> tuple[int, int, int]:
    parts = version.strip().split(".")
    if len(parts) != 3 or any(not part.isdigit() for part in parts):
        raise SystemExit(f"Unsupported version format: {version!r}")
    return int(parts[0]), int(parts[1]), int(parts[2])


def update_core_version_header(platform_dir: Path, version: str) -> None:
    major, minor, patch = parse_version(version)
    header_path = platform_dir / "cores" / "nrf54l15" / "CoreVersionGenerated.h"
    content = f"""#ifndef NRF54L15_CLEAN_CORE_VERSION_GENERATED_H
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
"""
    header_path.write_text(content, encoding="utf-8")


def make_platform_entry(
    platform_name: str,
    archive_name: str,
    archive_url: str,
    archive_sha256: str,
    archive_size: int,
    version: str,
    repo_url: str,
    architecture: str,
) -> dict:
    return {
        "name": platform_name,
        "architecture": architecture,
        "version": version,
        "category": "Arduino",
        "url": archive_url,
        "archiveFileName": archive_name,
        "checksum": f"SHA-256:{archive_sha256}",
        "size": str(archive_size),
        "boards": [{"name": "XIAO nRF54L15 / Sense"}],
        "help": {"online": repo_url},
        "toolsDependencies": [
            {"packager": "arduino", "name": "arm-none-eabi-gcc", "version": "7-2017q4"},
            {"packager": "arduino", "name": "openocd", "version": "0.11.0-arduino2"},
            {"packager": architecture, "name": HOST_TOOL_NAME, "version": HOST_TOOL_VERSION},
        ],
        "discoveryDependencies": [],
        "monitorDependencies": [],
    }


def make_tool_system_entry(host: str, archive_name: str, archive_url: str, archive_sha256: str, archive_size: int) -> dict:
    return {
        "host": host,
        "url": archive_url,
        "archiveFileName": archive_name,
        "checksum": f"SHA-256:{archive_sha256}",
        "size": str(archive_size),
    }


def make_index(packager: str, repo_url: str) -> dict:
    return {
        "packages": [
            {
                "name": packager,
                "maintainer": "XIAO nRF54L15",
                "websiteURL": repo_url,
                "email": "",
                "help": {"online": repo_url},
                "platforms": [],
                "tools": [],
            }
        ]
    }


def load_existing_index(index_path: Path, packager: str, repo_url: str) -> dict:
    if not index_path.is_file():
        return make_index(packager=packager, repo_url=repo_url)

    data = json.loads(index_path.read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        raise SystemExit(f"Unexpected package index structure in {index_path}")

    packages = data.get("packages")
    if not isinstance(packages, list) or not packages or not isinstance(packages[0], dict):
        return make_index(packager=packager, repo_url=repo_url)

    package = packages[0]
    package["name"] = packager
    package["maintainer"] = "XIAO nRF54L15"
    package["websiteURL"] = repo_url
    package["email"] = ""
    package["help"] = {"online": repo_url}
    if not isinstance(package.get("platforms"), list):
        package["platforms"] = []
    if not isinstance(package.get("tools"), list):
        package["tools"] = []
    return data


def merge_platform(index: dict, platform_entry: dict) -> dict:
    package = index.setdefault("packages", [{}])[0]
    platforms = [p for p in package.get("platforms", []) if isinstance(p, dict) and p.get("version") != platform_entry["version"]]
    platforms.append(platform_entry)
    platforms.sort(key=lambda p: version_sort_key(str(p.get("version", ""))), reverse=True)
    package["platforms"] = platforms
    return index


def merge_tool(index: dict, tool_entry: dict) -> dict:
    package = index.setdefault("packages", [{}])[0]
    tools = [t for t in package.get("tools", []) if not (isinstance(t, dict) and t.get("name") == tool_entry["name"] and t.get("version") == tool_entry["version"])]
    tools.append(tool_entry)
    tools.sort(key=lambda t: (str(t.get("name", "")), version_sort_key(str(t.get("version", "")))))
    package["tools"] = tools
    return index


def prune_platforms(index: dict, keep: int) -> dict:
    if keep <= 0:
        return index
    data = json.loads(json.dumps(index))
    package = data["packages"][0]
    package["platforms"] = package.get("platforms", [])[:keep]
    return data


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", default=Path(__file__).resolve().parents[1], type=Path)
    parser.add_argument("--version", default=None)
    parser.add_argument("--source-version", default=None)
    parser.add_argument("--packager", default="nrf54l15clean")
    parser.add_argument("--platform-name", default="XIAO nRF54L15 Boards")
    parser.add_argument("--release-base-url", default="https://github.com/REPLACE_ME/REPLACE_ME/releases/download/v{version}")
    parser.add_argument("--repo-url", default="https://github.com/REPLACE_ME/REPLACE_ME")
    parser.add_argument("--dist-dir", default=None, type=Path)
    parser.add_argument("--existing-index", default=None, type=Path)
    parser.add_argument("--archive-index", default=None, type=Path)
    parser.add_argument("--stable-keep", default=12, type=int)
    return parser.parse_args()


def detect_source_version(root: Path, packager: str) -> str:
    hardware_root = root / "hardware" / packager
    if not hardware_root.is_dir():
        raise SystemExit(f"Hardware root not found: {hardware_root}")
    dirs = sorted(p.name for p in hardware_root.iterdir() if p.is_dir())
    if not dirs:
        raise SystemExit(f"No platform source directories found under: {hardware_root}")
    return packager if packager in dirs else dirs[-1]


def read_platform_version(platform_dir: Path) -> str:
    platform_txt = platform_dir / "platform.txt"
    if not platform_txt.is_file():
        raise SystemExit(f"platform.txt not found: {platform_txt}")
    for line in platform_txt.read_text(encoding="utf-8").splitlines():
        if line.startswith("version="):
            value = line.split("=", 1)[1].strip()
            if value:
                return value
    raise SystemExit(f"version=... not found in {platform_txt}")


def main() -> int:
    args = parse_args()
    root = args.root.resolve()
    dist_dir = args.dist_dir.resolve() if args.dist_dir else (root / "dist")
    dist_dir.mkdir(parents=True, exist_ok=True)

    source_version = args.source_version if args.source_version else detect_source_version(root=root, packager=args.packager)
    platform_dir = root / "hardware" / args.packager / source_version
    if not platform_dir.is_dir():
        raise SystemExit(f"Platform directory not found: {platform_dir}")

    version = args.version if args.version else read_platform_version(platform_dir)
    update_core_version_header(platform_dir, version)

    release_base_url = args.release_base_url.format(version=version)

    archive_name = f"{args.packager}-{version}.tar.bz2"
    archive_path = dist_dir / archive_name
    build_archive(platform_dir, archive_path, f"{args.packager}-{version}")
    archive_sha256 = sha256_file(archive_path)
    archive_size = archive_path.stat().st_size

    host_tools_src = root / "tools" / "board_manager" / HOST_TOOL_NAME
    if not host_tools_src.is_dir():
        raise SystemExit(f"Host-tools source directory not found: {host_tools_src}")

    tool_systems = []
    for host, ext in HOST_TOOL_HOSTS:
        tool_archive_name = f"{HOST_TOOL_NAME}-{HOST_TOOL_VERSION}-{host}{ext}"
        tool_archive_path = dist_dir / tool_archive_name
        build_archive(host_tools_src, tool_archive_path, f"{HOST_TOOL_NAME}-{HOST_TOOL_VERSION}")
        tool_systems.append(
            make_tool_system_entry(
                host=host,
                archive_name=tool_archive_name,
                archive_url=f"{release_base_url}/{tool_archive_name}",
                archive_sha256=sha256_file(tool_archive_path),
                archive_size=tool_archive_path.stat().st_size,
            )
        )

    tool_entry = {"name": HOST_TOOL_NAME, "version": HOST_TOOL_VERSION, "systems": tool_systems}

    full_index_path = args.archive_index.resolve() if args.archive_index else (root / f"package_{args.packager}_archive_index.json")
    if full_index_path.is_file():
        existing_full = load_existing_index(full_index_path, packager=args.packager, repo_url=args.repo_url)
    else:
        fallback_index = args.existing_index.resolve() if args.existing_index else (root / f"package_{args.packager}_index.json")
        existing_full = load_existing_index(fallback_index, packager=args.packager, repo_url=args.repo_url)

    platform_entry = make_platform_entry(
        platform_name=args.platform_name,
        archive_name=archive_name,
        archive_url=f"{release_base_url}/{archive_name}",
        archive_sha256=archive_sha256,
        archive_size=archive_size,
        version=version,
        repo_url=args.repo_url,
        architecture=args.packager,
    )

    full_index = merge_tool(merge_platform(existing_full, platform_entry), tool_entry)
    stable_index = prune_platforms(full_index, keep=args.stable_keep)

    (dist_dir / f"package_{args.packager}_index.json").write_text(json.dumps(stable_index, indent=2) + "\n", encoding="utf-8")
    (dist_dir / f"package_{args.packager}_stable_index.json").write_text(json.dumps(stable_index, indent=2) + "\n", encoding="utf-8")
    (dist_dir / f"package_{args.packager}_archive_index.json").write_text(json.dumps(full_index, indent=2) + "\n", encoding="utf-8")

    print(f"platform archive: {archive_path}")
    print(f"platform sha256:  {archive_sha256}")
    print(f"platform size:    {archive_size}")
    for system in tool_systems:
        print(f"tool archive:     {dist_dir / system['archiveFileName']}")
    print(f"stable index:     {dist_dir / f'package_{args.packager}_index.json'}")
    print(f"archive index:    {dist_dir / f'package_{args.packager}_archive_index.json'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
