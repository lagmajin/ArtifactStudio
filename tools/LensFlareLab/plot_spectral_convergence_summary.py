"""Plot measured RGB-flux convergence for the coated and uncoated path sweep."""

import argparse
import csv
import math
from collections import defaultdict
from pathlib import Path

from PIL import Image, ImageDraw


BANDS = (3, 7, 15, 31)
COATINGS = ("off", "on")
COLORS = {"mean": (100, 205, 255), "maximum": (255, 149, 105)}


def load_summary(path):
    cases = defaultdict(list)
    with path.open(newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            bands = int(row["bands"])
            if bands not in BANDS:
                continue
            flux = sum(float(row[key]) for key in ("flux_r", "flux_g", "flux_b"))
            # Zero-flux path/field pairs have undefined shape error and should
            # not dilute the image-error statistics.
            if flux <= 1e-30:
                continue
            cases[(row["coating"], bands)].append(
                float(row["rgb_l1_percent_vs_61"]))
    summary = []
    for coating in COATINGS:
        counts = {len(cases[(coating, bands)]) for bands in BANDS}
        if len(counts) != 1 or not counts or next(iter(counts)) == 0:
            raise ValueError(f"incomplete {coating} cases across {BANDS}: {counts}")
        for bands in BANDS:
            values = cases[(coating, bands)]
            summary.append({"coating": coating, "bands": bands,
                "valid_cases": len(values), "mean_error_percent": sum(values) / len(values),
                "maximum_error_percent": max(values)})
    return summary


def make_chart(summary, path):
    panel_w, panel_h = 560, 520
    canvas = Image.new("RGB", (2 * panel_w + 48, panel_h + 96), (18, 21, 26))
    draw = ImageDraw.Draw(canvas)
    y_min, y_max = 0.03, 40.0
    for panel, coating in enumerate(COATINGS):
        x0, y0 = 24 + panel * (panel_w + 24), 54
        plot = (x0 + 66, y0 + 40, x0 + panel_w - 24, y0 + panel_h - 48)
        draw.text((x0 + 8, y0 + 4),
                  f"{'Coated' if coating == 'on' else 'Uncoated'} paths (7 valid path/field cases)",
                  fill=(235, 238, 244))
        draw.rectangle(plot, outline=(85, 92, 105))
        for exponent in range(-2, 2):
            value = 10.0**exponent
            if y_min <= value <= y_max:
                y = plot[3] - int((math.log10(value) - math.log10(y_min)) /
                                  (math.log10(y_max) - math.log10(y_min)) *
                                  (plot[3] - plot[1]))
                draw.line((plot[0], y, plot[2], y), fill=(45, 50, 60))
                draw.text((x0 + 7, y - 7), f"{value:g}%", fill=(155, 163, 175))
        for threshold, label in ((1.0, "1%"), (0.3, "0.3%")):
            y = plot[3] - int((math.log10(threshold) - math.log10(y_min)) /
                              (math.log10(y_max) - math.log10(y_min)) *
                              (plot[3] - plot[1]))
            draw.line((plot[0], y, plot[2], y), fill=(90, 94, 103), width=1)
            draw.text((plot[2] - 32, y - 14), label, fill=(170, 174, 182))
        for series, key in (("mean", "mean_error_percent"),
                            ("maximum", "maximum_error_percent")):
            points = []
            for index, bands in enumerate(BANDS):
                row = next(item for item in summary if item["coating"] == coating
                           and item["bands"] == bands)
                error = max(row[key], y_min)
                px = plot[0] + round(index * (plot[2] - plot[0]) / (len(BANDS) - 1))
                py = plot[3] - round((math.log10(error) - math.log10(y_min)) /
                    (math.log10(y_max) - math.log10(y_min)) *
                    (plot[3] - plot[1]))
                points.append((px, py))
                draw.ellipse((px - 4, py - 4, px + 4, py + 4), fill=COLORS[series])
                draw.text((px - 15, plot[3] + 8), str(bands), fill=(190, 197, 207))
            draw.line(points, fill=COLORS[series], width=3)
        draw.text((plot[0], y0 + panel_h - 22), "wavelength samples (61-band reference)",
                  fill=(200, 207, 218))
        draw.line((plot[0] + 8, y0 + panel_h + 16, plot[0] + 36, y0 + panel_h + 16),
                  fill=COLORS["mean"], width=3)
        draw.text((plot[0] + 42, y0 + panel_h + 9), "mean", fill=COLORS["mean"])
        draw.line((plot[0] + 110, y0 + panel_h + 16, plot[0] + 138, y0 + panel_h + 16),
                  fill=COLORS["maximum"], width=3)
        draw.text((plot[0] + 144, y0 + panel_h + 9), "maximum", fill=COLORS["maximum"])
    draw.text((24, 18), "Ghost RGB-flux error against 61-band integration (log scale)",
              fill=(240, 243, 248))
    path.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=Path(
        "tools/LensFlareLab/examples/coating-spectrum-field-path-convergence-dual-basis.csv"))
    parser.add_argument("--csv", type=Path, default=Path(
        "tools/LensFlareLab/examples/spectral-convergence-summary.csv"))
    parser.add_argument("--image", type=Path, default=Path(
        "tools/LensFlareLab/examples/spectral-convergence-summary.png"))
    args = parser.parse_args()
    summary = load_summary(args.input)
    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(summary[0]))
        writer.writeheader()
        writer.writerows(summary)
    make_chart(summary, args.image)
    for row in summary:
        print(f"coating={row['coating']} bands={row['bands']} "
              f"valid={row['valid_cases']} mean={row['mean_error_percent']:.3f}% "
              f"max={row['maximum_error_percent']:.3f}%")
    print(f"wrote {args.csv} and {args.image}")


if __name__ == "__main__":
    main()
