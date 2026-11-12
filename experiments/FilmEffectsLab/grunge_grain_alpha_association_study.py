#!/usr/bin/env python3
"""Compare Film Grunge grain response for straight/premultiplied alpha models."""
from __future__ import annotations

import hashlib
import json
import platform
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

from grunge_grain_spatial_scale_study import FRAME, SEED, grain_field, round_positive_like_std

ROOT = Path(__file__).resolve().parents[2]
PLAN = ROOT / "experiments/FilmEffectsLab/plans/grunge-grain-alpha-association-r1.json"
SOURCE = ROOT / "Artifact/src/Effect/ArtifactCreativeEffects.cpp"
IMAGE_HEADER = ROOT / "ArtifactCore/include/Image/ImageF32x4_RGBA.ixx"
MODEL_HELPER = ROOT / "experiments/FilmEffectsLab/grunge_grain_spatial_scale_study.py"
OUT = ROOT / "experiments/FilmEffectsLab/captures/grunge-grain-alpha-association-r1"
W, H, SIZE = 256, 128, 1.4
BACKGROUNDS = {"black": np.array([0, 0, 0], np.float32),
               "gray": np.array([.18, .18, .18], np.float32),
               "white": np.array([1, 1, 1], np.float32)}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fixture() -> tuple[np.ndarray, np.ndarray]:
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    cx, cy, rx, ry = 128.0, 64.0, 78.0, 42.0
    normalized_radius = np.sqrt(((xx-cx)/rx)**2 + ((yy-cy)/ry)**2)
    signed_px = (normalized_radius - 1.0) * ry
    alpha = np.clip(.5 - signed_px/8.0, 0, 1).astype(np.float32)
    logical_rgb = np.array([.82, .34, .12], np.float32)
    physical_bgr = logical_rgb[::-1].copy()
    return alpha, physical_bgr


def composite(stored_bgr: np.ndarray, alpha: np.ndarray, background: np.ndarray, association: str) -> np.ndarray:
    if association == "straight":
        return stored_bgr * alpha[..., None] + background * (1.0-alpha[..., None])
    return stored_bgr + background * (1.0-alpha[..., None])


def to_png_rgb(bgr: np.ndarray, path: Path) -> None:
    rgb = bgr[..., ::-1]
    Image.fromarray(np.uint8(np.clip(rgb, 0, 1)*255+.5), "RGB").save(path)


def region_masks(alpha: np.ndarray) -> dict[str, np.ndarray]:
    return {
        "transparent": alpha == 0,
        "low_alpha": (alpha > 0) & (alpha <= .1),
        "edge": (alpha > .1) & (alpha < .9),
        "near_opaque": (alpha >= .9) & (alpha < 1),
        "opaque": alpha == 1,
    }


def error_stats(delta: np.ndarray, mask: np.ndarray) -> dict:
    values = delta[mask]
    if values.size == 0:
        return {"pixel_count": 0, "mae": None, "rms": None, "max_abs": None}
    return {"pixel_count": int(mask.sum()), "mae": float(np.mean(np.abs(values))),
            "rms": float(np.sqrt(np.mean(values*values))), "max_abs": float(np.max(np.abs(values)))}


def main() -> None:
    plan = json.loads(PLAN.read_text(encoding="utf-8"))
    src_text = SOURCE.read_text(encoding="utf-8")
    header_text = IMAGE_HEADER.read_text(encoding="utf-8")
    markers = {
        "film_grunge_adds_chroma_grain": "destination[offset + channel] =" in src_text and "chromaGrain + damage" in src_text,
        "film_grunge_copies_alpha": "destination[offset + 3] = shiftedRow[x][3]" in src_text,
        "image_float_storage_is_bgra": "canonical B,G,R,A order" in header_text,
        "image_descriptor_is_preserved": "result.setColorDescriptor(image.colorDescriptor())" in src_text,
    }
    if not all(markers.values()):
        raise RuntimeError(f"Source contract markers not all found: {markers}")
    alpha, foreground_bgr = fixture()
    grain, block, frame_seed = grain_field(W, H, SIZE)
    assert block == round_positive_like_std(SIZE) == 1
    OUT.mkdir(parents=True, exist_ok=True)
    report = {
        "plan_id": plan["plan_id"], "plan_sha256": sha(PLAN), "script_sha256": sha(Path(__file__)),
        "source_sha256": sha(SOURCE), "image_header_sha256": sha(IMAGE_HEADER), "grain_model_helper_sha256": sha(MODEL_HELPER),
        "source_markers_verified": markers, "python": platform.python_version(), "numpy": np.__version__, "pillow": Image.__version__,
        "fixture": {"width": W, "height": H, "alpha_min": float(alpha.min()), "alpha_max": float(alpha.max()),
                    "alpha_region_pixel_counts": {k: int(v.sum()) for k, v in region_masks(alpha).items()},
                    "logical_foreground_rgb": [.82, .34, .12], "physical_storage": "BGRA"},
        "grain": {"grainAmount": .075, "requested_grainSize": SIZE, "rounded_grainBlock": block,
                  "seed": SEED, "frame": FRAME, "frameSeed_u32": frame_seed,
                  "model_assumption": "intended modulo-2^32 arithmetic; source signed overflow caveat applies"},
        "limits": "Conditional source-equation model only. Actual pipeline alpha descriptor/input association is unknown. Direct numeric compositing only; no transfer/color conversion or product display transform.",
        "conditions": [], "contact_sheets": [],
    }
    masks = region_masks(alpha)
    for association in ("straight", "premultiplied"):
        source_bgr = np.broadcast_to(foreground_bgr, (H, W, 3)).copy()
        if association == "premultiplied":
            source_bgr *= alpha[..., None]
        candidate_bgr = source_bgr + grain
        candidate_bgra = np.concatenate((candidate_bgr, alpha[..., None]), axis=2)
        assert np.isfinite(candidate_bgr).all()
        assert np.array_equal(candidate_bgra[..., 3], alpha)
        region_report = {}
        composites = {}
        baseline_composites = {}
        for name, bg in BACKGROUNDS.items():
            baseline = composite(source_bgr, alpha, bg, association)
            candidate = composite(candidate_bgr, alpha, bg, association)
            delta = candidate-baseline
            baseline_composites[name] = baseline
            composites[name] = candidate
            region_report[name] = {region: error_stats(delta, mask) for region, mask in masks.items()}
            to_png_rgb(candidate, OUT / f"{association}-{name}-candidate.png")
            to_png_rgb(baseline, OUT / f"{association}-{name}-baseline.png")
        transparent_delta = composites["gray"][masks["transparent"]] - baseline_composites["gray"][masks["transparent"]]
        premul_violation = None
        if association == "premultiplied":
            # B/G/R bounds in this synthetic bounded [0,1] fixture; this is not an HDR invariant.
            rgb = candidate_bgr
            premul_violation = {
                "channels_below_zero": int(np.count_nonzero(rgb < 0)),
                "channels_above_alpha": int(np.count_nonzero(rgb > alpha[..., None])),
                "channels_total": int(rgb.size),
                "violating_pixel_fraction": float(np.mean(np.any((rgb < 0) | (rgb > alpha[..., None]), axis=2))),
            }
        report["conditions"].append({
            "association": association, "alpha_identity": True,
            "stored_rgb_delta_vs_baseline": {region: error_stats(grain, mask) for region, mask in masks.items()},
            "composited_delta_by_background_and_alpha_region": region_report,
            "zero_alpha_gray_background_delta_mae": float(np.mean(np.abs(transparent_delta))) if transparent_delta.size else 0.0,
            "premultiplied_bounded_color_violations": premul_violation,
        })
        # Contact sheet: rows are backgrounds; columns baseline and candidate.
        sheet = Image.new("RGB", (2*W, 3*150), (28,29,32))
        draw, font = ImageDraw.Draw(sheet), ImageFont.load_default()
        for row, name in enumerate(BACKGROUNDS):
            for col, state in enumerate(("baseline", "candidate")):
                x, y = col*W, row*150
                draw.text((x+3, y+2), f"{association} / {name} / {state}", fill=(245,245,245), font=font)
                image = baseline_composites[name] if state == "baseline" else composites[name]
                tile = Image.fromarray(np.uint8(np.clip(image[..., ::-1], 0, 1)*255+.5), "RGB")
                sheet.paste(tile, (x, y+18))
        path = OUT / f"{association}-background-contact.png"
        sheet.save(path)
        report["contact_sheets"].append(str(path.relative_to(ROOT)))
    metrics_path = OUT / "metrics.json"
    metrics_path.write_text(json.dumps(report, indent=2, ensure_ascii=False)+"\n", encoding="utf-8", newline="\n")
    checksum = hashlib.sha256(metrics_path.read_bytes()).hexdigest()
    (OUT / "metrics.sha256").write_text(f"{checksum}  metrics.json\n", encoding="ascii", newline="\n")
    print(f"Wrote {len(report['conditions'])} association conditions to {OUT}")


if __name__ == "__main__":
    main()
