#!/usr/bin/env python3
"""Compare film grain and damage source equations at a translucent edge."""
from __future__ import annotations

import hashlib
import json
import platform
from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageDraw, ImageFont

from grunge_grain_spatial_scale_study import grain_field

ROOT = Path(__file__).resolve().parents[2]
PLAN = ROOT / "experiments/FilmEffectsLab/plans/film_suite_alpha_response-r1.json"
GRUNGE_SRC = ROOT / "Artifact/src/Effect/ArtifactCreativeEffects.cpp"
DAMAGE_SRC = ROOT / "Artifact/src/Effects/Rasterizer/FilmDamageEffect.cpp"
IMAGE_HEADER = ROOT / "ArtifactCore/include/Image/ImageF32x4_RGBA.ixx"
GRAIN_HELPER = ROOT / "experiments/FilmEffectsLab/grunge_grain_spatial_scale_study.py"
OUT = ROOT / "experiments/FilmEffectsLab/captures/film-suite-alpha-response-r1"
W, H = 256, 128
BGS = {"black": np.array([0, 0, 0], np.float32),
       "gray": np.array([.18, .18, .18], np.float32),
       "white": np.array([1, 1, 1], np.float32)}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def alpha_edge() -> tuple[np.ndarray, np.ndarray]:
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    radius = np.sqrt(((xx-128)/78)**2 + ((yy-64)/42)**2)
    signed = (radius-1)*42
    alpha = np.clip(.5-signed/8, 0, 1).astype(np.float32)
    # Logical RGB [.82,.34,.12] represented in physical BGRA storage.
    bgr = np.array([.12, .34, .82], np.float32)
    return alpha, bgr


def damage_plate() -> np.ndarray:
    overlay = np.zeros((H, W, 4), np.float32)
    cv2.line(overlay, (84, 10), (112, 116), (.90, .90, .90, .45), 1, cv2.LINE_AA)
    cv2.circle(overlay, (166, 48), 8, (.82, .82, .82, .60), -1, cv2.LINE_AA)
    cv2.line(overlay, (184, 12), (176, 112), (.04, .04, .04, .28), 1, cv2.LINE_AA)
    return overlay


def regions(alpha: np.ndarray) -> dict[str, np.ndarray]:
    return {"transparent": alpha == 0, "low": (alpha > 0) & (alpha <= .1),
            "edge": (alpha > .1) & (alpha < .9), "near_opaque": (alpha >= .9) & (alpha < 1),
            "opaque": alpha == 1}


def comp(rgb: np.ndarray, alpha: np.ndarray, bg: np.ndarray, association: str) -> np.ndarray:
    if association == "straight":
        return rgb * alpha[..., None] + bg * (1-alpha[..., None])
    return rgb + bg * (1-alpha[..., None])


def region_stats(delta: np.ndarray, mask: np.ndarray) -> dict:
    values = delta[mask]
    if not values.size:
        return {"pixels": 0, "mae": None, "rms": None, "max_abs": None}
    return {"pixels": int(mask.sum()), "mae": float(np.mean(np.abs(values))),
            "rms": float(np.sqrt(np.mean(values*values))), "max_abs": float(np.max(np.abs(values)))}


def save_bgr(rgb_bgr: np.ndarray, path: Path) -> None:
    Image.fromarray(np.uint8(np.clip(rgb_bgr[..., ::-1], 0, 1)*255+.5), "RGB").save(path)


def main() -> None:
    plan = json.loads(PLAN.read_text(encoding="utf-8"))
    grunge_text, damage_text = GRUNGE_SRC.read_text(encoding="utf-8"), DAMAGE_SRC.read_text(encoding="utf-8")
    header_text = IMAGE_HEADER.read_text(encoding="utf-8")
    markers = {
        "grunge_additive_rgb": "chromaGrain + damage" in grunge_text,
        "grunge_copies_alpha": "destination[offset + 3] = shiftedRow[x][3]" in grunge_text,
        "damage_overlay_rgb_blend": "value = value * (1.0f - damageAlpha) + damageRow[x][c] * damageAlpha" in damage_text,
        "damage_restores_alpha": "outputRow[x][3] = sourceRow[x][3]" in damage_text,
        "canonical_float_storage_bgra": "canonical B,G,R,A order" in header_text,
    }
    if not all(markers.values()):
        raise RuntimeError(f"Source equation markers mismatch: {markers}")
    alpha, foreground_bgr = alpha_edge()
    base = np.broadcast_to(foreground_bgr, (H, W, 3)).copy()
    overlay = damage_plate()
    grain, grain_block, frame_seed = grain_field(W, H, 1.4)
    effect_rgb = {
        "baseline": lambda source: source.copy(),
        "film_grunge_grain": lambda source: source + grain,
        "film_damage_overlay": lambda source: source*(1-overlay[..., 3, None]) + overlay[..., :3]*overlay[..., 3, None],
    }
    masks = regions(alpha)
    OUT.mkdir(parents=True, exist_ok=True)
    report = {
        "plan_id": plan["plan_id"], "plan_sha256": sha(PLAN), "script_sha256": sha(Path(__file__)),
        "grunge_source_sha256": sha(GRUNGE_SRC), "damage_source_sha256": sha(DAMAGE_SRC),
        "image_header_sha256": sha(IMAGE_HEADER), "grain_helper_sha256": sha(GRAIN_HELPER),
        "source_markers_verified": markers, "python": platform.python_version(),
        "numpy": np.__version__, "opencv": cv2.__version__, "pillow": Image.__version__,
        "fixture": {"size": [W, H], "alpha_region_pixels": {k: int(v.sum()) for k,v in masks.items()},
                    "physical_order": "BGRA", "logical_foreground_rgb": [.82,.34,.12],
                    "overlay_coverage": float(np.mean(overlay[...,3] > 0)), "overlay_alpha_max": float(overlay[...,3].max())},
        "grain_conditions": {"amount": .075, "requested_size": 1.4, "rounded_block": grain_block,
                              "seed": 1977, "frame": 0, "intended_uint32_wrap_model": True},
        "damage_conditions": {"grain": 0, "weave": 0, "flicker": 0, "filmBurn": 0,
                              "overlay": "fixed diagnostic line/circles; not C++ RNG placement parity"},
        "limits": "Conditional independent source-equation model. Actual descriptor, alpha conversion, output blend, product rendering and display transform remain unverified.",
        "conditions": [], "contact_sheets": [],
    }
    for association in ("straight", "premultiplied"):
        source = base.copy() if association == "straight" else base*alpha[...,None]
        outputs = {"baseline": source.copy()}
        for effect in ("film_grunge_grain", "film_damage_overlay"):
            outputs[effect] = effect_rgb[effect](source)
            if not np.isfinite(outputs[effect]).all():
                raise RuntimeError(f"Nonfinite values in {association}/{effect}")
        sheet = Image.new("RGB", (W*3, 3*150), (28,29,32))
        draw, font = ImageDraw.Draw(sheet), ImageFont.load_default()
        for row, bg_name in enumerate(BGS):
            bg = BGS[bg_name]
            composited = {name: comp(rgb, alpha, bg, association) for name,rgb in outputs.items()}
            for col, name in enumerate(("baseline", "film_grunge_grain", "film_damage_overlay")):
                x,y = col*W,row*150
                draw.text((x+3,y+2), f"{association} / {bg_name} / {name}", fill=(245,245,245), font=font)
                tile = Image.fromarray(np.uint8(np.clip(composited[name][...,::-1],0,1)*255+.5), "RGB")
                sheet.paste(tile,(x,y+18))
                save_bgr(composited[name], OUT/f"{association}-{bg_name}-{name}.png")
            for effect in ("film_grunge_grain", "film_damage_overlay"):
                delta = composited[effect]-composited["baseline"]
                report["conditions"].append({
                    "association": association, "effect": effect, "background": bg_name,
                    "alpha_identity": True,
                    "stored_rgb_delta_by_region": {k: region_stats(outputs[effect]-source,mask) for k,mask in masks.items()},
                    "composite_delta_by_region": {k: region_stats(delta,mask) for k,mask in masks.items()},
                    "zero_alpha_composite_delta_mae": region_stats(delta,masks["transparent"])["mae"],
                    "output_below_zero_channels": int(np.count_nonzero(outputs[effect] < 0)),
                    "output_above_one_channels": int(np.count_nonzero(outputs[effect] > 1)),
                    "premultiplied_bound_violating_pixel_fraction": (
                        float(np.mean(np.any((outputs[effect] < 0) | (outputs[effect] > alpha[...,None]),axis=2)))
                        if association == "premultiplied" else None),
                })
        sheet_path = OUT/f"{association}-film-suite-alpha-contact.png"
        sheet.save(sheet_path)
        report["contact_sheets"].append(str(sheet_path.relative_to(ROOT)))
    metrics = OUT/"metrics.json"
    metrics.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",encoding="utf-8",newline="\n")
    checksum=hashlib.sha256(metrics.read_bytes()).hexdigest()
    (OUT/"metrics.sha256").write_text(f"{checksum}  metrics.json\n",encoding="ascii",newline="\n")
    print(f"Wrote {len(report['conditions'])} effect/association/background conditions to {OUT}")


if __name__ == "__main__":
    main()
