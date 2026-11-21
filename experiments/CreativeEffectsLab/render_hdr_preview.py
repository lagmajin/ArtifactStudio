"""Export a tone-mapped design sheet and its scene-linear float32 masters."""

import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

from hdr_contract import split_shadow_reference
from reference import neon_ignition, paper_cutout, time_mosaic


def font(size):
    return ImageFont.truetype("segoeuib.ttf", size)


def artwork(label, size, color):
    mask = Image.new("L", (720, 360))
    draw = ImageDraw.Draw(mask)
    draw.text((360, 167), label, fill=255, font=font(size), anchor="mm")
    image = np.empty((360, 720, 4), np.float32)
    image[..., :3] = color
    image[..., 3] = np.asarray(mask, dtype=np.float32) / 255
    return image


def main():
    destination = Path(__file__).parent / "captures" / "hdr-preview"
    destination.mkdir(parents=True, exist_ok=True)
    paper = artwork("FORM", 176, (1.1, .56, .24))
    inset = artwork("FORM", 176, (.12, .8, .66))
    paper[120:162] = inset[120:162]
    paper = paper_cutout(paper, [(1, .49, .24), (.14, .44, .41), (1, .92, .74)],
                        5, (8, 13))
    curves = [[(81, 245), (152, 245), (147, 98), (220, 98)],
              [(220, 98), (294, 98), (284, 260), (362, 260)],
              [(362, 260), (439, 260), (435, 113), (507, 113)],
              [(507, 113), (579, 113), (566, 225), (642, 225)]]
    neon = neon_ignition((360, 720), curves, .87, 8, [.12, .62, 1], 4)
    shadow = split_shadow_reference(artwork("TYPE", 168, (3, 2.7, 2.3)), 32, .72)

    yy, xx = np.ogrid[:360, :720]

    def frame(t):
        phase = t * .13
        radius = np.hypot((xx % 240 - 120) / 95, (yy % 120 - 60) / 49)
        angle = np.arctan2(yy % 120 - 60, xx % 240 - 120)
        wave = np.exp(-((radius - (.57 + .16 * np.sin(angle * 3 + phase))) / .052) ** 2)
        result = np.empty((360, 720, 4), np.float32)
        result[..., :3] = (wave[..., None] *
                            np.array([1.1 + .8 * np.sin(phase),
                                      .48 + .3 * np.cos(phase), 2.8]))
        result[..., 3] = 1
        return result

    mosaic = time_mosaic(frame, 23, 0, delay=2)
    board = Image.new("RGB", (1536, 1000), "#10131c")
    draw = ImageDraw.Draw(board)
    draw.text((32, 22), "CREATIVE EFFECTS", font=font(51), fill="#f1f3fa")
    draw.text((34, 86), "SCENE-LINEAR FLOAT32  /  TONE-MAPPED PREVIEW",
              font=font(18), fill="#93a4bd")
    records = {}
    for index, (name, label, image) in enumerate((
            ("paper", "01  PAPER CUTOUT", paper),
            ("neon", "02  NEON IGNITION", neon),
            ("mosaic", "03  TIME MOSAIC", mosaic),
            ("shadow", "04  SPLIT COLOR SHADOW", shadow))):
        np.save(destination / f"{name}.npy", image, allow_pickle=False)
        records[name] = {"peak_linear_rgb": float(image[..., :3].max()),
                         "dtype": str(image.dtype), "shape": list(image.shape)}
        rgb = image[..., :3] * image[..., 3:] + .012 * (1 - image[..., 3:])
        mapped = rgb / (1 + rgb)
        srgb = np.where(mapped <= .0031308, 12.92 * mapped,
                        1.055 * mapped ** (1 / 2.4) - .055)
        x, y = 32 + (index % 2) * 752, 133 + (index // 2) * 425
        board.paste(Image.fromarray(np.rint(srgb * 255).astype(np.uint8)), (x, y))
        draw.text((x, y + 372), label, font=font(24), fill="#e8edf8")
    board.save(destination / "creative-effects.png")
    (destination / "manifest.json").write_text(json.dumps(records, indent=2),
                                               encoding="utf-8")


if __name__ == "__main__":
    main()
