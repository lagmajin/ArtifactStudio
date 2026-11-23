"""Study grain motion policies at the alpha edges of moving RGBA cutouts."""

from __future__ import annotations

import json
import platform
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from adaptive_palette_weighting_study import ASSETS, load_asset  # noqa: E402
sys.path.insert(0, str(ROOT))
from retro_lab import PRESETS, apply_retro, srgb_to_linear  # noqa: E402

CAPTURE = ROOT / "captures" / "grain-motion-alpha-edge"
ASSET_SPECS = (ASSETS[0], ASSETS[1], ASSETS[2])
SCENE_SIZE = (256, 192)
SPRITE_SIZE = (96, 96)
FRAME_COUNT = 20
SEED = 48117
FRAME_SEED_STEP = 431
POLICIES = ("screen-fixed", "frame-varying", "layer-material-fixed")
SHEET_FRAMES = (0, 4, 9, 15, 19)
LUMA = np.array([0.2126, 0.7152, 0.0722], dtype=np.float32)


def normalize_sprite(rgba: np.ndarray) -> Image.Image:
    source = Image.fromarray(rgba, "RGBA")
    source.thumbnail(SPRITE_SIZE, Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", SPRITE_SIZE, (0, 0, 0, 0))
    canvas.alpha_composite(source, ((SPRITE_SIZE[0] - source.width) // 2,
                                    (SPRITE_SIZE[1] - source.height) // 2))
    return canvas


def make_background(kind: str, seed: int) -> Image.Image:
    width, height = SCENE_SIZE
    if kind == "neutral-gray":
        rgb = np.full((height, width, 3), 118, dtype=np.uint8)
    else:
        yy, xx = np.mgrid[0:height, 0:width].astype(np.float32)
        rng = np.random.default_rng(seed)
        coarse = rng.normal(0.0, 1.0, (24, 32)).astype(np.float32)
        coarse = np.asarray(Image.fromarray(coarse, "F").resize((width, height), Image.Resampling.BILINEAR))
        r = 0.34 + 0.13 * np.sin(xx * .027) + 0.045 * coarse
        g = 0.40 + 0.11 * np.cos(yy * .031) + 0.040 * coarse
        b = 0.48 + 0.08 * np.sin((xx + yy) * .019) + 0.035 * coarse
        rgb = np.uint8(np.clip(np.stack((r, g, b), axis=2) * 255.0 + .5, 0, 255))
    return Image.fromarray(rgb, "RGB")


def position(frame: int) -> tuple[int, int]:
    return 24 + frame * 4, 46 + frame


def compose(background: Image.Image, sprite: Image.Image, frame: int) -> Image.Image:
    base = background.convert("RGBA")
    base.alpha_composite(sprite.convert("RGBA"), position(frame))
    return base


def linear_luma(image: Image.Image) -> np.ndarray:
    rgb = np.asarray(image.convert("RGB"), dtype=np.float32) / 255.0
    return srgb_to_linear(rgb) @ LUMA


def alpha_masks(sprite: Image.Image) -> dict[str, np.ndarray]:
    alpha = np.asarray(sprite.getchannel("A"), dtype=np.uint8)
    visible = alpha > 0
    edge = np.asarray(sprite.getchannel("A").filter(ImageFilter.MaxFilter(5)), dtype=np.uint8) > 0
    edge &= ~np.asarray(sprite.getchannel("A").filter(ImageFilter.MinFilter(5)), dtype=np.uint8).astype(bool)
    return {"opaque interior alpha>=250": alpha >= 250,
            "partial alpha": (alpha > 0) & (alpha < 250),
            "visible edge band 5x5": visible & edge,
            "all visible": visible}


def place_mask(mask: np.ndarray, frame: int) -> np.ndarray:
    x, y = position(frame)
    out = np.zeros((SCENE_SIZE[1], SCENE_SIZE[0]), dtype=bool)
    out[y:y + SPRITE_SIZE[1], x:x + SPRITE_SIZE[0]] = mask
    return out


def residual(on: Image.Image, off: Image.Image) -> np.ndarray:
    return srgb_to_linear(np.asarray(on.convert("RGB"), dtype=np.float32) / 255.0) - \
        srgb_to_linear(np.asarray(off.convert("RGB"), dtype=np.float32) / 255.0)


def region_stats(delta: np.ndarray, mask: np.ndarray, on: Image.Image, off: Image.Image) -> dict[str, object]:
    count = int(np.count_nonzero(mask))
    if not count:
        return {"pixel count": 0, "luma RMS": None, "luma P95 absolute": None,
                "RGB8 changed pixel ratio": None}
    luma = delta @ LUMA
    on_rgb = np.asarray(on.convert("RGB"), dtype=np.uint8)
    off_rgb = np.asarray(off.convert("RGB"), dtype=np.uint8)
    return {"pixel count": count,
            "luma RMS": float(np.sqrt(np.mean(luma[mask] ** 2))),
            "luma P95 absolute": float(np.percentile(np.abs(luma[mask]), 95)),
            "RGB8 changed pixel ratio": float(np.mean(np.any(on_rgb[mask] != off_rgb[mask], axis=1)))}


def local_luma_residual(delta: np.ndarray, frame: int) -> np.ndarray:
    x, y = position(frame)
    h, w = SPRITE_SIZE[1], SPRITE_SIZE[0]
    return (delta[y:y + h, x:x + w] @ LUMA)


def pair_stats(a: np.ndarray, b: np.ndarray, mask: np.ndarray) -> dict[str, object]:
    count = int(np.count_nonzero(mask))
    if not count:
        return {"pixel count": 0, "aligned residual change RMS": None, "correlation": None}
    av, bv = a[mask], b[mask]
    diff = bv - av
    denominator = float(np.std(av) * np.std(bv))
    correlation = float(np.mean((av - av.mean()) * (bv - bv.mean())) / denominator) if denominator > 1e-15 else None
    return {"pixel count": count,
            "aligned residual change RMS": float(np.sqrt(np.mean(diff * diff))),
            "correlation": correlation}


def save_sheet(asset_name: str, background_name: str, preset_name: str,
               outputs: dict[str, list[Image.Image]]) -> None:
    tile_w, tile_h, label_h = 192, 144, 20
    sheet = Image.new("RGB", (len(SHEET_FRAMES) * tile_w, len(POLICIES) * (tile_h + label_h)), (28, 29, 33))
    draw, font = ImageDraw.Draw(sheet), ImageFont.load_default()
    for row, policy in enumerate(POLICIES):
        for col, frame in enumerate(SHEET_FRAMES):
            x, y = col * tile_w, row * (tile_h + label_h)
            draw.text((x + 3, y + 3), f"{policy} / F{frame:02d}", fill=(245, 245, 245), font=font)
            thumb = outputs[policy][frame].convert("RGB")
            thumb.thumbnail((tile_w, tile_h), Image.Resampling.LANCZOS)
            sheet.paste(thumb, (x + (tile_w - thumb.width) // 2,
                                y + label_h + (tile_h - thumb.height) // 2))
    CAPTURE.mkdir(parents=True, exist_ok=True)
    sheet.save(CAPTURE / f"{asset_name}-{background_name}-{preset_name}.jpg", quality=92)

    detail = Image.new("RGB", (len(SHEET_FRAMES) * 192, len(POLICIES) * 212), (28, 29, 33))
    detail_draw = ImageDraw.Draw(detail)
    for row, policy in enumerate(POLICIES):
        for col, frame in enumerate(SHEET_FRAMES):
            x, y = col * 192, row * 212
            detail_draw.text((x + 3, y + 3), f"{policy} / F{frame:02d} / 2x", fill=(245, 245, 245), font=font)
            sx, sy = position(frame)
            crop = outputs[policy][frame].crop((sx, sy, sx + SPRITE_SIZE[0], sy + SPRITE_SIZE[1]))
            crop = crop.convert("RGB").resize((192, 192), Image.Resampling.NEAREST)
            detail.paste(crop, (x, y + 20))
    detail.save(CAPTURE / f"{asset_name}-{background_name}-{preset_name}-edge-detail.jpg", quality=95)


def main() -> dict[str, object]:
    CAPTURE.mkdir(parents=True, exist_ok=True)
    groups: list[dict[str, object]] = []
    pair_records: list[dict[str, object]] = []
    provenance: dict[str, object] = {}
    alpha_gates = 0

    for spec in ASSET_SPECS:
        rgba, entry, digest = load_asset(spec)
        asset_name = spec[3]
        sprite = normalize_sprite(rgba)
        masks = alpha_masks(sprite)
        alpha = np.asarray(sprite.getchannel("A"), dtype=np.uint8)
        if not np.any((alpha > 0) & (alpha < 255)):
            raise ValueError(f"asset lacks partial alpha edge: {asset_name}")
        provenance[asset_name] = {"path": spec[0], "SHA-256": digest,
                                  "provenance record": entry,
                                  "source dimensions": list(Image.fromarray(rgba, "RGBA").size),
                                  "normalized sprite dimensions": list(SPRITE_SIZE),
                                  "partial alpha pixel ratio": float(np.mean((alpha > 0) & (alpha < 255))),
                                  "visible pixel ratio": float(np.mean(alpha > 0))}

        for background_index, background_name in enumerate(("neutral-gray", "spatial-texture")):
            background = make_background(background_name, 509 + background_index)
            for preset_name, preset in PRESETS.items():
                layer_off_settings = dict(preset)
                layer_off_settings["grain"] = 0.0
                layer_off = apply_retro(sprite, **layer_off_settings, seed=SEED)
                layer_on = apply_retro(sprite, **preset, seed=SEED)
                if not np.array_equal(np.asarray(layer_off.getchannel("A")), alpha) or \
                   not np.array_equal(np.asarray(layer_on.getchannel("A")), alpha):
                    raise RuntimeError(f"layer alpha changed: {asset_name}/{preset_name}")
                alpha_gates += 2
                outputs: dict[str, list[Image.Image]] = {p: [] for p in POLICIES}
                residuals: dict[str, list[np.ndarray]] = {p: [] for p in POLICIES}
                stats_frames: list[dict[str, object]] = []

                for frame in range(FRAME_COUNT):
                    source_scene = compose(background, sprite, frame)
                    screen_off = apply_retro(source_scene, **layer_off_settings, seed=SEED)
                    frame_seed = SEED + FRAME_SEED_STEP * frame
                    screen_fixed = apply_retro(source_scene, **preset, seed=SEED)
                    frame_varying = apply_retro(source_scene, **preset, seed=frame_seed)
                    material_on = compose(background, layer_on, frame)
                    material_off = compose(background, layer_off, frame)
                    frame_outputs = {"screen-fixed": (screen_fixed, screen_off),
                                     "frame-varying": (frame_varying, screen_off),
                                     "layer-material-fixed": (material_on, material_off)}
                    frame_record: dict[str, object] = {"frame": frame, "position x/y": list(position(frame)),
                                                       "policies": {}}
                    for policy_name, (on, off) in frame_outputs.items():
                        outputs[policy_name].append(on)
                        delta = residual(on, off)
                        residuals[policy_name].append(delta)
                        placed_masks = {name: place_mask(mask, frame) for name, mask in masks.items()}
                        outside = ~placed_masks["all visible"]
                        frame_record["policies"][policy_name] = {
                            region: region_stats(delta, mask, on, off)
                            for region, mask in {**placed_masks, "outside object": outside}.items()}
                    stats_frames.append(frame_record)

                for policy_name in POLICIES:
                    for frame in range(FRAME_COUNT - 1):
                        a = local_luma_residual(residuals[policy_name][frame], frame)
                        b = local_luma_residual(residuals[policy_name][frame + 1], frame + 1)
                        pair_mask = masks["all visible"]
                        pair_records.append({"asset": asset_name, "background": background_name,
                                             "preset": preset_name, "policy": policy_name,
                                             "frame pair": [frame, frame + 1],
                                             **pair_stats(a, b, pair_mask)})
                save_sheet(asset_name, background_name, preset_name, outputs)
                groups.append({"asset": asset_name, "background": background_name,
                               "preset": preset_name, "settings": dict(preset),
                               "alpha masks pixel counts": {k: int(np.count_nonzero(v)) for k, v in masks.items()},
                               "frame records": stats_frames})

    summary: dict[str, object] = {}
    for policy in POLICIES:
        values = [r["aligned residual change RMS"] for r in pair_records
                  if r["policy"] == policy and r["aligned residual change RMS"] is not None]
        correlations = [r["correlation"] for r in pair_records
                        if r["policy"] == policy and r["correlation"] is not None]
        summary[policy] = {"aligned adjacent pair count": len(values),
                           "median object-aligned residual change RMS": float(np.median(values)),
                           "median object-aligned residual correlation": float(np.median(correlations))}

    report = {"study": "grain-motion-alpha-edge", "date": "2026-11-11",
              "environment": {"python": platform.python_version(), "numpy": np.__version__,
                              "pillow": Image.__version__},
              "plan": {"question": "How do scene-space and layer-material grain policies change residual response across moving RGBA alpha edges?",
                       "sources": provenance, "scene size": list(SCENE_SIZE),
                       "sprite size": list(SPRITE_SIZE), "frames": FRAME_COUNT,
                       "motion": "integer translation x += 4 px/frame, y += 1 px/frame",
                       "backgrounds": ["neutral-gray", "spatial-texture"],
                       "policies": {
                           "screen-fixed": "Run current apply_retro on the full composited scene with fixed seed 48117; pair against same-frame full-scene grain=0.",
                           "frame-varying": "Run current apply_retro on full scene with seed 48117 + 431*frame; pair against same-frame grain=0.",
                           "layer-material-fixed": "Run current apply_retro once on the RGBA sprite using fixed seed 48117; move that result over the same background and pair against grain=0 treatment of the same sprite."},
                       "alpha regions": "source alpha >=250 interior, 0<alpha<250 partial pixels, visible edge band from 5x5 MaxFilter/MinFilter, all visible, and outside object.",
                       "comparison": "Same-frame on/off residuals decoded from RGB8 sRGB to linear-light RGB; Rec.709 luma RMS/P95 and byte changed-pixel ratio. Adjacent residuals are extracted at object-local coordinates over source alpha>0.",
                       "composite domain": "Pillow RGBA alpha_composite on encoded sRGB values for all branches; this is a controlled lab condition, not a claim about the product compositor.",
                       "seed": SEED, "frame seed step": FRAME_SEED_STEP,
                       "acceptance": "No approved appearance target, perceptual observer, or pass/fail threshold; descriptive study only."},
              "alpha exact gates": alpha_gates, "policy summary": summary,
              "groups": groups, "adjacent object-aligned pair records": pair_records,
              "limitations": ["integer sprite translation only; no subpixel transforms, rotation, deformation, occlusion, or scene changes",
                              "scene-space and material-space policies have different operation order; paired residuals isolate grain response within each route but do not establish route quality",
                              "alpha composite is encoded-sRGB Pillow model; linear-light and product composition are unverified",
                              "Python/Pillow only; no Artifact runtime, Core/GPU, preview/export, performance, or memory validation"]}
    output = CAPTURE / "grain-motion-alpha-edge-study.json"
    output.write_text(json.dumps(report, indent=2, ensure_ascii=False, allow_nan=False) + "\n", encoding="utf-8")
    print(json.dumps({"report": str(output), "groups": len(groups),
                      "adjacent pairs": len(pair_records), "alpha exact gates": alpha_gates,
                      "summary": summary}, indent=2))
    return report


if __name__ == "__main__":
    main()
