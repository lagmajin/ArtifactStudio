"""Measure the temporal spectrum of deterministic grain update policies."""

from __future__ import annotations

import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
CAPTURE = ROOT / "captures" / "grain-temporal-correlation"
WIDTH, HEIGHT = 256, 192
FRAME_COUNT = 120
SIGMA = 0.025
SEED = 20261111
POLICIES = ("held", "ar1-0.95", "ar1-0.75", "ar1-0.50", "ar1-0.00", "refresh-every-8")


def make_base() -> np.ndarray:
    yy, xx = np.mgrid[0:HEIGHT, 0:WIDTH].astype(np.float32)
    base = np.stack((
        0.40 + 0.17 * np.sin(xx * 0.031) + 0.08 * np.cos(yy * 0.053),
        0.46 + 0.13 * np.cos((xx + yy) * 0.022),
        0.48 + 0.12 * np.sin((xx - 1.4 * yy) * 0.018),
    ), axis=2)
    base += (0.045 * np.exp(-((xx - 82) ** 2 + (yy - 96) ** 2) / (2 * 25 ** 2)))[:, :, None]
    base -= (0.06 * np.exp(-((xx - 191) ** 2 + (yy - 88) ** 2) / (2 * 39 ** 2)))[:, :, None]
    return np.clip(base, 0.12, 0.82).astype(np.float32)


def make_fields() -> dict[str, np.ndarray]:
    rng = np.random.default_rng(SEED)
    innovations = rng.normal(0.0, 1.0, (FRAME_COUNT, HEIGHT, WIDTH)).astype(np.float32)
    fields: dict[str, np.ndarray] = {"held": np.repeat(innovations[:1], FRAME_COUNT, axis=0)}
    for rho in (0.95, 0.75, 0.50, 0.00):
        sequence = np.empty_like(innovations)
        sequence[0] = innovations[0]
        innovation_scale = float(np.sqrt(1 - rho * rho))
        for frame in range(1, FRAME_COUNT):
            sequence[frame] = rho * sequence[frame - 1] + innovation_scale * innovations[frame]
        fields[f"ar1-{rho:.2f}"] = sequence
    refreshed = innovations.copy()
    for frame in range(FRAME_COUNT):
        refreshed[frame] = innovations[(frame // 8) * 8]
    fields["refresh-every-8"] = refreshed
    return fields


def mean_ci(values: np.ndarray) -> tuple[float, float]:
    if values.size < 2:
        return float(np.mean(values)), 0.0
    standard_error = float(np.std(values, ddof=1) / np.sqrt(values.size))
    return float(np.mean(values)), 1.96 * standard_error


def sequence_metrics(fields: dict[str, np.ndarray]) -> dict[str, object]:
    result: dict[str, object] = {}
    pairs = (1, 2, 4, 8, 16, 32)
    for policy, sequence in fields.items():
        sigma_per_frame = np.std(sequence, axis=(1, 2))
        lag_mae, lag_corr = {}, {}
        centered = sequence - np.mean(sequence, axis=(1, 2), keepdims=True)
        for lag in pairs:
            a, b = sequence[:-lag], sequence[lag:]
            delta = np.mean(np.abs(b - a), axis=(1, 2))
            mean, ci = mean_ci(delta)
            lag_mae[str(lag)] = {"mean residual adjacent-lag MAE": mean, "95% CI half-width across frame-pairs": ci}
            ca, cb = centered[:-lag], centered[lag:]
            numerator = np.mean(ca * cb, axis=(1, 2))
            denominator = np.sqrt(np.mean(ca * ca, axis=(1, 2)) * np.mean(cb * cb, axis=(1, 2)))
            lag_corr[str(lag)] = float(np.mean(numerator / np.maximum(denominator, 1e-12)))
        first = sequence[0] - np.mean(sequence[0])
        first_corr = []
        for frame in range(FRAME_COUNT):
            current = sequence[frame] - np.mean(sequence[frame])
            denom = float(np.sqrt(np.mean(first * first) * np.mean(current * current)))
            first_corr.append(float(np.mean(first * current) / max(denom, 1e-12)))
        result[policy] = {
            "mean field sigma": float(np.mean(sigma_per_frame)),
            "field sigma min/max": [float(np.min(sigma_per_frame)), float(np.max(sigma_per_frame))],
            "lag MAE": lag_mae,
            "lag correlation": lag_corr,
            "correlation with first frame": first_corr,
        }
    return result


def make_sheet(base: np.ndarray, fields: dict[str, np.ndarray]) -> Path:
    selected = (0, 1, 7, 8, 30, 60, 119)
    cell_w, cell_h, label_h = 192, 144, 20
    sheet = Image.new("RGB", (len(selected) * cell_w, len(POLICIES) * (cell_h + label_h)), (30, 31, 34))
    draw, font = ImageDraw.Draw(sheet), ImageFont.load_default()
    for row, policy in enumerate(POLICIES):
        for col, frame in enumerate(selected):
            x, y = col * cell_w, row * (cell_h + label_h)
            draw.text((x + 3, y + 3), f"{policy} · frame {frame:03d}", fill=(245, 245, 245), font=font)
            rgb = np.clip(base + SIGMA * fields[policy][frame, :, :, None], 0.0, 1.0)
            tile = Image.fromarray(np.uint8(rgb * 255 + 0.5), "RGB")
            tile = tile.resize((cell_w, cell_h), Image.Resampling.NEAREST)
            sheet.paste(tile, (x, y + label_h))
    CAPTURE.mkdir(parents=True, exist_ok=True)
    path = CAPTURE / "grain-temporal-correlation-sheet.png"
    sheet.save(path)
    return path


def main() -> dict[str, object]:
    base = make_base()
    fields = make_fields()
    sheet = make_sheet(base, fields)
    report: dict[str, object] = {
        "question": "How do temporal grain correlation and periodic refresh cadence affect residual change and persistence?",
        "evaluation plan": {
            "objective": "Describe algorithmic temporal response for held, AR(1), white-per-frame, and periodic-refresh Gaussian grain at matched field variance.",
            "baseline": "Same deterministic synthetic RGB scene with grain residual subtracted; it is not a target look or quality oracle.",
            "conditions": "256x192 static synthetic scene, 120 frames, monochrome additive Gaussian field sigma=.025, NumPy float32, seed 20261111, clip-safe base range [.12,.82], sRGB-like code-value model.",
            "metrics": "Per-frame field sigma; residual absolute difference at lags 1,2,4,8,16,32; normalized spatial correlation at those lags; correlation with frame 0; visual sheet at frames 0,1,7,8,30,60,119.",
            "acceptance": "No perceptual acceptance threshold set; report response only and do not rank look quality.",
            "visual reviewer": "Research author; inspect full sheet and representative temporal sequence after generation.",
            "required but unverified": "Real footage and moving content, color/alpha/transfer contract, subpixel transforms, preview/export and GPU/backend parity, production timing/memory, approved look criteria.",
        },
        "scene": {"width": WIDTH, "height": HEIGHT, "frames": FRAME_COUNT, "grain sigma": SIGMA,
                  "seed": SEED, "policy definitions": {
                      "held": "one Gaussian field reused for all frames",
                      "ar1-rho": "G[t]=rho*G[t-1]+sqrt(1-rho^2)*E[t], unit stationary variance",
                      "ar1-0.00": "independent Gaussian field each frame",
                      "refresh-every-8": "hold one fresh field for frames in each 8-frame block",
                  }},
        "metrics by policy": sequence_metrics(fields),
        "contact sheet": str(sheet.relative_to(ROOT)),
        "environment": {"numpy": np.__version__, "pillow": Image.__version__},
        "limits": "Synthetic still scene isolates temporal grain only. MAE/correlation describe field behavior and are not perceptual flicker or quality scores. No product runtime or Core behavior evaluated.",
    }
    CAPTURE.mkdir(parents=True, exist_ok=True)
    (CAPTURE / "grain-temporal-correlation-metrics.json").write_text(
        json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    report = main()
    print("Measured temporal grain correlation across 6 policies and 120 frames.")
