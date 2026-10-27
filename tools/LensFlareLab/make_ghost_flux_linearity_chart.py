"""Check ghost-render homogeneity under source-radiance scaling."""

import argparse
from pathlib import Path

from PIL import Image, ImageDraw

from make_ghost_frft_comparison import display_rgb, read_pfm


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="scale-1 source PFM")
    parser.add_argument("output", type=Path, help="output comparison PNG")
    parser.add_argument("--case", action="append", nargs=5, required=True,
                        metavar=("MODE", "SCALE", "SOURCE", "BASE", "SCALED"),
                        help="repeat for polygon/transport and each source scale")
    parser.add_argument("--crop", type=int, nargs=4, default=(300, 100, 590, 360),
                        metavar=("L", "T", "R", "B"))
    parser.add_argument("--gain", type=float, default=300000.0)
    args = parser.parse_args()

    base_input = read_pfm(args.input)
    left, top, right, bottom = args.crop
    width, height = right - left, bottom - top
    cases = {}
    for mode, scale_text, source_path, base_path, scaled_path in args.case:
        scale = float(scale_text)
        source, base, scaled = (read_pfm(Path(path))
                                for path in (source_path, base_path, scaled_path))
        if any(image[:2] != base_input[:2] for image in (source, base, scaled)):
            parser.error(f"PFM dimensions do not match for {mode} scale {scale}")
        if not (0 < scale and 0 <= left < right <= source[0]
                and 0 <= top < bottom <= source[1]):
            parser.error("invalid scale or crop")
        cases[(mode, scale)] = (source, base, scaled)

    scales = sorted({scale for _, scale in cases})
    modes = sorted({mode for mode, _ in cases})
    cell_width, cell_height, header = 250, 205, 40
    chart = Image.new("RGB", (cell_width * len(scales),
                              (cell_height + header) * len(modes)), (18, 21, 26))
    draw = ImageDraw.Draw(chart)
    weights = (0.2126, 0.7152, 0.0722)
    for row, mode in enumerate(modes):
        for col, scale in enumerate(scales):
            source, base, scaled = cases[(mode, scale)]
            pixels, reference_luma, measured_luma = [], [], []
            for y in range(top, bottom):
                for x in range(left, right):
                    i = x * 3
                    flare = tuple(max(0.0, (scaled[2][y][i + c] - source[2][y][i + c])
                                      / scale) for c in range(3))
                    reference = tuple(max(0.0, base[2][y][i + c]
                                          - base_input[2][y][i + c]) for c in range(3))
                    pixels.append(display_rgb(flare, args.gain))
                    reference_luma.append(sum(reference[c] * weights[c] for c in range(3)))
                    measured_luma.append(sum(flare[c] * weights[c] for c in range(3)))
            panel = Image.new("RGB", (width, height))
            panel.putdata(pixels)
            panel.thumbnail((cell_width, cell_height), Image.Resampling.LANCZOS)
            x0 = col * cell_width
            y0 = row * (cell_height + header)
            total_ref = sum(reference_luma)
            relative_l1 = sum(abs(a - b) for a, b in
                              zip(reference_luma, measured_luma)) / max(total_ref, 1e-30)
            flux_ratio = sum(measured_luma) / max(total_ref, 1e-30)
            draw.text((x0 + 6, y0 + 5), f"{mode} | source x{scale:g}",
                      fill=(225, 230, 238))
            draw.text((x0 + 6, y0 + 21),
                      f"normalized L1 {relative_l1:.3%} | Y {flux_ratio:.5f}x",
                      fill=(195, 204, 216))
            chart.paste(panel, (x0 + (cell_width - panel.width) // 2, y0 + header))
            print(f"{mode} sourceScale={scale:g} normalizedL1={relative_l1:.8%} "
                  f"normalizedYRatio={flux_ratio:.8f}")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    chart.save(args.output)
    print(f"saved {args.output}")


if __name__ == "__main__":
    main()
