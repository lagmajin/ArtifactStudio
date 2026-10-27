"""Compare geometric and scalar-Fresnel ghost renders across ray-grid sizes."""

import argparse
import array
import csv
import math
import subprocess
import sys
import time
from pathlib import Path

from PIL import Image, ImageDraw


def read_pfm(path: Path):
    with path.open("rb") as source:
        if source.readline().strip() != b"PF":
            raise ValueError(f"expected RGB PFM: {path}")
        width, height = map(int, source.readline().split())
        scale = float(source.readline())
        payload = source.read()
    values = array.array("f")
    values.frombytes(payload)
    if len(values) != width * height * 3:
        raise ValueError(f"invalid PFM payload: {path}")
    little_endian = scale < 0
    if little_endian != (sys.byteorder == "little"):
        values.byteswap()
    magnitude = abs(scale)
    if magnitude != 1.0:
        values = array.array("f", (value * magnitude for value in values))
    row_size = width * 3
    for top in range(height // 2):
        bottom = height - top - 1
        top_start = top * row_size
        bottom_start = bottom * row_size
        top_row = values[top_start:top_start + row_size]
        values[top_start:top_start + row_size] = values[bottom_start:bottom_start + row_size]
        values[bottom_start:bottom_start + row_size] = top_row
    return width, height, values


def flare_values(source, rendered):
    return array.array("f", (max(0.0, after - before)
                               for before, after in zip(source, rendered)))


def image_metrics(values, width, height):
    channel_flux = [0.0, 0.0, 0.0]
    peak = [0.0, 0.0, 0.0]
    luma_flux = 0.0
    weighted_x = 0.0
    weighted_y = 0.0
    for pixel in range(width * height):
        offset = pixel * 3
        red, green, blue = values[offset:offset + 3]
        channel_flux[0] += red
        channel_flux[1] += green
        channel_flux[2] += blue
        peak[0] = max(peak[0], red)
        peak[1] = max(peak[1], green)
        peak[2] = max(peak[2], blue)
        luma = red * 0.2126 + green * 0.7152 + blue * 0.0722
        luma_flux += luma
        weighted_x += (pixel % width) * luma
        weighted_y += (pixel // width) * luma
    centroid = (weighted_x / luma_flux, weighted_y / luma_flux) if luma_flux else (0.0, 0.0)
    return channel_flux, max(peak), luma_flux, centroid


def differences(values, reference, width, height):
    absolute_rgb = 0.0
    reference_rgb = 0.0
    for pixel in range(width * height):
        offset = pixel * 3
        red, green, blue = values[offset:offset + 3]
        rr, rg, rb = reference[offset:offset + 3]
        absolute_rgb += abs(red - rr) + abs(green - rg) + abs(blue - rb)
        reference_rgb += rr + rg + rb
    return 100.0 * absolute_rgb / max(reference_rgb, 1e-30)


def normalized_shape_difference(values, reference):
    value_flux = sum(values)
    reference_flux = sum(reference)
    scale = reference_flux / max(value_flux, 1e-30)
    absolute_rgb = sum(abs(value * scale - target) for value, target in zip(values, reference))
    return 100.0 * absolute_rgb / max(reference_flux, 1e-30)


def luma_centroid_distance(first, second, width, height):
    def centroid(values):
        total = 0.0
        xsum = 0.0
        ysum = 0.0
        for pixel in range(width * height):
            offset = pixel * 3
            luma = (values[offset] * 0.2126 + values[offset + 1] * 0.7152 +
                    values[offset + 2] * 0.0722)
            total += luma
            xsum += pixel % width * luma
            ysum += pixel // width * luma
        return (xsum / total, ysum / total) if total else (0.0, 0.0)

    a, b = centroid(first), centroid(second)
    return math.hypot(a[0] - b[0], a[1] - b[1])


def display_pixel(rgb, white, contrast):
    result = []
    divisor = math.log1p(contrast)
    for value in rgb:
        normalized = max(0.0, value) / max(white, 1e-30)
        linear = math.log1p(contrast * normalized) / divisor
        srgb = 12.92 * linear if linear <= 0.0031308 else 1.055 * linear ** (1.0 / 2.4) - 0.055
        result.append(round(min(1.0, max(0.0, srgb)) * 255))
    return tuple(result)


def make_contact_sheet(records, width, height, output, contrast):
    all_positive = sorted(value for record in records.values() for value in record["flare"]
                          if value > 0.0)
    if not all_positive:
        raise ValueError("renders contain no positive flare radiance")
    white = all_positive[min(len(all_positive) - 1, int(0.999 * (len(all_positive) - 1)))]
    grids = sorted({grid for _, grid in records})
    rows = ("Geometric", "Geometric residual to reference", "Fresnel", "Fresnel residual to reference")
    label_height = 24
    sheet = Image.new("RGB", (width * len(grids), (height + label_height) * len(rows)), (18, 21, 26))
    draw = ImageDraw.Draw(sheet)
    residuals = {}
    residual_limits = {}
    for model in ("geometric", "fresnel"):
        reference = records[(model, grids[-1])]["flare"]
        model_residuals = []
        for grid in grids:
            values = records[(model, grid)]["flare"]
            per_pixel = array.array("f")
            for offset in range(0, len(values), 3):
                per_pixel.append((values[offset] - reference[offset]) * 0.2126 +
                                 (values[offset + 1] - reference[offset + 1]) * 0.7152 +
                                 (values[offset + 2] - reference[offset + 2]) * 0.0722)
            residuals[(model, grid)] = per_pixel
            model_residuals.extend(abs(value) for value in per_pixel)
        model_residuals.sort()
        residual_limits[model] = max(model_residuals[
            min(len(model_residuals) - 1, int(0.999 * (len(model_residuals) - 1)))], 1e-30)
    for row, model in enumerate(("geometric", "geometric", "fresnel", "fresnel")):
        is_residual = row in (1, 3)
        reference = records[(model, grids[-1])]["flare"]
        for column, grid in enumerate(grids):
            values = records[(model, grid)]["flare"]
            panel = Image.new("RGB", (width, height))
            pixels = []
            if is_residual:
                limit = residual_limits[model]
                for delta in residuals[(model, grid)]:
                    amount = min(1.0, abs(delta) / limit)
                    rgb = (amount, 0.035 * amount, 0.01 * amount) if delta >= 0 else (0.01 * amount, 0.05 * amount, amount)
                    pixels.append(tuple(round(channel * 255) for channel in rgb))
            else:
                for offset in range(0, len(values), 3):
                    pixels.append(display_pixel(values[offset:offset + 3], white, contrast))
            panel.putdata(pixels)
            y = row * (height + label_height)
            x = column * width
            sheet.paste(panel, (x, y + label_height))
            draw.text((x + 6, y + 5), f"{rows[row]} | grid {grid}", fill=(210, 218, 228))
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    return white


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path, help="LensFlareLab standalone executable")
    parser.add_argument("input", type=Path, help="single-source or evaluation-chart RGB PFM")
    parser.add_argument("--output-dir", type=Path, default=Path("build/ghost-raygrid-study"))
    parser.add_argument("--grids", type=int, nargs="+", default=(32, 64, 128, 256))
    parser.add_argument("--spectral-samples", type=int, choices=(3, 7, 15, 31, 61), default=3)
    parser.add_argument("--distance-mm", type=float, default=100.0)
    parser.add_argument("--log-contrast", type=float, default=1000.0)
    parser.add_argument("--reuse-existing-renders", action="store_true",
                        help="analyze matching PFM files already present in --output-dir without rerendering")
    parser.add_argument("--extra-arg", action="append", default=[],
                        help="additional renderer option, repeat for each argument; use '=VALUE' for a token beginning with '--'")
    args = parser.parse_args()
    if not args.grids or any(grid < 8 or grid > 512 for grid in args.grids):
        parser.error("all ray grids must be in 8..512")
    args.grids = sorted(set(args.grids))
    if not math.isfinite(args.distance_mm) or args.distance_mm <= 0:
        parser.error("--distance-mm must be positive and finite")
    if not math.isfinite(args.log_contrast) or args.log_contrast <= 0:
        parser.error("--log-contrast must be positive and finite")

    width, height, source = read_pfm(args.input)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    records = {}
    timings_path = args.output_dir / "render-times.csv"
    timings = {}
    if timings_path.is_file():
        with timings_path.open(newline="", encoding="utf-8") as stream:
            for row in csv.DictReader(stream):
                timings[(row["model"], int(row["ray_grid"]))] = float(row["seconds"])
    for model in ("geometric", "fresnel"):
        for grid in args.grids:
            output = args.output_dir / f"{model}-grid-{grid}.pfm"
            command = [str(args.executable), str(args.input), str(output), "--auto", "--gain", "1",
                       "--ray-grid", str(grid), "--source-samples", "1", "--spectral-samples",
                       str(args.spectral_samples), "--diffraction", "off", "--ghost-report"]
            if model == "fresnel":
                command += ["--ghost-diffraction", "on", "--ghost-diffraction-distance-mm",
                            str(args.distance_mm), "--ghost-diffraction-report"]
            else:
                command += ["--ghost-diffraction", "off"]
            command += args.extra_arg
            if args.reuse_existing_renders:
                if not output.is_file():
                    raise FileNotFoundError(f"missing cached render: {output}")
                elapsed = timings.get((model, grid))
            else:
                print("$", " ".join(command), flush=True)
                start = time.perf_counter()
                result = subprocess.run(command, text=True, capture_output=True, check=True)
                elapsed = time.perf_counter() - start
                timings[(model, grid)] = elapsed
                print(result.stdout.rstrip(), flush=True)
                if result.stderr.strip():
                    print(result.stderr.rstrip(), flush=True)
            render_width, render_height, rendered = read_pfm(output)
            if (render_width, render_height) != (width, height):
                raise ValueError(f"render dimensions changed for {output}")
            flare = flare_values(source, rendered)
            flux, peak, luma_flux, centroid = image_metrics(flare, width, height)
            records[(model, grid)] = {"path": output, "flare": flare, "seconds": elapsed,
                                      "flux": flux, "peak": peak, "luma_flux": luma_flux,
                                      "centroid": centroid}

    with timings_path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(("model", "ray_grid", "seconds"))
        for (model, grid), seconds in sorted(timings.items()):
            writer.writerow((model, grid, f"{seconds:.6f}"))

    reference_grid = args.grids[-1]
    csv_path = args.output_dir / "metrics.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(("model", "ray_grid", "seconds", "red_flux", "green_flux", "blue_flux",
                         "rgb_flux", "luma_flux", "peak_rgb", "centroid_x", "centroid_y",
                         f"rgb_l1_percent_vs_grid_{reference_grid}", "centroid_distance_px_vs_reference",
                         "rgb_shape_l1_percent_vs_reference",
                         "fresnel_vs_geometric_rgb_l1_percent",
                         "fresnel_vs_geometric_shape_l1_percent"))
        for model in ("geometric", "fresnel"):
            reference = records[(model, reference_grid)]
            for grid in args.grids:
                record = records[(model, grid)]
                rgb_flux = sum(record["flux"])
                row_delta = differences(record["flare"], reference["flare"], width, height)
                shape_delta = normalized_shape_difference(record["flare"], reference["flare"])
                geo = records[("geometric", grid)]["flare"]
                wave = records[("fresnel", grid)]["flare"]
                wave_delta = differences(wave, geo, width, height)
                writer.writerow((model, grid, "" if record["seconds"] is None else f"{record['seconds']:.6f}",
                                 *(f"{value:.9g}" for value in record["flux"]),
                                 f"{rgb_flux:.9g}", f"{record['luma_flux']:.9g}",
                                 f"{record['peak']:.9g}", f"{record['centroid'][0]:.6f}",
                                 f"{record['centroid'][1]:.6f}", f"{row_delta:.6f}",
                                 f"{luma_centroid_distance(record['flare'], reference['flare'], width, height):.6f}",
                                 f"{shape_delta:.6f}",
                                 f"{wave_delta:.6f}",
                                 f"{normalized_shape_difference(wave, geo):.6f}"))

    contact_path = args.output_dir / "contact-sheet.png"
    white = make_contact_sheet(records, width, height, contact_path, args.log_contrast)
    print(f"saved {csv_path}")
    print(f"saved {contact_path}; shared log white p99.9={white:.9g}")


if __name__ == "__main__":
    main()
