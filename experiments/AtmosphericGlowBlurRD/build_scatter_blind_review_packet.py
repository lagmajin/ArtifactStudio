"""Create blinded equal-energy profile comparisons from saved offline outputs."""

from __future__ import annotations

import csv
import hashlib
import json
import random
from datetime import datetime, timezone
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw


LAB = Path(__file__).resolve().parent
SOURCE = LAB / "captures" / "scatter-profile-rd1"
OUTPUT = LAB / "captures" / "blind-review-scatter-profile-rd1"
KEY_OUTPUT = LAB / "captures" / "blind-review-scatter-profile-rd1-key"
SEED = 20261112


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def read_image(path: Path) -> np.ndarray:
    with Image.open(path) as image:
        return np.asarray(image.convert("RGBA"), dtype=np.uint8)


def header_panel(a: np.ndarray, b: np.ndarray) -> Image.Image:
    h, w = a.shape[:2]
    top = 34
    panel = Image.new("RGBA", (w * 2, h + top), (16, 19, 26, 255))
    draw = ImageDraw.Draw(panel)
    draw.text((12, 10), "A", fill=(255, 255, 255, 255))
    draw.text((w + 12, 10), "B", fill=(255, 255, 255, 255))
    panel.paste(Image.fromarray(a, "RGBA"), (0, top))
    panel.paste(Image.fromarray(b, "RGBA"), (w, top))
    return panel


def main() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    KEY_OUTPUT.mkdir(parents=True, exist_ok=True)
    source_manifest = SOURCE / "scatter_profile_manifest.json"
    manifest_hash = (SOURCE / "scatter_profile_manifest.sha256").read_text(encoding="ascii").split()[0]
    if sha256(source_manifest) != manifest_hash:
        raise RuntimeError("scatter profile manifest checksum mismatch")
    manifest = json.loads(source_manifest.read_text(encoding="utf-8"))
    if "luma equals the fixed multiscale" not in manifest["shared_conditions"]["normalization"]:
        raise RuntimeError("expected equal-energy model comparison")

    model_pairs = (
        ("multiscale_glow", "local_broad_veil"),
        ("single_gaussian_atmosphere_rms", "local_broad_veil"),
        ("single_gaussian_glow_rms", "multiscale_glow"),
    )
    # Fixed synthetic ROI bounds; coordinates are normalized to the study chart.
    rois = {
        "warm_source_and_halo": (0.10, 0.10, 0.45, 0.48),
        "cool_source_and_halo": (0.55, 0.12, 0.91, 0.52),
        "fine_detail": (0.08, 0.58, 0.42, 0.94),
    }
    rng = random.Random(SEED)
    key_records: list[dict[str, object]] = []
    with (OUTPUT / "review_answers_template.csv").open("w", newline="", encoding="utf-8-sig") as stream:
        writer = csv.writer(stream)
        writer.writerow(["stimulus", "more_atmospheric_A_or_B_or_same", "difference_noticeable_1_to_5",
                         "haze_or_washout_1_to_5", "detail_loss_1_to_5", "comments"])
        for idx, (left_model, right_model) in enumerate(model_pairs, start=1):
            shuffled = [(left_model, read_image(SOURCE / f"{left_model}.preview.png")),
                        (right_model, read_image(SOURCE / f"{right_model}.preview.png"))]
            rng.shuffle(shuffled)
            stimulus = f"P{idx:02d}"
            panel = header_panel(shuffled[0][1], shuffled[1][1])
            full_path = OUTPUT / f"{stimulus}.png"
            panel.save(full_path)
            roi_files: dict[str, str] = {}
            h, w = shuffled[0][1].shape[:2]
            for roi_name, (x0n, y0n, x1n, y1n) in rois.items():
                x0, x1, y0, y1 = round(x0n * w), round(x1n * w), round(y0n * h), round(y1n * h)
                crop_a = shuffled[0][1][y0:y1, x0:x1]
                crop_b = shuffled[1][1][y0:y1, x0:x1]
                roi_path = OUTPUT / f"{stimulus}-{roi_name}.png"
                header_panel(crop_a, crop_b).save(roi_path)
                roi_files[roi_name] = roi_path.name
            key_records.append({
                "stimulus": stimulus,
                "A": shuffled[0][0],
                "B": shuffled[1][0],
                "roi_panels": roi_files,
                "panel_sha256": sha256(full_path),
            })
            writer.writerow([stimulus, "", "", "", "", ""])

    readme = """# Blind review — equal-energy offline scatter profiles

Review each full A/B panel and its ROI crops at the same display conditions. A is left, B is right. Do not open `../blind-review-scatter-profile-rd1-key/review_key.json` until answers are frozen. Fill `review_answers_template.csv`; `same` is a valid atmospheric-character response.

Each compared output uses the same synthetic HDR input, highlight-selection mask, source seed, scene-linear Rec.709 processing, tone-map preview, and total added luma energy. The comparisons isolate scatter-profile shape more closely than the historical-product packet. The review is still exploratory: it is one synthetic image, one display transform, and an offline equation model. It cannot establish product appearance, HDR viewer behavior, temporal stability, or acceptance. Record reviewer role/date and display conditions with the returned answers. Treat preference, noticeability, haze, and detail loss as separate observations.
"""
    (OUTPUT / "README.md").write_text(readme, encoding="utf-8", newline="\n")

    key_path = KEY_OUTPUT / "review_key.json"
    key_path.write_text(json.dumps({
        "title": "Blind scatter profile review key",
        "seed": SEED,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "source_manifest_sha256": sha256(source_manifest),
        "stimuli": key_records,
    }, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    files = {}
    for path in sorted(OUTPUT.iterdir()):
        if path.is_file() and path.name not in ("packet_manifest.json", "packet_manifest.sha256"):
            files[path.name] = {"sha256": sha256(path), "bytes": path.stat().st_size}
    packet = {
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "generator_sha256": sha256(Path(__file__).resolve()),
        "source_scatter_manifest_sha256": sha256(source_manifest),
        "seed": SEED,
        "stimulus_count": len(key_records),
        "separate_key_path": "../blind-review-scatter-profile-rd1-key/review_key.json",
        "key_sha256": sha256(key_path),
        "files": files,
    }
    packet_path = OUTPUT / "packet_manifest.json"
    packet_path.write_text(json.dumps(packet, indent=2, ensure_ascii=False) + "\n",
                           encoding="utf-8", newline="\n")
    (OUTPUT / "packet_manifest.sha256").write_text(f"{sha256(packet_path)}  packet_manifest.json\n",
                                                   encoding="ascii", newline="\n")
    print(json.dumps({"output": str(OUTPUT), "stimuli": len(key_records),
                      "manifest_sha256": sha256(packet_path)}, indent=2))


if __name__ == "__main__":
    main()
