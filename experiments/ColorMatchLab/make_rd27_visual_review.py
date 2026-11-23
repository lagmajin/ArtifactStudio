"""Build a fixed exploratory comparison sheet from RD27 Core PNG outputs."""
from __future__ import annotations

import hashlib
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parent
CAPTURE = ROOT / "captures" / "rd27_core_natural_scene"
ROI = ROOT / "captures" / "rd18_lamp_white_roi"
NAMES = ("source", "reference", "Reinhard_i1.0", "MeanStddev_i1.0",
         "Histogram_i1.0")
LABELS = ("Source", "Camera JPEG", "Core Reinhard 1.0",
          "Core MeanStddev 1.0", "Core Histogram 1.0")
CELL_W, CELL_H = 300, 235
HEADER_H = 32


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    images = [Image.open(CAPTURE / f"{name}.png").convert("RGB")
              for name in NAMES]
    canvas = Image.new("RGB", (CELL_W * len(images),
                                HEADER_H + CELL_H * 3), (31, 33, 36))
    draw = ImageDraw.Draw(canvas)
    for column, (name, label, image) in enumerate(zip(NAMES, LABELS, images)):
        x = column * CELL_W
        draw.text((x + 8, 8), label, fill=(245, 245, 245))
        preview = image.copy()
        preview.thumbnail((CELL_W - 12, CELL_H - 12), Image.Resampling.LANCZOS)
        canvas.paste(preview, (x + (CELL_W - preview.width) // 2,
                               HEADER_H + (CELL_H - preview.height) // 2))

    for row, region in enumerate(("left_lampshade", "right_lampshade"), start=1):
        with Image.open(ROI / f"{region}_mask.png") as mask_image:
            mask = mask_image.convert("L")
            bbox = mask.getbbox()
        if bbox is None:
            raise RuntimeError(f"Empty mask: {region}")
        left, top, right, bottom = bbox
        pad = 18
        crop_box = (max(0, left - pad), max(0, top - pad),
                    min(1000, right + pad), min(667, bottom + pad))
        draw.text((8, HEADER_H + row * CELL_H + 6), region,
                  fill=(205, 213, 222))
        for column, image in enumerate(images):
            crop = image.crop(crop_box)
            scale = min((CELL_W - 16) // crop.width,
                        (CELL_H - 34) // crop.height)
            scaled = crop.resize((crop.width * scale, crop.height * scale),
                                 Image.Resampling.NEAREST)
            x = column * CELL_W + (CELL_W - scaled.width) // 2
            y = HEADER_H + row * CELL_H + 28 + (CELL_H - 34 - scaled.height) // 2
            canvas.paste(scaled, (x, y))

    output = CAPTURE / "core_comparison_sheet.png"
    canvas.save(output, format="PNG", optimize=False)
    print(f"{output} sha256={sha(output)} size={canvas.size}")


if __name__ == "__main__":
    main()
