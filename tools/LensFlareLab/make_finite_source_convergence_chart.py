"""Compare finite-source ray-bin renders against the densest supplied sample grid."""

import argparse
from pathlib import Path

from PIL import Image, ImageDraw

from make_ghost_frft_comparison import display_rgb, read_pfm


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="HDR source image")
    parser.add_argument("output", type=Path, help="comparison PNG")
    parser.add_argument("--case", action="append", nargs=2, required=True,
                        metavar=("SAMPLES", "RENDER"),
                        help="repeat for each source sample count, including reference")
    parser.add_argument("--crop", type=int, nargs=4, default=(300, 100, 590, 360),
                        metavar=("L", "T", "R", "B"))
    parser.add_argument("--gain", type=float, default=300000.0)
    parser.add_argument("--difference-gain", type=float, default=1000000.0)
    args = parser.parse_args()

    source = read_pfm(args.input)
    cases = sorted(((int(samples), read_pfm(Path(path)))
                    for samples, path in args.case), key=lambda item: item[0])
    if len(cases) < 2 or len({samples for samples, _ in cases}) != len(cases):
        parser.error("provide at least two unique sample counts")
    if any(render[:2] != source[:2] for _, render in cases):
        parser.error("all PFM dimensions must match")
    left, top, right, bottom = args.crop
    width, height = right - left, bottom - top
    if not (0 <= left < right <= source[0] and 0 <= top < bottom <= source[1]):
        parser.error("crop must lie inside the images")

    reference_count, reference = cases[-1]
    weights = (0.2126, 0.7152, 0.0722)
    reference_luma = []
    for y in range(top, bottom):
        for x in range(left, right):
            i = x * 3
            reference_luma.append(sum(max(0.0, reference[2][y][i + c]
                                          - source[2][y][i + c]) * weights[c]
                                      for c in range(3)))
    reference_flux = sum(reference_luma)

    columns, cell_width, cell_height, header = len(cases), 210, 215, 46
    chart = Image.new("RGB", (columns * cell_width, 2 * (cell_height + header)),
                      (18, 21, 26))
    draw = ImageDraw.Draw(chart)
    for column, (sample_count, render) in enumerate(cases):
        luma, display_pixels = [], []
        for y in range(top, bottom):
            for x in range(left, right):
                i = x * 3
                flare = tuple(max(0.0, render[2][y][i + c] - source[2][y][i + c])
                              for c in range(3))
                display_pixels.append(display_rgb(flare, args.gain))
                luma.append(sum(flare[c] * weights[c] for c in range(3)))
        flux = sum(luma)
        peak = max(luma, default=0.0)
        centroid_x = sum((left + i % width) * value for i, value in enumerate(luma)) / max(flux, 1e-30)
        centroid_y = sum((top + i // width) * value for i, value in enumerate(luma)) / max(flux, 1e-30)
        panel = Image.new("RGB", (width, height))
        panel.putdata(display_pixels)
        panel.thumbnail((cell_width, cell_height), Image.Resampling.LANCZOS)
        x0, y0 = column * cell_width, 0
        draw.text((x0 + 5, y0 + 4), f"{sample_count} source bins", fill=(225, 230, 238))
        draw.text((x0 + 5, y0 + 20), f"Y {flux:.5g} | peak {peak:.4g}",
                  fill=(195, 204, 216))
        draw.text((x0 + 5, y0 + 34), f"centroid {centroid_x:.2f}, {centroid_y:.2f}",
                  fill=(175, 186, 200))
        chart.paste(panel, (x0 + (cell_width - panel.width) // 2, y0 + header))

        if sample_count != reference_count:
            delta_pixels, absolute_l1 = [], 0.0
            for measured, reference_value in zip(luma, reference_luma):
                delta = measured - reference_value
                absolute_l1 += abs(delta)
                strength = min(1.0, abs(delta) * args.difference_gain)
                value = round(strength * 255)
                delta_pixels.append((value, round(value * 0.04), round(value * 0.02))
                                    if delta >= 0 else
                                    (round(value * 0.02), round(value * 0.06), value))
            difference = Image.new("RGB", (width, height))
            difference.putdata(delta_pixels)
            difference.thumbnail((cell_width, cell_height), Image.Resampling.LANCZOS)
            y0 = cell_height + header
            relative_l1 = absolute_l1 / max(reference_flux, 1e-30)
            flux_ratio = flux / max(reference_flux, 1e-30)
            draw.text((x0 + 5, y0 + 4), f"difference vs {reference_count}",
                      fill=(225, 230, 238))
            draw.text((x0 + 5, y0 + 20), f"L1 {relative_l1:.3%} | Y {flux_ratio:.5f}x",
                      fill=(195, 204, 216))
            chart.paste(difference,
                        (x0 + (cell_width - difference.width) // 2, y0 + header))
            print(f"samples={sample_count} cropY={flux:.9g} peakY={peak:.9g} "
                  f"centroid=({centroid_x:.5f},{centroid_y:.5f}) "
                  f"relativeL1vs{reference_count}={relative_l1:.8%} "
                  f"Yratio={flux_ratio:.8f}")
        else:
            print(f"samples={sample_count} cropY={flux:.9g} peakY={peak:.9g} "
                  f"centroid=({centroid_x:.5f},{centroid_y:.5f}) reference")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    chart.save(args.output)
    print(f"saved {args.output}")


if __name__ == "__main__":
    main()
