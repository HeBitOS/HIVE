#!/usr/bin/env python3
"""Build and validate a hivepkg repository from HAX application artifacts."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import struct
from pathlib import Path

NAME = re.compile(r"^[a-z0-9][a-z0-9+._-]{0,30}$")
VERSION = re.compile(r"^[A-Za-z0-9][A-Za-z0-9+._~-]{0,22}$")
HAX_META_MAGIC = 0x4D584148


def read_hax_metadata(path: Path) -> tuple[str, str, int]:
    data = path.read_bytes()
    if len(data) < 64 or data[:6] != b"\x7fELF\x02\x01":
        raise ValueError(f"not a little-endian ELF64 HAX file: {path}")
    section_offset = struct.unpack_from("<Q", data, 0x28)[0]
    section_size = struct.unpack_from("<H", data, 0x3A)[0]
    section_count = struct.unpack_from("<H", data, 0x3C)[0]
    names_index = struct.unpack_from("<H", data, 0x3E)[0]
    if (
        section_size < 64
        or names_index >= section_count
        or section_offset + section_size * section_count > len(data)
    ):
        raise ValueError(f"invalid ELF section table: {path}")

    def section(index: int) -> tuple[int, int, int]:
        offset = section_offset + section_size * index
        return (
            struct.unpack_from("<I", data, offset)[0],
            struct.unpack_from("<Q", data, offset + 24)[0],
            struct.unpack_from("<Q", data, offset + 32)[0],
        )

    _, names_offset, names_size = section(names_index)
    if names_offset + names_size > len(data):
        raise ValueError(f"invalid ELF section-name table: {path}")
    names = data[names_offset : names_offset + names_size]
    metadata = None
    for index in range(section_count):
        name_offset, payload_offset, payload_size = section(index)
        if name_offset >= len(names):
            continue
        end = names.find(b"\0", name_offset)
        if end < 0:
            continue
        if names[name_offset:end] == b".haxmeta":
            if payload_size < 104 or payload_offset + payload_size > len(data):
                raise ValueError(f"invalid .haxmeta section: {path}")
            metadata = data[payload_offset : payload_offset + 104]
            break
    if metadata is None:
        raise ValueError(f"missing .haxmeta section: {path}")
    magic, kind = struct.unpack_from("<II", metadata)
    if magic != HAX_META_MAGIC or kind == 0:
        raise ValueError(f"invalid HAX metadata: {path}")
    name = metadata[8:40].split(b"\0", 1)[0].decode("utf-8")
    description = metadata[40:104].split(b"\0", 1)[0].decode("utf-8")
    return name, description, kind


def load_manifest(path: Path) -> tuple[str, list[dict[str, object]]]:
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("schema") != 1:
        raise ValueError("manifest schema must be 1")
    arch = data.get("architecture")
    if arch not in {"x86_64", "all"}:
        raise ValueError(f"unsupported architecture: {arch!r}")
    packages = data.get("packages")
    if not isinstance(packages, list) or not packages:
        raise ValueError("packages must be a non-empty list")
    seen: set[str] = set()
    for package in packages:
        if not isinstance(package, dict):
            raise ValueError("each package must be an object")
        name = package.get("name")
        version = package.get("version")
        description = package.get("description")
        depends = package.get("depends", [])
        if not isinstance(name, str) or not NAME.fullmatch(name):
            raise ValueError(f"invalid package name: {name!r}")
        if name in seen:
            raise ValueError(f"duplicate package name: {name}")
        seen.add(name)
        if not isinstance(version, str) or not VERSION.fullmatch(version):
            raise ValueError(f"invalid version for {name}: {version!r}")
        if not isinstance(description, str) or not description or any(
            c in description for c in "|\r\n"
        ):
            raise ValueError(f"invalid description for {name}")
        if not isinstance(depends, list) or any(
            not isinstance(dep, str) or not NAME.fullmatch(dep)
            for dep in depends
        ):
            raise ValueError(f"invalid dependencies for {name}")
    for package in packages:
        for dependency in package.get("depends", []):
            if dependency not in seen:
                raise ValueError(
                    f"{package['name']} depends on unknown package {dependency}"
                )
    return arch, packages


def build_repository(manifest: Path, apps_dir: Path, output: Path) -> int:
    arch, packages = load_manifest(manifest)
    pool = output / "pool"
    pool.mkdir(parents=True, exist_ok=True)
    records: list[str] = []
    for package in sorted(packages, key=lambda item: str(item["name"])):
        name = str(package["name"])
        version = str(package["version"])
        source = apps_dir / f"{name}.hax"
        if not source.is_file():
            raise FileNotFoundError(f"missing HAX artifact: {source}")
        hax_name, _, _ = read_hax_metadata(source)
        if hax_name != name:
            raise ValueError(
                f"HAX metadata name {hax_name!r} does not match package {name!r}"
            )
        filename = f"{name}_{version}_{arch}.hax"
        target = pool / filename
        shutil.copyfile(source, target)
        payload = target.read_bytes()
        digest = hashlib.sha256(payload).hexdigest()
        depends = ",".join(str(dep) for dep in package.get("depends", []))
        description = str(package["description"])
        records.append(
            f"{name}|{version}|{arch}|{len(payload)}|{digest}|"
            f"pool/{filename}|{depends}|{description}\n"
        )
    (output / "Packages").write_text("".join(records), encoding="utf-8")
    print(f"hivepkg repository: {len(records)} packages -> {output}")
    return len(records)


def main() -> int:
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)
    check = sub.add_parser("check", help="validate package metadata")
    check.add_argument("manifest", type=Path)
    build = sub.add_parser("build", help="generate Packages and pool/")
    build.add_argument("--manifest", type=Path, required=True)
    build.add_argument("--apps-dir", type=Path, required=True)
    build.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.command == "check":
        _, packages = load_manifest(args.manifest)
        print(f"hivepkg manifest valid: {len(packages)} packages")
        return 0
    build_repository(args.manifest, args.apps_dir, args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
