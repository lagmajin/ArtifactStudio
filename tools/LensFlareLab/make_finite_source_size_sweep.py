"""Compare source-direction convergence for multiple finite emitter sizes."""

import argparse
from pathlib import Path

from PIL import Image, ImageDraw

from make_ghost_frft_comparison import display_rgb, read_pfm


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="comparison PNG")
    parser.add_argument("--size", action="append", nargs=5, required=True,
                        metavar=("RADIUS", "INPUT", "BINS25", "BINS81", "BINS225"),
                        help="repeat for each source radius and its three renders")
    parser.add_argument("--crop", type=int, nargs=4, default=(300, 100, 590, 360),
                        metavar=("L", "T", "R", "B"))
    parser.add_argument("--gain", type=float, default=64.0)
    parser.add_argument("--difference-gain", type=float, default=2000.0)
    args = parser.parse_args()

    studies = []
    for radius_text, input_path, bins25_path, bins81_path, bins225_path in args.size:
        radius = float(radius_text)
        source = read_pfm(Path(input_path))
        renders = [read_pfm(Path(path)) for path in
                   (bins25_path, bins81_path, bins225_path)]
        if any(render[:2] != source[:2] for render in renders):
            parser.error(f"PFM dimensions do not match for radius {radius:g}")
        studies.append((radius, source, renders))
    studies.sort(key=lambda study: study[0])

    left, top, right, bottom = args.crop
    width, height = right - left, bottom - top
    if any(not (0 <= left < right <= source[0] and 0 <= top < bottom <= source[1])
           for _, source, _ in studies):
        parser.error("crop must lie inside each image")
    columns, rows = 5, len(studies)
    cell_width, cell_height, header, row_gap = 210, 190, 43, 8
    chart = Image.new("RGB", (columns * cell_width,
                              rows * (cell_height + header + row_gap)), (18, 21, 26))
    draw = ImageDraw.Draw(chart)
    weights = (0.2126, 0.7152, 0.0722)

    for row, (radius, source, renders) in enumerate(studies):
        measured = []
        for render in renders:
            luma, pixels = [], []
            for y in range(top, bottom):
                for x in range(left, right):
                    i = x * 3
                    flare = tuple(max(0.0, render[2][y][i + c] - source[2][y][i + c])
                                  for c in range(3))
                    pixels.append(display_rgb(flare, args.gain))
                    luma.append(sum(flare[c] * weights[c] for c in range(3)))
            flux = sum(luma)
            peak = max(luma, default=0.0)
            cx = sum((left + i % width) * value for i, value in enumerate(luma)) / max(flux, 1e-30)
            cy = sum((top + i // width) * value for i, value in enumerate(luma)) / max(flux, 1e-30)
            measured.append((luma, pixels, flux, peak, cx, cy))

        reference = measured[-1]
        labels = ("25 source bins", "81 source bins", "225 source bins",
                  "25 vs 225", "81 vs 225")
        y0 = row * (cell_height + header + row_gap)
        draw.text((5, y0 + 4), f"Emitter Gaussian scale {radius:g} px",
                  fill=(225, 230, 238))
        for column, (label, result) in enumerate(zip(labels[:3], measured)):
            luma, pixels, flux, peak, cx, cy = result
            x0 = column * cell_width
            draw.text((x0 + 5, y0 + 19), f"{label} | Y {flux:.4g}",
                      fill=(195, 204, 216))
            draw.text((x0 + 5, y0 + 33), f"peak {peak:.4g} | c {cx:.2f},{cy:.2f}",
                      fill=(175, 186, 200))
            panel = Image.new("RGB", (width, height))
            panel.putdata(pixels)
            panel.thumbnail((cell_width, cell_height), Image.Resampling.LANCZOS)
            chart.paste(panel, (x0 + (cell_width - panel.width) // 2, y0 + header))

        for column, sample_index in enumerate((0, 1), start=3):
            current = measured[sample_index]
            relative_l1 = sum(abs(a - b) for a, b in zip(current[0], reference[0])) / max(
                reference[2], 1e-30)
            flux_ratio = current[2] / max(reference[2], 1e-30)
            pixels = []
            for value, reference_value in zip(current[0], reference[0]):
                delta = value - reference_value
                strength = min(1.0, abs(delta) * args.difference_gain)
                value8 = round(strength * 255)
                pixels.append((value8, round(value8 * 0.04), round(value8 * 0.02))
                              if delta >= 0 else
                              (round(value8 * 0.02), round(value8 * 0.06), value8))
            panel = Image.new("RGB", (width, height))
            panel.putdata(pixels)
            panel.thumbnail((cell_width, cell_height), Image.Resampling.LANCZOS)
            x0 = column * cell_width
            draw.text((x0 + 5, y0 + 19),
                      f"{labels[column]} | L1 {relative_l1:.2%}",
                      fill=(195, 204, 216))
            draw.text((x0 + 5, y0 + 33), f"Y {flux_ratio:.5f}x of 225",
                      fill=(175, 186, 200))
            chart.paste(panel, (x0 + (cell_width - panel.width) // 2, y0 + header))
            print(f"radius={radius:g} samples={25 if sample_index == 0 else 81} "
                  f"relativeL1vs225={relative_l1:.8%} Yratio={flux_ratio:.8f}")
        print(f"radius={radius:g} samples=225 cropY={reference[2]:.9g} "
              f"peakY={reference[3]:.9g} centroid=({reference[4]:.5f},{reference[5]:.5f})")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    chart.save(args.output)
    print(f"saved {args.output}")


if __name__ == "__main__":
    main()
