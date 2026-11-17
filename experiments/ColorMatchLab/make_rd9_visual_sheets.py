"""Build fixed, hash-pinned RD9 full-frame and ROI comparison sheets."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent
PREP = ROOT / "captures" / "rd9_real_skin_white_scene"
OUT = ROOT / "captures" / "rd9_real_skin_white_scored"
CONDITIONS = [("source", PREP / "source.png"),
              ("synthetic reference", PREP / "synthetic_cast_reference.png")]
CONDITIONS += [(name, OUT / f"{name}_i1.00.png") for name in ("Reinhard", "MeanStddev", "Histogram")]


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def make_sheet(path: Path, box: tuple[int, int, int, int] | None) -> None:
    tile_w, tile_h, label_h = ((384, 426, 28) if box is None else (260, 340, 28))
    sheet = Image.new("RGB", (tile_w * len(CONDITIONS), tile_h + label_h), (28, 28, 28))
    draw = ImageDraw.Draw(sheet)
    for index, (label, filename) in enumerate(CONDITIONS):
        image = Image.open(filename).convert("RGB")
        if box is not None:
            image = image.crop(box)
        image.thumbnail((tile_w, tile_h))
        x = index * tile_w + (tile_w - image.width) // 2
        y = label_h + (tile_h - image.height) // 2
        sheet.paste(image, (x, y))
        draw.text((index * tile_w + 6, 7), label, fill=(245, 245, 245))
    sheet.save(path, format="PNG", optimize=False)


def main() -> None:
    sheets = {"full_frame_comparison.png": None,
              "face_crop_comparison.png": (590, 240, 790, 505),
              "arm_crop_comparison.png": (510, 550, 655, 850),
              "blouse_crop_comparison.png": (510, 530, 790, 851)}
    for name, box in sheets.items():
        make_sheet(OUT / name, box)
    manifest_path = OUT / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest["visual_review"] = {
        "sheets": list(sheets),
        "review_status": "unblinded exploratory inspection only; not independent human review or an acceptance gate",
        "display": "same dimensions per comparison column; source, synthetic target, and intensity-1 source mirrors",
    }
    manifest["files"] = {p.name: sha(p) for p in sorted(OUT.iterdir())
                         if p.is_file() and p.name != "manifest.json"}
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps({"sheets": {name: sha(OUT / name) for name in sheets}}, indent=2))


if __name__ == "__main__":
    main()
