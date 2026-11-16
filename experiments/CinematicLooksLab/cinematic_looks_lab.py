#!/usr/bin/env python3
"""Deterministic image fixtures and candidate-look response study.

This is a hand-transcribed reference approximation, not Artifact's LUT runtime.
See EVALUATION_PLAN.md and README.md for scope and limitations.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import platform
import time
from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[1]
ASSETS = REPO / "experiments" / "GlowLab" / "assets"
DEFAULT_OUT = ROOT / "captures" / "rd0"
SEED = 20261116
SIZE = (1536, 1024)
LOOKS = ("baseline", "cinematic", "vintage", "cold", "warm", "high_contrast",
         "low_contrast", "desaturated", "kodak2383", "fuji3510")

LOOK_SOURCE = REPO / "ArtifactCore" / "src" / "Color" / "ColorLUT.cpp"
LOOK_SOURCE_IDS = {
    "cinematic": "cinematic", "vintage": "vintage", "cold": "cold", "warm": "warm",
    "high_contrast": "highContrast", "low_contrast": "lowContrast",
    "desaturated": "desaturated", "kodak2383": "kodak2383", "fuji3510": "fuji3510",
}

def look_source_fingerprint() -> str:
    return sha256(LOOK_SOURCE)

def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()

def write_json_lf(path: Path, value: object) -> None:
    with path.open("w", encoding="utf-8", newline="\n") as stream:
        json.dump(value, stream, indent=2, ensure_ascii=False)
        stream.write("\n")

def chart() -> tuple[np.ndarray, dict[str, list[int]]]:
    """Create chart in RGB uint8 with deterministic ROIs."""
    w, h = SIZE
    canvas = Image.new("RGB", SIZE, (24, 26, 30))
    draw = ImageDraw.Draw(canvas)
    rois: dict[str, list[int]] = {}
    panels = [("gray_ramp", (32, 32, 496, 200)),
              ("skin_patches", (528, 32, 992, 200)),
              ("color_patches", (1024, 32, 1504, 200)),
              ("fine_lines", (32, 232, 496, 448)),
              ("step_edges", (528, 232, 992, 448)),
              ("dark_highlight", (1024, 232, 1504, 448)),
              ("smooth_gradient", (32, 480, 736, 960)),
              ("color_gradient", (768, 480, 1504, 960))]
    for name, box in panels:
        x0, y0, x1, y1 = box
        rois[name] = [x0, y0, x1-x0, y1-y0]
        draw.rectangle(box, fill=(18, 20, 24), outline=(180, 184, 190), width=1)
    for x in range(40, 488):
        v = round((x - 40) * 255 / 447)
        draw.line((x, 50, x, 182), fill=(v, v, v))
    skin = [(90, 42, 32), (132, 76, 58), (170, 112, 88), (205, 154, 126),
            (232, 190, 162), (118, 66, 48), (186, 126, 94), (220, 168, 136)]
    for i, col in enumerate(skin):
        x = 540 + i * 55
        draw.rectangle((x, 54, x+43, 178), fill=col)
    colors = [(220, 30, 35), (20, 190, 65), (35, 75, 220), (220, 190, 25),
              (30, 200, 205), (195, 35, 195), (235, 235, 235), (40, 40, 40)]
    for i, col in enumerate(colors):
        x = 1038 + i * 57
        draw.rectangle((x, 54, x+44, 178), fill=col)
    for i in range(24):
        y = 250 + i * 8
        draw.line((40, y, 488, y), fill=(235, 235, 235) if i % 2 == 0 else (20, 20, 20), width=2)
    for x in range(540, 980):
        draw.line((x, 250, x, 430), fill=(245, 245, 245) if x < 760 else (18, 18, 18))
    for x in range(1036, 1490):
        val = 3 if x < 1250 else min(255, 120 + (x-1250))
        draw.line((x, 250, x, 430), fill=(val, val, val))
    arr = np.asarray(canvas).copy()
    yy, xx = np.mgrid[0:h, 0:w]
    mask = np.zeros((h, w), dtype=bool)
    mask[480:960, 32:736] = True
    grad = (20 + 215 * (xx[mask] - 32) / 703).astype(np.uint8)
    arr[mask] = np.stack((grad, grad, grad), axis=-1)
    mask2 = np.zeros((h, w), dtype=bool)
    mask2[480:960, 768:1504] = True
    u = ((xx[mask2] - 768) / 735 * 2 * np.pi).astype(np.float32)
    v = ((yy[mask2] - 480) / 479).astype(np.float32)
    arr[mask2, 0] = np.clip(128 + 100*np.sin(u), 0, 255).astype(np.uint8)
    arr[mask2, 1] = np.clip(128 + 95*np.sin(u + 2.1) * (1-v*0.25), 0, 255).astype(np.uint8)
    arr[mask2, 2] = np.clip(128 + 100*np.sin(u + 4.2) * (1-v*0.2), 0, 255).astype(np.uint8)
    return arr, rois

def load_cases() -> tuple[dict[str, np.ndarray], dict[str, dict]]:
    cases: dict[str, np.ndarray] = {}
    provenance: dict[str, dict] = {}
    generated, rois = chart()
    cases["chart"] = generated
    provenance["chart"] = {"source": "deterministic generated RGB chart", "seed": SEED,
                            "size": [SIZE[0], SIZE[1]], "rois": rois}
    for name, filename in (("neon_streets", "cc0-neon-streets-1536.jpg"),
                           ("street_lights", "cc0-street-light-night.jpg")):
        path = ASSETS / filename
        if not path.exists():
            provenance[name] = {"available": False, "expected_path": str(path)}
            continue
        with Image.open(path) as image:
            rgb = np.asarray(image.convert("RGB")).copy()
        cases[name] = rgb
        provenance[name] = {"available": True, "source_path": path.relative_to(REPO).as_posix(),
                             "sha256": sha256(path), "size": [int(rgb.shape[1]), int(rgb.shape[0])],
                             "provenance_record": "experiments/GlowLab/assets/SOURCES.md"}
    return cases, provenance

def apply_look(rgb8: np.ndarray, look: str, intensity: float) -> np.ndarray:
    source = rgb8.astype(np.float32) / 255.0
    if look == "baseline" or intensity <= 0:
        return rgb8.copy()
    # Mirrors BuiltinLUTs table generation in ColorLUT.cpp plus ColorLUT's
    # trilinear sampler. Inputs are quantized to the production 17^3 grid.
    x = np.clip(source[..., 0] * 16.0, 0, 16)
    y = np.clip(source[..., 1] * 16.0, 0, 16)
    z = np.clip(source[..., 2] * 16.0, 0, 16)
    x0, y0, z0 = np.floor(x).astype(np.int32), np.floor(y).astype(np.int32), np.floor(z).astype(np.int32)
    x1, y1, z1 = np.minimum(x0 + 1, 16), np.minimum(y0 + 1, 16), np.minimum(z0 + 1, 16)
    dx, dy, dz = (x-x0)[..., None], (y-y0)[..., None], (z-z0)[..., None]

    def value(ix: np.ndarray, iy: np.ndarray, iz: np.ndarray) -> np.ndarray:
        r, g, b = ix.astype(np.float32)/16.0, iy.astype(np.float32)/16.0, iz.astype(np.float32)/16.0
        sid = LOOK_SOURCE_IDS[look]
        if sid == "cinematic":
            return np.stack((np.clip(r*1.1,0,1), np.clip(g*.9,0,1), np.clip(b*1.15,0,1)), axis=-1)
        if sid == "vintage":
            gray = r*.3 + g*.59 + b*.11
            return np.stack((np.clip(gray*1.2+.1,0,1), np.clip(gray+.05,0,1), np.clip(gray*.8,0,1)), axis=-1)
        if sid == "cold": return np.stack((r*.9, g*.95, np.clip(b*1.2,0,1)), axis=-1)
        if sid == "warm": return np.stack((np.clip(r*1.2,0,1), g*1.05, b*.8), axis=-1)
        if sid == "highContrast":
            curve = lambda c: np.clip((c-.5)*1.5+.5,0,1)
            return np.stack((curve(r),curve(g),curve(b)),axis=-1)
        if sid == "lowContrast": return np.stack((r*.7+.15,g*.7+.15,b*.7+.15),axis=-1)
        if sid == "desaturated":
            gray = r*.3+g*.59+b*.11
            return np.stack((gray,gray,gray),axis=-1)
        if sid == "kodak2383": return np.stack((np.power(r,.9),np.power(g,.95),np.power(b,1.1)),axis=-1)
        return np.stack((np.power(r,.95),np.power(g,.92),np.power(b,1.05)),axis=-1)

    c000,c100,c010,c110 = value(x0,y0,z0),value(x1,y0,z0),value(x0,y1,z0),value(x1,y1,z0)
    c001,c101,c011,c111 = value(x0,y0,z1),value(x1,y0,z1),value(x0,y1,z1),value(x1,y1,z1)
    lerp = lambda a,b,t: a+(b-a)*t
    c00,c01,c10,c11 = lerp(c000,c100,dx),lerp(c001,c101,dx),lerp(c010,c110,dx),lerp(c011,c111,dx)
    c0,c1 = lerp(c00,c10,dy),lerp(c01,c11,dy)
    graded = lerp(c0,c1,dz)
    mixed = source + (graded - source) * np.float32(intensity)
    return np.clip(mixed * 255.0 + 0.5, 0, 255).astype(np.uint8)

def roi_stats(rgb: np.ndarray, roi: list[int] | None) -> dict:
    x, y, w, h = roi if roi else [0, 0, rgb.shape[1], rgb.shape[0]]
    crop = rgb[y:y+h, x:x+w].astype(np.float32) / 255.0
    luma = crop @ np.array([0.2126, 0.7152, 0.0722], dtype=np.float32)
    rgb_hist = [np.histogram(crop[..., c], bins=16, range=(0.0, 1.0))[0].tolist() for c in range(3)]
    luma_hist = np.histogram(luma, bins=32, range=(0.0, 1.0))[0].tolist()
    return {"pixels": int(crop.shape[0] * crop.shape[1]),
            "rgb_min": crop.min(axis=(0,1)).tolist(), "rgb_max": crop.max(axis=(0,1)).tolist(),
            "rgb_mean": crop.mean(axis=(0,1)).tolist(), "rgb_median": np.median(crop, axis=(0,1)).tolist(),
            "rgb_p01": np.quantile(crop, 0.01, axis=(0,1)).tolist(),
            "rgb_p99": np.quantile(crop, 0.99, axis=(0,1)).tolist(),
            "rgb_histogram_16_bins": rgb_hist, "luma_histogram_32_bins": luma_hist,
            "luma_mean": float(luma.mean()), "luma_std": float(luma.std()),
            "clipped_0_rate": float(np.mean(np.all(crop <= 0, axis=-1))),
            "clipped_1_rate": float(np.mean(np.any(crop >= 1, axis=-1))),
            "non_finite_values": int(np.size(crop) - np.count_nonzero(np.isfinite(crop)))}

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--rd", default="rd0")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUT)
    parser.add_argument("--intensities", type=float, nargs="+", default=[0.0, 0.5, 1.0])
    args = parser.parse_args()
    if any(v < 0 or v > 1 for v in args.intensities):
        parser.error("intensities must be in [0, 1]")
    out = args.output if args.output != DEFAULT_OUT else ROOT / "captures" / args.rd
    review_dir, images_dir = out / "review", out / "images"
    review_dir.mkdir(parents=True, exist_ok=True)
    images_dir.mkdir(parents=True, exist_ok=True)
    cases, provenance = load_cases()
    metrics: dict = {}
    runtime_samples: dict[str, list[float]] = {}
    manifest = {"rd": args.rd, "seed": SEED, "created_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                "environment": {"python": platform.python_version(), "numpy": np.__version__,
                                "opencv": cv2.__version__, "pillow": Image.__version__},
                "image_contract": {"storage": "RGB uint8", "analysis": "8-bit sRGB encoded code values normalized to [0,1]",
                                   "display": "RGB PNG interpreted as sRGB",
                                   "candidate_outputs": "chart and chart ROI: PNG; photographic full frames: JPEG quality 90, 4:4:4; metrics are computed before encoding",
                                   "alpha": "not represented in current fixtures; not evaluated"},
                "candidate_model": "17^3 BuiltinLUTs generation and trilinear sampler mirrored from ArtifactCore ColorLUT.cpp",
                "lut_source": {"path": "ArtifactCore/src/Color/ColorLUT.cpp", "sha256": look_source_fingerprint(),
                               "names": LOOK_SOURCE_IDS},
                "cases": provenance, "looks": list(LOOKS), "intensities": args.intensities}
    for case_name, src in cases.items():
        # Chart ROIs are defined in chart coordinates and must never be reused
        # on differently sized photographic plates.
        rois = provenance[case_name].get("rois", {}) if case_name == "chart" else {}
        metrics[case_name] = {}
        panels = []
        for look in LOOKS:
            intensities = [0.0] if look == "baseline" else args.intensities
            for intensity in intensities:
                label = f"{look}_i{intensity:.2f}"
                start = time.perf_counter()
                result = apply_look(src, look, intensity)
                elapsed = (time.perf_counter() - start) * 1000
                runtime_samples.setdefault(label, []).append(elapsed)
                safe = f"{case_name}__{label}"
                photo_case = case_name != "chart"
                image_path = images_dir / f"{safe}.jpg" if photo_case else images_dir / f"{safe}.png"
                if photo_case:
                    Image.fromarray(result).save(image_path, quality=90, subsampling=0)
                else:
                    Image.fromarray(result).save(image_path)
                roi_result = {"full": roi_stats(result, None)}
                for roi_name, roi in rois.items():
                    roi_result[roi_name] = roi_stats(result, roi)
                    x, y, w, h = roi
                    Image.fromarray(result[y:y+h, x:x+w]).save(images_dir / f"{safe}__roi_{roi_name}.png")
                delta = np.abs(result.astype(np.int16) - src.astype(np.int16))
                metrics[case_name][label] = {"elapsed_ms": elapsed, "regions": roi_result,
                    "response_delta": {"mae_code": float(delta.mean()),
                                       "rmse_code": float(np.sqrt(np.mean(delta.astype(np.float32)**2))),
                                       "max_code": int(delta.max()),
                                       "p95_code": float(np.quantile(delta, .95)),
                                       "channel_mae_code": delta.mean(axis=(0,1)).tolist(),
                                       "changed_code_rate": float(np.mean(np.any(delta > 0, axis=-1)))}}
                thumb = Image.fromarray(result)
                thumb.thumbnail((360, 240))
                panels.append((label, thumb.copy()))
        sheet = Image.new("RGB", (720, ((len(panels)+1)//2)*280), (24, 26, 30))
        draw = ImageDraw.Draw(sheet)
        for i, (title, thumb) in enumerate(panels):
            x, y = (i % 2) * 360, (i // 2) * 280
            sheet.paste(thumb, (x, y+28))
            draw.text((x+6, y+6), title, fill=(245, 245, 245))
        sheet.save(review_dir / f"{case_name}_comparison.png")
    gates = {"status": "unlocked", "rd0_purpose": "baseline and response characterization only",
             "candidate_quality_verdicts": "not evaluated; do not rank from response statistics",
             "thresholds": [], "next_step": "human review, then freeze justified thresholds before rd1 comparison"}
    write_json_lf(out / "manifest.json", manifest)
    write_json_lf(out / "metrics.json", metrics)
    write_json_lf(out / "gates.json", gates)
    timings = {k: {"median_ms": float(np.median(v)), "p95_ms": float(np.quantile(v, .95)), "samples": len(v)}
               for k, v in runtime_samples.items()}
    write_json_lf(out / "timings_reference_only.json", timings)
    print(f"Wrote {len(cases)} cases to {out}")

if __name__ == "__main__":
    main()
