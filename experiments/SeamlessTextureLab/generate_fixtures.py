"""Deterministic fixtures for the seamless-texture R&D lab (RD0).

Linear float RGBA, straight alpha by default, with a premultiplied variant of
the alpha fixture. 8-bit sRGB PNG previews are written for review only.
Offline: no build, no Artifact launch.

The fixtures are deliberately NOT tileable: each one has content that does not
match across the image border, so the naive "repeat the tile" baseline shows a
visible seam. The lab measures how well a candidate removes that seam without
destroying the texture itself.
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
    "checker": SEED + 11,
    "radial": SEED + 12,
    "clouds": SEED + 13,
    "blobs": SEED + 14,
    "alpha_patch": SEED + 15,
}


def srgb_to_linear(u: np.ndarray) -> np.ndarray:
    u = np.asarray(u, dtype=np.float64)
    return np.where(u <= 0.04045, u / 12.92, np.power((u + 0.055) / 1.055, 2.4))


def linear_to_srgb_u8(x: np.ndarray) -> np.ndarray:
    x = np.clip(np.asarray(x, dtype=np.float64), 0.0, 1.0)
    s = np.where(x <= 0.0031308, 12.92 * x, 1.055 * np.power(x, 1.0 / 2.4) - 0.055)
    return np.round(s * 255.0).astype(np.uint8)


def _rgba(h: int, w: int, rgb: np.ndarray, alpha: np.ndarray | None = None) -> np.ndarray:
    out = np.empty((h, w, 4), dtype=np.float32)
    out[..., :3] = np.clip(rgb, 0.0, None).astype(np.float32)
    out[..., 3] = 1.0 if alpha is None else alpha.astype(np.float32)
    return out


def _value_noise(h: int, w: int, seed: int, octaves: int = 5, base: int = 4) -> np.ndarray:
    """Multi-octave smooth noise in [0, 1] (deterministic, seed controlled)."""
    rng = np.random.default_rng(seed)
    total = np.zeros((h, w), dtype=np.float64)
    amplitude = 1.0
    norm = 0.0
    for octave in range(octaves):
        cells = max(2, base * (2 ** octave))
        grid = rng.random((cells, cells)).astype(np.float64)
        layer = cv2.resize(grid, (w, h), interpolation=cv2.INTER_CUBIC)
        total += amplitude * layer
        norm += amplitude
        amplitude *= 0.55
    total /= max(norm, 1e-9)
    lo, hi = float(total.min()), float(total.max())
    if hi - lo < 1e-9:
        return np.full((h, w), 0.5)
    return (total - lo) / (hi - lo)


def gen_checker(h: int, w: int, seed: int) -> tuple[np.ndarray, dict]:
    """Hard-edged squares: the hardest case for a feathered seam."""
    rng = np.random.default_rng(seed)
    rgb = np.full((h, w, 3), 0.18, dtype=np.float32)
    cell = max(8, w // 11)
    colors = [(0.85, 0.30, 0.20), (0.20, 0.55, 0.85), (0.90, 0.80, 0.25), (0.25, 0.65, 0.35)]
    for row in range(h // cell + 2):
        for col in range(w // cell + 2):
            idx = (row + col + int(rng.integers(0, 4))) % len(colors)
            rgb[row * cell:(row + 1) * cell, col * cell:(col + 1) * cell] = colors[idx]
    rgb += rng.normal(0.0, 0.0015, size=(h, w, 1)).astype(np.float32)
    return _rgba(h, w, rgb), {"seed": seed, "seam_expectation": "hard square edges cross the border"}


def gen_radial(h: int, w: int, seed: int) -> tuple[np.ndarray, dict]:
    """Radial gradient with a displaced centre: the border mismatch is smooth but visible."""
    rng = np.random.default_rng(seed)
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float64)
    cx, cy = w * 0.42, h * 0.58
    r = np.sqrt((xx - cx) ** 2 + (yy - cy) ** 2) / (w * 0.5)
    base = np.clip(1.0 - r, 0.0, 1.0)
    rgb = np.stack([base, np.clip(base ** 1.6, 0, 1), np.clip(base ** 0.7, 0, 1)], axis=2)
    rgb = 0.08 + 0.72 * rgb
    rgb += rng.normal(0.0, 0.002, size=(h, w, 1)).astype(np.float32)
    return _rgba(h, w, rgb), {"seed": seed, "seam_expectation": "gradient offset across the border"}


def gen_clouds(h: int, w: int, seed: int) -> tuple[np.ndarray, dict]:
    """Cloud-like multi-octave noise: the texture that tiling usually shows as a repeated motif."""
    n = _value_noise(h, w, seed, octaves=6, base=3)
    n2 = _value_noise(h, w, seed + 991, octaves=4, base=5)
    rgb = np.stack([n, n2, np.clip(n * 0.5 + n2 * 0.5, 0, 1)], axis=2) * 0.75 + 0.12
    return _rgba(h, w, rgb), {"seed": seed, "seam_expectation": "noise field does not match across the border"}


def gen_blobs(h: int, w: int, seed: int) -> tuple[np.ndarray, dict]:
    """Random coloured discs on a flat ground: strong low-frequency structure."""
    rng = np.random.default_rng(seed)
    rgb = np.full((h, w, 3), 0.30, dtype=np.float32)
    for _ in range(26):
        cx, cy = rng.integers(0, w), rng.integers(0, h)
        radius = int(rng.integers(max(6, w // 24), max(10, w // 8)))
        color = rng.random(3).astype(np.float32) * 0.6 + 0.2
        cv2.circle(rgb, (int(cx), int(cy)), radius, tuple(float(c) for c in color), -1, lineType=cv2.LINE_AA)
    rgb += rng.normal(0.0, 0.001, size=(h, w, 1)).astype(np.float32)
    return _rgba(h, w, rgb), {"seed": seed, "seam_expectation": "discs cross the border"}


def gen_alpha_patch(h: int, w: int, seed: int) -> tuple[np.ndarray, dict]:
    """Alpha boundary fixture so the premultiplied / straight contract is exercised."""
    rng = np.random.default_rng(seed)
    rgb = np.full((h, w, 3), 0.55, dtype=np.float32)
    alpha = np.zeros((h, w), dtype=np.float32)
    x0, x1 = int(w * 0.10), int(w * 0.90)
    y0, y1 = int(h * 0.12), int(h * 0.88)
    alpha[y0:y1, x0:x1] = 1.0
    band = max(3, int(w * 0.04))
    ramp = np.linspace(1.0, 0.0, band)
    alpha[y0:y1, x0:x0 + band] = np.maximum(alpha[y0:y1, x0:x0 + band], ramp[None, :])
    ramp2 = np.linspace(0.0, 1.0, band)
    alpha[y0:y1, x1 - band:x1] = np.maximum(alpha[y0:y1, x1 - band:x1], ramp2[None, :])
    alpha += rng.normal(0.0, 0.0004, size=(h, w)).astype(np.float32)
    alpha = np.clip(alpha, 0.0, 1.0)
    out = _rgba(h, w, rgb)
    out[..., 3] = alpha
    return out, {
        "seed": seed,
        "alpha_contract": "straight RGBA; transparent pixels keep hidden RGB = 0.55 linear",
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
    return {"npy": npy.name, "npy_sha256": sha256(npy.read_bytes()),
            "png": png.name, "png_sha256": sha256(png.read_bytes())}


def generate(output: Path) -> dict:
    output.mkdir(parents=True, exist_ok=True)
    fixture_dir = output / "fixtures"
    fixture_dir.mkdir(parents=True, exist_ok=True)
    manifest = {
        "lab": "SeamlessTextureLab", "rd": "rd0", "stage": "fixtures", "date": "2026-11-18",
        "seed": SEED,
        "convention": "linear float RGBA, straight alpha unless suffixed _premultiplied; sRGB OETF for PNG",
        "resolutions": RESOLUTIONS,
        "fixtures": {},
        "notes": [
            "Every fixture is intentionally non-tileable: the naive repeat baseline shows a seam.",
            "The alpha fixture carries a premultiplied variant for the straight / premultiplied contract.",
        ],
    }
    for res_name, size in RESOLUTIONS.items():
        h, w = size["h"], size["w"]
        for name, gen in (
            ("checker", gen_checker), ("radial", gen_radial),
            ("clouds", gen_clouds), ("blobs", gen_blobs),
        ):
            rgba, meta = gen(h, w, FIXTURE_SEEDS[name])
            entry = {"resolution": res_name, "shape": list(rgba.shape), **meta}
            entry.update(save_rgba(fixture_dir / f"{name}_{res_name}", rgba))
            manifest["fixtures"][f"{name}_{res_name}"] = entry
        rgba, meta = gen_alpha_patch(h, w, FIXTURE_SEEDS["alpha_patch"])
        entry = {"resolution": res_name, "shape": list(rgba.shape), **meta}
        entry.update(save_rgba(fixture_dir / f"alpha_patch_{res_name}", rgba))
        entry["premultiplied"] = save_rgba(fixture_dir / f"alpha_patch_{res_name}_premultiplied",
                                          to_premultiplied(rgba))
        manifest["fixtures"][f"alpha_patch_{res_name}"] = entry
    (output / "fixtures_manifest.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return manifest


def check(output: Path) -> None:
    manifest = json.loads((output / "fixtures_manifest.json").read_text(encoding="utf-8"))
    bad = []
    for key, entry in manifest["fixtures"].items():
        files = [(entry["npy"], entry["npy_sha256"]), (entry["png"], entry["png_sha256"])]
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
