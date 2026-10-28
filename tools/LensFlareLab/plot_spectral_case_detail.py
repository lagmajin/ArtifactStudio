"""Plot one ghost path's RGB flux and error from the spectral sweep CSV."""

import argparse
import csv
import math
from pathlib import Path

from PIL import Image, ImageDraw


BANDS = (3, 7, 15, 31, 61)
CHANNELS = (("red", "flux_r", (246, 105, 105)),
            ("green", "flux_g", (111, 222, 156)),
            ("blue", "flux_b", (113, 167, 255)))


def load_case(path, position, ghost_path, coating):
    rows = []
    with path.open(newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            if (row["position"] == position and row["path"] == ghost_path and
                    row["coating"] == coating and int(row["bands"]) in BANDS):
                rows.append({**row, "bands": int(row["bands"]),
                    "flux_r": float(row["flux_r"]), "flux_g": float(row["flux_g"]),
                    "flux_b": float(row["flux_b"]),
                    "error": float(row["rgb_l1_percent_vs_61"])})
    rows.sort(key=lambda row: row["bands"])
    if tuple(row["bands"] for row in rows) != BANDS:
        raise ValueError(f"expected bands {BANDS} for {position} {ghost_path} {coating}")
    if sum(rows[-1][key] for _, key, _ in CHANNELS) <= 0:
        raise ValueError("61-band reference case has no in-frame ghost flux")
    return rows


def make_chart(rows, position, ghost_path, coating, output):
    width, height = 1120, 620
    image = Image.new("RGB", (width, height), (18, 21, 26))
    draw = ImageDraw.Draw(image)
    draw.text((24, 17), f"Ghost path {ghost_path} | {position} | coating {coating}",
              fill=(240, 243, 248))
    panel_top, panel_bottom = 86, 520
    left_plot = (82, panel_top, 540, panel_bottom)
    right_plot = (654, panel_top, 1076, panel_bottom)
    draw.text((left_plot[0], 53), "Integrated channel flux (log scale)",
              fill=(230, 234, 241))
    draw.text((right_plot[0], 53), "RGB L1 error vs 61-band reference",
              fill=(230, 234, 241))
    draw.rectangle(left_plot, outline=(82, 90, 103))
    draw.rectangle(right_plot, outline=(82, 90, 103))

    flux_min, flux_max = 1e-5, 1.0
    for exponent in range(-5, 1):
        value = 10.0**exponent
        y = left_plot[3] - round((math.log10(value) - math.log10(flux_min)) /
            (math.log10(flux_max) - math.log10(flux_min)) *
            (left_plot[3] - left_plot[1]))
        draw.line((left_plot[0], y, left_plot[2], y), fill=(43, 48, 57))
        draw.text((25, y - 6), f"{value:g}", fill=(148, 157, 170))
    group_w = (left_plot[2] - left_plot[0]) / len(BANDS)
    bar_w = 20
    for index, row in enumerate(rows):
        center = left_plot[0] + (index + .5) * group_w
        for channel_index, (_, key, color) in enumerate(CHANNELS):
            value = max(flux_min, min(flux_max, row[key]))
            y = left_plot[3] - round((math.log10(value) - math.log10(flux_min)) /
                (math.log10(flux_max) - math.log10(flux_min)) *
                (left_plot[3] - left_plot[1]))
            x = round(center + (channel_index - 1) * (bar_w + 4))
            draw.rectangle((x, y, x + bar_w, left_plot[3] - 1), fill=color)
        draw.text((center - 9, left_plot[3] + 10), str(row["bands"]),
                  fill=(188, 196, 206))

    error_min, error_max = 0.01, 50.0
    for exponent in (-2, -1, 0, 1):
        value = 10.0**exponent
        y = right_plot[3] - round((math.log10(value) - math.log10(error_min)) /
            (math.log10(error_max) - math.log10(error_min)) *
            (right_plot[3] - right_plot[1]))
        draw.line((right_plot[0], y, right_plot[2], y), fill=(43, 48, 57))
        draw.text((right_plot[0] - 42, y - 6), f"{value:g}%",
                  fill=(148, 157, 170))
    points = []
    for index, row in enumerate(rows):
        x = right_plot[0] + round(index * (right_plot[2] - right_plot[0]) /
                                   (len(BANDS) - 1))
        value = max(error_min, row["error"])
        y = right_plot[3] - round((math.log10(value) - math.log10(error_min)) /
            (math.log10(error_max) - math.log10(error_min)) *
            (right_plot[3] - right_plot[1]))
        points.append((x, y))
        draw.ellipse((x - 5, y - 5, x + 5, y + 5), fill=(255, 181, 99))
        draw.text((x - 24, y - 23),
                  "ref" if row["bands"] == 61 else f"{row['error']:.2f}%",
                  fill=(255, 205, 151))
        draw.text((x - 12, right_plot[3] + 10), str(row["bands"]),
                  fill=(188, 196, 206))
    draw.line(points, fill=(255, 181, 99), width=3)
    draw.text((left_plot[0], 568), "Bands", fill=(200, 207, 218))
    draw.text((right_plot[0], 568), "Bands (61 is the numerical reference)",
              fill=(200, 207, 218))
    legend_x = left_plot[0] + 20
    for label, _, color in CHANNELS:
        draw.rectangle((legend_x, 548, legend_x + 12, 560), fill=color)
        draw.text((legend_x + 17, 547), label, fill=color)
        legend_x += 80
    output.parent.mkdir(parents=True, exist_ok=True)
    image.save(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=Path(
        "tools/LensFlareLab/examples/coating-spectrum-field-path-convergence-dual-basis.csv"))
    parser.add_argument("--position", choices=("center", "midfield", "offaxis"),
                        default="center")
    parser.add_argument("--path", default="10-9")
    parser.add_argument("--coating", choices=("on", "off"), default="on")
    parser.add_argument("--image", type=Path, default=Path(
        "tools/LensFlareLab/examples/spectral-case-center-10-9.png"))
    parser.add_argument("--csv", type=Path, default=Path(
        "tools/LensFlareLab/examples/spectral-case-center-10-9.csv"))
    args = parser.parse_args()
    rows = load_case(args.input, args.position, args.path, args.coating)
    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=("position", "field_u", "field_v",
            "path", "coating", "bands", "flux_r", "flux_g", "flux_b", "error"))
        writer.writeheader()
        for row in rows:
            writer.writerow({"position": row["position"], "field_u": row["field_u"],
                "field_v": row["field_v"], "path": row["path"],
                "coating": row["coating"], "bands": row["bands"],
                "flux_r": row["flux_r"], "flux_g": row["flux_g"],
                "flux_b": row["flux_b"], "error": row["error"]})
    make_chart(rows, args.position, args.path, args.coating, args.image)
    for row in rows:
        print(f"bands={row['bands']} RGB=({row['flux_r']:.9g},"
              f"{row['flux_g']:.9g},{row['flux_b']:.9g}) "
              f"L1_vs61={row['error']:.4f}%")
    print(f"wrote {args.image} and {args.csv}")


if __name__ == "__main__":
    main()
