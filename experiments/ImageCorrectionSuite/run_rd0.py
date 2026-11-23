from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image


ROOT = Path(__file__).resolve().parent
OUT = ROOT / "captures" / "rd0"
SEED = 20261116
W, H = 512, 320


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_image(path: Path, rgb: np.ndarray) -> None:
    srgb = np.where(rgb <= 0.0031308, rgb * 12.92, 1.055 * np.maximum(rgb, 0) ** (1 / 2.4) - 0.055)
    Image.fromarray(np.uint8(np.clip(srgb * 255 + 0.5, 0, 255)), "RGB").save(path, format="PNG", optimize=False)


def error_stats(a: np.ndarray, b: np.ndarray, region: np.ndarray) -> dict[str, float]:
    delta = np.abs(a[region].astype(np.float64) - b[region].astype(np.float64)).reshape(-1)
    return {"mae": float(delta.mean()), "p95": float(np.percentile(delta, 95)), "max": float(delta.max())}


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    yy, xx = np.mgrid[0:H, 0:W]
    x = xx / (W - 1)
    y = yy / (H - 1)
    rng = np.random.default_rng(SEED)
    base = np.stack((0.12 + 0.68 * x, 0.16 + 0.58 * y, 0.18 + 0.35 * x + 0.18 * y), axis=-1).astype(np.float32)
    grid = np.zeros((H, W, 3), np.float32)
    grid[:] = (0.06, 0.07, 0.08)
    grid[::32, :, :] = (0.85, 0.85, 0.85)
    grid[:, ::32, :] = (0.85, 0.85, 0.85)
    grid[::32, ::32, :] = (1.0, 0.22, 0.08)
    natural = np.clip(base + rng.normal(0, 0.025, base.shape).astype(np.float32), 0, 1)
    cases: dict[str, np.ndarray] = {"chart": grid, "smooth_color": base, "texture": natural}
    files: dict[str, dict[str, str]] = {}

    # 2: Known restoration damage, with clean truth and a damage mask.
    clean = natural.copy()
    damaged = clean.copy()
    damage = np.zeros((H, W), bool)
    # Dark scratches/dust over the natural image; keep a narrow known mask.
    damaged[70:73, 80:172] = 0.02
    damage[70:73, 80:172] = True
    damaged[120:126, 300:306] = 0.02
    damage[120:126, 300:306] = True
    cases["repair_clean"] = clean
    cases["repair_damaged"] = damaged
    Image.fromarray(damage.astype(np.uint8) * 255, "L").save(OUT / "repair_damage_mask.png", format="PNG", optimize=False)
    files["repair_damage_mask"] = {"path": "repair_damage_mask.png", "sha256": sha(OUT / "repair_damage_mask.png")}

    # 3: Synthetic moire overlay; reference chart is unmodulated.
    carrier = (0.5 + 0.19 * np.sin(2 * np.pi * xx / 6.3) * np.sin(2 * np.pi * yy / 7.1)).astype(np.float32)
    moire = np.clip(base * (0.64 + carrier[..., None] * 0.72), 0, 1).astype(np.float32)
    cases["moire_reference"] = base
    cases["moire_corrupted"] = moire

    # 4/5: Known radial channel offset and radial barrel/pincushion warp.
    cx, cy = (W - 1) * 0.5, (H - 1) * 0.5
    radius = np.sqrt(((xx - cx) / cx) ** 2 + ((yy - cy) / cy) ** 2)
    radial = np.clip(radius / np.sqrt(2), 0, 1)
    color_aberrated = grid.copy()
    dx = (xx - cx) / (cx + 1) * 4.0 * radial
    dy = (yy - cy) / (cy + 1) * 4.0 * radial
    for channel, sign in ((0, 1), (2, -1)):
        sx = np.clip(np.rint(xx + sign * dx).astype(int), 0, W - 1)
        sy = np.clip(np.rint(yy + sign * dy).astype(int), 0, H - 1)
        color_aberrated[..., channel] = grid[sy, sx, channel]
    cases["chromatic_reference"] = grid
    cases["chromatic_shifted"] = color_aberrated
    k = 0.08
    xn, yn = (xx - cx) / cx, (yy - cy) / cy
    r2 = xn * xn + yn * yn
    for label, coeff in (("barrel", k), ("pincushion", -k)):
        scale = 1.0 + coeff * r2
        sx = np.clip(np.rint(cx + xn * scale * cx).astype(int), 0, W - 1)
        sy = np.clip(np.rint(cy + yn * scale * cy).astype(int), 0, H - 1)
        cases[f"distortion_{label}"] = grid[sy, sx]
    cases["distortion_reference"] = grid

    # 10: Deterministic source/reference pair with per-channel affine color shift.
    source = np.clip(base * np.array([0.84, 1.08, 0.92], np.float32) + np.array([0.04, -0.03, 0.06], np.float32), 0, 1)
    reference = np.clip(base * np.array([0.95, 0.88, 1.05], np.float32) + np.array([0.02, 0.05, -0.02], np.float32), 0, 1)
    cases["color_match_source"] = source
    cases["color_match_reference"] = reference

    # 11: Discrete color-area chart for palette coverage analysis.
    palette_chart = np.zeros((H, W, 3), np.float32)
    colors = np.array([[0.04, 0.08, 0.25], [0.12, 0.46, 0.76], [0.86, 0.28, 0.12], [0.94, 0.78, 0.34]], np.float32)
    widths = [64, 128, 192, 128]
    cursor = 0
    for color, width in zip(colors, widths):
        palette_chart[:, cursor:cursor + width] = color
        cursor += width
    cases["palette_coverage"] = palette_chart

    for name, image in cases.items():
        npy = OUT / f"{name}.npy"
        png = OUT / f"{name}.png"
        np.save(npy, np.asarray(image, np.float32), allow_pickle=False)
        write_image(png, image)
        files[name] = {
            "npy": {"path": npy.name, "sha256": sha(npy)},
            "preview": {"path": png.name, "sha256": sha(png)},
        }

    # Descriptive response metrics only; no quality threshold or winner.
    clean_area = damage
    baseline_repair = error_stats(damaged, clean, clean_area)
    baseline_repair["outside_damage_max_abs"] = float(np.max(np.abs(damaged[~clean_area] - clean[~clean_area])))
    metadata = {
        "rd": "rd0",
        "seed": SEED,
        "dimensions": [W, H],
        "color_contract": "linear RGB float32 [0,1]; PNG uses sRGB transfer encoding",
        "purpose": "fixture and measurement sanity only; no candidate ranking or gate",
        "generator_sha256": sha(Path(__file__).resolve()),
        "files": files,
        "repair_baseline": baseline_repair,
        "notes": [
            "Synthetic responses do not establish real-photo quality.",
            "Distortion inverses, chromatic shift correction, moire suppression, and color transfer candidates are not executed in rd0.",
            "Palette fixture has analytically known area proportions; palette extraction is not executed in rd0.",
        ],
    }
    (OUT / "manifest.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
