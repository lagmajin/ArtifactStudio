"""Build fixed-view review sheets from actual RD22 Core PNG outputs."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageOps

ROOT = Path(__file__).resolve().parent
CAPTURE = ROOT / "captures" / "rd20_core_skin_white"
OUTPUTS = ROOT / "captures" / "rd22_core_outputs"
OUT = CAPTURE / "core_visual_review.png"
TILES = (
    ("Source", OUTPUTS / "source.png"),
    ("Synthetic target", OUTPUTS / "synthetic_reference.png"),
    ("Reinhard 1.0", OUTPUTS / "Reinhard_1.0.png"),
    ("MeanStddev 1.0", OUTPUTS / "MeanStddev_1.0.png"),
    ("Histogram 1.0", OUTPUTS / "Histogram_1.0.png"),
)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    images = [(label, Image.open(path).convert("RGB")) for label, path in TILES]
    tile_w, header_h = 288, 30
    panel_w = 260
    gap = 8
    bands = (
        ("Full frame", (0, 0, 1280, 851), 175),
        ("Face crop", (585, 235, 795, 505), 260),
        ("Arm crop", (505, 555, 665, 851), 270),
    )
    sheet_h = header_h + sum(height + header_h + gap for _, _, height in bands) + gap
    sheet = Image.new("RGB", (tile_w * len(images), sheet_h), (34, 35, 38))
    draw = ImageDraw.Draw(sheet)
    for col, (label, image) in enumerate(images):
        x = col * tile_w + (tile_w - panel_w) // 2
        draw.text((col * tile_w + 8, 8), label, fill=(245, 245, 245))
        y = header_h + gap
        for band_label, box, band_h in bands:
            if col == 0:
                draw.text((col * tile_w + 8, y + 4), band_label, fill=(190, 200, 210))
            crop = image.crop(box)
            crop.thumbnail((panel_w, band_h), Image.Resampling.LANCZOS)
            sheet.paste(crop, (x + (panel_w - crop.width) // 2,
                               y + header_h + (band_h - crop.height) // 2))
            y += band_h + header_h + gap
    sheet.save(OUT, format="PNG", optimize=False)
    manifest_path = CAPTURE / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest["core_png_outputs"] = {path.name: sha(path) for _, path in TILES}
    manifest["files"][OUT.name] = sha(OUT)
    manifest["visual_review_protocol"] = {
        "script_sha256": sha(Path(__file__)),
        "display": "same-size tile grid; outputs saved through ImageF32x4_RGBA::save PNG path",
        "rows": [name for name, _, _ in bands],
        "columns": [label for label, _ in TILES],
        "crop_boxes_xyxy": {name: box for name, box, _ in bands},
        "resizing": "aspect ratio preserved; fit within each row band",
        "review_status": "unblinded visual inspection completed by one R&D reviewer; not independently reviewed",
        "observation": "at this sheet scale, method differences are subtle but visible in face/arm tone; no obvious structural loss was observed; the patterned blouse is not evidence of neutral-white preservation",
    }
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps({"visual_review": str(OUT), "sha256": sha(OUT),
                      "outputs": manifest["core_png_outputs"]}, indent=2))


if __name__ == "__main__":
    main()
