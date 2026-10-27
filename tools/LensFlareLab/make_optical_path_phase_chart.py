"""Render wrapped optical-path phase maps from LensFlareLab CSV exports."""

import argparse
import colorsys
import csv
import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


def read_map(path):
    rows = []
    with Path(path).open(newline="", encoding="utf-8") as source:
        for row in csv.DictReader(source):
            rows.append(row)
    if not rows:
        raise ValueError(f"empty optical path map: {path}")
    grids = {}
    for row in rows:
        wavelength = int(round(float(row["wavelength_nm"])))
        u = float(row["pupil_u"])
        v = float(row["pupil_v"])
        grids.setdefault(wavelength, []).append(
            (u, v, int(row["valid"]), float(row["phase_cycles"]))
        )
    return grids


def phase_step_summary(samples, resolution, wavelength):
    cells = {}
    for u, v, valid, cycles in samples:
        x = round((u + 1.0) * resolution * 0.5 - 0.5)
        y = round((v + 1.0) * resolution * 0.5 - 0.5)
        cells[x, y] = cycles if valid else None
    steps = []
    for (x, y), cycles in cells.items():
        if cycles is None:
            continue
        for neighbor in ((x + 1, y), (x, y + 1)):
            other = cells.get(neighbor)
            if other is not None:
                steps.append(abs(cycles - other))
    if not steps:
        return (math.nan, math.nan, math.nan)
    steps.sort()
    percentile = lambda p: steps[min(len(steps) - 1, int((len(steps) - 1) * p))]
    return percentile(0.50), percentile(0.90), steps[-1]


def render_phase_grid(samples, resolution, scale):
    image = Image.new("RGB", (resolution * scale, resolution * scale), (18, 20, 24))
    for u, v, valid, cycles in samples:
        if not valid:
            continue
        x = min(resolution - 1, max(0, int((u + 1.0) * 0.5 * resolution)))
        y = min(resolution - 1, max(0, int((1.0 - v) * 0.5 * resolution)))
        phase = cycles % 1.0
        red, green, blue = colorsys.hsv_to_rgb(phase, 0.92, 0.96)
        color = (int(red * 255), int(green * 255), int(blue * 255))
        x0, y0 = x * scale, y * scale
        image.paste(color, (x0, y0, x0 + scale, y0 + scale))
    return image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", action="append", required=True, metavar="LABEL=CSV",
                        help="ray-grid label and --optical-path-map CSV (repeat 4 times)")
    parser.add_argument("output", type=Path, help="PNG image plate")
    args = parser.parse_args()
    cases = []
    for item in args.case:
        if "=" not in item:
            parser.error("each --case must be LABEL=CSV")
        label, filename = item.split("=", 1)
        maps = read_map(filename)
        reference = maps.get(550)
        if reference is None:
            parser.error(f"missing 550 nm rows in {filename}")
        resolution = round(len(reference) ** 0.5)
        if resolution * resolution != len(reference):
            parser.error(f"map dimensions are not square in {filename}")
        cases.append((label, resolution, maps))
    if len(cases) != 4:
        parser.error("exactly four --case arguments are required")

    for label, resolution, maps in cases:
        for wavelength in (610, 550, 460):
            p50, p90, maximum = phase_step_summary(
                maps[wavelength], resolution, wavelength)
            print(f"{label} {wavelength}nm adjacent valid pupil-ray phase step "
                  f"(cycles): p50={p50:.2f}, p90={p90:.2f}, max={maximum:.2f}")

    scale = max(1, min(4, 512 // max(resolution for _, resolution, _ in cases)))
    panel = 512
    margin = 18
    label_height = 28
    row_label_width = 70
    width = margin * 2 + row_label_width + 4 * (panel + margin)
    height = margin * 2 + label_height + 3 * (panel + label_height + margin)
    chart = Image.new("RGB", (width, height), (24, 27, 32))
    draw = ImageDraw.Draw(chart)
    font = ImageFont.load_default()
    for column, (label, resolution, maps) in enumerate(cases):
        x = margin + row_label_width + column * (panel + margin)
        draw.text((x, margin), f"{label} ({resolution}²)", fill=(230, 234, 240), font=font)
        for row_index, wavelength in enumerate((610, 550, 460)):
            y = margin + label_height + row_index * (panel + label_height + margin)
            if column == 0:
                draw.text((margin, y + panel // 2), f"{wavelength} nm", fill=(230, 234, 240), font=font)
            panel_image = render_phase_grid(maps[wavelength], resolution, scale)
            panel_image = panel_image.resize((panel, panel), Image.Resampling.NEAREST)
            chart.paste(panel_image, (x, y))
            draw.rectangle((x, y, x + panel - 1, y + panel - 1), outline=(92, 98, 108))

    args.output.parent.mkdir(parents=True, exist_ok=True)
    chart.save(args.output)
    print(f"Saved wrapped optical-path phase chart to {args.output}")


if __name__ == "__main__":
    main()
