#!/usr/bin/env python3
"""Exploratory morphology models for Film Grunge and Film Damage.

This is a source-equation study, not a C++ parity implementation. PCG64
placements intentionally do not reproduce std::mt19937 sequences.
"""
from __future__ import annotations

import hashlib
import json
import platform
from pathlib import Path

import cv2
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
PLAN = ROOT / "experiments/FilmEffectsLab/plans/damage-morphology-resolution-r1.json"
PHOTO = ROOT / "experiments/GlowLab/assets/cc0-neon-streets-1536.jpg"
OUT = ROOT / "experiments/FilmEffectsLab/captures/damage-morphology-resolution-r1"
SEED = 1977


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def synthetic(size: int = 1024) -> np.ndarray:
    y, x = np.mgrid[0:size, 0:size].astype(np.float32)
    v = np.full((size, size), .32, np.float32)
    v[:, size // 8:size // 4] = .12
    v[:, size // 4:size * 3 // 8] = .5
    v[:, size * 3 // 8:size // 2] = .82
    v[:, size // 2:size * 5 // 8] = .24 + .5 * x[:, size // 2:size * 5 // 8] / size
    v[(x.astype(int) // 8 + y.astype(int) // 8) % 2 == 0] = .68
    for k in range(16, size, 64):
        v[max(0, k - 1):k + 1, :] = .95
        v[:, max(0, k - 1):k + 1] = .04
    rgb = np.stack((v, np.clip(v * .86 + .06, 0, 1), np.clip(v * .7 + .1, 0, 1)), axis=-1)
    return rgb.astype(np.float32)


def photo_crop(size: int) -> np.ndarray:
    im = Image.open(PHOTO).convert("RGB")
    w, h = im.size
    side = min(w, h)
    left, top = (w - side) // 2, (h - side) // 2
    im = im.crop((left, top, left + side, top + side)).resize((size, size), Image.Resampling.LANCZOS)
    return np.asarray(im, dtype=np.float32) / 255.0


def grunge(src: np.ndarray, component: str, seed: int) -> tuple[np.ndarray, np.ndarray]:
    h, w, _ = src.shape
    rng = np.random.Generator(np.random.PCG64(seed))
    field = np.zeros((h, w), np.float32)
    if component == "scratch":
        n = int(.22 * max(1, w / 18))
        for _ in range(n):
            x = int(rng.integers(0, w))
            yy = np.arange(h, dtype=np.float32)
            col = .45 + .55 * np.sin(yy * .071 + x)
            for dx in (-1, 0, 1):
                xx = x + dx
                if 0 <= xx < w:
                    field[:, xx] += col * (1 if dx == 0 else .4)
        out = src + field[..., None]
        return out, field
    n = int(.18 * max(1, w * h / 900))
    for _ in range(n):
        x, y = int(rng.integers(w)), int(rng.integers(h))
        r, opacity = .6 + float(rng.random()) * 3.8, .10 + float(rng.random()) * .32
        x0, x1 = max(0, int(np.floor(x-r))), min(w, int(np.ceil(x+r))+1)
        y0, y1 = max(0, int(np.floor(y-r))), min(h, int(np.ceil(y+r))+1)
        yy, xx = np.mgrid[y0:y1, x0:x1].astype(np.float32)
        d = np.sqrt((xx-x)**2 + (yy-y)**2) / r
        field[y0:y1, x0:x1] -= opacity * np.clip(1-d, 0, 1)**2
    return src + field[..., None], field


def damage(src: np.ndarray, component: str, seed: int) -> tuple[np.ndarray, np.ndarray]:
    h, w, _ = src.shape
    rng = np.random.Generator(np.random.PCG64(seed))
    damage_rgba = np.zeros((h, w, 4), np.float32)
    if component == "scratch":
        n = int(.10 * 18)
        for _ in range(n):
            x = int(rng.integers(w)); start = int(rng.uniform(0, h * .35))
            end = min(h - 1, start + int((.25 + rng.random() * .7) * h))
            bright = .9 if rng.random() > .45 else .04
            a = .18 + float(rng.random()) * .45
            dx = float(rng.normal(0, 2))
            cv2.line(damage_rgba, (x, start), (x + int(dx), end),
                     (bright, bright, bright, a), 1, cv2.LINE_AA)
    else:
        n = int(.08 * w * h / 180)
        for _ in range(n):
            x, y = int(rng.integers(w)), int(rng.integers(h))
            r = 1 + int(rng.random() * 4)
            bright = .82 if rng.random() > .55 else .03
            a = .25 + float(rng.random()) * .5
            cv2.circle(damage_rgba, (x, y), r, (bright, bright, bright, a), -1, cv2.LINE_AA)
    alpha = np.clip(damage_rgba[..., 3], 0, 1)
    return src * (1-alpha[..., None]) + damage_rgba[..., :3] * alpha[..., None], alpha


def png(arr: np.ndarray, path: Path) -> None:
    rgb = np.clip(arr, 0, 1)
    # Diagnostic display only: encode linearized numeric values as sRGB.
    rgb = np.where(rgb <= .0031308, 12.92*rgb, 1.055*np.power(np.maximum(rgb, 0), 1/2.4)-.055)
    Image.fromarray(np.uint8(np.clip(rgb, 0, 1)*255 + .5), "RGB").save(path)


def components(mask: np.ndarray) -> dict:
    n, labels, stats, _ = cv2.connectedComponentsWithStats(mask.astype(np.uint8), 8)
    areas = stats[1:, cv2.CC_STAT_AREA] if n > 1 else np.array([], dtype=np.int32)
    return {"count": int(n-1), "area_total_px": int(areas.sum()), "area_median_px": float(np.median(areas)) if areas.size else 0.0,
            "area_max_px": int(areas.max()) if areas.size else 0}


def run() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    plan = json.loads(PLAN.read_text(encoding="utf-8"))
    photo_hash = sha(PHOTO)
    assert photo_hash == "8791bd593b699be76dcfb181d72dd8655caaae2e88c235b89ed5d9097aa319d4", photo_hash
    metrics = {"plan_id": plan["plan_id"], "plan_sha256": sha(PLAN), "script_sha256": sha(Path(__file__)),
               "photo_sha256": photo_hash, "python": platform.python_version(), "numpy": np.__version__,
               "opencv": cv2.__version__, "pillow": Image.__version__, "rng": "PCG64 exploratory; not std::mt19937 parity", "conditions": []}
    for material in ("synthetic", "photo"):
        for size in (512, 1024):
            full = synthetic(1024) if material == "synthetic" else photo_crop(1024)
            src = full if size == 1024 else np.asarray(
                Image.fromarray(np.uint8(np.clip(full, 0, 1) * 255 + .5), "RGB").resize(
                    (size, size), Image.Resampling.LANCZOS), dtype=np.float32) / 255.0
            base_name = f"{material}-{size}"
            png(src, OUT / f"{base_name}-baseline.png")
            for effect, comp, seed in (("grunge", "scratch", SEED+1), ("grunge", "dust", SEED+2),
                                       ("damage", "scratch", SEED+3), ("damage", "dust", SEED+4)):
                fn = grunge if effect == "grunge" else damage
                out, mask = fn(src, comp, seed)
                repeat, mask2 = fn(src, comp, seed)
                assert np.array_equal(out, repeat) and np.array_equal(mask, mask2)
                assert np.isfinite(out).all()
                delta = out-src
                active = np.any(np.abs(delta) > 1e-7, axis=2)
                raw = {"min": float(out.min()), "max": float(out.max()), "below_zero_values": int(np.count_nonzero(out < 0)),
                       "above_one_values": int(np.count_nonzero(out > 1)), "delta_mean_rgb": [float(x) for x in delta.mean((0,1))],
                       "delta_rms_rgb": [float(np.sqrt(np.mean(delta[...,c]**2))) for c in range(3)],
                       "active_pixels": int(active.sum()), "active_coverage": float(active.mean()),
                       "mask_coverage": float(np.count_nonzero(mask)/mask.size), "alpha_identity": True,
                       "finite": True, "deterministic_repeat": True}
                if comp == "dust": raw["connected_components"] = components(mask != 0)
                if comp == "scratch":
                    raw["active_row_span_fraction"] = float(np.count_nonzero(np.any(active, axis=1))/size)
                    raw["active_column_fraction"] = float(np.count_nonzero(np.any(active, axis=0))/size)
                tag = f"{base_name}-{effect}-{comp}"
                png(out, OUT / f"{tag}.png")
                crop = out[size//2-128:size//2+128, size//2-128:size//2+128]
                png(crop, OUT / f"{tag}-center256.png")
                metrics["conditions"].append({"material": material, "size": size, "effect": effect, "component": comp,
                                               "placement_seed": seed, "source_sha256": hashlib.sha256(src.tobytes()).hexdigest(), **raw})
    (OUT / "metrics.json").write_text(json.dumps(metrics, indent=2, ensure_ascii=False)+"\n", encoding="utf-8")
    print(f"Wrote {len(metrics['conditions'])} conditions to {OUT}")


if __name__ == "__main__":
    run()
