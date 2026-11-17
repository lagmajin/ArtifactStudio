"""Build reproducible face/neutral ROI sheets from RD1 source-mirror previews."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent
CAPTURE = ROOT / "captures" / "rd1_source_mirror"
MASK_PATH = ROOT / "captures" / "rd0" / "portrait_skin_roi_candidate.png"
OUT = ROOT / "captures" / "rd1_visual_review"
ITEMS = (
    ("source", "portrait_source.png"),
    ("synthetic cast", "portrait_synthetic_reference.png"),
    ("Reinhard 0.5", "Reinhard_source_mirror_i0.50.png"),
    ("MeanStddev 0.5", "MeanStddev_source_mirror_i0.50.png"),
    ("Histogram 0.5", "Histogram_source_mirror_i0.50.png"),
    ("Reinhard 1.0", "Reinhard_source_mirror_i1.00.png"),
    ("MeanStddev 1.0", "MeanStddev_source_mirror_i1.00.png"),
    ("Histogram 1.0", "Histogram_source_mirror_i1.00.png"),
)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    rd1 = json.loads((CAPTURE / "manifest.json").read_text(encoding="utf-8"))
    mask = Image.open(MASK_PATH).convert("L")
    bbox = mask.getbbox()
    if bbox is None:
        raise RuntimeError("Candidate skin ROI mask is empty")
    x0, y0, x1, y1 = bbox
    margin = 32
    face_crop = (max(0, x0-margin), max(0, y0-margin),
                 min(1536, x1+margin), min(1024, y1+margin))
    card_crop = (40, 616, 216, 760)
    OUT.mkdir(parents=True, exist_ok=True)
    artifacts = {}
    for name, crop in (("face_candidate_roi", face_crop),
                       ("inserted_neutral_card", card_crop)):
        tile_w, tile_h, label_h = 320, 220, 30
        sheet = Image.new("RGB", (tile_w * len(ITEMS), tile_h + label_h), (32, 32, 32))
        draw = ImageDraw.Draw(sheet)
        for index, (label, filename) in enumerate(ITEMS):
            image = Image.open(CAPTURE / filename).convert("RGB").crop(crop)
            image.thumbnail((tile_w, tile_h))
            x = index * tile_w + (tile_w - image.width) // 2
            y = label_h + (tile_h - image.height) // 2
            sheet.paste(image, (x, y))
            draw.text((index * tile_w + 6, 7), label, fill=(245, 245, 245))
        output = OUT / f"{name}.png"
        sheet.save(output, format="PNG", optimize=False)
        artifacts[output.name] = {
            "sha256": sha(output), "crop_xyxy": crop,
            "source_files": [filename for _, filename in ITEMS],
        }

    manifest = {
        "rd": "rd1-visual-review-preparation",
        "status": "equation-mirror source crops; not C++ output or acceptance",
        "source_portrait_sha256": rd1["source"]["sha256"],
        "core_source_sha256": rd1["implementation_boundary"]["source_sha256"],
        "runner_sha256": rd1["implementation_boundary"]["mirror_script_sha256"],
        "generator_sha256": sha(Path(__file__)),
        "roi_status": "unreviewed candidate",
        "artifacts": artifacts,
        "limitations": ["equation mirror only, no compiled Core output",
            "portrait cast is synthetic, not a natural pair",
            "neutral patch is artificial and hard-edged",
            "unblinded exploratory inspection; no acceptance gate",
            "display uses clipped sRGB PNG previews"],
    }
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n",
                                        encoding="utf-8", newline="\n")
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
