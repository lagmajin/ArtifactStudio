#!/usr/bin/env python3
"""Compare isolated Film Grunge and Film Damage equations in both stack orders."""
from __future__ import annotations

import hashlib
import json
import platform
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
PLAN = ROOT / "experiments/FilmEffectsLab/plans/film-suite-grunge-damage-stack-order-r1.json"
GRUNGE_SOURCE = ROOT / "Artifact/src/Effect/ArtifactCreativeEffects.cpp"
DAMAGE_SOURCE = ROOT / "Artifact/src/Effects/Rasterizer/FilmDamageEffect.cpp"
ORDER_SOURCE = ROOT / "Artifact/src/Engine/DAG/LayerGraphBuilder.cpp"
OUT = ROOT / "experiments/FilmEffectsLab/captures/film-suite-grunge-damage-stack-order-r1"
SIZES = ((512, 256), (1024, 512))
BACKGROUNDS = {"black": np.zeros(3, np.float32),
               "gray18": np.full(3, .18, np.float32),
               "white": np.ones(3, np.float32)}


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def fixture(width: int, height: int) -> tuple[np.ndarray, dict[str, np.ndarray]]:
    yy, xx = np.mgrid[0:height, 0:width].astype(np.float32)
    nx, ny = xx / max(1, width - 1), yy / max(1, height - 1)
    radius = np.sqrt(((nx - .5) / .39) ** 2 + ((ny - .5) / .43) ** 2)
    alpha = np.clip((1.04 - radius) * 18, 0, 1).astype(np.float32)
    rgb = np.empty((height, width, 3), np.float32)
    rgb[:] = (.84, .19, .06)
    rgb[(nx > .28) & (nx < .72)] = (.07, .56, .88)
    patch = (nx > .34) & (nx < .68) & (ny > .34) & (ny < .66)
    checker = ((xx.astype(np.int32) // 4 + yy.astype(np.int32) // 4) % 2) == 0
    rgb[patch & checker] = (.96, .91, .12)
    rgb[patch & ~checker] = (.025, .045, .085)
    rgb[(nx > .47) & (nx < .49)] = .99
    rgb[alpha == 0] = 0
    bgra = np.dstack((rgb[..., ::-1], alpha)).astype(np.float32)
    regions = {
        "transparent": alpha == 0,
        "low_alpha": (alpha > 0) & (alpha <= .1),
        "edge": (alpha > .1) & (alpha < .9),
        "near_opaque": (alpha >= .9) & (alpha < 1),
        "opaque": alpha == 1,
        "all": np.ones((height, width), bool),
    }
    return bgra, regions


def grunge_damage_field(width: int, height: int) -> np.ndarray:
    field = np.zeros((height, width), np.float32)
    yy, xx = np.mgrid[0:height, 0:width].astype(np.float32)
    # Source-shaped 3-pixel scratches with its y/column sinusoidal modulation.
    for normalized_x in (.14, .37, .61, .83, .89):
        column = int(normalized_x * width)
        modulation = .45 + .55 * np.sin(np.arange(height, dtype=np.float32) * .071 + column)
        for offset_x, gain in ((-1, .10), (0, .28), (1, .10)):
            x = column + offset_x
            if 0 <= x < width:
                field[:, x] += gain * modulation
    # Fixed dark dust spots with a linear radial profile, kept pixel-sized.
    for nx, ny, r, opacity in ((.24, .36, 3.2, .20), (.48, .72, 2.6, .16),
                               (.74, .42, 3.8, .23), (.88, .66, 2.2, .12)):
        cx, cy = nx * (width - 1), ny * (height - 1)
        dist = np.sqrt((xx - cx) ** 2 + (yy - cy) ** 2)
        inside = dist < r
        field[inside] -= opacity * (1 - dist[inside] / r)
    return field


def damage_overlay(width: int, height: int) -> np.ndarray:
    yy, xx = np.mgrid[0:height, 0:width].astype(np.float32)
    overlay_rgb = np.zeros((height, width, 3), np.float32)
    overlay_alpha = np.zeros((height, width), np.float32)

    def circle(nx: float, ny: float, radius: float, value: float, opacity: float) -> None:
        cx, cy = nx * (width - 1), ny * (height - 1)
        distance = np.sqrt((xx - cx) ** 2 + (yy - cy) ** 2)
        coverage = np.clip(radius + .5 - distance, 0, 1)
        mask = coverage * opacity
        # Mirrors same-value BGRA cv::Scalar marks; RGB order is symmetric.
        overlay_rgb[:] = overlay_rgb * (1 - mask[..., None]) + value * mask[..., None]
        overlay_alpha[:] = overlay_alpha * (1 - mask) + mask

    def line(x0n: float, y0n: float, x1n: float, y1n: float,
             value: float, opacity: float, width_px: float = 1.0) -> None:
        x0, y0 = x0n * (width - 1), y0n * (height - 1)
        x1, y1 = x1n * (width - 1), y1n * (height - 1)
        vx, vy = x1 - x0, y1 - y0
        denom = max(vx * vx + vy * vy, 1e-12)
        t = np.clip(((xx - x0) * vx + (yy - y0) * vy) / denom, 0, 1)
        distance = np.sqrt((xx - (x0 + t * vx)) ** 2 + (yy - (y0 + t * vy)) ** 2)
        coverage = np.clip(width_px * .5 + .5 - distance, 0, 1)
        mask = coverage * opacity
        overlay_rgb[:] = overlay_rgb * (1 - mask[..., None]) + value * mask[..., None]
        overlay_alpha[:] = overlay_alpha * (1 - mask) + mask

    circle(.19, .56, 3.0, .82, .55)
    circle(.54, .29, 4.0, .03, .42)
    circle(.79, .73, 2.0, .82, .36)
    line(.61, .12, .61, .88, .90, .38)
    line(.89, .12, .89, .88, .90, .38)
    return np.dstack((overlay_rgb[..., ::-1], overlay_alpha)).astype(np.float32)


def apply_grunge(image: np.ndarray, field: np.ndarray) -> np.ndarray:
    result = image.copy()
    result[..., :3] += field[..., None]
    return result


def apply_damage(image: np.ndarray, overlay: np.ndarray) -> np.ndarray:
    result = image.copy()
    alpha = overlay[..., 3:4]
    result[..., :3] = image[..., :3] * (1 - alpha) + overlay[..., :3] * alpha
    return result


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
    a = np.abs(x)
    return {"count": int(x.size), "mae": float(a.mean()),
            "rmse": float(np.sqrt(np.mean(x * x))),
            "p95_abs": float(np.quantile(a, .95)), "max_abs": float(a.max())}


def range_stats(image: np.ndarray, alpha_reference: np.ndarray, association: str) -> dict:
    rgb = image[..., :3].astype(np.float64)
    alpha = image[..., 3:4]
    return {"min_rgb": float(rgb.min()), "max_rgb": float(rgb.max()),
            "channels_lt_0_fraction": float(np.mean(rgb < 0)),
            "channels_gt_1_fraction": float(np.mean(rgb > 1)),
            "premultiplied_rgb_gt_alpha_channel_fraction": (float(np.mean(rgb > alpha))
                                                            if association == "premultiplied" else None),
            "nonfinite_rgb_channels": int(rgb.size - np.isfinite(rgb).sum()),
            "alpha_exact_fixture": bool(np.array_equal(image[..., 3], alpha_reference))}


def save_preview(path: Path, image: np.ndarray, association: str, bg: np.ndarray) -> None:
    rgb = composite(image, association, bg)
    Image.fromarray(np.rint(np.clip(rgb, 0, 1) * 255).astype(np.uint8), "RGB").save(path)


def contact_sheet(paths: list[tuple[str, Path]], target: Path) -> None:
    tile_w, tile_h, label_h = 320, 160, 28
    sheet = Image.new("RGB", (tile_w * len(paths), tile_h + label_h), (28, 29, 32))
    draw = ImageDraw.Draw(sheet)
    for i, (label, path) in enumerate(paths):
        image = Image.open(path).convert("RGB")
        image.thumbnail((tile_w, tile_h), Image.Resampling.LANCZOS)
        sheet.paste(image, (i * tile_w + (tile_w - image.width) // 2, 0))
        draw.text((i * tile_w + 6, tile_h + 6), label, fill=(235, 235, 235))
    sheet.save(target)


def detail_contact_sheet(items: list[tuple[str, Image.Image]], target: Path) -> None:
    tile_w, tile_h, label_h = 256, 256, 28
    sheet = Image.new("RGB", (tile_w * len(items), tile_h + label_h), (28, 29, 32))
    draw = ImageDraw.Draw(sheet)
    for i, (label, crop) in enumerate(items):
        sheet.paste(crop, (i * tile_w, 0))
        draw.text((i * tile_w + 6, tile_h + 6), label, fill=(235, 235, 235))
    sheet.save(target)


def render_crop(image: np.ndarray, association: str, background: np.ndarray,
                center_normalized: tuple[float, float], crop_size: int = 64) -> tuple[Image.Image, list[int]]:
    rgb = composite(image, association, background)
    display = Image.fromarray(np.rint(np.clip(rgb, 0, 1) * 255).astype(np.uint8), "RGB")
    width, height = display.size
    cx = int(round(center_normalized[0] * (width - 1)))
    cy = int(round(center_normalized[1] * (height - 1)))
    x0, y0 = max(0, cx - crop_size // 2), max(0, cy - crop_size // 2)
    x1, y1 = min(width, x0 + crop_size), min(height, y0 + crop_size)
    x0, y0 = max(0, x1 - crop_size), max(0, y1 - crop_size)
    crop = display.crop((x0, y0, x1, y1)).resize((crop_size * 4, crop_size * 4), Image.Resampling.NEAREST)
    return crop, [x0, y0, x1, y1]


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    plan = json.loads(PLAN.read_text(encoding="utf-8"))
    grunge_text = GRUNGE_SOURCE.read_text(encoding="utf-8")
    damage_text = DAMAGE_SOURCE.read_text(encoding="utf-8")
    order_text = ORDER_SOURCE.read_text(encoding="utf-8")
    markers = {
        "grunge_rgb_adds_damage": "+\n                    chromaGrain + damage" in grunge_text,
        "grunge_alpha_copied_from_shift": "destination[offset + 3] = shiftedRow[x][3]" in grunge_text,
        "damage_rgba_overlay_formula": "value * (1.0f - damageAlpha) + damageRow[x][c] * damageAlpha" in damage_text,
        "damage_restores_source_alpha": "outputRow[x][3] = sourceRow[x][3]" in damage_text,
        "both_effects_register_rasterizer": grunge_text.count("setPipelineStage(EffectPipelineStage::Rasterizer)") > 0 and "setPipelineStage(EffectPipelineStage::Rasterizer)" in damage_text,
        "graph_preserves_input_order_within_stage": "stageEffects[index].push_back(effect)" in order_text and "nodes[index - 1]->id()" in order_text,
    }
    cases, previews, detail_sheets = [], [], []
    for width, height in SIZES:
        straight_base, regions = fixture(width, height)
        grunge_field = grunge_damage_field(width, height)
        overlay = damage_overlay(width, height)
        fields = {
            "grunge_marks": np.abs(grunge_field) > 1e-8,
            "damage_marks": overlay[..., 3] > 1e-8,
            "overlap": (np.abs(grunge_field) > 1e-8) & (overlay[..., 3] > 1e-8),
        }
        for association in ("straight", "premultiplied"):
            base = straight_base.copy()
            if association == "premultiplied":
                base[..., :3] *= base[..., 3:4]
            baseline = base.copy()
            grunge_only = apply_grunge(base, grunge_field)
            damage_only = apply_damage(base, overlay)
            grunge_then_damage = apply_damage(grunge_only, overlay)
            damage_then_grunge = apply_grunge(damage_only, grunge_field)
            outputs = {
                "baseline": baseline,
                "grunge_only": grunge_only,
                "damage_only": damage_only,
                "grunge_then_damage": grunge_then_damage,
                "damage_then_grunge": damage_then_grunge,
            }
            order_delta = grunge_then_damage[..., :3] - damage_then_grunge[..., :3]
            row = {
                "width": width, "height": height, "association": association,
                "fixture_sha256": sha(straight_base.tobytes()),
                "grunge_field_sha256": sha(grunge_field.tobytes()),
                "damage_overlay_sha256": sha(overlay.tobytes()),
                "coverage": {name: float(mask.mean()) for name, mask in fields.items()},
                "pixel_counts": {name: int(mask.sum()) for name, mask in fields.items()},
                "outputs": {},
                "regions": {},
                "order_commutator": {},
                "detail_regions": {},
            }
            for name, image in outputs.items():
                row["outputs"][name] = range_stats(image, base[..., 3], association)
            for region_name, mask in {**regions, **fields}.items():
                row["regions"][region_name] = {"pixels": int(mask.sum()), "candidates": {}}
                for name, image in outputs.items():
                    delta = image[..., :3] - baseline[..., :3]
                    bg_delta = {}
                    for bg_name, bg in BACKGROUNDS.items():
                        comp_delta = composite(image, association, bg) - composite(baseline, association, bg)
                        bg_delta[bg_name] = stats(comp_delta[mask])
                    row["regions"][region_name]["candidates"][name] = {
                        "stored_rgb_delta": stats(delta[mask]),
                        "composited_delta": bg_delta,
                    }
                row["order_commutator"][region_name] = {
                    "stored_rgb": stats(order_delta[mask]),
                    "composited": {
                        bg_name: stats((composite(grunge_then_damage, association, bg) -
                                        composite(damage_then_grunge, association, bg))[mask])
                        for bg_name, bg in BACKGROUNDS.items()
                    },
                }
            cases.append(row)
            for roi_name, center in (("overlap", (.61, .50)), ("alpha_edge", (.89, .50))):
                tiles = []
                for name, image in outputs.items():
                    crop, bounds = render_crop(image, association, BACKGROUNDS["gray18"], center)
                    tiles.append((name.replace("_", " "), crop))
                    row["detail_regions"][roi_name] = {
                        "center_normalized": list(center), "crop_bounds_xyxy": bounds,
                        "source_pixels": 64 * 64, "display_pixels": 256 * 256,
                        "background": "gray18", "display_resampling": "nearest",
                    }
                detail_path = OUT / f"detail_{association}_{width}x{height}_{roi_name}.png"
                detail_contact_sheet(tiles, detail_path)
                detail_sheets.append(detail_path)
            if width == 512:
                sample_paths = []
                for name, image in outputs.items():
                    path = OUT / f"{association}_{name}_{width}x{height}.png"
                    save_preview(path, image, association, BACKGROUNDS["gray18"])
                    sample_paths.append((name.replace("_", " "), path))
                sheet_path = OUT / f"contact_{association}_{width}x{height}.png"
                contact_sheet(sample_paths, sheet_path)
                previews.append((association, sheet_path))

    metrics = {
        "study": plan["plan_id"],
        "environment": {"python": platform.python_version(), "numpy": np.__version__, "pillow": Image.__version__},
        "input_sha256": {"plan": sha(PLAN.read_bytes()), "grunge_source": sha(GRUNGE_SOURCE.read_bytes()),
                         "damage_source": sha(DAMAGE_SOURCE.read_bytes()), "order_source": sha(ORDER_SOURCE.read_bytes()),
                         "runner": sha(Path(__file__).read_bytes())},
        "source_markers": markers,
        "conditions": {"sizes_wh": SIZES, "associations": ["straight", "premultiplied"],
                       "backgrounds": list(BACKGROUNDS),
                       "effect_order": ["baseline", "grunge_only", "damage_only", "grunge_then_damage", "damage_then_grunge"]},
        "cases": cases,
        "preview_sheets": [str(path.relative_to(ROOT)).replace("\\", "/") for _, path in previews],
        "detail_sheets": [str(path.relative_to(ROOT)).replace("\\", "/") for path in detail_sheets],
        "limitations": plan["scope"]["excluded"],
    }
    (OUT / "metrics.json").write_text(json.dumps(metrics, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    checksums = {str(path.relative_to(ROOT)).replace("\\", "/"): sha(path.read_bytes())
                 for path in sorted(OUT.rglob("*")) if path.is_file() and path.name != "checksums.json"}
    (OUT / "checksums.json").write_text(json.dumps(checksums, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps({"cases": len(cases), "source_markers": markers,
                      "output": str(OUT), "max_order_rmse": max(c["order_commutator"]["all"]["stored_rgb"]["rmse"] for c in cases)}, ensure_ascii=False))


if __name__ == "__main__":
    main()
