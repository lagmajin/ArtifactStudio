#!/usr/bin/env python3
"""Measure display-referred RGB clipping and alpha in 8-bit PNG captures."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib


PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def read_png(path: Path) -> tuple[int, int, int, bytes]:
    data = path.read_bytes()
    if not data.startswith(PNG_SIGNATURE):
        raise ValueError(f"{path}: invalid PNG signature")
    offset = len(PNG_SIGNATURE)
    width = height = bit_depth = color_type = None
    compressed = bytearray()
    saw_end = False
    while offset < len(data):
        if offset + 12 > len(data):
            raise ValueError(f"{path}: truncated PNG chunk")
        length = struct.unpack_from(">I", data, offset)[0]
        chunk_type = data[offset + 4:offset + 8]
        chunk_end = offset + 12 + length
        if chunk_end > len(data):
            raise ValueError(f"{path}: chunk exceeds file size")
        payload = data[offset + 8:offset + 8 + length]
        expected_crc = struct.unpack_from(">I", data, offset + 8 + length)[0]
        actual_crc = zlib.crc32(chunk_type + payload) & 0xFFFFFFFF
        if actual_crc != expected_crc:
            raise ValueError(f"{path}: CRC mismatch in {chunk_type!r}")
        if chunk_type == b"IHDR":
            if width is not None or length != 13:
                raise ValueError(f"{path}: invalid IHDR")
            width, height, bit_depth, color_type, compression, filtering, interlace = struct.unpack(
                ">IIBBBBB", payload
            )
            if width <= 0 or height <= 0 or width * height > 50_000_000:
                raise ValueError(f"{path}: invalid or excessive dimensions")
            if bit_depth != 8 or color_type not in (2, 6):
                raise ValueError(f"{path}: only 8-bit RGB/RGBA PNG is supported")
            if compression != 0 or filtering != 0 or interlace != 0:
                raise ValueError(f"{path}: unsupported PNG encoding mode")
        elif chunk_type == b"IDAT":
            if width is None:
                raise ValueError(f"{path}: IDAT precedes IHDR")
            compressed.extend(payload)
        elif chunk_type == b"tRNS":
            raise ValueError(f"{path}: palette/key transparency chunks are unsupported")
        elif chunk_type == b"IEND":
            saw_end = True
            break
        offset = chunk_end
    if width is None or height is None or bit_depth is None or color_type is None or not saw_end:
        raise ValueError(f"{path}: incomplete PNG")

    channels = 4 if color_type == 6 else 3
    row_bytes = width * channels
    try:
        raw = zlib.decompress(compressed)
    except zlib.error as error:
        raise ValueError(f"{path}: invalid compressed image data: {error}") from error
    if len(raw) != height * (row_bytes + 1):
        raise ValueError(f"{path}: unexpected decompressed image size")

    pixels = bytearray(height * row_bytes)
    previous = bytearray(row_bytes)
    source_offset = 0
    for y in range(height):
        filter_type = raw[source_offset]
        source_offset += 1
        scanline = bytearray(raw[source_offset:source_offset + row_bytes])
        source_offset += row_bytes
        for i in range(row_bytes):
            left = scanline[i - channels] if i >= channels else 0
            above = previous[i]
            upper_left = previous[i - channels] if i >= channels else 0
            if filter_type == 1:
                predictor = left
            elif filter_type == 2:
                predictor = above
            elif filter_type == 3:
                predictor = (left + above) // 2
            elif filter_type == 4:
                p = left + above - upper_left
                pa, pb, pc = abs(p - left), abs(p - above), abs(p - upper_left)
                predictor = left if pa <= pb and pa <= pc else above if pb <= pc else upper_left
            elif filter_type == 0:
                predictor = 0
            else:
                raise ValueError(f"{path}: unknown PNG filter {filter_type}")
            scanline[i] = (scanline[i] + predictor) & 0xFF
        start = y * row_bytes
        pixels[start:start + row_bytes] = scanline
        previous = scanline
    return width, height, channels, bytes(pixels)


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def _distribution(values: list[int]) -> dict[str, float | int]:
    ordered = sorted(values)
    histogram = [0] * 256
    for value in ordered:
        histogram[value] += 1
    return {
        "sampleCount": len(values),
        "min": ordered[0] / 255.0,
        "max": ordered[-1] / 255.0,
        "mean": sum(ordered) / (255.0 * len(ordered)),
        "p50": ordered[(len(ordered) - 1) // 2] / 255.0,
        "p95": ordered[int((len(ordered) - 1) * 0.95)] / 255.0,
        "p99": ordered[int((len(ordered) - 1) * 0.99)] / 255.0,
        "histogram256": histogram,
    }


def _measure(width: int, height: int, channels: int, pixels: bytes) -> dict[str, object]:
    result: dict[str, object] = {"dimensions": {"width": width, "height": height}}
    rgb: dict[str, object] = {}
    for channel, label in enumerate("RGB"):
        values = list(pixels[channel::channels])
        low = sum(value == 0 for value in values)
        high = sum(value == 255 for value in values)
        rgb[label] = {
            **_distribution(values),
            "lowClipCount": low,
            "lowClipRate": low / len(values),
            "highClipCount": high,
            "highClipRate": high / len(values),
        }
    any_clipped = 0
    for index in range(0, len(pixels), channels):
        if any(pixels[index + channel] in (0, 255) for channel in range(3)):
            any_clipped += 1
    result["displayReferredRGB"] = rgb
    result["anyRGBChannelAtEndpoint"] = {
        "pixelCount": width * height,
        "clippedPixelCount": any_clipped,
        "clippedPixelRate": any_clipped / (width * height),
        "endpointDefinition": "encoded channel byte is exactly 0 or 255",
    }
    if channels == 4:
        alpha = list(pixels[3::channels])
        result["alpha"] = _distribution(alpha)
        result["alphaCoverage"] = {
            "transparentPixelCount": sum(value == 0 for value in alpha),
            "opaquePixelCount": sum(value == 255 for value in alpha),
            "partialPixelCount": sum(0 < value < 255 for value in alpha),
        }
    else:
        result["alpha"] = None
    return result


def _parse_roi(value: str, width: int, height: int) -> dict[str, int | str]:
    parts = value.split(":", 1)
    if len(parts) != 2 or not parts[0].strip():
        raise ValueError(f"invalid ROI {value!r}; expected name:x,y,width,height")
    try:
        x, y, roi_width, roi_height = (
            int(part.strip()) for part in parts[1].split(",")
        )
    except ValueError as error:
        raise ValueError(
            f"invalid ROI {value!r}; expected name:x,y,width,height"
        ) from error
    if x < 0 or y < 0 or roi_width <= 0 or roi_height <= 0:
        raise ValueError(f"ROI must have nonnegative origin and positive size: {value!r}")
    if x + roi_width > width or y + roi_height > height:
        raise ValueError(f"ROI is outside {width}x{height} image: {value!r}")
    return {"name": parts[0].strip(), "x": x, "y": y,
            "width": roi_width, "height": roi_height}


def _crop_pixels(width: int, height: int, channels: int, pixels: bytes,
                 roi: dict[str, int | str]) -> bytes:
    row_size = int(roi["width"]) * channels
    x_offset = int(roi["x"]) * channels
    output = bytearray(int(roi["height"]) * row_size)
    for row in range(int(roi["height"])):
        source_start = (int(roi["y"]) + row) * width * channels + x_offset
        target_start = row * row_size
        output[target_start:target_start + row_size] = pixels[
            source_start:source_start + row_size
        ]
    return bytes(output)


def _parse_background(value: str) -> tuple[str, tuple[int, int, int]]:
    name, separator, encoded = value.partition(":")
    if not separator or not name.strip():
        raise ValueError(f"invalid background {value!r}; expected name:RRGGBB")
    encoded = encoded.removeprefix("#")
    if len(encoded) != 6:
        raise ValueError(f"invalid background {value!r}; expected six hex digits")
    try:
        color = tuple(int(encoded[index:index + 2], 16)
                      for index in (0, 2, 4))
    except ValueError as error:
        raise ValueError(f"invalid background color {value!r}") from error
    return name.strip(), color


def _composite_rgba(pixels: bytes, background: tuple[int, int, int],
                    alpha_association: str) -> bytes:
    output = bytearray(len(pixels) // 4 * 3)
    target = 0
    for source in range(0, len(pixels), 4):
        alpha = pixels[source + 3] / 255.0
        for channel in range(3):
            foreground = pixels[source + channel] / 255.0
            bg = background[channel] / 255.0
            value = (foreground + bg * (1.0 - alpha)
                     if alpha_association == "premultiplied"
                     else foreground * alpha + bg * (1.0 - alpha))
            output[target] = max(0, min(255, round(value * 255.0)))
            target += 1
    return bytes(output)


def _rgb_bytes(pixels: bytes, channels: int) -> bytes:
    return bytes(
        channel
        for index in range(0, len(pixels), channels)
        for channel in pixels[index:index + 3]
    )


def _rgb_difference(base: bytes, candidate: bytes) -> dict[str, float | int]:
    if len(base) != len(candidate) or not base or len(base) % 3:
        raise ValueError("RGB difference requires equally sized non-empty images")
    absolute = [abs(a - b) for a, b in zip(base, candidate)]
    pixel_count = len(absolute) // 3
    changed_pixels = sum(
        any(absolute[index + channel] for channel in range(3))
        for index in range(0, len(absolute), 3)
    )
    return {
        "sampleCount": len(absolute),
        "mae": sum(absolute) / (255.0 * len(absolute)),
        "maxAbsoluteError": max(absolute) / 255.0,
        "changedPixelRate": changed_pixels / pixel_count,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("--output", type=Path, help="write JSON report here")
    parser.add_argument(
        "--roi", action="append", default=[], metavar="NAME:X,Y,W,H",
        help="add a top-left-origin region for separate statistics and differences",
    )
    parser.add_argument(
        "--composite-background", action="append", default=[], metavar="NAME:RRGGBB",
        help="compare RGBA images composited over this encoded RGB background; repeatable",
    )
    parser.add_argument(
        "--alpha-association", choices=("straight", "premultiplied"),
        help="required with --composite-background; states how stored RGB relates to alpha",
    )
    args = parser.parse_args()
    if args.output and args.output.resolve() in {
        args.baseline.resolve(), args.candidate.resolve()
    }:
        parser.error("--output must not overwrite either input image")
    if args.composite_background and not args.alpha_association:
        parser.error("--alpha-association is required with --composite-background")
    try:
        base_width, base_height, base_channels, base_pixels = read_png(args.baseline)
        cand_width, cand_height, cand_channels, cand_pixels = read_png(args.candidate)
        if (base_width, base_height) != (cand_width, cand_height):
            raise ValueError("baseline/candidate dimensions differ")
        if base_channels != cand_channels:
            raise ValueError("baseline/candidate alpha channel presence differs")
    except (OSError, ValueError, OverflowError) as error:
        parser.error(str(error))

    report: dict[str, object] = {
        "tool": "analyze_shader_graph_png.py",
        "measurementSpace": "8-bit encoded display-referred RGB; exact 0/255 endpoint rates; alpha is the normalized stored PNG alpha byte",
        "baseline": {"path": str(args.baseline), "sha256": _sha256(args.baseline),
                     "measurements": _measure(base_width, base_height, base_channels, base_pixels)},
        "candidate": {"path": str(args.candidate), "sha256": _sha256(args.candidate),
                       "measurements": _measure(cand_width, cand_height, cand_channels, cand_pixels)},
        "alphaDifference": None,
        "limitations": [
            "Supports non-interlaced 8-bit RGB or RGBA PNG only.",
            "Palette and RGB transparency-key PNG chunks are unsupported.",
            "PNG RGB is display-referred and quantized; it is not a linear HDR quality measurement.",
            "RGB endpoint rates indicate encoded values at 0/255, not scene-linear clipping before the display transform.",
            "No quality verdict is inferred from these statistics.",
            "ROI metrics use top-left-origin encoded PNG bytes and do not export crop images.",
        ],
    }
    if base_channels == 4:
        base_alpha = base_pixels[3::4]
        cand_alpha = cand_pixels[3::4]
        differences = [abs(a - b) for a, b in zip(base_alpha, cand_alpha)]
        report["alphaDifference"] = {
            "sampleCount": len(differences),
            "mae": sum(differences) / (255.0 * len(differences)),
            "maxAbsoluteError": max(differences) / 255.0,
            "changedPixelRate": sum(value != 0 for value in differences) / len(differences),
        }
    try:
        regions = [_parse_roi(value, base_width, base_height) for value in args.roi]
    except ValueError as error:
        parser.error(str(error))
    region_reports: dict[str, object] = {}
    for region in regions:
        name = str(region["name"])
        if name in region_reports:
            parser.error(f"duplicate ROI name: {name}")
        base_crop = _crop_pixels(base_width, base_height, base_channels,
                                 base_pixels, region)
        candidate_crop = _crop_pixels(cand_width, cand_height, cand_channels,
                                       cand_pixels, region)
        region_report: dict[str, object] = {
            "bounds": {key: region[key] for key in ("x", "y", "width", "height")},
            "baseline": _measure(int(region["width"]), int(region["height"]),
                                 base_channels, base_crop),
            "candidate": _measure(int(region["width"]), int(region["height"]),
                                   cand_channels, candidate_crop),
            "storedRGBDifference": _rgb_difference(
                _rgb_bytes(base_crop, base_channels),
                _rgb_bytes(candidate_crop, cand_channels),
            ),
        }
        if base_channels == 4:
            base_alpha = base_crop[3::4]
            candidate_alpha = candidate_crop[3::4]
            alpha_error = [abs(a - b) for a, b in zip(base_alpha, candidate_alpha)]
            region_report["alphaDifference"] = {
                "sampleCount": len(alpha_error),
                "mae": sum(alpha_error) / (255.0 * len(alpha_error)),
                "maxAbsoluteError": max(alpha_error) / 255.0,
                "changedPixelRate": sum(value != 0 for value in alpha_error) / len(alpha_error),
            }
        region_reports[name] = region_report
    report["regions"] = region_reports

    composite_reports: dict[str, object] = {}
    if args.composite_background:
        if base_channels != 4:
            parser.error("--composite-background requires RGBA PNG inputs")
        assert args.alpha_association is not None
        for value in args.composite_background:
            try:
                name, background = _parse_background(value)
            except ValueError as error:
                parser.error(str(error))
            if name in composite_reports:
                parser.error(f"duplicate composite background name: {name}")
            base_composite = _composite_rgba(
                base_pixels, background, args.alpha_association
            )
            candidate_composite = _composite_rgba(
                cand_pixels, background, args.alpha_association
            )
            composite_reports[name] = {
                "backgroundRGB8": list(background),
                "alphaAssociation": args.alpha_association,
                "compositingSpace": "encoded-8-bit RGB; review-only, not linear-light",
                "difference": _rgb_difference(base_composite, candidate_composite),
            }
    report["compositeDifferences"] = composite_reports
    encoded = json.dumps(report, indent=2, allow_nan=False) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(encoded, encoding="utf-8")
    else:
        print(encoded, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
