"""Deterministic fixtures for the material (texture/detail) separation R&D (RD0).

Linear float RGBA, straight alpha by default, with a premultiplied variant of
the alpha fixture. 8-bit sRGB PNG previews are written for review only.
Offline: no build, no Artifact launch.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import cv2
import numpy as np

SEED = 20261118
RESOLUTIONS = {"small": {"h": 320, "w": 512}, "large": {"h": 1024, "w": 1536}}
FIXTURE_SEEDS = {
    "color_ramp": SEED + 1,
    "thin_lines": SEED + 2,
    "texture": SEED + 3,
    "flat_highlight": SEED + 4,
    "alpha_boundary": SEED + 5,
    "sequence": SEED + 6,
}
COLORS_SRGB = [
    (0.80, 0.10, 0.10), (0.10, 0.60, 0.20), (0.10, 0.20, 0.80), (0.90, 0.80, 0.10),
    (0.10, 0.70, 0.80), (0.70, 0.20, 0.70), (0.75, 0.55, 0.45), (0.95, 0.95, 0.95),
]


def srgb_to_linear(u: np.ndarray) -> np.ndarray:
    u = np.asarray(u, dtype=np.float64)
    return np.where(u <= 0.04045, u / 12.92, np.power((u + 0.055) / 1.055, 2.4))


def linear_to_srgb_u8(x: np.ndarray) -> np.ndarray:
    x = np.clip(np.asarray(x, dtype=np.float64), 0.0, 1.0)
    s = np.where(x <= 0.0031308, 12.92 * x, 1.055 * np.power(x, 1.0 / 2.4) - 0.055)
    return np.round(s * 255.0).astype(np.uint8)


def _rgba(h: int, w: int, rgb: np.ndarray, alpha: float = 1.0) -> np.ndarray:
    out = np.empty((h, w, 4), dtype=np.float32)
    out[..., :3] = np.clip(rgb, 0.0, None).astype(np.float32)
    out[..., 3] = np.float32(alpha)
    return out


def _rect(x0: float, y0: float, x1: float, y1: float, w: int, h: int) -> list[int]:
    return [int(round(x0 * w)), int(round(y0 * h)), int(round(x1 * w)), int(round(y1 * h))]


def gen_color_ramp(h: int, w: int, seed: int) -> tuple[np.ndarray, dict]:
    """8 colour patches + a linear grey ramp (midtone preservation probe)."""
    rng = np.random.default_rng(seed)
    rgb = np.full((h, w, 3), 0.18, dtype=np.float32)
    rois: dict[str, list[int]] = {}
    cell_w, cell_h = w / 4, h / 5
    for i, srgb in enumerate(COLORS_SRGB):
        col, row = i % 4, i // 4
        cx, cy = int((col + 0.5) * cell_w), int((row + 0.5) * cell_h)
        pw, ph = max(8, int(cell_w * 0.62)), max(8, int(cell_h * 0.62))
        color = srgb_to_linear(np.array(srgb, dtype=np.float64)).astype(np.float32)
        rgb[cy - ph // 2: cy + ph // 2, cx - pw // 2: cx + pw // 2] = color
        rois[f"patch_{i}"] = [cx - pw // 4, cy - ph // 4, cx + pw // 4, cy + ph // 4]
    y0, y1 = int(h * 0.48), int(h * 0.74)
    x0, x1 = int(w * 0.08), int(w * 0.92)
    levels = np.linspace(0.02, 0.92, x1 - x0, dtype=np.float32)
    rgb[y0:y1, x0:x1] = levels[None, :, None]
    rois["gray_ramp"] = [x0, y0, x1, y1]
    rois["flat_bg"] = _rect(0.02, 0.78, 0.30, 0.94, w, h)
    rgb[y0:y1, x0:x1] += rng.normal(0.0, 0.0005, size=(y1 - y0, x1 - x0, 1)).astype(np.float32)
    return _rgba(h, w, rgb), {"rois": rois, "seed": seed}


def gen_thin_lines(h: int, w: int, seed: int) -> tuple[np.ndarray, dict]:
    """Thin line chart (1..4 px) on a flat ground."""
    rng = np.random.default_rng(seed)
    rgb = np.full((h, w, 3), 0.25, dtype=np.float32)
    scale = w / 512.0
    for i, base_width in enumerate((1, 2, 3, 4)):
        width = max(1, int(round(base_width * scale)))
        y = int(h * (0.18 + 0.16 * i))
        cv2.line(rgb, (int(w * 0.08), y), (int(w * 0.92), y), (0.92, 0.92, 0.92), width, lineType=cv2.LINE_AA)
    for i in range(3):
        pts = []
        x0 = int(w * (0.12 + 0.27 * i))
        for t in np.linspace(0, 1, 24):
            pts.append((x0 + int(t * w * 0.18), int(h * (0.62 + 0.06 * np.sin(t * 6.0 + i)))))
        cv2.polylines(rgb, [np.array(pts, dtype=np.int32)], False, (0.85, 0.70, 0.30), max(1, int(scale)), lineType=cv2.LINE_AA)
    rgb = np.clip(rgb + rng.normal(0.0, 0.001, size=rgb.shape).astype(np.float32), 0.0, None)
    return _rgba(h, w, rgb), {
        "rois": {
            "thin_lines": _rect(0.05, 0.10, 0.95, 0.56, w, h),
            "branches": _rect(0.08, 0.55, 0.80, 0.76, w, h),
            "flat_bg": _rect(0.02, 0.80, 0.30, 0.95, w, h),
        },
        "seed": seed,
    }


def gen_texture(h: int, w: int, seed: int) -> tuple[np.ndarray, dict]:
    """Procedural micro texture ('material') next to a perfectly flat region.

    Multi-octave value noise: coarse illumination (low frequency) plus fine
    grain (high frequency). The flat region carries only sensor-like noise so
    that local-contrast comparisons have a reference.
    """
    rng = np.random.default_rng(seed)
    rgb = np.full((h, w, 3), 0.30, dtype=np.float32)
    # sensor-like noise floor over the whole image so that the flat region has
    # a non-zero std (otherwise local-contrast ratios are meaningless)
    rgb += rng.normal(0.0, 0.0006, size=rgb.shape).astype(np.float32)
    block = max(4, h // 12)
    noise = np.zeros((h, w), dtype=np.float32)
    amplitudes = (0.055, 0.028, 0.014, 0.007)
    for k, amp in enumerate(amplitudes):
        bh, bw = max(2, block // (2 ** k)), max(2, block // (2 ** k))
        layer = rng.normal(0.0, 1.0, size=(bh, bw)).astype(np.float32)
        noise += cv2.resize(layer, (w, h), interpolation=cv2.INTER_CUBIC) * amp
    noise += rng.normal(0.0, 0.0015, size=(h, w)).astype(np.float32)
    tx0, tx1 = int(w * 0.40), int(w * 0.88)
    ty0, ty1 = int(h * 0.12), int(h * 0.88)
    base_color = srgb_to_linear(np.array([0.62, 0.58, 0.50], dtype=np.float64)).astype(np.float32)
    region = rgb[ty0:ty1, tx0:tx1]
    region[:] = (base_color[None, None, :] * (1.0 + noise[ty0:ty1, tx0:tx1][..., None])).astype(np.float32)
    rgb[ty0:ty1, tx0:tx1] = region
    return _rgba(h, w, rgb), {
        "rois": {
            "texture_region": [tx0, ty0, tx1, ty1],
            "flat_region": [int(w * 0.04), int(h * 0.20), int(w * 0.30), int(h * 0.80)],
            "texture_coarse": [tx0, ty0, tx1, ty0 + max(2, (ty1 - ty0) // 4)],
        },
        "seed": seed,
        "note": "multi-octave value noise; coarse octave is 'low frequency tone', fine octaves are 'texture'",
    }


def gen_flat_highlight(h: int, w: int, seed: int) -> tuple[np.ndarray, dict]:
    """Flat fields of different level plus a small highlight."""
    rng = np.random.default_rng(seed)
    levels = np.array([0.05, 0.20, 0.50, 0.80], dtype=np.float32)
    rgb = np.empty((h, w, 3), dtype=np.float32)
    for i in range(4):
        rgb[int(h * (0.10 + 0.20 * i)): int(h * (0.10 + 0.20 * (i + 1))), :] = levels[i]
    yy, xx = np.mgrid[0:h, 0:w]
    spot = 2.0 * np.exp(-(((xx - w * 0.72) / (w * 0.035)) ** 2 + ((yy - h * 0.30) / (h * 0.06)) ** 2))
    rgb += spot[..., None].astype(np.float32)
    rgb += rng.normal(0.0, 0.0008, size=rgb.shape).astype(np.float32)
    return _rgba(h, w, rgb), {
        "rois": {
            "flat_low": _rect(0.05, 0.12, 0.30, 0.26, w, h),
            "flat_mid": _rect(0.05, 0.52, 0.30, 0.66, w, h),
            "highlight": _rect(0.67, 0.24, 0.77, 0.36, w, h),
        },
        "seed": seed,
    }


def gen_alpha_boundary(h: int, w: int, seed: int) -> tuple[np.ndarray, dict]:
    """Opaque/semi-transparent alpha boundary with hidden RGB behind alpha=0."""
    rng = np.random.default_rng(seed)
    rgb = np.full((h, w, 3), 0.5, dtype=np.float32)
    alpha = np.zeros((h, w), dtype=np.float32)
    left = srgb_to_linear(np.array(COLORS_SRGB[0])).astype(np.float32)
    right = srgb_to_linear(np.array(COLORS_SRGB[2])).astype(np.float32)
    y0, y1 = int(h * 0.18), int(h * 0.82)
    x0, x1 = int(w * 0.12), int(w * 0.88)
    xm = (x0 + x1) // 2
    rgb[y0:y1, x0:xm] = left
    rgb[y0:y1, xm:x1] = right
    alpha[y0:y1, x0:xm] = 1.0
    band = max(2, int(w * 0.035))
    ramp = np.linspace(1.0, 0.0, band, dtype=np.float32)
    alpha[y0:y1, xm - band: xm] = np.maximum(alpha[y0:y1, xm - band: xm], ramp[None, :])
    ramp2 = np.linspace(0.0, 1.0, band, dtype=np.float32)
    alpha[y0:y1, xm: xm + band] = np.maximum(alpha[y0:y1, xm: xm + band], ramp2[None, :])
    alpha[y0:y1, xm + band: x1] = 1.0
    alpha += rng.normal(0.0, 0.0005, size=(h, w)).astype(np.float32)
    alpha = np.clip(alpha, 0.0, 1.0)
    alpha[:y0, :] = 0.0
    alpha[y1:, :] = 0.0
    out = _rgba(h, w, rgb, 1.0)
    out[..., 3] = alpha
    return out, {
        "rois": {
            "boundary": _rect(0.40, 0.20, 0.60, 0.80, w, h),
            "semi_transparent": _rect(0.45, 0.25, 0.55, 0.75, w, h),
            "transparent_hidden_rgb": _rect(0.02, 0.02, 0.10, 0.14, w, h),
            "opaque_foreground": _rect(0.15, 0.25, 0.35, 0.75, w, h),
        },
        "seed": seed,
        "alpha_contract": "straight RGBA; fully transparent pixels keep hidden RGB = 0.5 linear",
    }


def gen_sequence(h: int, w: int, seed: int, frames: int = 8) -> tuple[list[np.ndarray], dict]:
    """8 frames of the texture fixture translated by 0.5 px per frame."""
    rng = np.random.default_rng(seed)
    base = np.full((h, w, 3), 0.22, dtype=np.float32)
    coarse = rng.normal(0.0, 0.03, size=(max(2, h // 16), max(2, w // 16), 1)).astype(np.float32)
    coarse = cv2.resize(coarse, (w, h), interpolation=cv2.INTER_CUBIC)
    if coarse.ndim == 2:
        coarse = coarse[..., None]
    base = np.clip(base + coarse, 0.0, None)
    ph, pw = int(h * 0.28), int(w * 0.22)
    base[int(h * 0.35): int(h * 0.35) + ph, int(w * 0.30): int(w * 0.30) + pw] = \
        srgb_to_linear(np.array(COLORS_SRGB[4])).astype(np.float32)
    out = []
    for k in range(frames):
        m = np.array([[1.0, 0.0, 0.5 * k], [0.0, 1.0, 0.0]], dtype=np.float32)
        moved = cv2.warpAffine(base, m, (w, h), flags=cv2.INTER_LINEAR, borderMode=cv2.BORDER_REPLICATE)
        out.append(_rgba(h, w, moved))
    return out, {
        "rois": {
            "moving_patch": _rect(0.25, 0.30, 0.60, 0.70, w, h),
            "static_flat": _rect(0.02, 0.02, 0.20, 0.16, w, h),
        },
        "seed": seed,
        "translation_per_frame_px": 0.5,
        "frames": frames,
    }


def to_premultiplied(rgba: np.ndarray) -> np.ndarray:
    out = rgba.copy()
    out[..., :3] *= out[..., 3:4]
    return out


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def save_rgba(path: Path, rgba: np.ndarray) -> dict:
    npy, png = path.with_suffix(".npy"), path.with_suffix(".png")
    np.save(npy, rgba.astype(np.float32))
    cv2.imwrite(str(png), cv2.cvtColor(linear_to_srgb_u8(rgba[..., :3]), cv2.COLOR_RGB2BGR))
    return {
        "npy": npy.name, "npy_sha256": sha256(npy.read_bytes()),
        "png": png.name, "png_sha256": sha256(png.read_bytes()),
    }


def generate(output: Path) -> dict:
    output.mkdir(parents=True, exist_ok=True)
    fixture_dir = output / "fixtures"
    fixture_dir.mkdir(parents=True, exist_ok=True)
    manifest = {
        "lab": "MaterialSeparationLab", "rd": "rd0", "stage": "fixtures", "date": "2026-11-18",
        "seed": SEED,
        "convention": "linear float RGBA, straight alpha unless suffixed _premultiplied; sRGB OETF for PNG",
        "resolutions": RESOLUTIONS,
        "fixtures": {},
        "notes": ["Sequence frames are generated at 512x320 only; resolution comparisons use still fixtures."],
    }
    for res_name, size in RESOLUTIONS.items():
        h, w = size["h"], size["w"]
        for name, gen in (
            ("color_ramp", gen_color_ramp), ("thin_lines", gen_thin_lines),
            ("texture", gen_texture), ("flat_highlight", gen_flat_highlight),
            ("alpha_boundary", gen_alpha_boundary),
        ):
            rgba, meta = gen(h, w, FIXTURE_SEEDS[name])
            entry = {"resolution": res_name, "shape": list(rgba.shape), **meta}
            entry.update(save_rgba(fixture_dir / f"{name}_{res_name}", rgba))
            if name == "alpha_boundary":
                entry["premultiplied"] = save_rgba(fixture_dir / f"{name}_{res_name}_premultiplied", to_premultiplied(rgba))
            manifest["fixtures"][f"{name}_{res_name}"] = entry
    h, w = RESOLUTIONS["small"]["h"], RESOLUTIONS["small"]["w"]
    frames, meta = gen_sequence(h, w, FIXTURE_SEEDS["sequence"])
    seq = {"resolution": "small", "shape": list(frames[0].shape), **meta, "frames": []}
    for k, frame in enumerate(frames):
        seq["frames"].append(save_rgba(fixture_dir / f"sequence_{k:02d}_small", frame))
    manifest["fixtures"]["sequence_small"] = seq
    (output / "fixtures_manifest.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return manifest


def check(output: Path) -> None:
    manifest = json.loads((output / "fixtures_manifest.json").read_text(encoding="utf-8"))
    bad = []
    for key, entry in manifest["fixtures"].items():
        files = []
        if "frames" in entry:
            for f in entry["frames"]:
                files += [(f["npy"], f["npy_sha256"]), (f["png"], f["png_sha256"])]
        else:
            files += [(entry["npy"], entry["npy_sha256"]), (entry["png"], entry["png_sha256"])]
            if "premultiplied" in entry:
                pm = entry["premultiplied"]
                files += [(pm["npy"], pm["npy_sha256"]), (pm["png"], pm["png_sha256"])]
        for name, expected in files:
            if sha256((output / "fixtures" / name).read_bytes()) != expected:
                bad.append(f"{key}/{name}")
    if bad:
        raise SystemExit("hash mismatch: " + ", ".join(bad))
    print(f"fixture hash check OK ({len(manifest['fixtures'])} fixtures)")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", default="captures/rd0")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    output = Path(args.output)
    if args.check:
        check(output)
        return
    manifest = generate(output)
    print(f"generated {len(manifest['fixtures'])} fixtures -> {output}")


if __name__ == "__main__":
    main()
