"""Create a multi-case geometric versus aperture-FrFT render plate."""

import argparse
from pathlib import Path

from PIL import Image, ImageDraw

from make_ghost_frft_comparison import display_rgb, read_pfm


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--case", action="append", nargs=4, required=True,
                        metavar=("LABEL", "INPUT", "GEOMETRIC", "FRFT"),
                        help="repeat once per comparison case")
    parser.add_argument("--gain", type=float, default=300.0)
    args = parser.parse_args()

    loaded = []
    for label, input_path, geometry_path, frft_path in args.case:
        source, geometric, frft = (read_pfm(Path(path))
                                   for path in (input_path, geometry_path, frft_path))
        if source[:2] != geometric[:2] or source[:2] != frft[:2]:
            parser.error(f"PFM dimensions do not match for case {label}")
        loaded.append((label, source, geometric, frft))

    width, height = loaded[0][1][:2]
    panel_width = min(width, 480)
    panel_height = round(height * panel_width / width)
    columns = len(loaded)
    label_height = 24
    sheet = Image.new("RGB", (panel_width * columns * 2,
                              panel_height * 2 + label_height * 2), (18, 21, 26))
    draw = ImageDraw.Draw(sheet)
    for case_index, (label, source, geometric, frft) in enumerate(loaded):
        for row, rendered, suffix in ((0, geometric, "geometric"), (1, frft, "aperture FrFT")):
            panel = Image.new("RGB", (width, height))
            pixels = []
            luma_sum = 0.0
            for y in range(height):
                for x in range(width):
                    offset = x * 3
                    src = source[2][y][offset:offset + 3]
                    value = tuple(max(0.0, rendered[2][y][offset + c] - src[c])
                                  for c in range(3))
                    pixels.append(display_rgb(value, args.gain))
                    luma_sum += sum(value[c] * w for c, w in
                                    enumerate((0.2126, 0.7152, 0.0722)))
            panel.putdata(pixels)
            panel = panel.resize((panel_width, panel_height), Image.Resampling.LANCZOS)
            x = case_index * panel_width * 2 + row * panel_width
            y = label_height + (0 if suffix == "geometric" else panel_height + label_height)
            sheet.paste(panel, (x, y))
            if row == 0:
                draw.text((x + 5, 5), f"{label} | geometric", fill=(225, 230, 238))
            else:
                draw.text((x + 5, panel_height + label_height + 5),
                          f"{label} | aperture FrFT", fill=(225, 230, 238))
        geo, wave = [], []
        for y in range(height):
            for x in range(width):
                offset = x * 3
                src = source[2][y][offset:offset + 3]
                g = tuple(max(0.0, geometric[2][y][offset + c] - src[c]) for c in range(3))
                f = tuple(max(0.0, frft[2][y][offset + c] - src[c]) for c in range(3))
                geo.append(sum(g[c] * w for c, w in enumerate((0.2126, 0.7152, 0.0722))))
                wave.append(sum(f[c] * w for c, w in enumerate((0.2126, 0.7152, 0.0722))))
        geo_sum = sum(geo)
        delta = sum(abs(a - b) for a, b in zip(geo, wave))
        print(f"{label}: geometricY={geo_sum:.8g} frftY={sum(wave):.8g} "
              f"relativeL1={delta / max(geo_sum, 1e-30):.6%}")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(args.output)
    print(f"saved {args.output}")


if __name__ == "__main__":
    main()
