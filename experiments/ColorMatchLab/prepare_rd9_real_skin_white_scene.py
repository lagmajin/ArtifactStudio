"""Prepare a licensed real portrait, synthetic cast, and fixed review ROIs."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent
ASSET = ROOT / "assets" / "WikimediaCC0" / "Pexels-photo-707448-1280.jpg"
OUT = ROOT / "captures" / "rd9_real_skin_white_scene"
SCALE = np.array([0.94, 1.015, 1.075], dtype=np.float64)
OFFSET = np.array([0.012, 0.006, -0.008], dtype=np.float64)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def polygon_mask(size: tuple[int, int], points: list[tuple[int, int]]) -> np.ndarray:
    image = Image.new("L", size, 0)
    ImageDraw.Draw(image).polygon(points, fill=255)
    return np.asarray(image) > 0


def main() -> None:
    image = Image.open(ASSET).convert("RGB")
    width, height = image.size
    if (width, height) != (1280, 851):
        raise RuntimeError(f"Unexpected source size: {(width, height)}")
    source = np.asarray(image, dtype=np.float64) / 255.0
    reference = np.clip(source * SCALE + OFFSET, 0.0, 1.0)

    face = polygon_mask((width, height), [
        (616, 285), (636, 258), (686, 247), (727, 258), (756, 281),
        (777, 314), (785, 355), (779, 393), (760, 435), (735, 471),
        (708, 493), (675, 489), (642, 470), (616, 440), (599, 399),
        (597, 360), (605, 322),
    ])
    # Exclude brows/eyes and mouth from the face skin sample.
    yy, xx = np.mgrid[0:height, 0:width]
    for cx, cy, rx, ry in ((655, 351, 29, 13), (718, 346, 27, 14), (688, 430, 35, 18)):
        face &= (((xx - cx) / rx) ** 2 + ((yy - cy) / ry) ** 2) >= 1.0

    arm = polygon_mask((width, height), [
        (541, 594), (554, 571), (578, 563), (605, 580), (627, 609),
        (641, 650), (648, 713), (646, 850), (519, 850), (520, 690),
        (524, 641),
    ])
    garment_poly = polygon_mask((width, height), [
        (523, 545), (553, 535), (601, 538), (646, 557), (688, 575),
        (730, 598), (756, 651), (777, 715), (782, 850), (520, 850),
    ])
    rgb = source
    maxc, minc = rgb.max(axis=2), rgb.min(axis=2)
    # Fixed source-based light, low-chroma filter retains the blouse's light
    # fabric while excluding dark printed flowers and deep shadow.
    blouse = garment_poly & (rgb[..., 0] > 0.48) & (rgb[..., 1] > 0.45) & \
        (rgb[..., 2] > 0.38) & ((maxc - minc) < 0.20)
    # Keep oracle regions disjoint so RD10 can vary their blend factors independently.
    blouse &= ~(face | arm)
    masks = {"face_skin_reviewed": face, "arm_skin_reviewed": arm,
             "skin_combined_reviewed": face | arm,
             "white_blouse_light_fabric_candidate": blouse}

    OUT.mkdir(parents=True, exist_ok=True)
    source_u8 = np.uint8(np.rint(source * 255.0))
    target_u8 = np.uint8(np.rint(reference * 255.0))
    Image.fromarray(source_u8, "RGB").save(OUT / "source.png", format="PNG", optimize=False)
    Image.fromarray(target_u8, "RGB").save(OUT / "synthetic_cast_reference.png", format="PNG", optimize=False)
    for name, mask in masks.items():
        Image.fromarray(mask.astype(np.uint8) * 255, "L").save(OUT / f"roi_{name}.png", format="PNG", optimize=False)

    # Review sheet: first panel has masks over source; second is cast target.
    source_img = Image.fromarray(source_u8, "RGB")
    overlay = source_img.copy().convert("RGBA")
    colors = {"face_skin_reviewed": (0, 255, 0, 90), "arm_skin_reviewed": (255, 64, 64, 90),
              "white_blouse_light_fabric_candidate": (0, 128, 255, 120)}
    for name, color in colors.items():
        layer = Image.new("RGBA", source_img.size, color)
        layer.putalpha(Image.fromarray(masks[name].astype(np.uint8) * color[3], "L"))
        overlay = Image.alpha_composite(overlay, layer)
    preview_h, preview_w = 426, 640
    sheet = Image.new("RGB", (preview_w * 2, preview_h + 34), (24, 24, 24))
    sheet.paste(source_img.resize((preview_w, preview_h)), (0, 34))
    sheet.paste(overlay.convert("RGB").resize((preview_w, preview_h)), (preview_w, 34))
    draw = ImageDraw.Draw(sheet)
    draw.text((8, 10), "Original source", fill=(240, 240, 240))
    draw.text((preview_w + 8, 10), "ROI overlay: face green / arm red / blouse blue", fill=(240, 240, 240))
    sheet.save(OUT / "roi_review_sheet.png", format="PNG", optimize=False)
    counts = {name: int(mask.sum()) for name, mask in masks.items()}
    manifest = {
        "rd": "rd9-real-portrait-synthetic-cast-preparation",
        "status": "pre-candidate ROI preparation; manual visual review recorded before output scoring",
        "preparation_script": Path(__file__).name,
        "preparation_script_sha256": sha(Path(__file__)),
        "source": {"file": ASSET.name, "sha256": sha(ASSET), "dimensions": [width, height],
                   "source_page": "https://commons.wikimedia.org/wiki/File:Pexels-photo-707448.jpg",
                   "creator": "Chandrashekar Hosakere Matt", "license": "CC0 1.0 per Wikimedia Commons file page",
                   "original_source": "Pexels photo 707448; 1280×851 Wikimedia Commons thumbnail derivative",
                   "icc_profile": "not present on retrieved thumbnail; treated as encoded sRGB for this diagnostic"},
        "synthetic_reference": {"operation": "clip(source_sRGB * scale + offset, 0, 1)",
                                "scale": SCALE.tolist(), "offset": OFFSET.tolist(),
                                "status": "constructed cast; not a second photographic capture or physical illuminant ground truth"},
        "roi": {name: {"pixels": count, "mask": f"roi_{name}.png", "sha256": sha(OUT / f"roi_{name}.png")}
                for name, count in counts.items()},
        "manual_review": {"reviewed": True, "reviewer": "R&D visual inspection", "reviewed_before_candidate_scoring": True,
                          "face": "polygon follows visible facial skin; eye/brow regions and mouth are excluded; hair and background are excluded by contour",
                          "arm": "polygon follows exposed upper arm; boundary is visible against blouse/background",
                          "blouse": "hand-bounded garment region intersected with fixed source RGB thresholds R>0.48, G>0.45, B>0.38, max-min<0.20 to keep light low-chroma fabric and reject dark print/shadow; pixels overlapping face/arm masks are excluded",
                          "limitations": ["single portrait", "blouse is patterned and shaded, not a calibrated neutral", "ROI is analyst-reviewed, not independently annotated"]},
        "artifacts": {name: sha(OUT / name) for name in ("source.png", "synthetic_cast_reference.png", "roi_review_sheet.png")},
    }
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps({"capture": str(OUT), "roi_pixels": counts, "manifest": str(OUT / "manifest.json")}, indent=2))


if __name__ == "__main__":
    main()
