"""Measure RGB edge profiles from source-subtracted LensFlareLab PFM renders.

Example:
  python tools/LensFlareLab/measure_ghost_edge_profiles.py \
      --case point build/point-input.pfm build/point-ghost.pfm \
      --case extended build/extended-input.pfm build/extended-ghost.pfm \
      --output build/ghost-edge-profiles.csv

The profile averages a horizontal sensor edge over a configurable row range.
It reports threshold crossings and local fringe strength, not a fitted optical
PSF or a physical sensor response.
"""

import argparse
import csv
import math
import struct
from pathlib import Path


CHANNELS = ("R", "G", "B")


def read_pfm(path):
    with path.open("rb") as image:
        if image.readline().strip() != b"PF":
            raise ValueError(f"expected RGB PFM: {path}")
        width, height = map(int, image.readline().split())
        scale = float(image.readline())
        payload = image.read()
    count = width * height * 3
    if scale == 0 or not math.isfinite(scale) or len(payload) != count * 4:
        raise ValueError(f"invalid PFM payload: {path}")
    values = struct.unpack(("<" if scale < 0 else ">") + f"{count}f", payload)
    factor = abs(scale)
    return width, height, [tuple(value * factor for value in
                                 values[row * width * 3:(row + 1) * width * 3])
                           for row in reversed(range(height))]


def edge_crossing(profile, plateau, fraction, start, end):
    threshold = plateau * fraction
    for x in range(start, end):
        if profile[x] >= threshold and profile[x + 1] < threshold:
            denominator = profile[x] - profile[x + 1]
            if denominator > 0:
                return x + (profile[x] - threshold) / denominator
    return float("nan")


def measure_case(source_path, render_path, row_range, plateau_range,
                 search_range, fringe_range):
    source, rendered = read_pfm(source_path), read_pfm(render_path)
    if source[:2] != rendered[:2]:
        raise ValueError(f"image dimensions differ: {source_path} / {render_path}")
    width, height = source[:2]
    y0, y1 = row_range
    px0, px1 = plateau_range
    sx0, sx1 = search_range
    fx0, fx1 = fringe_range
    if not (0 <= y0 < y1 <= height and 0 <= px0 < px1 <= width and
            0 <= sx0 < sx1 < width and 0 <= fx0 < fx1 <= width):
        raise ValueError("profile ranges must be increasing and inside the image")

    profiles = [[0.0] * width for _ in CHANNELS]
    row_count = y1 - y0
    for y in range(y0, y1):
        source_row, rendered_row = source[2][y], rendered[2][y]
        for x in range(width):
            offset = x * 3
            for channel in range(3):
                profiles[channel][x] += max(
                    0.0, rendered_row[offset + channel] - source_row[offset + channel]) / row_count

    measured = []
    for channel, profile in zip(CHANNELS, profiles):
        plateau = sum(profile[px0:px1]) / (px1 - px0)
        if plateau <= 1e-30:
            raise ValueError(f"no ghost signal in plateau for {render_path} channel {channel}")
        x90 = edge_crossing(profile, plateau, 0.9, sx0, sx1)
        x50 = edge_crossing(profile, plateau, 0.5, sx0, sx1)
        x10 = edge_crossing(profile, plateau, 0.1, sx0, sx1)
        fringe_peak = max(profile[fx0:fx1]) / plateau * 100.0
        measured.append({"channel": channel, "plateau_radiance": plateau,
                         "x90_px": x90, "x50_px": x50, "x10_px": x10,
                         "transition_10_90_px": x10 - x90,
                         "fringe_window_peak_percent": fringe_peak})
    x50_values = [item["x50_px"] for item in measured]
    spread = max(x50_values) - min(x50_values)
    for item in measured:
        item["rgb_x50_separation_px"] = spread
    return measured


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", action="append", nargs=3, required=True,
                        metavar=("LABEL", "SOURCE_PFM", "RENDER_PFM"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--average-rows", type=int, nargs=2, default=(249, 266),
                        metavar=("TOP", "BOTTOM"), help="half-open sensor row range")
    parser.add_argument("--plateau-columns", type=int, nargs=2, default=(540, 561),
                        metavar=("LEFT", "RIGHT"), help="interior reference window")
    parser.add_argument("--search-columns", type=int, nargs=2, default=(540, 599),
                        metavar=("LEFT", "RIGHT"), help="descending edge search window")
    parser.add_argument("--fringe-columns", type=int, nargs=2, default=(571, 591),
                        metavar=("LEFT", "RIGHT"), help="outer-edge fringe peak window")
    args = parser.parse_args()

    rows = []
    for label, source_name, render_name in args.case:
        measurements = measure_case(Path(source_name), Path(render_name), args.average_rows,
                                    args.plateau_columns, args.search_columns,
                                    args.fringe_columns)
        for item in measurements:
            rows.append({"case": label, **item})
        print(f"{label}: RGB x50 separation={measurements[0]['rgb_x50_separation_px']:.3f} px; "
              "10-90 width=" + "/".join(f"{item['transition_10_90_px']:.2f}"
                                          for item in measurements) + " px (R/G/B)")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=("case", "channel", "plateau_radiance",
            "x90_px", "x50_px", "x10_px", "transition_10_90_px",
            "fringe_window_peak_percent", "rgb_x50_separation_px"))
        writer.writeheader()
        writer.writerows(rows)
    print(f"wrote {args.output}")


if __name__ == "__main__":
    main()
