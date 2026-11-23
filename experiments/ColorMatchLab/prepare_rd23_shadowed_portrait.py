"""Prepare a source-only reviewed skin ROI and fixed cast for a second CC0 portrait."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent
ASSET = ROOT.parent / "CinematicLooksLab" / "assets" / "portrait-2024-1536.jpg"
OUT = ROOT / "captures" / "rd23_shadowed_portrait"
SCALE = np.array([0.94, 1.015, 1.075], dtype=np.float64)
OFFSET = np.array([0.012, 0.006, -0.008], dtype=np.float64)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    im = Image.open(ASSET).convert("RGB")
    rgb = np.asarray(im, dtype=np.float64) / 255.0
    w, h = im.size
    if (w, h) != (1536, 1024):
        raise RuntimeError(f"Unexpected CC0 portrait dimensions: {(w, h)}")
    face_image = Image.new("L", (w, h), 0)
    draw = ImageDraw.Draw(face_image)
    draw.polygon([
        (644, 292), (647, 265), (660, 242), (681, 225), (708, 214),
        (744, 211), (780, 216), (810, 231), (831, 255), (843, 286),
        (844, 319), (834, 352), (817, 382), (793, 408), (765, 426),
        (738, 427), (710, 414), (686, 394), (668, 370), (653, 341),
        (645, 316),
    ], fill=255)
    # Exclude eyes and mouth; polygon is fixed from source-only inspection.
    for cx, cy, rx, ry in ((720, 312, 34, 22), (804, 312, 32, 22), (768, 403, 48, 22)):
        draw.ellipse((cx-rx, cy-ry, cx+rx, cy+ry), fill=0)
    face = np.asarray(face_image) > 0
    target = np.clip(rgb * SCALE + OFFSET, 0.0, 1.0)
    OUT.mkdir(parents=True, exist_ok=True)
    Image.fromarray(np.uint8(np.rint(rgb * 255)), "RGB").save(OUT / "source.png", optimize=False)
    Image.fromarray(np.uint8(np.rint(target * 255)), "RGB").save(OUT / "synthetic_cast_reference.png", optimize=False)
    Image.fromarray(face.astype(np.uint8) * 255, "L").save(OUT / "face_skin_candidate.png", optimize=False)
    overlay = im.convert("RGBA")
    color = Image.new("RGBA", (w, h), (0, 255, 80, 90))
    color.putalpha(Image.fromarray(face.astype(np.uint8) * 90, "L"))
    overlay = Image.alpha_composite(overlay, color)
    sheet = Image.new("RGB", (w, 36 + h), (28, 29, 32))
    sheet.paste(im, (0, 36))
    sheet.paste(overlay.convert("RGB"), (0, 36))
    ImageDraw.Draw(sheet).text((10, 10), "Source-only face candidate mask", fill=(245, 245, 245))
    sheet.save(OUT / "roi_review_sheet.png", optimize=False)
    detail = im.crop((600, 180, 880, 470)).resize(
        (840, 870), Image.Resampling.LANCZOS)
    detail.save(OUT / "face_detail.png", optimize=False)
    manifest = {
        "rd": "rd23-shadowed-cc0-portrait-synthetic-cast-preparation",
        "status": "source-only candidate mask visually reviewed; exploratory only, no Core output or scoring",
        "source": {
            "path": "experiments/CinematicLooksLab/assets/portrait-2024-1536.jpg",
            "sha256": sha(ASSET), "dimensions": [w, h],
            "source_page": "https://commons.wikimedia.org/wiki/File:Portrait_2024.jpg",
            "license": "CC0 1.0 per Wikimedia Commons source page; see experiments/CinematicLooksLab/assets/SOURCES.md",
            "embedded_photographer_credit": "Louis Rivest-Henault",
        },
        "reference": {"operation": "clip(source_sRGB * scale + offset, 0, 1)",
                      "scale": SCALE.tolist(), "offset": OFFSET.tolist(),
                      "role": "constructed cast, not captured WB reference or physical ground truth"},
        "roi": {"face_skin_candidate": {"pixels": int(face.sum()),
                 "mask": "face_skin_candidate.png", "sha256": sha(OUT / "face_skin_candidate.png"),
                 "polygon_xy": [[644,292],[647,265],[660,242],[681,225],[708,214],[744,211],[780,216],[810,231],[831,255],[843,286],[844,319],[834,352],[817,382],[793,408],[765,426],[738,427],[710,414],[686,394],[668,370],[653,341],[645,316]],
                 "exclusions": "ellipse masks for eyes and mouth"}},
        "source_only_observation": "portrait has strong shadow/highlight split across face; after reviewing an enlarged source crop, fixed eye and mouth exclusions were repositioned to cover visible features. The contour and exclusions remain a single-review candidate, not independent annotation.",
        "visual_review": {"reviewed": True, "reviewer": "R&D visual inspection", "scope": "source-only full-frame overlay and enlarged face crop", "result": "face contour is approximately within visible facial skin and eye/mouth features are excluded at overview scale", "acceptance": False},
        "artifacts": {name: sha(OUT / name) for name in
                      ("source.png", "synthetic_cast_reference.png", "face_skin_candidate.png", "roi_review_sheet.png", "face_detail.png")},
        "limits": ["one CC0 portrait; no participant consent record", "synthetic target, not paired capture",
                   "face mask is a single-review candidate, not independent annotation", "no measured neutral-white material"]
    }
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps({"capture": str(OUT), "mask_pixels": int(face.sum()), "manifest": str(OUT / "manifest.json")}, indent=2))


if __name__ == "__main__":
    main()
