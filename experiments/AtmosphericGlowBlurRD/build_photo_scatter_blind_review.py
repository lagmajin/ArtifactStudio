"""Blind A/B packet for matched-energy scatter profiles on CC0 plates."""

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
SOURCE = LAB / "captures" / "scatter-photo-rd1"
OUTPUT = LAB / "captures" / "blind-review-photo-scatter-rd1"
KEY_OUTPUT = LAB / "captures" / "blind-review-photo-scatter-rd1-key"
SEED = 20261112
PAIRS = (("multiscale_glow", "local_broad_veil"),
         ("single_gaussian_atmosphere_rms", "local_broad_veil"),
         ("single_gaussian_glow_rms", "multiscale_glow"))


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def read_rgba(path: Path) -> np.ndarray:
    with Image.open(path) as image:
        return np.asarray(image.convert("RGBA"), dtype=np.uint8)


def panel(a: np.ndarray, b: np.ndarray) -> Image.Image:
    h, w = a.shape[:2]
    header = 34
    result = Image.new("RGBA", (w * 2, h + header), (16, 19, 26, 255))
    draw = ImageDraw.Draw(result)
    draw.text((12, 10), "A", fill=(255, 255, 255, 255))
    draw.text((w + 12, 10), "B", fill=(255, 255, 255, 255))
    result.paste(Image.fromarray(a, "RGBA"), (0, header))
    result.paste(Image.fromarray(b, "RGBA"), (w, header))
    return result


def main() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    KEY_OUTPUT.mkdir(parents=True, exist_ok=True)
    source_manifest = SOURCE / "scatter_photo_manifest.json"
    expected = (SOURCE / "scatter_photo_manifest.sha256").read_text(encoding="ascii").split()[0]
    if sha256(source_manifest) != expected:
        raise RuntimeError("photo scatter manifest checksum mismatch")
    study = json.loads(source_manifest.read_text(encoding="utf-8"))
    rng = random.Random(SEED)
    key_records: list[dict[str, object]] = []
    tasks = [(case, case_record, model_a, model_b)
             for case, case_record in study["cases"].items()
             for model_a, model_b in PAIRS]
    rng.shuffle(tasks)
    with (OUTPUT / "review_answers_template.csv").open("w", newline="", encoding="utf-8-sig") as stream:
        writer = csv.writer(stream)
        writer.writerow(["stimulus", "more_atmospheric_A_or_B_or_same", "perceived_brightness_A_or_B_or_same",
                         "difference_noticeable_1_to_5",
                         "haze_or_washout_1_to_5", "detail_loss_1_to_5", "comments"])
        index = 0
        for case, case_record, model_a, model_b in tasks:
            index += 1
            shuffled = [(model_a, read_rgba(SOURCE / f"{case}.{model_a}.preview.png")),
                        (model_b, read_rgba(SOURCE / f"{case}.{model_b}.preview.png"))]
            rng.shuffle(shuffled)
            stimulus = f"F{index:02d}"
            full_path = OUTPUT / f"{stimulus}.png"
            panel(shuffled[0][1], shuffled[1][1]).save(full_path)
            roi_panels: dict[str, str] = {}
            for roi, (x0, y0, x1, y1) in case_record["roi_boxes_xyxy"].items():
                crop_a = shuffled[0][1][y0:y1, x0:x1]
                crop_b = shuffled[1][1][y0:y1, x0:x1]
                roi_path = OUTPUT / f"{stimulus}-{roi}.png"
                panel(crop_a, crop_b).save(roi_path)
                roi_panels[roi] = roi_path.name
            key_records.append({"stimulus": stimulus, "case": case,
                                "A": shuffled[0][0], "B": shuffled[1][0],
                                "roi_panels": roi_panels,
                                "panel_sha256": sha256(full_path)})
            writer.writerow([stimulus, "", "", "", "", "", ""])

    readme = """# Blind review — matched-energy scatter on CC0 photographs

Review each A/B pair at the same display conditions. A is left, B is right. Use each full image and its fixed practical-light/detail ROI crops at 1:1 where possible. Do not open the key at `../blind-review-photo-scatter-rd1-key/review_key.json` until responses are frozen. Fill `review_answers_template.csv`; `same` is valid.

Within each image, all compared outputs use the same source, highlight mask, scene-linear luma normalization, and preview transform; total added luma energy is matched to the multiscale Glow-like profile. Tone-mapped preview brightness is not normalized separately: rate perceived brightness and atmospheric character independently. Both A/B sides and the order of all six stimuli are shuffled using a recorded seed. Each asset is an example rather than a perceptual or physical ground truth. Results are SDR previews of offline CPU equations and cannot establish product appearance, HDR behavior, runtime parity, or acceptance. Record reviewer role/date and display conditions with the returned form.
"""
    (OUTPUT / "README.md").write_text(readme, encoding="utf-8", newline="\n")
    key_path = KEY_OUTPUT / "review_key.json"
    key_path.write_text(json.dumps({"seed": SEED,
                                    "generated_utc": datetime.now(timezone.utc).isoformat(),
                                    "source_manifest_sha256": sha256(source_manifest),
                                    "stimuli": key_records}, indent=2, ensure_ascii=False) + "\n",
                       encoding="utf-8", newline="\n")
    files = {p.name: {"sha256": sha256(p), "bytes": p.stat().st_size}
             for p in sorted(OUTPUT.iterdir()) if p.is_file() and p.name not in
             ("packet_manifest.json", "packet_manifest.sha256")}
    packet = {"generated_utc": datetime.now(timezone.utc).isoformat(),
              "generator_sha256": sha256(Path(__file__).resolve()),
              "source_manifest_sha256": sha256(source_manifest), "seed": SEED,
              "stimulus_count": len(key_records),
              "separate_key_path": "../blind-review-photo-scatter-rd1-key/review_key.json",
              "key_sha256": sha256(key_path), "files": files}
    packet_path = OUTPUT / "packet_manifest.json"
    packet_path.write_text(json.dumps(packet, indent=2, ensure_ascii=False) + "\n",
                           encoding="utf-8", newline="\n")
    (OUTPUT / "packet_manifest.sha256").write_text(f"{sha256(packet_path)}  packet_manifest.json\n",
                                                   encoding="ascii", newline="\n")
    print(json.dumps({"output": str(OUTPUT), "stimuli": len(key_records),
                      "manifest_sha256": sha256(packet_path)}, indent=2))


if __name__ == "__main__":
    main()
