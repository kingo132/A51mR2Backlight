#!/usr/bin/env python3
"""Verify A51mR2Backlight fallback signatures in AMDRadeonX6000Framebuffer."""

from __future__ import annotations

import argparse
from pathlib import Path

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

SIGNATURES = {
    "legacy": {
        "dce_panel_cntl_hw_init": PANEL_LEGACY,
        "dce_driver_set_backlight": SET_BACKLIGHT_LEGACY,
    },
    "modern": {
        "dce_panel_cntl_hw_init": PANEL_MODERN,
        "dce_driver_set_backlight": SET_BACKLIGHT_MODERN,
    },
}


def offsets(data: bytes, needle: bytes) -> list[int]:
    result: list[int] = []
    start = 0
    while True:
        i = data.find(needle, start)
        if i < 0:
            return result
        result.append(i)
        start = i + 1


def verify(binary: Path, family: str) -> bool:
    data = binary.read_bytes()
    print(f"{binary}:")
    good = True

    for name, signature in SIGNATURES[family].items():
        hits = offsets(data, signature)
        positions = ", ".join(f"0x{x:X}" for x in hits) if hits else "none"
        state = "OK" if len(hits) == 1 else "UNSAFE"
        print(
            f"  {state:6} {name:28} {len(signature):3} bytes  "
            f"{len(hits)} match(es) [{positions}]"
        )
        good &= len(hits) == 1

    return good


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "binary",
        type=Path,
        nargs="+",
        help="one or more AMDRadeonX6000Framebuffer executables",
    )
    parser.add_argument(
        "--family",
        choices=("legacy", "modern"),
        default="modern",
        help="signature family (Big Sur/Monterey/Ventura: legacy; Sonoma+: modern)",
    )
    args = parser.parse_args()

    ok = True
    for i, binary in enumerate(args.binary):
        if i:
            print()
        ok &= verify(binary, args.family)

    if ok:
        print("\nPASS: every required fallback signature is unique.")
        return 0

    print("\nFAIL: at least one fallback signature is missing or ambiguous.")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
