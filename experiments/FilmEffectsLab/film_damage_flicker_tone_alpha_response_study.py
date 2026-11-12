#!/usr/bin/env python3
"""Characterize isolated Film Damage flicker gain on a tone/alpha chart."""
from __future__ import annotations

import hashlib
import json
import platform
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
PLAN = ROOT / "experiments/FilmEffectsLab/plans/film-damage-flicker-tone-alpha-response-r1.json"
SOURCE = ROOT / "Artifact/src/Effects/Rasterizer/FilmDamageEffect.cpp"
OUT = ROOT / "experiments/FilmEffectsLab/captures/film-damage-flicker-tone-alpha-response-r1"
WIDTH, HEIGHT = 512, 256
FLICKERS = (0.0, 0.08, 0.25, 0.5, 1.0)
Z_VALUES = (-3.0, -2.0, -1.0, 0.0, 1.0, 2.0, 3.0)
BACKGROUNDS = {"black": np.zeros(3, np.float32),
               "gray18": np.full(3, .18, np.float32),
               "white": np.ones(3, np.float32)}
PATCHES = (
    ("black", (0.01, 0.01, 0.01)),
    ("shadow", (0.08, 0.08, 0.08)),
    ("gray18", (0.18, 0.18, 0.18)),
    ("midtone", (0.5, 0.5, 0.5)),
    ("high", (0.8, 0.8, 0.8)),
    ("near_white", (0.98, 0.98, 0.98)),
    ("red", (0.98, 0.03, 0.02)),
    ("green", (0.03, 0.92, 0.05)),
    ("blue", (0.04, 0.08, 0.98)),
    ("cyan", (0.02, 0.9, 0.9)),
    ("magenta", (0.9, 0.02, 0.9)),
    ("yellow", (0.92, 0.9, 0.03)),
)


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def fixture() -> tuple[np.ndarray, dict[str, np.ndarray], dict[str, np.ndarray]]:
    yy, xx = np.mgrid[0:HEIGHT, 0:WIDTH].astype(np.float32)
    nx, ny = xx / (WIDTH - 1), yy / (HEIGHT - 1)
    radial = np.sqrt(((nx - .5) / .39) ** 2 + ((ny - .5) / .43) ** 2)
    alpha = np.clip((1.04 - radial) * 18, 0, 1).astype(np.float32)
    rgb = np.zeros((HEIGHT, WIDTH, 3), np.float32)
    patch_regions: dict[str, np.ndarray] = {}
    x_edges = np.linspace(.11, .89, len(PATCHES) + 1)
    ymask = (ny >= .25) & (ny <= .75)
    for index, (name, color) in enumerate(PATCHES):
        mask = (nx >= x_edges[index]) & (nx < x_edges[index + 1]) & ymask
        rgb[mask] = color
        patch_regions[name] = mask & (alpha >= .99)
    # Fine black/white detail lines and checker sit across the saturated patch group.
    detail = (nx > .51) & (nx < .96) & (ny > .34) & (ny < .66)
    checker = ((xx.astype(np.int32) // 4 + yy.astype(np.int32) // 4) % 2) == 0
    rgb[detail & checker] = .98
    rgb[detail & ~checker] = .015
    for x in (WIDTH // 3, WIDTH // 2, WIDTH * 2 // 3):
        rgb[:, x:x + 1] = 1.0
    rgb[alpha == 0] = 0
    bgra = np.dstack((rgb[..., ::-1], alpha)).astype(np.float32)
    source_luma = .2126 * rgb[..., 0] + .7152 * rgb[..., 1] + .0722 * rgb[..., 2]
    regions = {
        "all": np.ones(alpha.shape, bool),
        "transparent": alpha == 0,
        "low_alpha": (alpha > 0) & (alpha <= .1),
        "edge": (alpha > .1) & (alpha < .9),
        "near_opaque": (alpha >= .9) & (alpha < 1),
        "opaque": alpha == 1,
        "shadow": (source_luma <= .1) & (alpha >= .99),
        "midtone": (source_luma > .1) & (source_luma < .8) & (alpha >= .99),
        "highlight": (source_luma >= .8) & (alpha >= .99),
    }
    return bgra, regions, patch_regions


def apply_gain(source: np.ndarray, gain: float) -> np.ndarray:
    output = source.copy()
    output[..., :3] *= np.float32(gain)
    return output


def composite(image: np.ndarray, association: str, background: np.ndarray) -> np.ndarray:
    rgb = image[..., :3][..., ::-1]
    alpha = image[..., 3:4]
    if association == "straight":
        return rgb * alpha + background * (1 - alpha)
    return rgb + background * (1 - alpha)


def stats(values: np.ndarray) -> dict:
    x = np.asarray(values, np.float64).reshape(-1)
    if x.size == 0:
        return {"count": 0, "mae": 0.0, "rmse": 0.0, "p95_abs": 0.0, "max_abs": 0.0}
    absolute = np.abs(x)
    return {"count": int(x.size), "mae": float(absolute.mean()),
            "rmse": float(np.sqrt(np.mean(x * x))),
            "p95_abs": float(np.quantile(absolute, .95)), "max_abs": float(absolute.max())}


def range_stats(output: np.ndarray, alpha_reference: np.ndarray, gain: float, association: str) -> dict:
    rgb = output[..., :3].astype(np.float64)
    alpha = output[..., 3:4]
    return {"gain": float(gain), "gain_sign": "negative" if gain < 0 else "zero" if gain == 0 else "positive",
            "min_rgb": float(rgb.min()), "max_rgb": float(rgb.max()),
            "rgb_channels_lt_0_fraction": float(np.mean(rgb < 0)),
            "rgb_channels_gt_1_fraction": float(np.mean(rgb > 1)),
            "premultiplied_rgb_gt_alpha_channel_fraction": (float(np.mean(rgb > alpha))
                                                            if association == "premultiplied" else None),
            "nonfinite_rgb_channels": int(rgb.size - np.isfinite(rgb).sum()),
            "alpha_exact_fixture": bool(np.array_equal(output[..., 3], alpha_reference))}


def display_rgb(image: np.ndarray, association: str) -> Image.Image:
    composite_rgb = composite(image, association, BACKGROUNDS["gray18"])
    return Image.fromarray(np.rint(np.clip(composite_rgb, 0, 1) * 255).astype(np.uint8), "RGB")


def make_contact_sheet(association: str, items: list[tuple[str, Image.Image]], path: Path) -> None:
    cols, tile_w, tile_h, label_h = len(Z_VALUES), 128, 64, 23
    rows = len(FLICKERS)
    sheet = Image.new("RGB", (cols * tile_w, rows * (tile_h + label_h)), (28, 29, 32))
    draw = ImageDraw.Draw(sheet)
    for index, (label, image) in enumerate(items):
        image = image.resize((tile_w, tile_h), Image.Resampling.BILINEAR)
        x, y = (index % cols) * tile_w, (index // cols) * (tile_h + label_h)
        sheet.paste(image, (x, y))
        draw.text((x + 3, y + tile_h + 4), label, fill=(235, 235, 235))
    sheet.save(path)


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    plan = json.loads(PLAN.read_text(encoding="utf-8"))
    source_text = SOURCE.read_text(encoding="utf-8")
    markers = {
        "flicker_gain_formula": "const float flickerGain = 1.0f + normal(rng) * flicker;" in source_text,
        "gain_applied_to_rgb": "float value = sourceRow[x][c] * flickerGain + grainNoise;" in source_text,
        "damage_overlay_follows_gain": "value = value * (1.0f - damageAlpha) + damageRow[x][c] * damageAlpha;" in source_text,
        "burn_addition_follows_gain": "if (c == 2) value += burn * 1.35f;" in source_text,
        "alpha_restored_from_source": "outputRow[x][3] = sourceRow[x][3]" in source_text,
        "flicker_control_clamped_0_1": "std::clamp(v, 0.0f, 1.0f) : 0.08f" in source_text,
    }
    source, regions, patches = fixture()
    cases, sheets = [], {}
    for association in ("straight", "premultiplied"):
        associated_source = source.copy()
        if association == "premultiplied":
            associated_source[..., :3] *= associated_source[..., 3:4]
        association_baseline = associated_source.copy()
        sheet_items = []
        for flicker in FLICKERS:
            for z in Z_VALUES:
                gain = np.float32(np.float32(1.0) +
                                  np.float32(np.float32(z) * np.float32(flicker)))
                output = apply_gain(associated_source, gain)
                row = {
                    "association": association, "flicker": flicker, "z_diagnostic": z,
                    "source_sha256": sha(associated_source.tobytes()),
                    "range": range_stats(output, associated_source[..., 3], float(gain), association),
                    "regions": {}, "patches": {},
                }
                for name, mask in regions.items():
                    row["regions"][name] = {"pixels": int(mask.sum()), "stored_rgb_delta": stats(output[..., :3][mask] - association_baseline[..., :3][mask]),
                                             "composite_delta": {bg_name: stats((composite(output, association, bg) -
                                                                                 composite(association_baseline, association, bg))[mask])
                                                                  for bg_name, bg in BACKGROUNDS.items()}}
                for name, mask in patches.items():
                    rgb_out = output[..., :3][..., ::-1][mask]
                    rgb_src = associated_source[..., :3][..., ::-1][mask]
                    luma_out = rgb_out @ np.asarray((.2126, .7152, .0722), np.float32)
                    luma_src = rgb_src @ np.asarray((.2126, .7152, .0722), np.float32)
                    # Opponent components use numeric RGB values directly; they are not perceptual color-difference units.
                    cr_out, cb_out = rgb_out[:, 0] - luma_out, rgb_out[:, 2] - luma_out
                    cr_src, cb_src = rgb_src[:, 0] - luma_src, rgb_src[:, 2] - luma_src
                    row["patches"][name] = {
                        "pixels": int(mask.sum()),
                        "source_luma_mean": float(luma_src.mean()) if mask.any() else 0.0,
                        "output_luma_mean": float(luma_out.mean()) if mask.any() else 0.0,
                        "luma_delta_mean": float((luma_out - luma_src).mean()) if mask.any() else 0.0,
                        "cr_delta_mean": float((cr_out - cr_src).mean()) if mask.any() else 0.0,
                        "cb_delta_mean": float((cb_out - cb_src).mean()) if mask.any() else 0.0,
                    }
                cases.append(row)
                sheet_items.append((f"f={flicker:g}, z={z:+g}", display_rgb(output, association)))
        sheet_path = OUT / f"contact_{association}.png"
        make_contact_sheet(association, sheet_items, sheet_path)
        sheets[association] = str(sheet_path.relative_to(ROOT)).replace("\\", "/")

    metrics = {
        "study": plan["plan_id"],
        "environment": {"python": platform.python_version(), "numpy": np.__version__, "pillow": Image.__version__},
        "input_sha256": {"plan": sha(PLAN.read_bytes()), "source": sha(SOURCE.read_bytes()),
                         "runner": sha(Path(__file__).read_bytes()), "straight_fixture": sha(source.tobytes())},
        "source_markers": markers,
        "conditions": {"resolution_wh": [WIDTH, HEIGHT], "flickers": FLICKERS, "z_diagnostic_values": Z_VALUES,
                       "associations": ["straight", "premultiplied"], "backgrounds": list(BACKGROUNDS),
                       "patches": [name for name, _ in PATCHES]},
        "patch_rois": {name: {"x_fraction": [float(x / WIDTH) for x in np.where(mask.any(axis=0))[0][[0, -1]]],
                               "y_fraction": [float(y / HEIGHT) for y in np.where(mask.any(axis=1))[0][[0, -1]]],
                               "opaque_pixel_count": int(mask.sum())}
                       for name, mask in patches.items()},
        "contact_sheets": sheets,
        "cases": cases,
        "limitations": plan["scope"]["excluded"],
    }
    metrics_path = OUT / "metrics.json"
    metrics_path.write_text(json.dumps(metrics, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    checksums = {str(path.relative_to(ROOT)).replace("\\", "/"): sha(path.read_bytes())
                 for path in sorted(OUT.rglob("*")) if path.is_file() and path.name != "checksums.json"}
    (OUT / "checksums.json").write_text(json.dumps(checksums, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    negative = [c for c in cases if c["range"]["gain"] < 0]
    print(json.dumps({"cases": len(cases), "source_markers": markers,
                      "negative_gain_cases": len(negative), "max_rgb": max(c["range"]["max_rgb"] for c in cases),
                      "output": str(OUT)}, ensure_ascii=False))


if __name__ == "__main__":
    main()
