"""Measure how the cone-radius proxy changes prescription-scatter predictions.

Reads the per-surface ray-trace CSV and compares r50, r90, r99, RMS, and the
equal-area disk radius derived from RMS in Peterson Eq. 8. These are sensitivity
cases, not equally validated physical interpretations of the model parameter.
"""

import argparse
import csv
import math
from collections import OrderedDict
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

from evaluate_harvey_spectral_scaling import RGB_BANDS
from evaluate_prescription_spectral_surface_scatter import render_band


PROXIES = (("r50", 2, 1.0), ("r90", 3, 1.0), ("r99", 4, 1.0),
           ("rms", 5, 1.0), ("rms-eq-disk", 5, math.sqrt(2.0)))
COLORS = ((255, 110, 104), (111, 223, 160), (110, 174, 255))


def load_cases(path):
    grouped = OrderedDict()
    with path.open(newline="", encoding="utf-8") as handle:
        for record in csv.DictReader(handle):
            key = (float(record["field_deg"]), record["channel"])
            if key not in grouped:
                grouped[key] = {
                    "field_deg": key[0], "channel": key[1],
                    "wavelength_nm": float(record["wavelength_nm"]),
                    "accepted": int(record["accepted_rays"]),
                    "launched": int(record["launched_rays"]),
                    "slope": float(record["S"]),
                    "b": float(record["B_sr-1"]),
                    "l_mrad": float(record["L_mrad"]),
                    "reference_nm": float(record["reference_wavelength_nm"]),
                    "grid": int(record["grid"]),
                    "width": int(record["scatter_width_px"]),
                    "height": int(record["scatter_height_px"]),
                    "pitch_um": float(record["pixel_pitch_um"]),
                    "focal_mm": float(record["focal_length_mm"]),
                    "transmission": float(record["transmission_assumption"]),
                    "na": float(record["NA_assumption"]), "rows": []}
            grouped[key]["rows"].append((
                int(record["surface_index"]), int(record["surface_rays"]),
                float(record["r50_mm"]), float(record["r90_mm"]),
                float(record["r99_mm"]), float(record["rms_mm"])))
    for case in grouped.values():
        case["rows"].sort(key=lambda row: row[0])
        if len(case["rows"]) != 14:
            raise ValueError(f"field {case['field_deg']} {case['channel']} has "
                             f"{len(case['rows'])} surfaces; expected 14")
    return grouped


def evaluate(input_csv):
    grouped = load_cases(input_csv)
    prototype = next(iter(grouped.values()))
    pitch_mm = prototype["pitch_um"] * 1e-3
    axis = (np.arange(prototype["width"]) + .5 - prototype["width"] / 2) * pitch_mm
    xx, yy = np.meshgrid(axis, axis)
    rho = np.hypot(xx, yy)
    pixel_area = pitch_mm**2
    values = []
    for (field, channel), case in grouped.items():
        # Recover the reference-wavelength parameters from a derived RGB band.
        ratio = case["wavelength_nm"] / case["reference_nm"]
        b_ref = case["b"] * ratio ** (4.0 + case["slope"])
        l_ref_mrad = case["l_mrad"] / ratio
        for proxy, radius_index, radius_scale in PROXIES:
            result = render_band(case, case["wavelength_nm"], case["slope"],
                b_ref, l_ref_mrad, case["reference_nm"], case["transmission"],
                case["na"], rho, pixel_area, radius_index, radius_scale)
            values.append({"field": field, "channel": channel,
                "wavelength_nm": case["wavelength_nm"], "proxy": proxy,
                "accepted": case["accepted"], "launched": case["launched"],
                "entrance_radius_mm": result["entrance_radius"],
                "crop_fraction": float(sum(result["surface_contributions"])),
                "surface_contributions": result["surface_contributions"]})
    return prototype, values


def make_chart(path, fields, rows):
    panel_w, panel_h, header = 500, 390, 32
    canvas = Image.new("RGB", (2 * panel_w, 2 * (panel_h + header)),
                       (18, 21, 26))
    draw = ImageDraw.Draw(canvas)
    left, right, top, bottom = 58, panel_w - 18, 43, panel_h - 42
    ymax = max(r["crop_fraction"] * 100 for r in rows)
    ymax = max(.5, math.ceil(ymax * 2.0) / 2.0)
    proxies = [name for name, _, _ in PROXIES]
    for index, field in enumerate(fields):
        x0 = (index % 2) * panel_w
        y0 = (index // 2) * (panel_h + header)
        draw.text((x0 + 8, y0 + 7),
                  f"{field:g}° field  crop fraction vs radius proxy",
                  fill=(235, 238, 244))
        plot = (x0 + left, y0 + top, x0 + right, y0 + bottom)
        draw.rectangle(plot, outline=(80, 88, 100))
        for tick in np.linspace(0.0, ymax, 5):
            y = plot[3] - int(tick / ymax * (plot[3] - plot[1]))
            draw.line((plot[0], y, plot[2], y), fill=(43, 48, 57))
            draw.text((x0 + 8, y - 6), f"{tick:.2g}%", fill=(150, 158, 170))
        for channel_index, (channel, _) in enumerate(RGB_BANDS):
            points = []
            for proxy_index, proxy in enumerate(proxies):
                row = next(r for r in rows if r["field"] == field and
                           r["channel"] == channel and r["proxy"] == proxy)
                x = plot[0] + int(proxy_index / (len(proxies) - 1) *
                                   (plot[2] - plot[0]))
                y = plot[3] - int(row["crop_fraction"] * 100 / ymax *
                                  (plot[3] - plot[1]))
                points.append((x, y))
                draw.ellipse((x - 3, y - 3, x + 3, y + 3),
                             fill=COLORS[channel_index])
            if len(points) > 1:
                draw.line(points, fill=COLORS[channel_index], width=2)
        labels = {"rms-eq-disk": "rms disk"}
        for proxy_index, proxy in enumerate(proxies):
            x = plot[0] + int(proxy_index / (len(proxies) - 1) *
                              (plot[2] - plot[0]))
            label = labels.get(proxy, proxy)
            bounds = draw.textbbox((0, 0), label)
            draw.text((x - (bounds[2] - bounds[0]) / 2, plot[3] + 5),
                      label, fill=(170, 177, 188))
        for channel_index, (channel, _) in enumerate(RGB_BANDS):
            draw.text((plot[0] + 8 + channel_index * 82, plot[1] + 6),
                      channel, fill=COLORS[channel_index])
        draw.text((plot[0], y0 + panel_h - 14),
                  "RMS-eq-disk radius = sqrt(2) * RMS",
                  fill=(200, 207, 218))
    path.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input-csv", type=Path, default=Path(
        "tools/LensFlareLab/examples/prescription-spectral-surface-scatter.csv"))
    parser.add_argument("--csv", type=Path, default=Path(
        "tools/LensFlareLab/examples/prescription-scatter-radius-proxy-sensitivity.csv"))
    parser.add_argument("--image", type=Path, default=Path(
        "tools/LensFlareLab/examples/prescription-scatter-radius-proxy-sensitivity.png"))
    args = parser.parse_args()
    prototype, rows = evaluate(args.input_csv)
    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(("field_deg", "channel", "wavelength_nm", "radius_proxy",
            "radius_scale",
            "accepted_rays", "launched_rays", "entrance_radius_mm",
            "crop_fraction", "crop_percent", "delta_percent_vs_r90",
            "top_surface_index", "top_surface_crop_share_percent"))
        for row in rows:
            reference = next(r["crop_fraction"] for r in rows
                if r["field"] == row["field"] and r["channel"] == row["channel"]
                and r["proxy"] == "r90")
            top_index = int(np.argmax(row["surface_contributions"]))
            top_share = (row["surface_contributions"][top_index] /
                         max(row["crop_fraction"], 1e-300) * 100.0)
            writer.writerow((row["field"], row["channel"], row["wavelength_nm"],
                row["proxy"], next(scale for name, _, scale in PROXIES
                                    if name == row["proxy"]),
                row["accepted"], row["launched"],
                f"{row['entrance_radius_mm']:.12g}",
                f"{row['crop_fraction']:.12g}",
                f"{row['crop_fraction']*100:.9g}",
                f"{100*(row['crop_fraction']/reference-1):.9g}",
                top_index, f"{top_share:.9g}"))
    fields = sorted({row["field"] for row in rows})
    if len(fields) != 4:
        raise ValueError(f"expected four field cases for the comparison plate; got {len(fields)}")
    make_chart(args.image, fields, rows)
    for field in fields:
        for channel, _ in RGB_BANDS:
            by_proxy = {row["proxy"]: row["crop_fraction"] for row in rows
                        if row["field"] == field and row["channel"] == channel}
            top_by_proxy = {row["proxy"]: int(np.argmax(row["surface_contributions"]))
                for row in rows if row["field"] == field and row["channel"] == channel}
            print(f"field={field:g} {channel}: " + " ".join(
                f"{proxy}={by_proxy[proxy]*100:.4f}%/surf{top_by_proxy[proxy]}"
                for proxy, _, _ in PROXIES))
    print(f"wrote {args.csv} and {args.image}; source grid={prototype['grid']}²")
    print("Radius-statistic cases are sensitivity bounds, not calibrated cone-radius choices.")


if __name__ == "__main__":
    main()
