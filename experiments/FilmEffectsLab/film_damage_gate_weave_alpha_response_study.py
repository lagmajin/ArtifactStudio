#!/usr/bin/env python3
"""Characterize Film Damage fixed gate-weave shifts at alpha edges offline."""
from __future__ import annotations

import hashlib
import json
import platform
from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
PLAN = ROOT / "experiments/FilmEffectsLab/plans/film-damage-gate-weave-alpha-response-r1.json"
SOURCE = ROOT / "Artifact/src/Effects/Rasterizer/FilmDamageEffect.cpp"
OUT = ROOT / "experiments/FilmEffectsLab/captures/film-damage-gate-weave-alpha-response-r1"
RESOLUTIONS = ((512, 256), (1024, 512))
SHIFTS = ((0.0, 0.0), (1.5, 0.0), (-1.5, 0.0), (0.0, 0.675),
          (0.0, -0.675), (1.5, 0.675), (-1.5, -0.675), (3.0, 1.35), (-3.0, -1.35))
BGS = {"black": np.zeros(3, np.float32), "gray": np.full(3, .18, np.float32),
       "white": np.ones(3, np.float32)}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fixture(width: int, height: int) -> tuple[np.ndarray, np.ndarray, dict[str, np.ndarray], np.ndarray]:
    yy, xx = np.mgrid[0:height, 0:width].astype(np.float32)
    nx, ny = xx / (width - 1), yy / (height - 1)
    radius = np.sqrt(((xx - width * .5) / (width * .37)) ** 2 +
                     ((yy - height * .5) / (height * .40)) ** 2)
    alpha = np.clip((1.04 - radius) * 18.0, 0, 1).astype(np.float32)
    rgb = np.empty((height, width, 3), np.float32)
    rgb[:] = (.88, .22, .06)  # warm foreground over a transparent exterior
    rgb[(nx > .30) & (nx < .70)] = (.08, .72, .94)
    checker = ((xx.astype(np.int32) // 4 + yy.astype(np.int32) // 4) % 2) == 0
    detail_region = (nx > .40) & (nx < .72) & (ny > .36) & (ny < .64)
    rgb[detail_region & checker] = (.98, .96, .12)
    rgb[detail_region & ~checker] = (.05, .08, .12)
    for col in range(width // 4, width * 3 // 4, 19):
        rgb[:, col:col + 1] = 1.0
    rgb[alpha == 0] = 0.0
    straight = np.dstack((rgb[..., ::-1], alpha)).astype(np.float32)
    premul = straight.copy()
    premul[..., :3] *= alpha[..., None]
    regions = {
        "transparent": alpha == 0,
        "low_alpha": (alpha > 0) & (alpha <= .1),
        "edge": (alpha > .1) & (alpha < .9),
        "near_opaque": (alpha >= .9) & (alpha < 1),
        "opaque": alpha == 1,
        "all": np.ones(alpha.shape, bool),
    }
    detail = detail_region & (alpha >= .98)
    return straight, premul, regions, detail


def warp_and_restore_alpha(source: np.ndarray, dx: float, dy: float) -> np.ndarray:
    h, w = source.shape[:2]
    matrix = np.asarray(((1.0, 0.0, dx), (0.0, 1.0, dy)), np.float64)
    warped = cv2.warpAffine(source, matrix, (w, h), flags=cv2.INTER_LINEAR,
                            borderMode=cv2.BORDER_REFLECT101)
    warped[..., 3] = source[..., 3]
    return warped


def composite(bgra: np.ndarray, association: str, bg: np.ndarray) -> np.ndarray:
    rgb = bgra[..., :3][..., ::-1]
    alpha = bgra[..., 3:4]
    if association == "straight":
        return rgb * alpha + bg * (1 - alpha)
    return rgb + bg * (1 - alpha)


def stats(delta: np.ndarray) -> dict:
    x = np.abs(delta.astype(np.float64)).reshape(-1)
    return {"mae": float(x.mean()), "rmse": float(np.sqrt(np.mean(x * x))),
            "p95_abs": float(np.quantile(x, .95)), "max_abs": float(x.max()) if x.size else 0.0}


def save_preview(path: Path, rgb: np.ndarray) -> None:
    bgr8 = np.rint(np.clip(rgb[..., ::-1], 0, 1) * 255).astype(np.uint8)
    cv2.imwrite(str(path), bgr8)


def contact_sheet(items: list[tuple[str, Path]], path: Path) -> None:
    tiles = []
    for label, source in items:
        image = Image.open(source).convert("RGB")
        image.thumbnail((384, 192))
        tile = Image.new("RGB", (400, 220), (34, 34, 34))
        tile.paste(image, ((400 - image.width) // 2, 22))
        ImageDraw.Draw(tile).text((8, 4), label, fill=(245, 245, 245))
        tiles.append(tile)
    cols = 4
    sheet = Image.new("RGB", (cols * 400, ((len(tiles) + cols - 1) // cols) * 220), (24, 24, 24))
    for i, tile in enumerate(tiles):
        sheet.paste(tile, ((i % cols) * 400, (i // cols) * 220))
    sheet.save(path)


def main() -> dict:
    OUT.mkdir(parents=True, exist_ok=True)
    source_text = SOURCE.read_text(encoding="utf-8")
    markers = {
        "source_affine_translation": "1.0, 0.0, weaveX, 0.0, 1.0, weaveY" in source_text,
        "linear_border_reflect": "cv::INTER_LINEAR" in source_text and "cv::BORDER_REFLECT101" in source_text,
        "original_alpha_restore": "outputRow[x][3] = sourceRow[x][3]" in source_text,
        "working_rgb_source": "const auto* sourceRow = working.ptr<cv::Vec4f>(y)" in source_text,
        "default_gate_weave": "float gateWeave = 1.5f" in source_text,
    }
    if not all(markers.values()):
        raise RuntimeError(f"Source markers changed: {markers}")

    report = {"plan_id": "film-damage-gate-weave-alpha-response-r1", "plan_sha256": sha(PLAN),
              "script_sha256": sha(Path(__file__)), "source_sha256": sha(SOURCE),
              "python": platform.python_version(), "numpy": np.__version__, "opencv": cv2.__version__,
              "source_markers_verified": markers, "displacements_px": [list(v) for v in SHIFTS],
              "regions": {}, "dimensions": {}, "preview_sha256": {},
              "limits": "Fixed-displacement OpenCV equation model only. Shifts are diagnostics, not std::mt19937 draws or captured app output."}
    previews = []
    selected = {(0.0, 0.0), (1.5, .675), (-1.5, -.675), (3.0, 1.35)}

    for width, height in RESOLUTIONS:
        straight, premul, regions, detail = fixture(width, height)
        alpha = straight[..., 3]
        dim = {"region_pixel_counts": {k: int(v.sum()) for k, v in regions.items()},
               "detail_pixel_count": int(detail.sum()), "association_cases": {}}
        report["regions"][f"{width}x{height}"] = dim["region_pixel_counts"]
        for association, src in (("straight", straight), ("premultiplied", premul)):
            base_composites = {bg: composite(src, association, color) for bg, color in BGS.items()}
            cases = {}
            for dx, dy in SHIFTS:
                candidate = warp_and_restore_alpha(src, dx, dy)
                key = f"dx{dx}_dy{dy}"
                record = {"dx_px": dx, "dy_px": dy,
                          "normalized_dx": dx / width, "normalized_dy": dy / height,
                          "alpha_max_abs_delta": float(np.max(np.abs(candidate[..., 3] - alpha))),
                          "finite_output": bool(np.isfinite(candidate).all()),
                          "premul_rgb_gt_alpha_fraction": (float(np.mean(candidate[..., :3] > candidate[..., 3:4] + 1e-6)) if association == "premultiplied" else None),
                          "transparent_rgb_mean_abs": (float(np.mean(np.abs(candidate[regions['transparent'], :3]))) if regions['transparent'].any() else 0.0),
                          "zero_shift_rgb_max_abs_from_input": (float(np.max(np.abs(candidate[..., :3] - src[..., :3]))) if (dx, dy) == (0.0, 0.0) else None),
                          "region_composite_delta": {}, "changed_pixel_fraction_by_background": {}}
                for region_name, mask in regions.items():
                    record["region_composite_delta"][region_name] = {}
                    for bg, color in BGS.items():
                        output = composite(candidate, association, color)
                        delta = output[mask] - base_composites[bg][mask]
                        record["region_composite_delta"][region_name][bg] = stats(delta)
                        record["changed_pixel_fraction_by_background"].setdefault(bg, {})[region_name] = float(np.mean(np.any(np.abs(delta) > 1e-6, axis=1)))
                if detail.any():
                    rgb = candidate[detail, :3][:, ::-1].astype(np.float64)
                    src_rgb = src[detail, :3][:, ::-1].astype(np.float64)
                    y = rgb @ np.asarray((.299, .587, .114))
                    y0 = src_rgb @ np.asarray((.299, .587, .114))
                    dy_idx, dx_idx = np.where(detail)
                    x0, x1 = int(dx_idx.min()), int(dx_idx.max()) + 1
                    y0_idx, y1_idx = int(dy_idx.min()), int(dy_idx.max()) + 1
                    out_crop = candidate[y0_idx:y1_idx, x0:x1, :3][:, :, ::-1].astype(np.float64)
                    src_crop = src[y0_idx:y1_idx, x0:x1, :3][:, :, ::-1].astype(np.float64)
                    out_y_crop = out_crop @ np.asarray((.299, .587, .114))
                    src_y_crop = src_crop @ np.asarray((.299, .587, .114))
                    grad_out = .5 * (float(np.mean(np.diff(out_y_crop, axis=0) ** 2)) + float(np.mean(np.diff(out_y_crop, axis=1) ** 2)))
                    grad_src = .5 * (float(np.mean(np.diff(src_y_crop, axis=0) ** 2)) + float(np.mean(np.diff(src_y_crop, axis=1) ** 2)))
                    record["detail_contrast"] = {"source_y_std": float(y0.std()),
                                                  "output_y_std": float(y.std()),
                                                  "std_ratio": float(y.std() / y0.std()) if y0.std() else None,
                                                  "gradient_energy_ratio": grad_out / grad_src if grad_src else None}
                else:
                    record["detail_contrast"] = None
                cases[key] = record

                if (dx, dy) in selected:
                    gray = composite(candidate, association, BGS["gray"])
                    preview = OUT / f"{association}-dx{dx}-dy{dy}-{width}x{height}.png"
                    save_preview(preview, gray)
                    previews.append((f"{association} {width}x{height} dx{dx} dy{dy}", preview))
            dim["association_cases"][association] = cases
            input_preview = OUT / f"{association}-input-{width}x{height}.png"
            save_preview(input_preview, base_composites["gray"])
            previews.append((f"{association} input {width}x{height}", input_preview))
        # The 512 and 1024 pixel translations with doubled offsets have equal normalized shifts.
        dim["normalized_shift_pair"] = {"512x256_dx1.5_dy.675": [1.5 / 512, .675 / 256],
                                        "1024x512_dx3_dy1.35": [3 / 1024, 1.35 / 512]}
        report["dimensions"][f"{width}x{height}"] = dim

    contact_sheet(previews, OUT / "contact-sheet.png")
    report["preview_sha256"] = {p.name: sha(p) for _, p in previews}
    report["preview_sha256"]["contact-sheet.png"] = sha(OUT / "contact-sheet.png")
    metrics = OUT / "metrics.json"
    metrics.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    (OUT / "metrics.sha256").write_text(f"{sha(metrics)}  metrics.json\n", encoding="ascii", newline="\n")
    print(json.dumps({"metrics": str(metrics), "plan_sha256": report["plan_sha256"],
                      "script_sha256": report["script_sha256"], "markers": markers,
                      "preview_count": len(previews) + 1}, indent=2))
    return report


if __name__ == "__main__":
    main()
