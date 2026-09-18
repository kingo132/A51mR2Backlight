#!/usr/bin/env python3
"""Verify A51mR2Backlight fallback signatures in an AMDRadeonX6000Framebuffer binary."""

from __future__ import annotations

import argparse
from pathlib import Path

SIGNATURES = {
    "dce_panel_cntl_hw_init (legacy)": bytes.fromhex(
        "55 48 89 E5 41 57 41 56 41 55 41 54 53 50 49 89 FD 4C 8D 45"
    ),
    "dce_panel_cntl_hw_init (modern)": bytes.fromhex(
        "55 48 89 E5 41 57 41 56 41 54 53 48 83 EC 10 48 89 FB 4C 8D"
    ),
    "dce_driver_set_backlight (legacy)": bytes.fromhex(
        "55 48 89 E5 41 57 41 56 41 55 41 54 53 50 41 89 F7 49 89 FE"
    ),
    "dce_driver_set_backlight (modern)": bytes.fromhex(
        "55 48 89 E5 41 57 41 56 41 55 41 54 53 50 41 89 F6 48 89 FB"
    ),
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


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", type=Path, help="path to AMDRadeonX6000Framebuffer executable")
    parser.add_argument(
        "--family",
        choices=("legacy", "modern", "all"),
        default="modern",
        help="signature family to require (Tahoe uses modern)",
    )
    args = parser.parse_args()

    data = args.binary.read_bytes()
    required = 0
    good = 0

    for name, sig in SIGNATURES.items():
        if args.family != "all" and f"({args.family})" not in name:
            continue
        required += 1
        hits = offsets(data, sig)
        positions = ", ".join(f"0x{x:X}" for x in hits) if hits else "none"
        state = "OK" if len(hits) == 1 else "UNSAFE"
        print(f"{state:6}  {name}: {len(hits)} match(es) [{positions}]")
        good += len(hits) == 1

    if good == required:
        print("\nPASS: every required fallback signature is unique.")
        return 0

    print("\nFAIL: do not rely on signature fallback for this binary.")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
