"""Isolate alpha association in BOX/nearest resize without palette mapping."""

from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from adaptive_palette_resize_phase_study import PHASES, SCALES, SOURCE_CROP, pan_origins  # noqa: E402
from adaptive_palette_temporal_stability_study import ASSETS, get_masks, load_asset  # noqa: E402
from adaptive_palette_alpha_resize_study import BACKGROUNDS, composite  # noqa: E402

CAPTURE = ROOT / "captures" / "alpha-resize-association-isolation"
ASSOCIATIONS = ("straight", "premultiplied")
ALPHA_MODES = ("preserve source", "resize alpha")


def resize(crop: np.ndarray, scale: float, association: str, alpha_mode: str) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    h, w = crop.shape[:2]
    size = (max(1, int(round(w * scale))), max(1, int(round(h * scale))))
    alpha = crop[:, :, 3].astype(np.float32) / 255
    rgb = crop[:, :, :3].astype(np.float32) / 255
    if association == "premultiplied":
        rgb = rgb * alpha[:, :, None]
    rgb8 = np.uint8(np.clip(rgb * 255 + .5, 0, 255))
    low_rgb = np.asarray(Image.fromarray(rgb8, "RGB").resize(size, Image.Resampling.BOX), dtype=np.uint8)
    low_alpha = np.asarray(Image.fromarray(crop[:, :, 3], "L").resize(size, Image.Resampling.BOX), dtype=np.uint8)
    low_float = low_rgb.astype(np.float32) / 255
    low_a = low_alpha.astype(np.float32) / 255
    if association == "premultiplied":
        low_float = np.divide(low_float, low_a[:, :, None], out=np.zeros_like(low_float), where=low_a[:, :, None] > 1e-6)
    up_rgb = np.asarray(Image.fromarray(np.uint8(np.clip(low_float * 255 + .5, 0, 255)), "RGB").resize((w, h), Image.Resampling.NEAREST), dtype=np.uint8)
    up_alpha = crop[:, :, 3] if alpha_mode == "preserve source" else np.asarray(Image.fromarray(low_alpha, "L").resize((w, h), Image.Resampling.NEAREST), dtype=np.uint8)
    return up_rgb, up_alpha, low_alpha


def metrics(rgb: np.ndarray, alpha: np.ndarray, source: np.ndarray) -> dict[str, object]:
    masks = get_masks(source[:, :, 3])
    result: dict[str, object] = {"alpha changed ratio": float(np.mean(alpha != source[:, :, 3]))}
    for bg, color in BACKGROUNDS.items():
        output = composite(rgb, alpha, color)
        reference = composite(source[:, :, :3], source[:, :, 3], color)
        entry = {}
        for region, mask in masks.items():
            delta = output[mask] - reference[mask]
            entry[region] = float(np.mean(delta ** 2) ** .5) if delta.size else 0.0
        result[bg + " composited sRGB RMSE"] = entry
    edge = masks["alpha edge"]
    delta_code = rgb[edge].astype(np.float32) - source[:, :, :3][edge].astype(np.float32)
    result["edge RGB8 RMSE from source"] = float(np.mean(delta_code ** 2) ** .5) if delta_code.size else 0.0
    return result


def study_asset(spec: tuple[str, str, str, str]) -> dict[str, object]:
    rgba, provenance, digest = load_asset(spec)
    h, w = rgba.shape[:2]
    scales = {}
    for scale in SCALES:
        alignment = 2 if scale == .5 else 4
        locked, cycling = pan_origins(w, SOURCE_CROP, alignment)
        y = max(0, (h - SOURCE_CROP) // 2)
        y -= y % alignment
        phase_result = {}
        sheets = {}
        for phase, positions in ((PHASES[0], locked), (PHASES[1], cycling)):
            rows = []
            frame_metrics = []
            for x in positions:
                crop = rgba[y:y + SOURCE_CROP, x:x + SOURCE_CROP]
                rendered = {}
                for assoc in ASSOCIATIONS:
                    rendered[assoc] = {}
                    for alpha_mode in ALPHA_MODES:
                        rgb, alpha, low_alpha = resize(crop, scale, assoc, alpha_mode)
                        rendered[assoc][alpha_mode] = (rgb, alpha)
                        if not np.array_equal(alpha, crop[:, :, 3]) and alpha_mode == "preserve source":
                            raise ValueError("source alpha preservation failed")
                rows.append({"x": x, "crop": crop, "rendered": rendered})
                frame_metrics.append({
                    assoc: {mode: metrics(*rendered[assoc][mode], crop) for mode in ALPHA_MODES}
                    for assoc in ASSOCIATIONS
                })
            temporal = {}
            alpha_full = rgba[:, :, 3]
            for assoc in ASSOCIATIONS:
                temporal[assoc] = {}
                for alpha_mode in ALPHA_MODES:
                    pair_values = []
                    for i, (x0, x1) in enumerate(zip(positions, positions[1:])):
                        xlo, xhi = max(x0, x1), min(x0 + SOURCE_CROP, x1 + SOURCE_CROP)
                        sl = (slice(None), slice(xlo - x0, xhi - x0))
                        sr = (slice(None), slice(xlo - x1, xhi - x1))
                        a = rows[i]["rendered"][assoc][alpha_mode][0][sl]
                        b = rows[i + 1]["rendered"][assoc][alpha_mode][0][sr]
                        delta = b.astype(np.int16) - a.astype(np.int16)
                        region = alpha_full[y:y + SOURCE_CROP, xlo:xhi] > 0
                        values = delta[region]
                        pair_values.append({
                            "changed visible ratio >1/255": float(np.mean(np.max(np.abs(values), axis=1) > 1.0)) if values.size else 0.0,
                            "visible RGB8 RMSE": float(np.mean(values.astype(np.float32) ** 2) ** .5) if values.size else 0.0,
                        })
                    temporal[assoc][alpha_mode] = {
                        "mean adjacent changed visible ratio >1/255": float(np.mean([p["changed visible ratio >1/255"] for p in pair_values])),
                        "mean adjacent visible RGB8 RMSE": float(np.mean([p["visible RGB8 RMSE"] for p in pair_values])),
                    }
            phase_result[phase] = {"frame metrics": frame_metrics, "temporal": temporal, "positions x": positions}
            if phase == PHASES[0]:
                sheets[phase] = rows
        # A compact sheet shows source and the four resize policies for initial/middle/final locked crop.
        selected = [0, len(sheets[PHASES[0]]) // 2, len(sheets[PHASES[0]]) - 1]
        tile = 144
        labels = [("source", None, None)] + [(a, m, None) for a in ASSOCIATIONS for m in ALPHA_MODES]
        sheet = Image.new("RGB", (len(selected) * tile, len(labels) * (tile + 18)), (28, 29, 33))
        draw, font = ImageDraw.Draw(sheet), ImageFont.load_default()
        for col, index in enumerate(selected):
            row = sheets[PHASES[0]][index]
            crop = row["crop"]
            for r, (assoc, mode, _) in enumerate(labels):
                x0, y0 = col * tile, r * (tile + 18)
                if assoc == "source":
                    panel = Image.fromarray(crop, "RGBA")
                    label = "source"
                else:
                    rgb, alpha = row["rendered"][assoc][mode]
                    panel = Image.fromarray(np.dstack((rgb, alpha)), "RGBA")
                    label = f"{assoc}/{mode}"
                draw.text((x0 + 2, y0 + 2), f"F{index:02d} {label}", fill=(245, 245, 245), font=font)
                panel = panel.resize((tile, tile), Image.Resampling.NEAREST)
                panel = Image.alpha_composite(Image.new("RGBA", panel.size, (128, 128, 128, 255)), panel)
                sheet.paste(panel.convert("RGB"), (x0, y0 + 18))
        contact = CAPTURE / f"{spec[3]}-{scale:.2f}-resize-only.png"
        contact.parent.mkdir(parents=True, exist_ok=True)
        sheet.save(contact)
        scales[f"{scale:.2f}"] = {"alignment source pixels": alignment, "phase locked x": locked, "phase cycling x": cycling,
                                   "phase locked y": y, "phase cycling y": y, "phase metrics": phase_result,
                                   "contact sheet": str(contact.relative_to(ROOT))}
    return {"asset": spec[3], "source": spec[0], "sha256": digest, "provenance": provenance, "dimensions": [w, h], "scales": scales}


def main() -> dict[str, object]:
    report = {
        "question": "What does alpha association change in BOX/nearest resizing alone, with and without restoring source alpha?",
        "method": "Three SHA-verified CC0 images; 384px crops and nine horizontal positions; scale .5/.75; straight or premultiplied RGB BOX downscale and nearest upscale, no palette/effect. Compare source-alpha preservation against nearest-upscaled BOX alpha. Report source error after linear-light compositing on black/gray/white, edge RGB error, alpha change, and same-world overlap changes for phase-locked/cycling pan paths.",
        "limits": "Python/Pillow uint8 research model; not Core or Artifact runtime, and not a perceptual quality score. Premultiply is quantized to uint8 before BOX. Color profiles are ignored. Static source pans only; no motion, GPU parity, or performance measurement.",
        "assets": [study_asset(spec) for spec in ASSETS],
    }
    CAPTURE.mkdir(parents=True, exist_ok=True)
    (CAPTURE / "alpha-resize-association-isolation-metrics.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    result = main()
    print(f"Isolated alpha association in resize-only study across {len(result['assets'])} assets.")
