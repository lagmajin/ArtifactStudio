#!/usr/bin/env python3
"""Measure the Film Damage Film Burn equation on deterministic local fixtures."""
from __future__ import annotations

import hashlib
import json
import platform
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
PLAN = ROOT / "experiments/FilmEffectsLab/plans/film-damage-film-burn-alpha-response-r1.json"
SOURCE = ROOT / "Artifact/src/Effects/Rasterizer/FilmDamageEffect.cpp"
OUT = ROOT / "experiments/FilmEffectsLab/captures/film-damage-film-burn-alpha-response-r1"
SIZES = ((512, 512), (512, 256), (1024, 512))
VALUES = (0.0, 0.12, 0.25, 0.5, 0.75, 1.0)
SIDES = {"left": -0.08, "right": 1.08}
BACKGROUNDS = {"black": np.zeros(3, np.float32),
               "gray18": np.full(3, 0.18, np.float32),
               "white": np.ones(3, np.float32)}


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def fixture(width: int, height: int) -> tuple[np.ndarray, dict[str, np.ndarray]]:
    yy, xx = np.mgrid[0:height, 0:width].astype(np.float32)
    nx, ny = xx / max(width - 1, 1), yy / max(height - 1, 1)
    radius = np.sqrt(((nx - .5) / .39) ** 2 + ((ny - .5) / .43) ** 2)
    alpha = np.clip((1.04 - radius) * 18.0, 0, 1).astype(np.float32)
    rgb = np.empty((height, width, 3), np.float32)
    rgb[:] = (.86, .22, .07)
    rgb[(nx > .28) & (nx < .72)] = (.08, .62, .92)
    detail = (nx > .34) & (nx < .68) & (ny > .36) & (ny < .64)
    checker = ((xx.astype(np.int32) // 4 + yy.astype(np.int32) // 4) % 2) == 0
    rgb[detail & checker] = (.96, .92, .10)
    rgb[detail & ~checker] = (.035, .055, .10)
    for col in range(width // 5, width * 4 // 5, 23):
        rgb[:, col:col + 1] = .98
    rgb[alpha == 0] = 0
    bgra = np.dstack((rgb[..., ::-1], alpha)).astype(np.float32)
    regions = {
        "transparent": alpha == 0,
        "low_alpha": (alpha > 0) & (alpha <= .1),
        "edge": (alpha > .1) & (alpha < .9),
        "near_opaque": (alpha >= .9) & (alpha < 1),
        "opaque": alpha == 1,
        "all": np.ones(alpha.shape, bool),
    }
    return bgra, regions


def burn_field(width: int, height: int, amount: float, center_x: float) -> np.ndarray:
    yy, xx = np.mgrid[0:height, 0:width].astype(np.float32)
    nx, ny = xx / max(width - 1, 1), yy / max(height - 1, 1)
    distance = np.sqrt((nx - center_x) ** 2 + (ny - .5) ** 2)
    ring_radius = .18 + amount * .8
    return (amount * np.clip(1 - np.abs(distance - ring_radius) / .12, 0, 1)).astype(np.float32)


def apply(source: np.ndarray, burn: np.ndarray) -> np.ndarray:
    out = source.copy()
    # The source storage is BGRA; the C++ loop adds warm color to RGB channels.
    out[..., 0] += burn * .08
    out[..., 1] += burn * .48
    out[..., 2] += burn * 1.35
    return out


def composite(bgra: np.ndarray, association: str, background: np.ndarray) -> np.ndarray:
    rgb, alpha = bgra[..., :3][..., ::-1], bgra[..., 3:4]
    if association == "straight":
        return rgb * alpha + background * (1 - alpha)
    return rgb + background * (1 - alpha)


def stats(values: np.ndarray) -> dict:
    x = np.abs(values.astype(np.float64)).reshape(-1)
    if x.size == 0:
        return {"count": 0, "mae": 0.0, "rmse": 0.0, "p95_abs": 0.0, "max_abs": 0.0}
    return {"count": int(x.size), "mae": float(x.mean()),
            "rmse": float(np.sqrt(np.mean(x * x))),
            "p95_abs": float(np.quantile(x, .95)), "max_abs": float(x.max())}


def preview(path: Path, bgra: np.ndarray, background: np.ndarray) -> None:
    rgb = composite(bgra, "straight", background)
    Image.fromarray(np.rint(np.clip(rgb, 0, 1) * 255).astype(np.uint8), "RGB").save(path)


def make_sheet(files: list[tuple[str, Path]], path: Path) -> None:
    tile_w, tile_h, label_h = 256, 256, 28
    cols = 4
    rows = (len(files) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * tile_w, rows * (tile_h + label_h)), (28, 29, 32))
    draw = ImageDraw.Draw(sheet)
    for i, (label, filename) in enumerate(files):
        im = Image.open(filename).convert("RGB")
        im.thumbnail((tile_w, tile_h), Image.Resampling.LANCZOS)
        x, y = (i % cols) * tile_w, (i // cols) * (tile_h + label_h)
        sheet.paste(im, (x + (tile_w - im.width) // 2, y))
        draw.text((x + 6, y + tile_h + 5), label, fill=(235, 235, 235))
    sheet.save(path)


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    plan = json.loads(PLAN.read_text(encoding="utf-8"))
    rows, sheets = [], {"square": [], "wide": []}
    for width, height in SIZES:
        base, regions = fixture(width, height)
        fixture_hash = sha(base.tobytes())
        for side, center_x in SIDES.items():
            for amount in VALUES:
                field = burn_field(width, height, amount, center_x)
                ring = field > 0
                outputs, baselines = {}, {}
                for association in ("straight", "premultiplied"):
                    associated_base = base.copy()
                    if association == "premultiplied":
                        associated_base[..., :3] *= associated_base[..., 3:4]
                    outputs[association] = apply(associated_base, field)
                    baselines[association] = apply(associated_base, np.zeros_like(field))
                output = outputs["straight"]
                row = {
                    "width": width, "height": height, "side": side,
                    "center": [center_x, .5], "film_burn": amount,
                    "fixture_sha256": fixture_hash,
                    "alpha_exact": bool(np.array_equal(output[..., 3], base[..., 3])),
                    "burn": {"coverage_fraction": float(ring.mean()),
                             "peak": float(field.max()), "mean_positive": float(field[ring].mean()) if ring.any() else 0.0,
                             "positive_pixels": int(ring.sum()),
                             "warm_delta_rgb_peak": [float((field * 1.35).max()), float((field * .48).max()), float((field * .08).max())]},
                    "output_range": {},
                    "regions": {},
                }
                for association, associated_output in outputs.items():
                    rgb = associated_output[..., :3]
                    row["output_range"][association] = {
                        "min_rgb": float(rgb.min()), "max_rgb": float(rgb.max()),
                        "rgb_channels_gt_1_fraction": float(np.mean(rgb > 1)),
                        "nonfinite_rgb_channels": int(np.size(rgb) - np.isfinite(rgb).sum()),
                        "rgb_channels_gt_alpha_fraction": float(np.mean(rgb > associated_output[..., 3:4])),
                    }
                for region_name, mask in regions.items():
                    row["regions"][region_name] = {"pixels": int(mask.sum()), "backgrounds": {}}
                    for bg_name, background in BACKGROUNDS.items():
                        bg_stats = {}
                        for association in ("straight", "premultiplied"):
                            delta = composite(outputs[association], association, background) - composite(baselines[association], association, background)
                            bg_stats[association] = stats(delta[mask])
                        row["regions"][region_name]["backgrounds"][bg_name] = bg_stats
                if ring.any():
                    ys, xs = np.nonzero(ring)
                    row["burn"]["normalized_bbox"] = [float(xs.min() / max(width - 1, 1)),
                                                        float(ys.min() / max(height - 1, 1)),
                                                        float(xs.max() / max(width - 1, 1)),
                                                        float(ys.max() / max(height - 1, 1))]
                    row["burn"]["normalized_centroid"] = [float((xs / max(width - 1, 1)).mean()),
                                                           float((ys / max(height - 1, 1)).mean())]
                rows.append(row)
                if width == 512 and (height == 512 or height == 256):
                    filename = OUT / f"burn_{width}x{height}_{side}_{amount:.2f}.png"
                    preview(filename, output, BACKGROUNDS["gray18"])
                    key = "square" if height == 512 else "wide"
                    sheets[key].append((f"{side} / burn {amount:.2f}", filename))
    for key, files in sheets.items():
        make_sheet(files, OUT / f"contact_{key}.png")
    source_text = SOURCE.read_text(encoding="utf-8")
    source_markers = {
        "radius_formula_present": "0.18f + filmBurn * 0.8f" in source_text,
        "ring_width_formula_present": "std::abs(distance - burnRadius) / 0.12f" in source_text,
        "red_gain_present": "burn * 1.35f" in source_text,
        "alpha_restored_from_source": "outputRow[x][3] = sourceRow[x][3]" in source_text,
        "random_left_right_centers_present": "? -0.08f : 1.08f" in source_text,
    }
    metrics = {
        "study": plan["plan_id"],
        "environment": {"python": platform.python_version(), "numpy": np.__version__},
        "inputs_sha256": {
            "plan": sha(PLAN.read_bytes()),
            "source": sha(SOURCE.read_bytes()),
            "runner": sha(Path(__file__).read_bytes()),
        },
        "source_markers": source_markers,
        "conditions": {"sizes_wh": SIZES, "film_burn_values": VALUES, "centers": SIDES,
                       "backgrounds": list(BACKGROUNDS), "alpha_associations": ["straight", "premultiplied"]},
        "cases": rows,
        "artifact_sha256": {},
        "limitations": plan["scope"]["excluded"],
    }
    metrics_path = OUT / "metrics.json"
    metrics_path.write_text(json.dumps(metrics, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    for path in sorted(OUT.iterdir()):
        if path.is_file() and path.name not in {"checksums.json", "metrics.json"}:
            metrics["artifact_sha256"][str(path.relative_to(ROOT)).replace("\\", "/")] = sha(path.read_bytes())
    metrics_path.write_text(json.dumps(metrics, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    checksums = {str(path.relative_to(ROOT)).replace("\\", "/"): sha(path.read_bytes())
                 for path in sorted(OUT.iterdir()) if path.is_file() and path.name != "checksums.json"}
    (OUT / "checksums.json").write_text(json.dumps(checksums, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps({"cases": len(rows), "output": str(OUT), "source_markers": source_markers,
                      "max_rgb": max(r["output_range"]["straight"]["max_rgb"] for r in rows)}, ensure_ascii=False))


if __name__ == "__main__":
    main()
