"""Generate deterministic fixtures for buffer precision / alpha chain R&D.

Measurement source is float32 RGBA NPY (linear scene-referred, straight alpha).
PNG files are display-only (clamped to [0, 1] then sRGB OETF).
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import cv2
import numpy as np

GENERATOR_VERSION = "0.1.0"
DEFAULT_SEED = 20261117
N = 256


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def srgb_oetf(x: np.ndarray) -> np.ndarray:
    x = np.clip(x, 0.0, 1.0)
    return np.where(
        x <= 0.0031308,
        12.92 * x,
        1.055 * np.power(x, 1.0 / 2.4) - 0.055,
    )


def save_display_png(path: Path, rgba: np.ndarray) -> None:
    rgb = srgb_oetf(rgba[..., :3])
    out = np.empty(rgba.shape, dtype=np.uint8)
    out[..., 0] = np.round(rgb[..., 2] * 255.0).astype(np.uint8)
    out[..., 1] = np.round(rgb[..., 1] * 255.0).astype(np.uint8)
    out[..., 2] = np.round(rgb[..., 0] * 255.0).astype(np.uint8)
    out[..., 3] = np.round(np.clip(rgba[..., 3], 0.0, 1.0) * 255.0).astype(np.uint8)
    if not cv2.imwrite(str(path), out):
        raise RuntimeError(f"failed to write {path}")


def write_fixture(root: Path, fixture_id: str, description: str, purpose: str,
                  rgba: np.ndarray, limitations: list[str]) -> dict:
    folder = root / fixture_id
    folder.mkdir(parents=True, exist_ok=True)
    np.save(folder / "source.npy", rgba)
    save_display_png(folder / "source.png", rgba)

    files = {}
    for path in sorted(folder.iterdir()):
        if path.is_file():
            files[path.name] = {
                "sha256": sha256_file(path),
                "bytes": path.stat().st_size,
            }
    return {
        "id": fixture_id,
        "description": description,
        "purpose": purpose,
        "resolution": [N, N],
        "rgb_min": float(np.min(rgba[..., :3])),
        "rgb_max": float(np.max(rgba[..., :3])),
        "alpha_min": float(np.min(rgba[..., 3])),
        "alpha_max": float(np.max(rgba[..., 3])),
        "non_finite": int(np.count_nonzero(~np.isfinite(rgba))),
        "limitations": limitations,
        "files": files,
    }


def make_alpha_fringe() -> np.ndarray:
    rgba = np.empty((N, N, 4), dtype=np.float32)
    rgba[..., :3] = 0.18
    rgba[..., 3] = 1.0
    alphas = (0.0, 0.01, 0.1, 0.5, 0.9, 1.0)
    band = N // len(alphas)
    color = np.array([0.9, 0.2, 0.6], dtype=np.float32)
    hidden = np.array([3.0, -0.2, 1.5], dtype=np.float32)
    for index, alpha in enumerate(alphas):
        y0 = index * band
        y1 = (index + 1) * band
        rgba[y0:y1, :, :3] = color
        rgba[y0:y1, :, 3] = alpha
    # Hidden (invisible) RGB under alpha == 0 and alpha == 0.01.
    rgba[0:2 * band, :, :3] = hidden
    return rgba


def make_chromatic_edge() -> np.ndarray:
    rgba = np.empty((N, N, 4), dtype=np.float32)
    x = np.arange(N, dtype=np.float32)[None, :]
    left = np.array([0.05, 0.10, 0.80], dtype=np.float32)
    right = np.array([0.90, 0.30, 0.10], dtype=np.float32)
    t = np.clip((x - 96.0) / 64.0, 0.0, 1.0)
    rgba[..., :3] = left[None, None, :] * (1.0 - t[..., None]) + right[None, None, :] * t[..., None]
    rgba[..., 3] = np.clip((x - 80.0) / 96.0, 0.0, 1.0)
    return rgba


def make_transparent_hidden_rgb() -> np.ndarray:
    rgba = np.empty((N, N, 4), dtype=np.float32)
    rgba[..., :3] = 0.0
    rgba[..., 3] = 0.0
    rgba[:, :, 0] = 1.5
    rgba[:, :, 1] = -0.2
    rgba[:, :, 2] = 0.7
    cy, cx = np.mgrid[0:N, 0:N]
    disc = ((cx - 128) ** 2 + (cy - 128) ** 2) < 48 ** 2
    rgba[disc, :3] = (0.25, 0.55, 0.15)
    rgba[disc, 3] = 1.0
    return rgba


def make_hdr_ramp() -> np.ndarray:
    rgba = np.empty((N, N, 4), dtype=np.float32)
    ramp = np.linspace(0.0, 4.0, N, dtype=np.float32)
    rgba[..., 0] = ramp[:, None]
    rgba[..., 1] = ramp[:, None] * 0.8
    rgba[..., 2] = ramp[:, None] * 0.6
    rgba[..., 3] = 1.0
    return rgba


def make_banding_ramp() -> np.ndarray:
    rgba = np.empty((N, N, 4), dtype=np.float32)
    x = np.arange(N, dtype=np.float32)[None, :]
    wave = 0.003 * np.sin(x / 9.0)
    ramp = 0.2 + 0.1 * (x / (N - 1.0)) + wave
    rgba[..., 0] = ramp
    rgba[..., 1] = ramp * 0.98
    rgba[..., 2] = ramp * 0.96
    rgba[..., 3] = 1.0
    return rgba


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path,
                        default=Path("experiments/PrecisionLab/fixtures/precision-rd1"))
    parser.add_argument("--seed", type=int, default=DEFAULT_SEED)
    args = parser.parse_args()

    root = args.output_dir
    root.mkdir(parents=True, exist_ok=True)
    seed_note = f"fixed seed {args.seed}; fixture geometry is deterministic"

    specs = [
        ("alpha_fringe", "multi-level alpha edge with asymmetric color and hidden RGB",
         "black fringe / color shift at partial alpha", make_alpha_fringe()),
        ("chromatic_edge", "diagonal alpha ramp across a complementary RGB edge",
         "order-dependent hue shift on translucent boundaries", make_chromatic_edge()),
        ("transparent_hidden_rgb", "alpha=0 field with HDR / negative hidden RGB and an opaque disc",
         "invisible RGB leaking into composite", make_transparent_hidden_rgb()),
        ("hdr_ramp", "linear ramp from 0 to 4",
         "clipping and highlight flattening at 8-bit ingress", make_hdr_ramp()),
        ("banding_ramp", "slow 0.2-0.3 linear ramp with small wave",
         "banding after 8-bit quantization", make_banding_ramp()),
    ]

    entries = [
        write_fixture(root, fixture_id, description, purpose, rgba,
                      [seed_note, "Synthetic fixture; not a photograph or a runtime capture."])
        for fixture_id, description, purpose, rgba in specs
    ]

    manifest = {
        "bundle": root.name,
        "generator": "build_precision_fixtures.py",
        "generator_version": GENERATOR_VERSION,
        "seed": args.seed,
        "environment": {"numpy_version": np.__version__, "opencv_version": cv2.__version__},
        "color_contract": {
            "measurement_format": "float32 RGBA NPY",
            "color_space": "linear scene-referred",
            "alpha": "straight",
            "display_format": "8-bit BGRA PNG, clamped [0,1] then sRGB OETF",
            "view_transform": "none (Artifact active view transform is not used)",
        },
        "fixtures": entries,
        "limitations": [
            "No Artifact code is executed; no GPU dispatch, runtime capture, or performance measurement.",
            "No pass/fail threshold is derived from these fixtures alone.",
        ],
    }
    manifest_path = root / "manifest.json"
    manifest_path.write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    print(f"Wrote {len(entries)} fixtures and {manifest_path}")


if __name__ == "__main__":
    main()
