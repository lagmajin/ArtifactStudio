#!/usr/bin/env python3
"""Measure Add Noise mode-specific luma/chroma response offline."""
from __future__ import annotations

import hashlib
import json
import platform
from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
PLAN = ROOT / "experiments/FilmEffectsLab/plans/add-noise-chroma-response-r1.json"
SOURCE = ROOT / "Artifact/src/Effects/AddNoise/AddNoiseEffect.cpp"
INTERFACE = ROOT / "Artifact/include/Effects/AddNoise/AddNoiseEffect.ixx"
OUT = ROOT / "experiments/FilmEffectsLab/captures/add-noise-chroma-response-r1"
PATCHES = [
    ("gray_002", (.02, .02, .02)), ("gray_010", (.10, .10, .10)),
    ("gray_500", (.50, .50, .50)), ("gray_900", (.90, .90, .90)),
    ("gray_980", (.98, .98, .98)), ("red", (.90, .10, .10)),
    ("green", (.10, .90, .10)), ("blue", (.10, .10, .90)),
    ("cyan", (.10, .80, .80)), ("magenta", (.80, .10, .80)),
    ("yellow", (.80, .80, .10)), ("red_boundary", (1.0, .04, .04)),
]
SEEDS = (0, 73, 1977, 4099)
SIZES = (1.5, 3.0)
AMOUNTS = (.1, .4)
MODES = ("color", "mono", "luma")
RESOLUTIONS = ((512, 256), (1024, 512))
F32, U32 = np.float32, np.uint32


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def hash_noise(x: np.ndarray, y: np.ndarray, channel: int, seed: int) -> np.ndarray:
    with np.errstate(over="ignore"):
        value = (x.astype(U32) * U32(73856093) ^ y.astype(U32) * U32(19349663) ^
                 U32(channel) * U32(83492791) ^ U32(seed))
        value ^= value >> U32(16)
        value *= U32(0x7FEB352D)
        value ^= value >> U32(15)
        value *= U32(0x846CA68B)
        value ^= value >> U32(16)
    return ((value & U32(0x00FFFFFF)).astype(F32) / F32(16777215.0) * F32(2.0) - F32(1.0))


def make_chart(width: int, height: int) -> tuple[np.ndarray, dict[str, np.ndarray]]:
    image = np.zeros((height, width, 4), np.float32)
    image[..., 3] = 1.0
    masks = {}
    cols, rows, gutter = 4, 3, 6
    cell_w, cell_h = width // cols, height // rows
    for index, (name, rgb) in enumerate(PATCHES):
        col, row = index % cols, index // cols
        x0, x1 = col * cell_w + gutter, (col + 1) * cell_w - gutter
        y0, y1 = row * cell_h + gutter, (row + 1) * cell_h - gutter
        image[y0:y1, x0:x1, :3] = np.asarray(rgb[::-1], np.float32)
        mask = np.zeros((height, width), bool)
        mask[y0:y1, x0:x1] = True
        masks[name] = mask
    return image, masks


def noise_fields(width: int, height: int, size: float, seed: int) -> dict[str, np.ndarray]:
    xs = np.floor(np.arange(width, dtype=F32) / F32(size)).astype(U32)[None, :]
    ys = np.floor(np.arange(height, dtype=F32) / F32(size)).astype(U32)[:, None]
    return {"r": hash_noise(xs, ys, 1, seed), "g": hash_noise(xs, ys, 2, seed),
            "b": hash_noise(xs, ys, 3, seed)}


def apply(source: np.ndarray, fields: dict[str, np.ndarray], amount: float, mode: str) -> np.ndarray:
    out = source.copy()
    if mode == "color":
        out[..., 0] = np.clip(source[..., 0] + fields["b"] * F32(amount), 0, 1)
        out[..., 1] = np.clip(source[..., 1] + fields["g"] * F32(amount), 0, 1)
        out[..., 2] = np.clip(source[..., 2] + fields["r"] * F32(amount), 0, 1)
    elif mode == "mono":
        n = fields["r"] * F32(amount)
        out[..., :3] = np.clip(source[..., :3] + n[..., None], 0, 1)
    else:
        b, g, r = source[..., 0], source[..., 1], source[..., 2]
        lum = b * F32(.114) + g * F32(.587) + r * F32(.299)
        new_lum = np.clip(lum + fields["r"] * F32(amount), 0, 1)
        diff = new_lum - lum
        out[..., 0] = np.clip(b + diff, 0, 1)
        out[..., 1] = np.clip(g + diff, 0, 1)
        out[..., 2] = np.clip(r + diff, 0, 1)
    return out


def luma(rgb: np.ndarray) -> np.ndarray:
    return rgb[..., 0] * F32(.299) + rgb[..., 1] * F32(.587) + rgb[..., 2] * F32(.114)


def summary(delta: np.ndarray) -> dict:
    d = delta.astype(np.float64).reshape(-1)
    absolute = np.abs(d)
    return {"signed_mean": float(d.mean()), "mae": float(absolute.mean()),
            "rmse": float(np.sqrt(np.mean(d * d))), "p95_abs": float(np.quantile(absolute, .95)),
            "max_abs": float(absolute.max()) if d.size else 0.0}


def circular_delta_deg(a: np.ndarray, b: np.ndarray) -> np.ndarray:
    return (a - b + 180.0) % 360.0 - 180.0


def patch_metrics(source: np.ndarray, output: np.ndarray, mask: np.ndarray,
                  fields: dict[str, np.ndarray], amount: float, mode: str) -> dict:
    src = source[mask, :3][:, ::-1].astype(np.float64)  # logical RGB from BGRA
    dst = output[mask, :3][:, ::-1].astype(np.float64)
    sy, dy = luma(src), luma(dst)
    src_cb, src_cr = src[:, 2] - sy, src[:, 0] - sy  # B-Y, R-Y
    dst_cb, dst_cr = dst[:, 2] - dy, dst[:, 0] - dy
    src_chroma = np.sqrt(src_cb * src_cb + src_cr * src_cr)
    dst_chroma = np.sqrt(dst_cb * dst_cb + dst_cr * dst_cr)
    valid_hue = src_chroma >= .05
    src_hue = np.degrees(np.arctan2(src_cr, src_cb))
    dst_hue = np.degrees(np.arctan2(dst_cr, dst_cb))
    hue_delta = circular_delta_deg(dst_hue[valid_hue], src_hue[valid_hue])
    saturation_src = src.max(axis=1) - src.min(axis=1)
    saturation_dst = dst.max(axis=1) - dst.min(axis=1)
    luma_delta = dy - sy
    source_bgra = source[mask]
    output_bgra = output[mask]
    channel = output_bgra[:, :3].astype(np.float64)
    result = {
        "pixel_count": int(mask.sum()),
        "input_rgb": {"mean": src.mean(axis=0).tolist(), "std": src.std(axis=0).tolist()},
        "output_rgb_mean": dst.mean(axis=0).tolist(),
        "luma_delta": summary(luma_delta),
        "chroma_magnitude_delta": summary(dst_chroma - src_chroma),
        "saturation_proxy_delta": summary(saturation_dst - saturation_src),
        "hue_abs_delta_degrees": {"valid_pixel_count": int(valid_hue.sum()),
                                   "mean_abs": float(np.mean(np.abs(hue_delta))) if hue_delta.size else None,
                                   "p95_abs": float(np.quantile(np.abs(hue_delta), .95)) if hue_delta.size else None},
        "rgb_exact_bound_fraction": float(np.mean((channel == 0) | (channel == 1))),
        "rgb_within_1e_6_bound_fraction": float(np.mean((channel <= 1e-6) | (channel >= 1 - 1e-6))),
        "alpha_max_abs_delta": float(np.max(np.abs(output_bgra[:, 3] - source_bgra[:, 3]))),
    }
    if mode == "luma":
        lum = luma(src)
        n = fields["r"][mask].astype(np.float64) * amount
        result["luma_intermediate_lower_clamp_fraction"] = float(np.mean(lum + n <= 0))
        result["luma_intermediate_upper_clamp_fraction"] = float(np.mean(lum + n >= 1))
    else:
        result["luma_intermediate_lower_clamp_fraction"] = None
        result["luma_intermediate_upper_clamp_fraction"] = None
    return result


def save_png(path: Path, bgra: np.ndarray) -> None:
    rgb8 = np.rint(np.clip(bgra[..., :3][..., ::-1], 0, 1) * 255).astype(np.uint8)
    cv2.imwrite(str(path), rgb8[..., ::-1])


def contact_sheet(items: list[tuple[str, Path]], output: Path) -> None:
    tiles = []
    for label, path in items:
        image = Image.open(path).convert("RGB")
        image.thumbnail((384, 192))
        tile = Image.new("RGB", (400, 220), (34, 34, 34))
        tile.paste(image, ((400 - image.width) // 2, 22))
        ImageDraw.Draw(tile).text((8, 4), label, fill=(245, 245, 245))
        tiles.append(tile)
    columns = 3
    sheet = Image.new("RGB", (columns * 400, ((len(tiles) + columns - 1) // columns) * 220), (24, 24, 24))
    for i, tile in enumerate(tiles):
        sheet.paste(tile, ((i % columns) * 400, (i // columns) * 220))
    sheet.save(output)


def main() -> dict:
    OUT.mkdir(parents=True, exist_ok=True)
    src_text = SOURCE.read_text(encoding="utf-8")
    intf_text = INTERFACE.read_text(encoding="utf-8")
    markers = {
        "numeric_seed": "seed = static_cast<uint32_t>(seed_)" in src_text,
        "hash_coordinate_salts": all(token in src_text for token in ("73856093u", "19349663u", "83492791u")),
        "color_mode_salts": all(token in src_text for token in ("noiseAt(ux, uy, 3u)", "noiseAt(ux, uy, 2u)", "noiseAt(ux, uy, 1u)")),
        "luma_weights": "row[x][0] * 0.114f" in src_text and "row[x][1] * 0.587f" in src_text and "row[x][2] * 0.299f" in src_text,
        "luma_common_delta": "float diff = newLuma - luma;" in src_text,
        "rgb_clamp": "std::clamp(p[0] + diff, 0.0f, 1.0f)" in src_text,
        "alpha_passthrough": "dst = src;" in src_text,
        "size_lod_scaled": "node.resolutionScaledParameterMask = (1u << 1)" in intf_text,
    }
    if not all(markers.values()):
        raise RuntimeError(f"Source marker mismatch: {markers}")

    report = {"plan_id": "add-noise-chroma-response-r1", "plan_sha256": sha(PLAN),
              "script_sha256": sha(Path(__file__)), "source_sha256": sha(SOURCE),
              "interface_sha256": sha(INTERFACE), "python": platform.python_version(),
              "numpy": np.__version__, "opencv": cv2.__version__, "patches": {n: list(c) for n, c in PATCHES},
              "parameters": {"seeds": list(SEEDS), "sizes": list(SIZES), "amounts": list(AMOUNTS),
                             "modes": list(MODES), "resolutions": [list(r) for r in RESOLUTIONS]},
              "metrics_space": "Direct linear numeric RGB; Rec.601 weights per Add Noise equation; Cb=B-Y and Cr=R-Y; hue omitted if source opponent chroma magnitude <.05.",
              "source_markers_verified": markers, "dimensions": {},
              "limits": "Synthetic opaque patch source-equation model only; no transfer/display transform, actual app output, GPU dispatch, alpha edge, perceptual approval, or performance."}
    previews = []
    baseline_checks = []
    for width, height in RESOLUTIONS:
        chart, masks = make_chart(width, height)
        dim = {"patch_pixel_counts": {k: int(v.sum()) for k, v in masks.items()}, "cases": {}}
        # The amount-zero baseline must be a bit-exact identity on this bounded fixture.
        fields0 = noise_fields(width, height, 1.5, 73)
        for mode in MODES:
            baseline = apply(chart, fields0, 0.0, mode)
            baseline_checks.append({"resolution": f"{width}x{height}", "mode": mode,
                                    "exact_input_match": bool(np.array_equal(chart, baseline)),
                                    "alpha_max_abs_delta": float(np.max(np.abs(chart[..., 3] - baseline[..., 3])))})
        for size in SIZES:
            for seed in SEEDS:
                field = noise_fields(width, height, size, seed)
                for amount in AMOUNTS:
                    for mode in MODES:
                        out = apply(chart, field, amount, mode)
                        key = f"size{size}|seed{seed}|amount{amount}|{mode}"
                        dim["cases"][key] = {name: patch_metrics(chart, out, mask, field, amount, mode)
                                             for name, mask in masks.items()}
                        if seed == 73 and size == 1.5:
                            path = OUT / f"{mode}-seed73-size1.5-amount{amount}-{width}x{height}.png"
                            save_png(path, out)
                            previews.append((f"{width}x{height} {mode} amount {amount}", path))
        input_path = OUT / f"input-{width}x{height}.png"
        save_png(input_path, chart)
        previews.append((f"{width}x{height} input", input_path))
        report["dimensions"][f"{width}x{height}"] = dim
    report["baseline_checks"] = baseline_checks
    contact_sheet(previews, OUT / "contact-sheet.png")
    report["preview_sha256"] = {p.name: sha(p) for _, p in previews}
    report["preview_sha256"]["contact-sheet.png"] = sha(OUT / "contact-sheet.png")
    metrics = OUT / "metrics.json"
    metrics.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    (OUT / "metrics.sha256").write_text(f"{sha(metrics)}  metrics.json\n", encoding="ascii", newline="\n")
    print(json.dumps({"metrics": str(metrics), "plan_sha256": report["plan_sha256"],
                      "script_sha256": report["script_sha256"], "markers": markers,
                      "baseline_exact": all(x["exact_input_match"] for x in baseline_checks),
                      "preview_count": len(previews) + 1}, indent=2))
    return report


if __name__ == "__main__":
    main()
