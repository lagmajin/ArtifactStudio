from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent
OUT = ROOT / "captures" / "rd0"
SEED, WIDTH, HEIGHT = 20261116, 512, 320


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def preview(path: Path, image: np.ndarray) -> None:
    value = np.clip(image, 0.0, 1.0)
    value = np.where(value <= 0.0031308, value * 12.92,
                     1.055 * np.maximum(value, 0.0) ** (1.0 / 2.4) - 0.055)
    Image.fromarray(np.uint8(np.clip(np.rint(value * 255), 0, 255)), "RGB").save(path, format="PNG", optimize=False)


def measure(src: np.ndarray, out: np.ndarray, ref: np.ndarray, mask: np.ndarray) -> dict[str, object]:
    a, b, r = (v[mask].astype(np.float64) for v in (src, out, ref))
    lum = np.array([0.2126, 0.7152, 0.0722])
    chroma = b - b.mean(axis=1, keepdims=True)
    delta = np.abs(b - r)
    return {
        "pixels": int(mask.sum()),
        "rgb_min": np.min(b, axis=0).tolist(), "rgb_mean": np.mean(b, axis=0).tolist(),
        "rgb_p95": np.percentile(b, 95, axis=0).tolist(), "rgb_max": np.max(b, axis=0).tolist(),
        "luminance_shift_mean": float(np.mean((b - a) @ lum)),
        "source_rgb_mae": float(np.mean(np.abs(b - a))),
        "reference_rgb_mae": float(delta.mean()), "reference_rgb_p95": float(np.percentile(delta, 95)),
        "neutral_chroma_mean_abs": float(np.mean(np.abs(chroma))),
        "clipped_low_ratio": float(np.mean(b <= 0)), "clipped_high_ratio": float(np.mean(b >= 1)),
        "non_finite_values": int(b.size - np.count_nonzero(np.isfinite(b))),
    }


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    yy, xx = np.mgrid[0:HEIGHT, 0:WIDTH]
    x, y = xx / (WIDTH - 1), yy / (HEIGHT - 1)
    rng = np.random.default_rng(SEED)
    illumination = 0.26 + 0.48 * x + 0.10 * np.sin(2 * np.pi * y)
    grain = rng.normal(0, 0.012, (HEIGHT, WIDTH)).astype(np.float32)
    source = np.stack((illumination * .94, illumination * .91, illumination * .88), -1).astype(np.float32)
    source += grain[..., None] * np.array([1, .8, .65], np.float32)
    reference = np.stack((illumination * .91 + .035, illumination * .96 + .006,
                          illumination * 1.04 - .018), -1).astype(np.float32)
    reference += grain[..., None] * np.array([.75, .9, 1], np.float32)
    source, reference = np.clip(source, .005, .98), np.clip(reference, 0, 1)

    masks = {name: np.zeros((HEIGHT, WIDTH), bool) for name in ("skin_like", "neutral_white", "saturated", "smooth_ramp", "texture")}
    patches = [
        ("skin_like", 28, 38, 125, 104, (.63, .36, .27), (.68, .40, .34)),
        ("neutral_white", 155, 38, 252, 104, (.82, .78, .72), (.86, .86, .84)),
        ("saturated", 282, 38, 484, 104, (.16, .22, .78), (.30, .18, .68)),
    ]
    for name, x0, y0, x1, y1, a, b in patches:
        source[y0:y1, x0:x1], reference[y0:y1, x0:x1] = a, b
        masks[name][y0:y1, x0:x1] = True
    masks["smooth_ramp"][120:212, 20:492] = True
    masks["texture"][232:302, 20:492] = True

    # Analytical diagnostic only; intentionally not presented as a Core output.
    src_mean, src_std = source.mean((0, 1)), source.std((0, 1))
    ref_mean, ref_std = reference.mean((0, 1)), reference.std((0, 1))
    control = (source - src_mean) * (ref_std / np.maximum(src_std, 1e-6)) + ref_mean
    files: dict[str, str] = {}
    for name, image in (("synthetic_source", source), ("synthetic_reference", reference),
                        ("offline_mean_std_control", control)):
        npy, png = OUT / f"{name}.npy", OUT / f"{name}.png"
        np.save(npy, image.astype(np.float32), allow_pickle=False)
        preview(png, image)
        files[npy.name], files[png.name] = sha(npy), sha(png)
    roi_manifest, metrics = {}, {}
    for name, mask in masks.items():
        path = OUT / f"roi_{name}.png"
        Image.fromarray(mask.astype(np.uint8) * 255, "L").save(path, format="PNG", optimize=False)
        roi_manifest[name] = {"path": path.name, "sha256": sha(path), "pixels": int(mask.sum())}
        metrics[name] = measure(source, control, reference, mask)
        files[path.name] = sha(path)
    whole = np.ones((HEIGHT, WIDTH), bool)
    metrics["whole_frame"] = measure(source, control, reference, whole)
    raw = control.reshape(-1, 3)
    metrics["whole_frame"]["negative_channel_ratio"] = float(np.mean(raw < 0))
    metrics["whole_frame"]["above_one_channel_ratio"] = float(np.mean(raw > 1))
    metrics["whole_frame"]["linear_luminance_p01_p50_p99"] = np.percentile(raw @ [0.2126, .7152, .0722], [1, 50, 99]).tolist()
    manifest = {
        "rd": "rd0", "purpose": "synthetic response characterization only; no Core execution, gate, or ranking",
        "seed": SEED, "dimensions": [WIDTH, HEIGHT],
        "condition": "linear RGB float32, opaque, fixed dimensions; metrics unclipped; previews clipped then sRGB encoded",
        "reference_role": "known regional numeric transformation, not photographic or perceptual ground truth",
        "control": "offline per-channel mean/std equation, not AutoColorMatcher implementation",
        "generator_sha256": sha(Path(__file__).resolve()), "files": files,
        "roi_masks": roi_manifest, "metrics": metrics,
        "unverified": ["Core output and C++ differential", "licensed paired natural photos and reviewed skin/white ROIs",
                       "human review and pre-registered gates", "alpha/hidden RGB, ICC, HDR, GPU/backend, product routes, performance"],
    }
    path = OUT / "manifest.json"
    path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps({"manifest": str(path), "skin_like": metrics["skin_like"],
                      "neutral_white": metrics["neutral_white"], "whole_frame": metrics["whole_frame"]}, indent=2))


if __name__ == "__main__":
    main()
