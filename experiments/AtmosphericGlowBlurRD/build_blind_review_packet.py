"""Build an anonymized SDR visual review packet from saved captures only."""

from __future__ import annotations

import csv
import hashlib
import json
import random
from datetime import datetime, timezone
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[2]
SOURCE_DIR = Path(__file__).resolve().parent / "captures" / "product-glow-sdr-rd1"
PRODUCT_DIR = ROOT / "experiments" / "GlowLab" / "captures" / "glow-suite-foundation-neon-gain-sweep-cpu-rd1"
PRODUCT_MANIFEST = PRODUCT_DIR / "product-suite-manifest.json"
OUTPUT = Path(__file__).resolve().parent / "captures" / "blind-review-sdr-rd1"
KEY_OUTPUT = Path(__file__).resolve().parent / "captures" / "blind-review-sdr-rd1-key"
SEED = 20261112


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def load_rgba(path: Path) -> np.ndarray:
    with Image.open(path) as image:
        return np.asarray(image.convert("RGBA"), dtype=np.uint8)


def main() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    manifest = json.loads(PRODUCT_MANIFEST.read_text(encoding="utf-8"))
    comparison_path = SOURCE_DIR / "comparison_manifest.json"
    comparison = json.loads(comparison_path.read_text(encoding="utf-8"))
    expected_hash = (SOURCE_DIR / "comparison_manifest.sha256").read_text(encoding="ascii").split()[0]
    if sha256(comparison_path) != expected_hash:
        raise RuntimeError("comparison manifest checksum mismatch")
    if comparison["capture_input"]["sha256"] != manifest["entries"][0]["source_capture_sha256"]:
        raise RuntimeError("captured PNG input mismatch")
    if comparison["binary"]["source_revision_recorded"]:
        raise RuntimeError("source provenance changed; review packet description needs review")

    records = {float(row["properties"]["glowGain"]): row for row in manifest["entries"]}
    rng = random.Random(SEED)
    stimulus_map: list[dict[str, object]] = []
    answer_path = OUTPUT / "review_answers_template.csv"
    with answer_path.open("w", newline="", encoding="utf-8-sig") as stream:
        writer = csv.writer(stream)
        writer.writerow(["stimulus", "more_atmospheric_A_or_B_or_same", "difference_noticeable_1_to_5",
                         "haze_or_washout_1_to_5", "detail_loss_1_to_5", "comments"])
        for index, gain in enumerate((0.10, 0.25, 0.50, 1.00), start=1):
            record = records[gain]
            product_path = PRODUCT_DIR / record["product_output"]
            candidate_path = SOURCE_DIR / f"gain-{gain:.2f}-candidate-scene_linear.rgba.npy"
            product = load_rgba(product_path)
            candidate_linear = np.load(candidate_path, allow_pickle=False)
            if candidate_linear.shape != product.shape or candidate_linear.dtype != np.float32:
                raise RuntimeError(f"unexpected candidate shape/dtype at gain {gain}")
            linear = np.maximum(candidate_linear[..., :3], 0.0)
            encoded = np.where(linear <= 0.0031308, 12.92 * linear,
                               1.055 * np.power(linear, 1.0 / 2.4) - 0.055)
            candidate = np.concatenate((np.rint(np.clip(encoded, 0, 1) * 255).astype(np.uint8),
                                        np.rint(np.clip(candidate_linear[..., 3:4], 0, 1) * 255).astype(np.uint8)),
                                       axis=2)
            choices = [("historical_glow", product), ("offline_candidate", candidate)]
            rng.shuffle(choices)
            stimulus = f"S{index:02d}"
            panel_height, panel_width = product.shape[:2]
            header = 44
            panel = Image.new("RGBA", (panel_width * 2, panel_height + header), (16, 19, 26, 255))
            draw = ImageDraw.Draw(panel)
            draw.text((16, 14), "A", fill=(255, 255, 255, 255))
            draw.text((panel_width + 16, 14), "B", fill=(255, 255, 255, 255))
            panel.paste(Image.fromarray(choices[0][1], "RGBA"), (0, header))
            panel.paste(Image.fromarray(choices[1][1], "RGBA"), (panel_width, header))
            image_path = OUTPUT / f"{stimulus}.png"
            panel.save(image_path)
            roi_files: dict[str, str] = {}
            for roi_name in ("central_intersection", "red_practicals", "dark_city_blocks"):
                nx0, ny0, nx1, ny1 = comparison["roi_basis"][roi_name]
                x0, x1 = round(nx0 * panel_width), round(nx1 * panel_width)
                y0, y1 = round(ny0 * panel_height), round(ny1 * panel_height)
                crop_panel = Image.new("RGBA", ((x1 - x0) * 2, (y1 - y0) + header), (16, 19, 26, 255))
                crop_draw = ImageDraw.Draw(crop_panel)
                crop_draw.text((16, 14), "A", fill=(255, 255, 255, 255))
                crop_draw.text(((x1 - x0) + 16, 14), "B", fill=(255, 255, 255, 255))
                for side, (_identity, image) in enumerate(choices):
                    crop_panel.paste(Image.fromarray(image[y0:y1, x0:x1], "RGBA"),
                                     (side * (x1 - x0), header))
                roi_path = OUTPUT / f"{stimulus}-{roi_name}.png"
                crop_panel.save(roi_path)
                roi_files[roi_name] = roi_path.name
            stimulus_map.append({
                "stimulus": stimulus,
                "A": choices[0][0],
                "B": choices[1][0],
                "historical_gain": gain,
                "offline_candidate_amount": gain,
                "candidate_model": "scene-linear candidate from historical SDR look comparison",
                "product_sha256": sha256(product_path),
                "candidate_npy_sha256": sha256(candidate_path),
                "blind_panel_sha256": sha256(image_path),
                "roi_panels": roi_files,
            })
            writer.writerow([stimulus, "", "", "", "", ""])

    instructions = """# Blind visual review — SDR exploratory packet

Review each full paired image and its named ROI crops at 1:1 where possible, using the same display and viewing conditions. A is shown on the left and B on the right. The paired PNGs do not identify the processing method or parameter values. Fill `review_answers_template.csv` without opening `review_key.json` until all answers are recorded.

For each pair, rate perceptible difference, atmospheric character, apparent haze/washout, and detail loss. Use the comments field for regions and artifacts. `same` is a valid response. This is an exploratory preference/identifiability review, not a pass/fail test: parameter values are numerically paired, but historical Glow gain and candidate amount are not semantically equivalent. Images are SDR 8-bit outputs from one historical installed CPU capture and one offline model, not source-current runtime, HDR, or GPU captures. The capture's source revision is unavailable.

The reviewer should not have read the method key or prior numerical comparison before answering. Record reviewer role/date and display conditions separately. One reviewer is not enough to claim robust visual acceptance; retain disagreements and uncertainty.
"""
    (OUTPUT / "README.md").write_text(instructions, encoding="utf-8", newline="\n")
    key = {
        "title": "Blind review key; keep separate until answers are frozen",
        "seed": SEED,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "stimuli": stimulus_map,
        "input_sha256": comparison["capture_input"]["sha256"],
        "comparison_manifest_sha256": sha256(comparison_path),
        "product_capture_manifest_sha256": sha256(PRODUCT_MANIFEST),
        "limitation": "Exploratory SDR blind comparison only; candidate amount and Glow gain are not equivalent controls.",
    }
    KEY_OUTPUT.mkdir(parents=True, exist_ok=True)
    key_path = KEY_OUTPUT / "review_key.json"
    key_path.write_text(json.dumps(key, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    packet_manifest = {
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "generator_sha256": sha256(Path(__file__).resolve()),
        "seed": SEED,
        "stimulus_count": len(stimulus_map),
        "separate_key_path": "../blind-review-sdr-rd1-key/review_key.json",
        "key_sha256": sha256(key_path),
        "files": {},
    }
    for path in sorted(OUTPUT.iterdir()):
        if path.is_file() and path.name not in ("packet_manifest.json", "packet_manifest.sha256"):
            packet_manifest["files"][path.name] = {"sha256": sha256(path), "bytes": path.stat().st_size}
    packet_path = OUTPUT / "packet_manifest.json"
    packet_path.write_text(json.dumps(packet_manifest, indent=2, ensure_ascii=False) + "\n",
                           encoding="utf-8", newline="\n")
    (OUTPUT / "packet_manifest.sha256").write_text(f"{sha256(packet_path)}  packet_manifest.json\n",
                                                   encoding="ascii", newline="\n")
    print(json.dumps({"output": str(OUTPUT), "stimuli": len(stimulus_map),
                      "manifest_sha256": sha256(packet_path)}, indent=2))


if __name__ == "__main__":
    main()
