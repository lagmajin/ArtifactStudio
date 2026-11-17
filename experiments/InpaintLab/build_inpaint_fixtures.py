"""Generate deterministic synthetic fixtures for the Content-Aware Fill R&D.

Measurement source is float32 RGBA NPY (linear scene-referred, straight alpha).
PNG files are display-only: clamped to [0, 1] and encoded with an sRGB OETF.
This script does not build, test, or launch ArtifactStudio.
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
STILL_SIZE = 256
SEQUENCE_SIZE = 160
SEQUENCE_FRAMES = 8


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


def save_mask_png(path: Path, mask: np.ndarray) -> None:
    if not cv2.imwrite(str(path), mask):
        raise RuntimeError(f"failed to write {path}")


def composite_over(rgba: np.ndarray, bg_rgb: tuple[float, float, float]) -> np.ndarray:
    a = rgba[..., 3:4]
    bg = np.asarray(bg_rgb, dtype=np.float32)[None, None, :]
    out = rgba[..., :3].copy()
    out[...] = rgba[..., :3] * a + bg * (1.0 - a)
    return out


def make_rgba(h: int, w: int, rgb: tuple[float, float, float], alpha: float = 1.0) -> np.ndarray:
    out = np.empty((h, w, 4), dtype=np.float32)
    out[..., 0] = rgb[0]
    out[..., 1] = rgb[1]
    out[..., 2] = rgb[2]
    out[..., 3] = alpha
    return out


def rect_mask(h: int, w: int, x0: int, y0: int, x1: int, y1: int) -> np.ndarray:
    mask = np.zeros((h, w), dtype=np.uint8)
    cv2.rectangle(mask, (x0, y0), (x1, y1), 255, thickness=-1)
    return mask


def circle_mask(h: int, w: int, cx: int, cy: int, radius: int) -> np.ndarray:
    mask = np.zeros((h, w), dtype=np.uint8)
    cv2.circle(mask, (cx, cy), radius, 255, thickness=-1)
    return mask


def insert_object(
    ground_truth: np.ndarray,
    mask: np.ndarray,
    rgb: tuple[float, float, float],
    alpha: float | None = None,
) -> np.ndarray:
    source = ground_truth.copy()
    region = mask > 0
    source[region, 0] = rgb[0]
    source[region, 1] = rgb[1]
    source[region, 2] = rgb[2]
    if alpha is not None:
        source[region, 3] = alpha
    return source


def write_fixture(
    root: Path,
    fixture_id: str,
    description: str,
    purpose: str,
    ground_truth: np.ndarray,
    mask: np.ndarray,
    source: np.ndarray,
    limitations: list[str],
) -> dict:
    folder = root / fixture_id
    folder.mkdir(parents=True, exist_ok=True)

    arrays = {
        "source.npy": source,
        "mask.npy": mask,
        "ground_truth.npy": ground_truth,
    }
    for name, array in arrays.items():
        np.save(folder / name, array)

    save_display_png(folder / "source.png", source)
    save_mask_png(folder / "mask.png", mask)
    save_display_png(folder / "ground_truth.png", ground_truth)

    files: dict[str, dict] = {}
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
        "resolution": [int(ground_truth.shape[1]), int(ground_truth.shape[0])],
        "mask_pixels": int(np.count_nonzero(mask)),
        "source_value_range": {
            "min": float(np.min(source[..., :3])),
            "max": float(np.max(source[..., :3])),
            "non_finite": int(np.count_nonzero(~np.isfinite(source[..., :3]))),
        },
        "limitations": limitations,
        "files": files,
    }


def build_still_fixtures(seed: int) -> list[tuple[str, str, str, np.ndarray, np.ndarray, np.ndarray, list[str]]]:
    n = STILL_SIZE
    fixtures: list[tuple[str, str, str, np.ndarray, np.ndarray, np.ndarray, list[str]]] = []

    # 1. Smooth gradient: continuity inside a flat region.
    t = np.linspace(0.0, 1.0, n, dtype=np.float32)[None, :]
    gt = make_rgba(n, n, (0.0, 0.0, 0.0))
    gt[..., 0] = 0.05 + 0.65 * t
    gt[..., 1] = 0.10 + 0.50 * t
    gt[..., 2] = 0.22 + 0.35 * t
    mask = rect_mask(n, n, 96, 96, 159, 159)
    src = insert_object(gt, mask, (0.95, 0.15, 0.08))
    fixtures.append((
        "flat_gradient",
        "linear RGB gradient with a solid rectangular object",
        "flat-region continuity, color banding inside the filled area",
        gt, mask, src,
        ["Synthetic gradient; not a photograph. Ground truth is the clean plate, not a naturalness proof."],
    ))

    # 2. Textured wall: repeated texture continuity.
    rng = np.random.default_rng(seed + 1)
    noise = rng.normal(0.0, 1.0, (n, n)).astype(np.float32)
    texture = cv2.GaussianBlur(noise, (0, 0), 6.0)
    texture = texture / (float(np.std(texture)) + 1e-6)
    yy = np.arange(n, dtype=np.float32)[:, None]
    lines = (np.mod(yy, 32.0) < 2.0).astype(np.float32) * -0.12
    base = np.clip(0.35 + 0.08 * texture + lines, 0.0, 1.0)
    gt = make_rgba(n, n, (0.0, 0.0, 0.0))
    gt[..., 0] = base
    gt[..., 1] = base * 0.95
    gt[..., 2] = base * 0.88
    mask = circle_mask(n, n, 128, 128, 30)
    src = insert_object(gt, mask, (0.85, 0.20, 0.10))
    fixtures.append((
        "textured_wall",
        "deterministic filtered noise with periodic mortar lines",
        "texture stitching, unnatural repetition, line continuation",
        gt, mask, src,
        ["Exact noise reconstruction is not required; judge continuity and naturalness visually."],
    ))

    # 3. High-contrast edge crossing the masked region.
    gt = make_rgba(n, n, (0.0, 0.0, 0.0))
    gt[:, : n // 2, :3] = (0.04, 0.05, 0.07)
    gt[:, n // 2 :, :3] = (0.85, 0.88, 0.92)
    mask = rect_mask(n, n, 104, 48, 152, 208)
    src = insert_object(gt, mask, (0.60, 0.10, 0.10))
    fixtures.append((
        "high_contrast_edge",
        "vertical dark/bright boundary crossed by a tall object",
        "edge disappearance, edge drift, halo at the boundary",
        gt, mask, src,
        ["The clean plate defines the expected edge position; inpaint cannot know the edge shape."],
    ))

    # 4. Thin line that must be reconnected.
    gt = make_rgba(n, n, (0.18, 0.20, 0.24))
    gt[126:130, :, :3] = (0.95, 0.90, 0.70)
    mask = rect_mask(n, n, 96, 104, 160, 152)
    src = insert_object(gt, mask, (0.10, 0.50, 0.80))
    fixtures.append((
        "thin_line",
        "four-pixel bright line interrupted by an object",
        "broken, thickened, or recolored thin structures",
        gt, mask, src,
        ["Line preservation is a structural check, not a pixel-exactness check."],
    ))

    # 5. Straight-alpha contract and background composites.
    gt = make_rgba(n, n, (0.20, 0.40, 0.60))
    gt[:, n // 2 :, 3] = 0.5
    mask = rect_mask(n, n, 88, 88, 168, 168)
    src = insert_object(gt, mask, (0.90, 0.30, 0.10), alpha=1.0)
    fixtures.append((
        "alpha_edge",
        "opaque left half, 0.5-alpha right half, object crossing both",
        "alpha change outside the mask, transparent RGB changes, composite edge",
        gt, mask, src,
        [
            "Expected alpha behavior for inpaint inside the mask is not yet defined.",
            "Composite PNGs use a simple linear composite; the app view transform is not applied.",
        ],
    ))

    # 6. HDR highlight above 1.0.
    gt = make_rgba(n, n, (0.02, 0.02, 0.03))
    yy, xx = np.mgrid[0:n, 0:n]
    disc = ((xx - 180) ** 2 + (yy - 90) ** 2) < (12 ** 2)
    gt[disc, :3] = (4.0, 3.2, 1.8)
    mask = rect_mask(n, n, 160, 70, 200, 110)
    src = insert_object(gt, mask, (0.50, 0.10, 0.10))
    fixtures.append((
        "hdr_highlight",
        "linear RGB values up to 4.0 covered by an object",
        "clipping to 0-1, highlight loss, non-finite values",
        gt, mask, src,
        [
            "Display PNG clamps HDR; NPY is the measurement source.",
            "An implementation that clamps cannot reproduce this ground truth; judge as a contract question.",
        ],
    ))

    return fixtures


def build_sequence(seed: int) -> tuple[list[tuple[str, str, str, np.ndarray, np.ndarray, np.ndarray, list[str]]], list[str]]:
    n = SEQUENCE_SIZE
    rng = np.random.default_rng(seed + 2)
    noise = rng.normal(0.0, 1.0, (n, n)).astype(np.float32)
    texture = cv2.GaussianBlur(noise, (0, 0), 3.0)
    texture = texture / (float(np.std(texture)) + 1e-6)
    base = np.clip(0.30 + 0.06 * texture, 0.0, 1.0)

    ground_truth = make_rgba(n, n, (0.0, 0.0, 0.0))
    ground_truth[..., 0] = base
    ground_truth[..., 1] = base * 0.98
    ground_truth[..., 2] = base * 0.94

    fixtures: list[tuple[str, str, str, np.ndarray, np.ndarray, np.ndarray, list[str]]] = []
    ids: list[str] = []
    for i in range(SEQUENCE_FRAMES):
        x0 = 20 + i * 14
        mask = rect_mask(n, n, x0, 64, x0 + 28, 96)
        src = insert_object(ground_truth, mask, (0.90, 0.20, 0.10))
        fixture_id = f"sequence_moving_object_{i:03d}"
        ids.append(fixture_id)
        fixtures.append((
            fixture_id,
            f"frame {i}: a bright object at x={x0} over a static textured background",
            "temporal stability of the static background (flicker / shimmer)",
            ground_truth, mask, src,
            [
                "The clean plate is identical in all frames; object motion is not flicker.",
                "No camera motion or optical flow is modeled in this fixture set.",
            ],
        ))
    return fixtures, ids


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("experiments/InpaintLab/fixtures/inpaint-rd1"),
        help="fixture output directory (relative to repository root)",
    )
    parser.add_argument("--seed", type=int, default=DEFAULT_SEED)
    args = parser.parse_args()

    root = args.output_dir
    root.mkdir(parents=True, exist_ok=True)

    still = build_still_fixtures(args.seed)
    sequence, sequence_ids = build_sequence(args.seed)

    entries = [
        write_fixture(root, *fixture) for fixture in still
    ] + [
        write_fixture(root, *fixture) for fixture in sequence
    ]

    manifest = {
        "bundle": root.name,
        "generator": "build_inpaint_fixtures.py",
        "generator_version": GENERATOR_VERSION,
        "seed": args.seed,
        "environment": {
            "numpy_version": np.__version__,
            "opencv_version": cv2.__version__,
        },
        "color_contract": {
            "measurement_format": "float32 RGBA NPY",
            "color_space": "linear scene-referred",
            "alpha": "straight",
            "display_format": "8-bit BGRA PNG, clamped [0,1] then sRGB OETF",
            "view_transform": "none (Artifact active view transform is not used)",
        },
        "groups": {
            "sequence_moving_object": sequence_ids,
        },
        "fixtures": entries,
        "limitations": [
            "All fixtures are synthetic. Ground truth is a clean plate, not a naturalness proof.",
            "No Artifact code is executed, no GPU dispatch, no runtime capture, no performance measurement.",
            "No threshold or pass/fail verdict is derived from these fixtures alone.",
        ],
    }

    manifest_path = root / "manifest.json"
    manifest_path.write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    print(f"Wrote {len(entries)} fixtures and {manifest_path}")


if __name__ == "__main__":
    main()
