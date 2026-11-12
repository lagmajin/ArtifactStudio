"""Compare screen-fixed, frame-varying, and source-locked grain on photo pans."""

from __future__ import annotations

import json
import platform
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from full_suite_photo_grain_seed_variance_study import (  # noqa: E402
    ASSET_NAMES, MANIFEST, load_source, resize_long_edge,
)
sys.path.insert(0, str(ROOT))
from retro_lab import PRESETS, apply_retro, srgb_to_linear  # noqa: E402
from full_suite_photo_grain_quantization_counterfactual_study import render_prebyte_float  # noqa: E402

CAPTURE = ROOT / "captures" / "full-suite-photo-grain-motion-policy"
SOURCE_NAMES = (ASSET_NAMES[0], ASSET_NAMES[1], ASSET_NAMES[3])
EDGE = 512
FRAME_COUNT = 24
PAN_STEP = 2
BASE_SEED = 8411
FRAME_SEED_STEP = 977
POLICIES = ("screen-fixed", "frame-varying", "source-locked")
SHEET_FRAMES = (0, 1, 2, 11, 23)


def make_origins(size: tuple[int, int]) -> list[tuple[int, int]]:
    width, height = size
    if width >= height:
        if height < EDGE:
            raise ValueError(f"source short edge {height} is below {EDGE}")
        start = max(0, (width - EDGE - PAN_STEP * (FRAME_COUNT - 1)) // 2)
        return [(min(start + i * PAN_STEP, width - EDGE), (height - EDGE) // 2)
                for i in range(FRAME_COUNT)]
    if width < EDGE:
        raise ValueError(f"source short edge {width} is below {EDGE}")
    start = max(0, (height - EDGE - PAN_STEP * (FRAME_COUNT - 1)) // 2)
    return [((width - EDGE) // 2, min(start + i * PAN_STEP, height - EDGE))
            for i in range(FRAME_COUNT)]


def crop_frame(image: Image.Image, origin: tuple[int, int]) -> Image.Image:
    x, y = origin
    return image.crop((x, y, x + EDGE, y + EDGE))


def grain_residual(on: np.ndarray, off: np.ndarray) -> np.ndarray:
    return srgb_to_linear(on.astype(np.float32) / 255.0) - srgb_to_linear(off.astype(np.float32) / 255.0)


def pair_metrics(a: np.ndarray, b: np.ndarray, mask: np.ndarray) -> dict[str, object]:
    count = int(np.count_nonzero(mask))
    if not count:
        return {"valid pixel count": 0, "luma residual delta RMS": None,
                "luma residual delta P95 absolute": None, "luma correlation": None}
    weights = np.array([.2126, .7152, .0722], dtype=np.float32)
    la = a @ weights
    lb = b @ weights
    diff = la[mask] - lb[mask]
    va, vb = la[mask], lb[mask]
    denom = float(np.std(va) * np.std(vb))
    corr = float(np.mean((va - va.mean()) * (vb - vb.mean())) / denom) if denom > 1e-15 else None
    return {"valid pixel count": count,
            "luma residual delta RMS": float(np.sqrt(np.mean(diff * diff))),
            "luma residual delta P95 absolute": float(np.percentile(np.abs(diff), 95)),
            "luma correlation": corr}


def valid_midtones(off_a: np.ndarray, off_b: np.ndarray) -> np.ndarray:
    la = srgb_to_linear(off_a.astype(np.float32) / 255.0) @ np.array([.2126, .7152, .0722], dtype=np.float32)
    lb = srgb_to_linear(off_b.astype(np.float32) / 255.0) @ np.array([.2126, .7152, .0722], dtype=np.float32)
    return (la > .05) & (la < .95) & (lb > .05) & (lb < .95)


def as_rgb8(image: Image.Image) -> np.ndarray:
    return np.asarray(image.convert("RGB"), dtype=np.uint8)


def save_sheet(source: str, preset_name: str, frames: dict[str, list[np.ndarray]]) -> None:
    cell = 256
    label_h = 24
    sheet = Image.new("RGB", (len(SHEET_FRAMES) * cell, len(POLICIES) * (cell + label_h)), (28, 29, 32))
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.load_default()
    for row, policy in enumerate(POLICIES):
        for col, frame_index in enumerate(SHEET_FRAMES):
            x, y = col * cell, row * (cell + label_h)
            draw.text((x + 4, y + 4), f"{policy} / frame {frame_index}", fill=(245, 245, 245), font=font)
            im = Image.fromarray(frames[policy][frame_index], "RGB")
            im.thumbnail((cell, cell), Image.Resampling.LANCZOS)
            sheet.paste(im, (x + (cell - im.width) // 2, y + label_h + (cell - im.height) // 2))
    stem = Path(source).stem
    sheet.save(CAPTURE / f"{stem}-{preset_name}.jpg", quality=93)


def main() -> dict[str, object]:
    CAPTURE.mkdir(parents=True, exist_ok=True)
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    records: list[dict[str, object]] = []
    pair_records: list[dict[str, object]] = []
    parity_checks = 0
    sources: dict[str, object] = {}

    for source_name in SOURCE_NAMES:
        original, source_meta = load_source(source_name, manifest)
        world = resize_long_edge(original, 1280)
        origins = make_origins(world.size)
        sources[source_name] = {**source_meta, "motion image size": list(world.size),
                                "frame size": [EDGE, EDGE], "camera origins x/y": [list(p) for p in origins]}
        for preset_name, preset in PRESETS.items():
            grain = float(preset["grain"])
            field_rng = np.random.default_rng(BASE_SEED)
            world_delta = field_rng.normal(0.0, grain, size=(world.height, world.width)).astype(np.float32)
            per_policy: dict[str, list[np.ndarray]] = {p: [] for p in POLICIES}
            off_frames: list[np.ndarray] = []
            residuals: dict[str, list[np.ndarray]] = {p: [] for p in POLICIES}

            for frame_index, origin in enumerate(origins):
                frame = crop_frame(world, origin).convert("RGBA")
                off_settings = dict(preset)
                off_settings["grain"] = 0.0
                off = apply_retro(frame, **off_settings, seed=BASE_SEED)
                off_rgb = as_rgb8(off)
                off_frames.append(off_rgb)
                frame_seed = BASE_SEED + frame_index * FRAME_SEED_STEP
                for policy in POLICIES:
                    source_alpha = np.asarray(frame.getchannel("A"), dtype=np.uint8)
                    if policy == "screen-fixed":
                        output = apply_retro(frame, **preset, seed=BASE_SEED)
                        out_rgb = as_rgb8(output)
                        policy_alpha = np.asarray(output.getchannel("A"), dtype=np.uint8)
                    elif policy == "frame-varying":
                        output = apply_retro(frame, **preset, seed=frame_seed)
                        out_rgb = as_rgb8(output)
                        policy_alpha = np.asarray(output.getchannel("A"), dtype=np.uint8)
                    else:
                        x, y = origin
                        delta = world_delta[y:y + EDGE, x:x + EDGE]
                        prebyte = render_prebyte_float(frame, preset, BASE_SEED, grain_delta=delta)
                        out_rgb = np.uint8(prebyte * 255.0)
                        # The source-locked RGB counterfactual leaves the source alpha plane untouched.
                        policy_alpha = source_alpha
                    if not np.array_equal(source_alpha, policy_alpha):
                        raise RuntimeError(f"alpha mismatch {source_name}/{preset_name}/{policy}/{frame_index}")
                    per_policy[policy].append(out_rgb)
                    residuals[policy].append(grain_residual(out_rgb, off_rgb))

                # Verify the source-locked delta injection against the current suite's screen field at an aligned origin.
                if frame_index == 0:
                    local_rng = np.random.default_rng(BASE_SEED)
                    aligned_delta = local_rng.normal(0.0, grain, size=(EDGE, EDGE)).astype(np.float32)
                    replay = render_prebyte_float(frame, preset, BASE_SEED, grain_delta=aligned_delta)
                    expected = as_rgb8(apply_retro(frame, **preset, seed=BASE_SEED))
                    if not np.array_equal(np.uint8(replay * 255.0), expected):
                        raise RuntimeError(f"grain delta parity failure {source_name}/{preset_name}")
                    parity_checks += 1

                for policy in POLICIES:
                    rgb = per_policy[policy][-1]
                    metrics = residuals[policy][-1]
                    midtone = valid_midtones(off_rgb, off_rgb)
                    luma_delta = metrics @ np.array([.2126, .7152, .0722], dtype=np.float32)
                    records.append({"source": source_name, "preset": preset_name, "policy": policy,
                                    "frame": frame_index, "seed": BASE_SEED if policy != "frame-varying" else frame_seed,
                                    "origin x/y": list(origin), "valid midtone pixels": int(np.count_nonzero(midtone)),
                                    "paired residual luma RMS": float(np.sqrt(np.mean(luma_delta[midtone] ** 2))),
                                    "paired residual luma P95 abs": float(np.percentile(np.abs(luma_delta[midtone]), 95)),
                                    "RGB8 changed pixel ratio": float(np.mean(np.any(rgb != off_rgb, axis=2)))})

            # Adjacent frame residual changes in screen alignment and common source/world coordinates.
            for policy in POLICIES:
                for i in range(FRAME_COUNT - 1):
                    a, b = residuals[policy][i], residuals[policy][i + 1]
                    screen_mask = valid_midtones(off_frames[i], off_frames[i + 1])
                    pair_records.append({"source": source_name, "preset": preset_name, "policy": policy,
                                         "pair": [i, i + 1], "alignment": "screen",
                                         **pair_metrics(a, b, screen_mask)})
                    x0, y0 = origins[i]
                    x1, y1 = origins[i + 1]
                    left, top = max(x0, x1), max(y0, y1)
                    right, bottom = min(x0 + EDGE, x1 + EDGE), min(y0 + EDGE, y1 + EDGE)
                    wa = (slice(top - y0, bottom - y0), slice(left - x0, right - x0))
                    wb = (slice(top - y1, bottom - y1), slice(left - x1, right - x1))
                    world_mask = valid_midtones(off_frames[i][wa], off_frames[i + 1][wb])
                    pair_records.append({"source": source_name, "preset": preset_name, "policy": policy,
                                         "pair": [i, i + 1], "alignment": "source/world",
                                         "overlap origin x/y": [left, top],
                                         **pair_metrics(a[wa], b[wb], world_mask)})
            save_sheet(source_name, preset_name, per_policy)

    def aggregate(policy: str, alignment: str) -> dict[str, object]:
        selected = [r for r in pair_records if r["policy"] == policy and r["alignment"] == alignment]
        values = [float(r["luma residual delta RMS"]) for r in selected if r["luma residual delta RMS"] is not None]
        correlations = [float(r["luma correlation"]) for r in selected if r["luma correlation"] is not None]
        return {"pair count": len(values), "median adjacent residual change RMS": float(np.median(values)),
                "median adjacent residual correlation": float(np.median(correlations)) if correlations else None}

    summary = {"screen-fixed": {a: aggregate("screen-fixed", a) for a in ("screen", "source/world")},
               "frame-varying": {a: aggregate("frame-varying", a) for a in ("screen", "source/world")},
               "source-locked": {a: aggregate("source-locked", a) for a in ("screen", "source/world")}}
    report = {"study": "full-suite-photo-grain-motion-policy", "date": "2026-11-11",
              "environment": {"python": platform.python_version(), "numpy": np.__version__},
              "conditions": {"sources": sources, "presets": {k: dict(v) for k, v in PRESETS.items()},
                             "frame count per source/preset": FRAME_COUNT, "frame size": [EDGE, EDGE],
                             "pan step pixels": PAN_STEP, "base seed": BASE_SEED,
                             "frame seed step": FRAME_SEED_STEP,
                             "policy definitions": {
                                 "screen-fixed": "Current suite apply_retro with one seed on every frame; random grain field repeats in viewport coordinates.",
                                 "frame-varying": "Current suite apply_retro with deterministic distinct seed per frame.",
                                 "source-locked": "Current non-grain suite stages, plus one SHA-independent seeded Gaussian delta field cropped in source-image coordinates."}},
              "parity gates": {"source-locked local grain delta to current apply_retro RGB8 exact matches": parity_checks,
                               "alpha passthrough checked for all policy/frame outputs": True},
              "summary": summary, "frame records": records, "adjacent pair records": pair_records,
              "evaluation": {"status": "descriptive study; no approved target, observer, or acceptance threshold",
                             "limitations": ["integer crop translation only; no subpixel motion, rotation, occlusion, or scene change",
                                             "grain residual is computed against same-frame grain-off output; other retro stages remain active",
                                             "source-locked policy is an R&D counterfactual and grain is added after the non-grain suite stages",
                                             "Pillow/Python model only; no Artifact runtime, Core/GPU, preview/export, performance, or memory validation"]}}
    output = CAPTURE / "grain-motion-policy-study.json"
    output.write_text(json.dumps(report, indent=2, ensure_ascii=False, allow_nan=False) + "\n", encoding="utf-8")
    print(json.dumps({"report": str(output), "frame records": len(records), "adjacent pairs": len(pair_records),
                      "parity checks": parity_checks, "summary": summary}, indent=2))
    return report


if __name__ == "__main__":
    main()
