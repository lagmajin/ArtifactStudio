"""Create fixed full-frame and ROI comparison sheets for RD12."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent
PREP = ROOT / "captures" / "rd9_real_skin_white_scene"
OUT = ROOT / "captures" / "rd12_cast_strength_sensitivity"
DIRECTIONS = ("cool", "warm", "green")
STRENGTHS = (0.5, 1.0, 1.5)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def make_sheet(path: Path, box: tuple[int, int, int, int] | None) -> None:
    panels = [("source", PREP / "source.png")]
    for direction in DIRECTIONS:
        for strength in STRENGTHS:
            cast_id = f"{direction}_s{strength:.1f}"
            panels.append((f"{cast_id} target", OUT / f"{cast_id}_target.png"))
            panels.append((f"{cast_id} Histogram i1",
                           OUT / f"{cast_id}_Histogram_i1.00.png"))
    tw, th, label_h = (256, 170, 26) if box is None else (210, 240, 26)
    columns = 4
    rows = (len(panels) + columns - 1) // columns
    sheet = Image.new("RGB", (columns * tw, rows * (th + label_h)), (25, 25, 25))
    draw = ImageDraw.Draw(sheet)
    for index, (label, image_path) in enumerate(panels):
        image = Image.open(image_path).convert("RGB")
        if box is not None:
            image = image.crop(box)
        image.thumbnail((tw, th))
        col, row = index % columns, index // columns
        x, y = col * tw, row * (th + label_h)
        sheet.paste(image, (x + (tw - image.width) // 2,
                            y + label_h + (th - image.height) // 2))
        draw.text((x + 5, y + 5), label, fill=(245, 245, 245))
    sheet.save(path, format="PNG", optimize=False)


def main() -> None:
    sheets = {
        "full_frame_comparison.png": None,
        "face_crop_comparison.png": (590, 240, 790, 505),
        "arm_crop_comparison.png": (510, 550, 655, 850),
        "blouse_crop_comparison.png": (510, 530, 790, 851),
    }
    for name, box in sheets.items():
        make_sheet(OUT / name, box)
    manifest_path = OUT / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest["visual_review"] = {
        "sheets": list(sheets),
        "generator": Path(__file__).name,
        "generator_sha256": sha(Path(__file__)),
        "review_status": "unblinded exploratory preview inspection only; not independent human review or a quality gate",
        "panel_order": "source, then target and Histogram intensity-1 output for cool/warm/green × offset multipliers 0.5/1.0/1.5",
    }
    manifest["files"] = {p.name: sha(p) for p in sorted(OUT.iterdir())
                         if p.is_file() and p.name != "manifest.json"}
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n",
                             encoding="utf-8", newline="\n")
    print(json.dumps({"sheet_count": len(sheets), "artifact_count": len(manifest["files"])}, indent=2))


if __name__ == "__main__":
    main()
