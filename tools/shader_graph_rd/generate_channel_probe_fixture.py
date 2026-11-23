#!/usr/bin/env python3
"""Generate a deterministic 4x4 RGBA channel-probe PNG fixture."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import struct
import zlib


def _chunk(kind: bytes, payload: bytes) -> bytes:
    crc = zlib.crc32(kind + payload) & 0xFFFFFFFF
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", crc)


def encode_probe_png() -> bytes:
    width = height = 4
    scanlines = bytearray()
    for y in range(height):
        scanlines.append(0)  # PNG filter: None
        for x in range(width):
            # Each axis and channel has a distinct response; alpha spans 0..255.
            scanlines.extend((x * 85, y * 85, (x + y) * 42, x * 85))
    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return (
        b"\x89PNG\r\n\x1a\n"
        + _chunk(b"IHDR", header)
        + _chunk(b"IDAT", zlib.compress(bytes(scanlines), level=9))
        + _chunk(b"IEND", b"")
    )


def pixel_golden() -> dict[str, object]:
    return {
        "width": 4,
        "height": 4,
        "pixelOrder": "top-to-bottom, left-to-right",
        "channels": "RGBA uint8",
        "uvMapping": "intentionally unspecified; image-origin and sampling convention are runtime evaluation targets",
        "pixels": [
            [[x * 85, y * 85, (x + y) * 42, x * 85] for x in range(4)]
            for y in range(4)
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path(__file__).resolve().parents[2]
        / "tests/fixtures/shader-graph-rd/textures/channel_probe_4x4_rgba.png",
    )
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(encode_probe_png())
    golden_path = args.output.with_name("channel_probe_4x4_rgba_pixels.json")
    golden_path.write_text(
        json.dumps(pixel_golden(), indent=2) + "\n", encoding="utf-8"
    )
    print(f"Wrote {args.output}")
    print(f"Wrote {golden_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
