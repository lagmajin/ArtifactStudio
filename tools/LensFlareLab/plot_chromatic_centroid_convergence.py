"""Plot chromatic ghost-centroid spread error against 61-band reference."""

import argparse
import csv
import math
import statistics
from collections import defaultdict
from pathlib import Path

from PIL import Image, ImageDraw

BANDS = (3, 7, 15, 31)
COLORS = {"median": (103, 202, 255), "maximum": (255, 145, 105)}


def summarize(path):
    cases = defaultdict(dict)
    with path.open(newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            bands = int(row["bands"])
            if bands not in (*BANDS, 61):
                continue
            flux = sum(float(row[key]) for key in ("flux_r", "flux_g", "flux_b"))
            if flux <= 1e-30:
                continue
            key = (row["coating"], row["position"], row["path"])
            cases[key][bands] = float(row["max_rgb_centroid_separation_px"])
    errors = defaultdict(list)
    for (coating, position, ghost_path), values in cases.items():
        if 61 not in values or any(band not in values for band in BANDS):
            raise ValueError(f"incomplete case {coating} {position} {ghost_path}")
        for bands in BANDS:
            errors[(coating, bands)].append((position, ghost_path,
                abs(values[bands] - values[61])))
    result = []
    for coating in ("off", "on"):
        counts = {len(errors[(coating, bands)]) for bands in BANDS}
        if len(counts) != 1 or not counts or next(iter(counts)) == 0:
            raise ValueError(f"inconsistent valid cases for {coating}: {counts}")
        for bands in BANDS:
            values = [row[2] for row in errors[(coating, bands)]]
            result.append({"coating": coating, "bands": bands,
                "valid_cases": len(values),
                "median_abs_spread_error_px": statistics.median(values),
                "maximum_abs_spread_error_px": max(values)})
    return result


def make_chart(rows, path):
    panel_w, panel_h = 550, 500
    image = Image.new("RGB", (2 * panel_w + 48, panel_h + 112), (18, 21, 26))
    draw = ImageDraw.Draw(image)
    low, high = 0.005, 100.0
    for panel, coating in enumerate(("off", "on")):
        x0, y0 = 24 + panel * (panel_w + 24), 58
        plot = (x0 + 70, y0 + 38, x0 + panel_w - 18, y0 + panel_h - 48)
        title = "Uncoated" if coating == "off" else "Coated"
        draw.text((x0 + 8, y0 + 3), f"{title} | 7 valid path/field cases",
                  fill=(235, 238, 244))
        draw.rectangle(plot, outline=(82, 90, 103))
        for exponent in range(-2, 3):
            value = 10.0**exponent
            y = plot[3] - round((math.log10(value) - math.log10(low)) /
                (math.log10(high) - math.log10(low)) * (plot[3] - plot[1]))
            draw.line((plot[0], y, plot[2], y), fill=(43, 48, 57))
            draw.text((x0 + 8, y - 6), f"{value:g}px", fill=(150, 158, 170))
        for series, field in (("median", "median_abs_spread_error_px"),
                              ("maximum", "maximum_abs_spread_error_px")):
            points = []
            for index, bands in enumerate(BANDS):
                row = next(r for r in rows if r["coating"] == coating and
                           r["bands"] == bands)
                value = max(low, row[field])
                x = plot[0] + round(index * (plot[2] - plot[0]) / (len(BANDS) - 1))
                y = plot[3] - round((math.log10(value) - math.log10(low)) /
                    (math.log10(high) - math.log10(low)) * (plot[3] - plot[1]))
                points.append((x, y))
                draw.ellipse((x - 4, y - 4, x + 4, y + 4), fill=COLORS[series])
                draw.text((x - 14, plot[3] + 8), str(bands), fill=(190, 197, 207))
            draw.line(points, fill=COLORS[series], width=3)
        draw.text((plot[0], y0 + panel_h - 20),
                  "wavelength samples (61-band reference)", fill=(200, 207, 218))
        legend_y = y0 + panel_h + 24
        for xoff, series in ((16, "median"), (115, "maximum")):
            draw.line((plot[0] + xoff, legend_y, plot[0] + xoff + 22, legend_y),
                      fill=COLORS[series], width=3)
            draw.text((plot[0] + xoff + 28, legend_y - 7), series,
                      fill=COLORS[series])
    draw.text((24, 18),
        "Absolute error in maximum RGB-centroid spread vs 61 bands (log pixels)",
        fill=(240, 243, 248))
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=Path(
        "tools/LensFlareLab/examples/coating-spectrum-field-path-convergence-dual-basis.csv"))
    parser.add_argument("--csv", type=Path, default=Path(
        "tools/LensFlareLab/examples/chromatic-centroid-convergence.csv"))
    parser.add_argument("--image", type=Path, default=Path(
        "tools/LensFlareLab/examples/chromatic-centroid-convergence.png"))
    args = parser.parse_args()
    rows = summarize(args.input)
    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    make_chart(rows, args.image)
    for row in rows:
        print(f"coating={row['coating']} bands={row['bands']} valid={row['valid_cases']} "
              f"median_error={row['median_abs_spread_error_px']:.4f}px "
              f"max_error={row['maximum_abs_spread_error_px']:.4f}px")
    print(f"wrote {args.csv} and {args.image}")


if __name__ == "__main__":
    main()
