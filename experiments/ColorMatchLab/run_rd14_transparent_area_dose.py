"""Measure alpha-zero hidden-RGB influence as transparent area grows."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent
PREP = ROOT / "captures" / "rd9_real_skin_white_scene"
OUT = ROOT / "captures" / "rd14_transparent_area_dose"
CORE = ROOT.parent.parent / "ArtifactCore" / "src" / "Color" / "AutoColorMatch.cpp"
EXPECTED_CORE_SHA256 = "62b4354eff5de8462b007632a801fe7bb930220b338e2ea2eb7533b82e60a4ab"
FRACTIONS = (0.0, 0.01, 0.05, 0.10, 0.20)
INTENSITY = 1.0
HIDDEN_RGB = {"black": [0.0, 0.0, 0.0], "magenta": [1.0, 0.0, 1.0]}

from run_rd1_source_mirror import (  # noqa: E402
    apply_histogram, apply_mean_std, apply_reinhard,
)
from run_rd13_transparent_hidden_rgb import (  # noqa: E402
    composite, encode_preview, sha, summarize_output, summarize_pixels,
)


def create_sheet(source: np.ndarray, alpha_by_fraction: dict[float, np.ndarray],
                 outputs: dict[tuple[str, float, str], np.ndarray],
                 method: str) -> str:
    labels = ["baseline opaque", "alpha source black", "black hidden output",
              "alpha source magenta", "magenta hidden output"]
    cell_w, cell_h, label_h = 256, 170, 26
    sheet = Image.new("RGB", (cell_w * len(labels), (cell_h + label_h) * len(FRACTIONS)),
                      (26, 26, 26))
    draw = ImageDraw.Draw(sheet)
    for row, fraction in enumerate(FRACTIONS):
        alpha = alpha_by_fraction[fraction]
        transparent = alpha == 0.0
        black_input, magenta_input = source.copy(), source.copy()
        black_input[transparent] = HIDDEN_RGB["black"]
        magenta_input[transparent] = HIDDEN_RGB["magenta"]
        images = [source, composite(black_input, alpha, [0, 0, 0]),
                  composite(outputs[(method, fraction, "black")], alpha, [0, 0, 0]),
                  composite(magenta_input, alpha, [0, 0, 0]),
                  composite(outputs[(method, fraction, "magenta")], alpha, [0, 0, 0])]
        for col, (label, rgb) in enumerate(zip(labels, images)):
            x, y = col * cell_w, row * (cell_h + label_h)
            preview = encode_preview(rgb)
            preview.thumbnail((cell_w, cell_h))
            sheet.paste(preview, (x + (cell_w - preview.width) // 2, y + label_h))
            draw.text((x + 5, y + 5), f"{fraction:.0%}: {label}", fill=(242, 242, 242))
    path = OUT / f"{method}_area_dose_comparison.png"
    sheet.save(path, format="PNG", optimize=False)
    return path.name


def main() -> None:
    if sha(CORE) != EXPECTED_CORE_SHA256:
        raise RuntimeError("AutoColorMatch.cpp changed; re-audit equation mirrors")
    prep_manifest = json.loads((PREP / "manifest.json").read_text(encoding="utf-8"))
    source = np.asarray(Image.open(PREP / "source.png").convert("RGB"), dtype=np.float64) / 255.0
    h, w = source.shape[:2]
    target_info = prep_manifest["synthetic_reference"]
    target = np.clip(source * np.asarray(target_info["scale"]) +
                     np.asarray(target_info["offset"]), 0.0, 1.0)
    prep_masks = {name: np.asarray(Image.open(PREP / item["mask"]).convert("L")) > 0
                  for name, item in prep_manifest["roi"].items()}
    alpha_by_fraction: dict[float, np.ndarray] = {}
    visible_masks: dict[float, dict[str, np.ndarray]] = {}
    for fraction in FRACTIONS:
        alpha = np.ones((h, w), dtype=np.float64)
        strip_width = int(w * fraction)
        if strip_width:
            alpha[:, :strip_width] = 0.0
        alpha_by_fraction[fraction] = alpha
        visible = alpha > 0.0
        visible_masks[fraction] = {"visible_pixels": visible,
                                   **{name: mask & visible for name, mask in prep_masks.items()}}

    OUT.mkdir(parents=True, exist_ok=True)
    functions = {"Reinhard": apply_reinhard, "MeanStddev": apply_mean_std,
                 "Histogram": apply_histogram}
    outputs: dict[tuple[str, float, str], np.ndarray] = {}
    records = []
    for fraction in FRACTIONS:
        alpha = alpha_by_fraction[fraction]
        np.save(OUT / f"alpha_{fraction:.2f}.npy", alpha.astype(np.float32), allow_pickle=False)
        visible = alpha > 0.0
        variants = {name: source.copy() for name in HIDDEN_RGB}
        for name, hidden in HIDDEN_RGB.items():
            variants[name][~visible] = hidden
        for method, function in functions.items():
            for variant, input_rgb in variants.items():
                output = function(input_rgb, target, INTENSITY)
                outputs[(method, fraction, variant)] = output
                key = f"{method}_area{fraction:.2f}_{variant}"
                np.save(OUT / f"{key}.npy", output.astype(np.float32), allow_pickle=False)
                encode_preview(output).save(OUT / f"{key}.png", format="PNG", optimize=False)
            out_black = outputs[(method, fraction, "black")]
            out_magenta = outputs[(method, fraction, "magenta")]
            masks = visible_masks[fraction]
            records.append({
                "method": method, "intensity": INTENSITY,
                "transparent_fraction_requested": fraction,
                "transparent_columns": int(np.floor(w * fraction)),
                "transparent_pixel_fraction_actual": float(np.mean(~visible)),
                "hidden_variant_visible_input_identical": bool(
                    np.array_equal(variants["black"][visible], variants["magenta"][visible])),
                "regions": {name: summarize_pixels(out_black, out_magenta, mask)
                            for name, mask in masks.items()},
                "variant_output_metrics": {
                    variant: {name: summarize_output(outputs[(method, fraction, variant)],
                                                     variants[variant], target, mask)
                              for name, mask in masks.items()}
                    for variant in HIDDEN_RGB
                },
            })
    sheets = [create_sheet(source, alpha_by_fraction, outputs, method)
              for method in functions]
    files = {path.name: sha(path) for path in sorted(OUT.iterdir())
             if path.is_file() and path.name != "manifest.json"}
    manifest = {
        "rd": "rd14-transparent-area-dose-response",
        "status": "predeclared synthetic RGBA dose-response; equation mirrors only; no quality gate",
        "source": {"file": "RD9 portrait", "prep_manifest_sha256": sha(PREP / "manifest.json"),
                   "source_sha256": sha(PREP / "source.png"), "dimensions": [w, h]},
        "target": {"construction": target_info["operation"],
                   "scale": target_info["scale"], "offset": target_info["offset"],
                   "sha256": sha(PREP / "synthetic_cast_reference.png")},
        "alpha": {"model": "straight alpha for fixed black composite preview; binary values only",
                  "pattern": "full-height left strip alpha=0 at x<floor(width*fraction); alpha=1 elsewhere",
                  "fractions_requested": list(FRACTIONS),
                  "hidden_rgb_variants": HIDDEN_RGB},
        "method": {"intensity": INTENSITY, "methods": list(functions),
                   "mirror_source": "run_rd1_source_mirror.py",
                   "mirror_source_sha256": sha(ROOT / "run_rd1_source_mirror.py"),
                   "core_source_sha256": sha(CORE),
                   "expected_core_source_sha256": EXPECTED_CORE_SHA256},
        "measurement": "paired visible output RGB difference, CIE Lab D65/2-degree DeltaE76, and each variant's source/target color error, Cstar/hue, range and finite values; full visible region plus fixed face/arm/skin/blouse ROI intersections",
        "condition_pair_count": len(records), "results": records,
        "visual_review": {"sheets": sheets,
                          "review_status": "unblinded exploratory black-composite review; not independent review or acceptance gate"},
        "limitations": ["one CC0 portrait and one constructed cast target",
                        "the alpha strip is synthetic and spatially simplified",
                        "not compiled Core output or a C++ differential",
                        "no estimate of real-project prevalence or alpha association"],
        "script": Path(__file__).name, "script_sha256": sha(Path(__file__)),
        "files": files,
    }
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n",
                                        encoding="utf-8", newline="\n")
    print(json.dumps({"capture": str(OUT), "condition_pairs": len(records),
                      "visual_sheets": len(sheets), "artifacts": len(files)}, indent=2))


if __name__ == "__main__":
    main()
