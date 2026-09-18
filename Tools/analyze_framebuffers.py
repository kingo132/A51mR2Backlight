#!/usr/bin/env python3
"""Generate metadata for AMDRadeonX6000Framebuffer reference binaries.

The script is intentionally self-contained and only relies on Python's stdlib.
It records hashes, Mach-O UUID/source version, symbol presence and validated
backlight-signature offsets.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import uuid
from pathlib import Path

LC_SYMTAB = 0x2
LC_UUID = 0x1B
LC_SOURCE_VERSION = 0x2A

PANEL_LEGACY = bytes.fromhex(
    "55 48 89 E5 41 57 41 56 41 55 41 54 53 50 49 89 FD 4C 8D 45"
)
PANEL_MODERN = bytes.fromhex(
    "55 48 89 E5 41 57 41 56 41 54 53 48 83 EC 10 48 89 FB 4C 8D"
)
SET_BACKLIGHT_LEGACY = bytes.fromhex(
    "55 48 89 E5 41 57 41 56 41 55 41 54 53 50 41 89 F7 49 89 FE"
)
SET_BACKLIGHT_MODERN = bytes.fromhex(
    "55 48 89 E5 41 57 41 56 41 55 41 54 53 50 41 89 F6 48 89 FB "
    "45 31 E4 4C 8D 6D D0 45 89 65 00 4C 8D 7D D4 45 89 27 48 8B "
    "7F 08 48 8B 43 28 8B 70 10 48 8B 43 30 0F B6 50 0B 48 8B 4B "
    "38 44 8B 51 28 8B 49 2C 44 0F B6 48 0A 4D 89 F8"
)

FRAMEBUFFER_SET = "__ZN35AMDRadeonX6000_AmdRadeonFramebuffer25setAttributeForConnectionEijm"
FRAMEBUFFER_GET = "__ZN35AMDRadeonX6000_AmdRadeonFramebuffer25getAttributeForConnectionEijPm"


def offsets(data: bytes, needle: bytes) -> list[int]:
    found: list[int] = []
    start = 0
    while True:
        pos = data.find(needle, start)
        if pos < 0:
            return found
        found.append(pos)
        start = pos + 1


def source_version(value: int) -> str:
    return ".".join(
        str(v)
        for v in (
            value >> 40,
            (value >> 30) & 0x3FF,
            (value >> 20) & 0x3FF,
            (value >> 10) & 0x3FF,
            value & 0x3FF,
        )
    )


def macho_metadata(data: bytes) -> tuple[dict[str, str], set[str]]:
    header = struct.unpack_from("<IiiIIIII", data, 0)
    if header[0] != 0xFEEDFACF:
        raise ValueError("expected a little-endian 64-bit Mach-O executable")

    ncmds = header[4]
    command_offset = 32
    symtab: tuple[int, int, int, int] | None = None
    metadata: dict[str, str] = {}

    for _ in range(ncmds):
        command, command_size = struct.unpack_from("<II", data, command_offset)
        if command == LC_SYMTAB:
            symtab = struct.unpack_from("<IIII", data, command_offset + 8)
        elif command == LC_UUID:
            metadata["macho_uuid"] = str(
                uuid.UUID(bytes=data[command_offset + 8 : command_offset + 24])
            ).upper()
        elif command == LC_SOURCE_VERSION:
            raw = struct.unpack_from("<Q", data, command_offset + 8)[0]
            metadata["source_version"] = source_version(raw)
        command_offset += command_size

    symbols: set[str] = set()
    if symtab is not None:
        symoff, nsyms, stroff, strsize = symtab
        strings = data[stroff : stroff + strsize]
        for i in range(nsyms):
            n_strx = struct.unpack_from("<I", data, symoff + i * 16)[0]
            if n_strx <= 0 or n_strx >= len(strings):
                continue
            end = strings.find(b"\0", n_strx)
            if end < 0:
                continue
            try:
                symbols.add(strings[n_strx:end].decode())
            except UnicodeDecodeError:
                pass

    return metadata, symbols


def choose_family(label: str) -> str:
    lowered = label.lower()
    return "modern" if any(name in lowered for name in ("sonoma", "sequoia", "tahoe")) else "legacy"


def analyze(path: Path) -> dict[str, object]:
    data = path.read_bytes()
    metadata, symbols = macho_metadata(data)
    family = choose_family(path.name)
    panel = PANEL_MODERN if family == "modern" else PANEL_LEGACY
    backlight = SET_BACKLIGHT_MODERN if family == "modern" else SET_BACKLIGHT_LEGACY

    return {
        "file": path.name,
        "size": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
        **metadata,
        "signature_family": family,
        "dce_panel_cntl_hw_init": {
            "matches": [f"0x{x:X}" for x in offsets(data, panel)],
            "symbol_present": "_dce_panel_cntl_hw_init" in symbols,
        },
        "dce_driver_set_backlight": {
            "matches": [f"0x{x:X}" for x in offsets(data, backlight)],
            "symbol_present": "_dce_driver_set_backlight" in symbols,
        },
        "framebuffer_methods": {
            "set_attribute_symbol_present": FRAMEBUFFER_SET in symbols,
            "get_attribute_symbol_present": FRAMEBUFFER_GET in symbols,
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("binaries", nargs="+", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    report = {
        "schema": 1,
        "binaries": [analyze(path) for path in args.binaries],
    }
    encoded = json.dumps(report, indent=2) + "\n"

    if args.output:
        args.output.write_text(encoded)
    else:
        print(encoded, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
