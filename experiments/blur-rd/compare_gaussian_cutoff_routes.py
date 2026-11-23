"""Compare Gaussian CPU-wrapper cutoff and resident shader equations.

This is an offline synthetic source-equation study. It does not launch Artifact,
run a GPU shader, or claim a production route or active display transform.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[2]
BUNDLE = Path(__file__).resolve().parent / "fixtures" / "gaussian-r1"
OUTPUT = Path(__file__).resolve().parent / "results" / "gaussian_alpha_edge_cpu_resident_cutoff_2026-11-12.json"
PREVIEW = Path(__file__).resolve().parent / "results" / "previews" / "gaussian_alpha_edge_cpu_resident_sigma16_64.png"
ANALYZER_PATH = Path(__file__).resolve().parent / "analyze_gaussian_fixture_bundle.py"
SPEC = importlib.util.spec_from_file_location("gaussian_fixture_analyzer", ANALYZER_PATH)
assert SPEC and SPEC.loader
ANALYZER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(ANALYZER)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def stats(values: np.ndarray) -> dict[str, float | int]:
    absolute = np.abs(values.astype(np.float64)).reshape(-1)
    if not absolute.size:
        return {"components": 0, "mae": 0.0, "rmse": 0.0, "p95": 0.0, "max": 0.0,
                "components_over_1_255": 0}
    return {
        "components": int(absolute.size),
        "mae": float(absolute.mean()),
        "rmse": float(np.sqrt(np.mean(absolute * absolute))),
        "p95": float(np.quantile(absolute, 0.95)),
        "max": float(absolute.max()),
        "components_over_1_255": int(np.count_nonzero(absolute > 1.0 / 255.0)),
    }


def display(linear_rgb: np.ndarray) -> np.ndarray:
    return ANALYZER.linear_to_srgb(linear_rgb)


def make_sparse_hdr_stroke(edge_fixture: np.ndarray) -> np.ndarray:
    result = np.zeros_like(edge_fixture)
    center = result.shape[0] // 2
    result[center, center, 3] = 0.5
    # Authored premultiplied BGRA, corresponding to straight R=1.6, G=-.08, B=.35.
    result[center, center, :3] = np.array((0.175, -0.04, 0.8), dtype=np.float32)
    return result


def compare_case(name: str, image: np.ndarray, sigma: float) -> dict[str, object]:
    cpu_radius = len(ANALYZER.gaussian_weights(sigma, resident=False)) // 2
    resident_radius = len(ANALYZER.gaussian_weights(sigma, resident=True)) // 2

    straight = ANALYZER.straight_from_premultiplied(image)
    cpu_straight = ANALYZER.blur_cpu_wrapper(straight, sigma)
    cpu_alpha = cpu_straight[..., 3:4]
    cpu_premultiplied = cpu_straight[..., :3] * cpu_alpha
    resident = ANALYZER.blur_standalone_gpu_wrapper(straight, sigma)
    resident_alpha = resident[..., 3:4]
    resident_premultiplied = resident[..., :3] * resident_alpha

    nonzero_input = image[..., 3] > 0.0
    ys, xs = np.nonzero(nonzero_input)
    margin = max(cpu_radius, resident_radius) + 2
    source_roi = np.zeros(image.shape[:2], dtype=bool)
    source_roi[
        max(0, int(ys.min()) - margin):min(image.shape[0], int(ys.max()) + margin + 1),
        max(0, int(xs.min()) - margin):min(image.shape[1], int(xs.max()) + margin + 1),
    ] = True
    interior_margin = max(cpu_radius, resident_radius)
    interior_roi = np.zeros(image.shape[:2], dtype=bool)
    interior_roi[interior_margin:image.shape[0] - interior_margin,
                 interior_margin:image.shape[1] - interior_margin] = True

    tail = (cpu_alpha[..., 0] > 0.0) & (cpu_alpha[..., 0] <= 1.0e-5)
    resident_tail = (resident_alpha[..., 0] > 0.0) & (resident_alpha[..., 0] <= 1.0e-5)
    dropped_premultiplied = ANALYZER.blur_cpu(image, sigma)[..., :3] - cpu_premultiplied
    route_delta = cpu_premultiplied - resident_premultiplied

    backgrounds = {
        "black": np.zeros(3, dtype=np.float32),
        "gray": np.full(3, 0.18, dtype=np.float32),
        "white": np.ones(3, dtype=np.float32),
        "green": np.array((0.08, 0.45, 0.12), dtype=np.float32),
    }
    background_metrics = {}
    for bg_name, bg in backgrounds.items():
        cpu_composite = cpu_premultiplied + bg * (1.0 - cpu_alpha)
        resident_composite = resident_premultiplied + bg * (1.0 - resident_alpha)
        no_cutoff_premultiplied = ANALYZER.blur_cpu(image, sigma)[..., :3]
        no_cutoff_composite = no_cutoff_premultiplied + bg * (1.0 - cpu_alpha)
        cutoff_error = display(no_cutoff_composite) - display(cpu_composite)
        route_error = display(cpu_composite) - display(resident_composite)
        background_metrics[bg_name] = {
            "cpu_cutoff_vs_no_cutoff_display_tail": stats(cutoff_error[tail]),
            "cpu_wrapper_vs_resident_display_source_roi": stats(route_error[source_roi]),
            "cpu_wrapper_vs_resident_display_full": stats(route_error),
            "cpu_wrapper_vs_resident_display_border_excluded_roi": stats(route_error[interior_roi]),
        }

    height, width = image.shape[:2]
    return {
        "case": name,
        "sigma_px": sigma,
        "dimensions_px": [width, height],
        "threshold": 1.0e-5,
        "cpu_kernel_width_px": 2 * cpu_radius + 1,
        "cpu_support_radius_px": cpu_radius,
        "resident_shader_radius_px": resident_radius,
        "resident_support_width_px": 2 * resident_radius + 1,
        "cpu_tail_pixels": int(tail.sum()),
        "resident_tail_pixels": int(resident_tail.sum()),
        "cpu_tail_bounds_xyxy": (
            [int(np.nonzero(tail)[1].min()), int(np.nonzero(tail)[0].min()),
             int(np.nonzero(tail)[1].max()) + 1, int(np.nonzero(tail)[0].max()) + 1]
            if tail.any() else None
        ),
        "cpu_tail_premultiplied_rgb_energy_lost": float(np.abs(dropped_premultiplied).sum(dtype=np.float64)),
        "cpu_wrapper_reassociated_premultiplied_energy": float(cpu_premultiplied.sum(dtype=np.float64)),
        "resident_premultiplied_rgb_energy": float(resident_premultiplied.sum(dtype=np.float64)),
        "cpu_vs_resident_premultiplied_rgb_source_roi": stats(route_delta[source_roi]),
        "cpu_vs_resident_premultiplied_rgb_border_excluded_roi": stats(route_delta[interior_roi]),
        "backgrounds": background_metrics,
        "border_rules": "CPU OpenCV Reflect101; resident equation coordinate clamp. Full-frame route delta can include border differences.",
    }


def write_preview(edge: np.ndarray, sparse: np.ndarray) -> None:
    sigma_values = (16.0, 64.0)
    cases = (("wide alpha edge", edge), ("sparse HDR point", sparse))
    backgrounds = ("black", "gray", "white")
    tile_size = 240
    label_height = 24
    rows = len(cases) * len(sigma_values) * len(backgrounds) * 2
    canvas = Image.new("RGB", (tile_size * 3, rows * (tile_size + label_height)), (30, 33, 38))
    draw = ImageDraw.Draw(canvas)
    row = 0
    for case_name, image in cases:
        for sigma in sigma_values:
            straight = ANALYZER.straight_from_premultiplied(image)
            cpu = ANALYZER.blur_cpu_wrapper(straight, sigma)
            cpu_pm = cpu[..., :3] * cpu[..., 3:4]
            resident = ANALYZER.blur_standalone_gpu_wrapper(straight, sigma)
            resident_pm = resident[..., :3] * resident[..., 3:4]
            cpu_ref = ANALYZER.blur_cpu(image, sigma)
            no_cutoff = cpu_ref[..., :3]
            for bg_name in backgrounds:
                bg = (np.zeros(3, dtype=np.float32) if bg_name == "black" else
                      np.full(3, 0.18, dtype=np.float32) if bg_name == "gray" else
                      np.ones(3, dtype=np.float32))
                images = (
                    cpu_pm + bg * (1.0 - cpu[..., 3:4]),
                    no_cutoff + bg * (1.0 - cpu[..., 3:4]),
                    resident_pm + bg * (1.0 - resident[..., 3:4]),
                )
                y0 = 0 if case_name == "wide alpha edge" else image.shape[0] // 2 - 96
                y1 = image.shape[0] if case_name == "wide alpha edge" else image.shape[0] // 2 + 96
                x0 = image.shape[1] // 2 - 96
                x1 = image.shape[1] // 2 + 96
                for exposure in (0, 4):
                    gain = float(2 ** exposure)
                    for col, (rgb, caption) in enumerate(zip(images, ("CPU cutoff", "CPU no cutoff", "resident equation"))):
                        srgb = np.clip(display(rgb[y0:y1, x0:x1] * gain), 0.0, 1.0)
                        srgb = np.asarray(np.rint(srgb[..., ::-1] * 255.0), dtype=np.uint8)
                        tile = Image.fromarray(srgb, mode="RGB").resize((tile_size, tile_size), Image.Resampling.NEAREST)
                        canvas.paste(tile, (col * tile_size, row * (tile_size + label_height) + label_height))
                        draw.text((col * tile_size + 4, row * (tile_size + label_height) + 4),
                                  f"{case_name}, sigma={sigma:g}, {bg_name}, +{exposure}EV: {caption}", fill=(245, 245, 245))
                    row += 1
    PREVIEW.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(PREVIEW)


def main() -> None:
    manifest = json.loads((BUNDLE / "manifest.json").read_text(encoding="utf-8"))
    entry = next(item for item in manifest["fixtures"] if item["name"] == "premultiplied_alpha_edge_hdr")
    fixture_path = ROOT / entry["path"]
    if digest(fixture_path) != entry["sha256"]:
        raise SystemExit("fixture hash mismatch")
    edge = np.load(fixture_path, allow_pickle=False)
    sparse = make_sparse_hdr_stroke(edge)
    measurements = [
        compare_case(case_name, image, sigma)
        for case_name, image in (("wide_alpha_edge", edge), ("sparse_hdr_stroke_derived", sparse))
        for sigma in (0.5, 1.0, 3.0, 16.0, 64.0)
    ]
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps({
        "schema": "artifact.blur-rd.alpha-edge-route-equation-comparison.v2",
        "runner": Path(__file__).relative_to(ROOT).as_posix(),
        "runner_sha256": digest(Path(__file__).resolve()),
        "analyzer_sha256": digest(ANALYZER_PATH),
        "fixture_manifest_sha256": digest(BUNDLE / "manifest.json"),
        "fixture_sources": {
            "wide_alpha_edge": {"path": entry["path"], "sha256": entry["sha256"],
                                 "alpha_association": "authored premultiplied", "purpose": "broad alpha edge; cutoff negative control"},
            "sparse_hdr_stroke_derived": {"derivation": "513x513 zero image with center alpha=.5 and premultiplied BGRA RGB=(.8,-.04,.175), corresponding to straight RGB=(1.6,-.08,.35)",
                                           "sha256": hashlib.sha256(sparse.tobytes(order="C")).hexdigest(),
                                           "purpose": "sparse high-dynamic-range cutoff stress case"},
        },
        "methods": "Reuse analyze_gaussian_fixture_bundle.py CPU wrapper and HLSL standalone GPU wrapper equation. Both receive the authored fixture explicitly unassociated to straight numeric RGB; outputs are reassociated for visible comparison. The GPU mirror follows shader RGB*alpha accumulation and per-pass unassociation, then the application-copied descriptor remains outside this numeric comparison. Display uses clamped linear-to-sRGB OETF only. The sparse derived BGRA value is (B=.175,G=-.04,R=.8 premultiplied), corresponding to straight HDR red=1.6.",
        "alpha_cutoff": 1.0e-5,
        "sigmas_px": [0.5, 1.0, 3.0, 16.0, 64.0],
        "measurements": measurements,
        "preview": PREVIEW.relative_to(ROOT).as_posix(),
        "scope_limit": "Source-equation mirrors only; no Artifact runtime capture, actual descriptor/dispatch/preview route, active view transform, perceptual acceptance, or performance. Sparse stroke is deterministic derived data, not a production asset. The former direct-RGBA blur_resident helper did not mirror current HLSL and its previous route comparison output is superseded by this regeneration.",
    }, indent=2) + "\n", encoding="utf-8", newline="\n")
    write_preview(edge, sparse)
    print(OUTPUT)
    print(PREVIEW)


if __name__ == "__main__":
    main()
