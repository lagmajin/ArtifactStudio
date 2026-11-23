#!/usr/bin/env python3
"""Float-domain contract probe mirroring ColorLUT + LUTEffect source equations.

This is an analytical Python mirror, not a C++ differential test or product render.
"""

from __future__ import annotations

import hashlib
import json
import math
import argparse
import platform
import time
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[1]
LUT_SOURCE = REPO / "ArtifactCore/src/Color/ColorLUT.cpp"
EFFECT_SOURCE = REPO / "Artifact/src/Effects/ColorCorrection/LUTEffect.cpp"
COMPOSITION_SOURCE = REPO / "Artifact/src/Render/ArtifactCompositionViewDrawing.cpp"
IMAGE_CONVERSION_SOURCE = REPO / "ArtifactCore/src/Image/ImageF32x4_RGBA.cpp"
PIXEL_CONVERSION_SOURCE = REPO / "ArtifactCore/include/Image/SurfacePixelConversion.ixx"
IMAGE_SURFACE_VIEW_SOURCE = REPO / "ArtifactCore/include/Image/ImageSurfaceView.ixx"
SOURCE_CONTRACT = ROOT / "SOURCE_CONTRACT.json"
ALPHA_FIXTURE = ROOT / "assets/alpha-association-chart.png"
ALPHA_FIXTURE_MANIFEST = ROOT / "assets/alpha-association-chart.json"
OUT = ROOT / "captures/rd1_lut_alpha_wrapper/float_contract_probe.json"

LOOKS = (
    "cinematic", "vintage", "cold", "warm", "highContrast", "lowContrast",
    "desaturated", "kodak2383", "fuji3510",
)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def make_lut(look: str) -> np.ndarray:
    """Return table indexed [z,y,x,channel], following ColorLUT::index."""
    axis = np.arange(17, dtype=np.float32) / np.float32(16.0)
    r, g, b = np.meshgrid(axis, axis, axis, indexing="xy")
    # meshgrid(x,y,z,indexing='xy') yields [y,x,z]; transpose to [z,y,x].
    r, g, b = (a.transpose(2, 0, 1) for a in (r, g, b))
    if look == "cinematic":
        values = (np.clip(r * 1.1, 0, 1), np.clip(g * 0.9, 0, 1), np.clip(b * 1.15, 0, 1))
    elif look == "vintage":
        gray = r * 0.3 + g * 0.59 + b * 0.11
        values = (np.clip(gray * 1.2 + 0.1, 0, 1), np.clip(gray + 0.05, 0, 1), np.clip(gray * 0.8, 0, 1))
    elif look == "cold":
        values = (r * 0.9, g * 0.95, np.clip(b * 1.2, 0, 1))
    elif look == "warm":
        values = (np.clip(r * 1.2, 0, 1), g * 1.05, b * 0.8)
    elif look == "highContrast":
        curve = lambda c: np.clip((c - 0.5) * 1.5 + 0.5, 0, 1)
        values = (curve(r), curve(g), curve(b))
    elif look == "lowContrast":
        values = (r * 0.7 + 0.15, g * 0.7 + 0.15, b * 0.7 + 0.15)
    elif look == "desaturated":
        gray = r * 0.3 + g * 0.59 + b * 0.11
        values = (gray, gray, gray)
    elif look == "kodak2383":
        values = (np.power(r, 0.9), np.power(g, 0.95), np.power(b, 1.1))
    else:
        values = (np.power(r, 0.95), np.power(g, 0.92), np.power(b, 1.05))
    # ColorLUT::setValue clamps each generated table entry before storing it.
    return np.clip(np.stack(values, axis=-1), 0.0, 1.0).astype(np.float32)


def sample(table: np.ndarray, color: tuple[float, float, float]) -> list[float]:
    safe = [min(max(float(c) if math.isfinite(float(c)) else 0.0, 0.0), 1.0) for c in color]
    fx, fy, fz = (np.float32(c * 16.0) for c in safe)
    x0, y0, z0 = (int(np.floor(v)) for v in (fx, fy, fz))
    x1, y1, z1 = min(x0 + 1, 16), min(y0 + 1, 16), min(z0 + 1, 16)
    dx, dy, dz = np.float32(fx - x0), np.float32(fy - y0), np.float32(fz - z0)

    def lerp(a: np.ndarray, b: np.ndarray, t: np.float32) -> np.ndarray:
        return a + (b - a) * t

    c000, c100 = table[z0, y0, x0], table[z0, y0, x1]
    c010, c110 = table[z0, y1, x0], table[z0, y1, x1]
    c001, c101 = table[z1, y0, x0], table[z1, y0, x1]
    c011, c111 = table[z1, y1, x0], table[z1, y1, x1]
    c00, c01 = lerp(c000, c100, dx), lerp(c001, c101, dx)
    c10, c11 = lerp(c010, c110, dx), lerp(c011, c111, dx)
    c0, c1 = lerp(c00, c10, dy), lerp(c01, c11, dy)
    result = lerp(c0, c1, dz)
    return [min(max(float(v) if math.isfinite(float(v)) else 0.0, 0.0), 1.0) for v in result]


def apply_effect(table: np.ndarray, rgba: tuple[float, float, float, float], intensity: float,
                 premultiplied: bool = False) -> list[float]:
    original = [float(v) for v in rgba[:3]]
    alpha = float(rgba[3])
    if premultiplied:
        if alpha <= 0.0:
            return [*original, alpha]
        original = [v / alpha for v in original]
    mapped = sample(table, tuple(original))
    if intensity >= 0.9999:
        rgb = mapped
    elif intensity > 0.0001:
        rgb = [a + (b - a) * intensity for a, b in zip(original, mapped)]
    else:
        rgb = original
    if premultiplied:
        rgb = [v * alpha for v in rgb]
    return [*rgb, alpha]


def json_numbers(values: list[float]) -> list[float | str]:
    return [float(v) if math.isfinite(float(v)) else str(v) for v in values]


def main(output_path: Path = OUT, run_id: str = "rd1_lut_alpha_wrapper") -> None:
    source_contract = json.loads(SOURCE_CONTRACT.read_text(encoding="utf-8"))
    actual_hashes = {
        LUT_SOURCE.relative_to(REPO).as_posix(): sha256(LUT_SOURCE),
        EFFECT_SOURCE.relative_to(REPO).as_posix(): sha256(EFFECT_SOURCE),
        COMPOSITION_SOURCE.relative_to(REPO).as_posix(): sha256(COMPOSITION_SOURCE),
        IMAGE_CONVERSION_SOURCE.relative_to(REPO).as_posix(): sha256(IMAGE_CONVERSION_SOURCE),
        PIXEL_CONVERSION_SOURCE.relative_to(REPO).as_posix(): sha256(PIXEL_CONVERSION_SOURCE),
        IMAGE_SURFACE_VIEW_SOURCE.relative_to(REPO).as_posix(): sha256(IMAGE_SURFACE_VIEW_SOURCE),
    }
    if actual_hashes != source_contract["source_files"]:
        raise SystemExit(
            "LUT source changed since mirror review; inspect the source, update the "
            "Python mirror/evidence, then refresh SOURCE_CONTRACT.json."
        )
    fixture_manifest = json.loads(ALPHA_FIXTURE_MANIFEST.read_text(encoding="utf-8"))
    fixture_sha256 = sha256(ALPHA_FIXTURE)
    if fixture_sha256 != fixture_manifest["sha256"]:
        raise SystemExit("Alpha fixture hash differs from its manifest; regenerate or review the fixture before probing.")
    with Image.open(ALPHA_FIXTURE) as fixture_image:
        fixture_pixels = np.asarray(fixture_image.convert("RGBA"))
    expected_size = tuple(fixture_manifest["size"])
    if fixture_pixels.shape[1::-1] != expected_size:
        raise SystemExit("Alpha fixture dimensions differ from its manifest.")
    row_height = fixture_pixels.shape[0] // len(fixture_manifest["rows_top_to_bottom"])
    patch_width = fixture_manifest["fixed_patch_width"]
    for row_index, color_row in enumerate(fixture_manifest["rows_top_to_bottom"]):
        y = row_index * row_height + row_height // 2
        for patch_index, alpha_byte in enumerate(fixture_manifest["fixed_alpha_bytes"]):
            x = patch_index * patch_width + patch_width // 2
            expected_rgba = [*color_row["rgb8"], alpha_byte]
            if fixture_pixels[y, x].tolist() != expected_rgba:
                raise SystemExit("Alpha fixture patch pixel differs from its manifest.")
    probes = {
        "black": (0.0, 0.0, 0.0, 0.0),
        "neutral_18_percent": (0.18, 0.18, 0.18, 0.5),
        "neutral_mid": (0.5, 0.5, 0.5, 1.0),
        "white": (1.0, 1.0, 1.0, 0.25),
        "out_of_range_hdr": (4.0, 0.5, -0.25, 0.37),
        "non_finite_rgb": (float("nan"), float("inf"), float("-inf"), 0.63),
    }
    output = {
        "scope": "float-domain source-equation mirror; not a C++ differential test or product image evaluation",
        "rd": run_id,
        "created_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "environment": {
            "python": platform.python_version(),
            "numpy": np.__version__,
            "pillow": Image.__version__,
        },
        "harness_sha256": sha256(Path(__file__).resolve()),
        "sources": {
            **actual_hashes,
        },
        "source_contract_sha256": sha256(SOURCE_CONTRACT),
        "format": "IEEE-like Python floats; 17^3 table and sampler arithmetic use numpy float32 where source stores float",
        "contract_notes": [
            "ColorLUT lookup sanitizes non-finite RGB to 0 and clamps lookup input/output to [0,1].",
            "LUTEffect transforms straight RGB; for a premultiplied descriptor it unpremultiplies RGB, applies and mixes the LUT in straight space, then re-premultiplies. Intensity 0 leaves source RGB unchanged.",
            "LUTEffect copies RGBA, preserves alpha, and leaves premultiplied RGB unchanged when alpha is non-positive.",
            "HDR RGB is not preserved at full intensity. Intermediate intensity mixes HDR source values with the clamped LUT output.",
        ],
        "probes": {},
        "alpha_association_probe": {
            "source_format": "QImage::Format_ARGB32_Premultiplied is tagged sRGB/Premultiplied at the composition image-effect boundary",
            "conversion_back": "ImageF32x4_RGBA::toQImage targets sRGB straight alpha and unpremultiplies source RGB",
            "straight_rgb": [0.8, 0.2, 0.1],
            "alpha_values": [0.0, 0.1, 0.25, 0.5, 1.0],
            "purpose": "Sweep identical straight RGB through straight and premultiplied source-equation paths; unpremultiply the latter for display comparison.",
            "fully_transparent_premultiplied_pixel": {
                "input_rgba": [0.0, 0.0, 0.0, 0.0],
                "expected_output_rgba": [0.0, 0.0, 0.0, 0.0],
                "source_rule": "Premultiplied alpha <= 0 leaves the copied pixel unchanged.",
            },
            "looks": {},
        },
        "alpha_fixture_patch_probe": {
            "fixture": ALPHA_FIXTURE.relative_to(REPO).as_posix(),
            "fixture_sha256": fixture_sha256,
            "scope": "source-equation mirror at the fixture's constant-color fixed-alpha patches; not a PNG decoder, C++ differential test, or product render",
            "alpha_bytes": fixture_manifest["fixed_alpha_bytes"],
            "colors": {},
        },
    }
    for look in LOOKS:
        table = make_lut(look)
        output["probes"][look] = {
            name: {
                "input_rgba": [v if math.isfinite(v) else str(v) for v in rgba],
                "intensity_0": json_numbers(apply_effect(table, rgba, 0.0)),
                "intensity_0_5": json_numbers(apply_effect(table, rgba, 0.5)),
                "intensity_1": json_numbers(apply_effect(table, rgba, 1.0)),
            }
            for name, rgba in probes.items()
        }
        straight_rgb = (0.8, 0.2, 0.1)
        alpha_results = {}
        for alpha in output["alpha_association_probe"]["alpha_values"]:
            straight = (*straight_rgb, alpha)
            premultiplied = tuple(v * alpha for v in straight_rgb) + (alpha,)
            intensity_results = {}
            for intensity in (0.0, 0.5, 1.0):
                straight_result = apply_effect(table, straight, intensity)
                premul_result = apply_effect(table, premultiplied, intensity, premultiplied=True)
                legacy_direct_result = apply_effect(table, premultiplied, intensity)
                unpremultiplied = ([v / alpha for v in premul_result[:3]] if alpha > 1e-6 else [0.0, 0.0, 0.0])
                legacy_unpremultiplied = ([v / alpha for v in legacy_direct_result[:3]]
                                          if alpha > 1e-6 else [0.0, 0.0, 0.0])
                expected_premultiplied = [v * alpha for v in straight_result[:3]]
                intensity_results[f"intensity_{intensity:g}"] = {
                    "straight_input_rgba": json_numbers(list(straight)),
                    "premultiplied_input_rgba": json_numbers(list(premultiplied)),
                    "straight_input_result": json_numbers(straight_result),
                    "premultiplied_input_after_effect": json_numbers(premul_result),
                    "premultiplied_input_after_output_unpremultiply": json_numbers(unpremultiplied + [alpha]),
                    "legacy_direct_premul_output_unpremultiplied": json_numbers(legacy_unpremultiplied + [alpha]),
                    "legacy_direct_premul_rgb_error_vs_straight": json_numbers(
                        [v - e for v, e in zip(legacy_unpremultiplied, straight_result[:3])]
                    ),
                    "premultiplied_input_after_output_unpremultiply_and_srgb8_clip": json_numbers(
                        [min(max(v, 0.0), 1.0) for v in unpremultiplied] + [alpha]
                    ),
                    "expected_premultiplied_rgb_if_straight_color_were_graded": json_numbers(expected_premultiplied),
                    "premultiplied_rgb_error": json_numbers([p - e for p, e in zip(premul_result[:3], expected_premultiplied)]),
                }
            alpha_results[f"alpha_{alpha:.2f}"] = intensity_results
        output["alpha_association_probe"]["looks"][look] = alpha_results
        fixture_look_results = {}
        for color_row in fixture_manifest["rows_top_to_bottom"]:
            color_name = color_row["id"]
            straight_rgb = tuple(float(v) / 255.0 for v in color_row["rgb8"])
            alpha_results_for_color = {}
            for alpha_byte in fixture_manifest["fixed_alpha_bytes"]:
                alpha = float(alpha_byte) / 255.0
                straight = (*straight_rgb, alpha)
                premultiplied = tuple(v * alpha for v in straight_rgb) + (alpha,)
                intensity_results = {}
                for intensity in (0.0, 0.5, 1.0):
                    straight_result = apply_effect(table, straight, intensity)
                    premul_result = apply_effect(table, premultiplied, intensity, premultiplied=True)
                    legacy_direct_result = apply_effect(table, premultiplied, intensity)
                    unpremultiplied = ([v / alpha for v in premul_result[:3]] if alpha > 1e-6 else [0.0, 0.0, 0.0])
                    legacy_unpremultiplied = ([v / alpha for v in legacy_direct_result[:3]]
                                              if alpha > 1e-6 else [0.0, 0.0, 0.0])
                    intensity_results[f"intensity_{intensity:g}"] = {
                        "straight_input_rgba": json_numbers(list(straight)),
                        "premultiplied_input_rgba": json_numbers(list(premultiplied)),
                        "straight_input_result": json_numbers(straight_result),
                        "premultiplied_input_after_output_unpremultiply": json_numbers(unpremultiplied + [alpha]),
                        "legacy_direct_premul_rgb_error_before_output_clip": json_numbers(
                            [v - e for v, e in zip(legacy_unpremultiplied, straight_result[:3])]
                        ),
                        "display_rgb_error_before_output_clip": json_numbers(
                            [a - b for a, b in zip(unpremultiplied, straight_result[:3])]
                        ),
                        "display_rgb_error_after_srgb8_clip": json_numbers([
                            min(max(a, 0.0), 1.0) - min(max(b, 0.0), 1.0)
                            for a, b in zip(unpremultiplied, straight_result[:3])
                        ]),
                    }
                alpha_results_for_color[f"alpha_byte_{alpha_byte:03d}"] = intensity_results
            fixture_look_results[color_name] = alpha_results_for_color
        output["alpha_fixture_patch_probe"]["colors"][look] = fixture_look_results
    fixture_error_summary = {}
    for look, color_results in output["alpha_fixture_patch_probe"]["colors"].items():
        candidates = []
        for color_name, alpha_results_for_color in color_results.items():
            for alpha_key, intensity_results in alpha_results_for_color.items():
                if alpha_key == "alpha_byte_000":
                    continue  # RGB at zero alpha is unobservable after output unpremultiplication.
                for intensity_key, values in intensity_results.items():
                    error = [float(v) for v in values["display_rgb_error_before_output_clip"]]
                    candidates.append({
                        "max_abs_rgb_error_before_output_clip": max(abs(v) for v in error),
                        "color": color_name,
                        "alpha_patch": alpha_key,
                        "intensity": intensity_key,
                        "error_rgb": error,
                        "max_abs_rgb_error_after_srgb8_clip_at_same_case": max(
                            abs(float(v)) for v in values["display_rgb_error_after_srgb8_clip"]
                        ),
                    })
        fixture_error_summary[look] = max(
            candidates, key=lambda item: item["max_abs_rgb_error_before_output_clip"]
        )
    output["alpha_fixture_patch_probe"]["max_visible_alpha_error_by_look"] = {
        "metric": "worst per-channel RGB difference after output unpremultiplication, before and after [0,1] output clipping; alpha-byte-0 pixels excluded",
        "scope": "source-equation mirror summary only; not a quality gate or C++/runtime result",
        "values": fixture_error_summary,
    }
    legacy_error_summary = {}
    for look, color_results in output["alpha_fixture_patch_probe"]["colors"].items():
        errors = []
        for alpha_results_for_color in color_results.values():
            for alpha_key, intensity_results in alpha_results_for_color.items():
                if alpha_key == "alpha_byte_000":
                    continue
                for values in intensity_results.values():
                    errors.extend(abs(float(v)) for v in
                                  values["legacy_direct_premul_rgb_error_before_output_clip"])
        legacy_error_summary[look] = max(errors, default=0.0)
    output["alpha_fixture_patch_probe"]["legacy_direct_max_visible_alpha_error_by_look"] = {
        "metric": "worst absolute per-channel RGB difference for the previous direct-premultiplied-input equation after output unpremultiplication; alpha-byte-0 pixels excluded",
        "scope": "analytical source-equation comparison only; not runtime output or a quality gate",
        "values": legacy_error_summary,
    }
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", encoding="utf-8", newline="\n") as stream:
        json.dump(output, stream, ensure_ascii=False, indent=2, allow_nan=False)
        stream.write("\n")
    print(
        f"Wrote {len(LOOKS)} LUTs × {len(probes)} RGBA probes plus "
        f"{len(LOOKS)} × {len(fixture_manifest['rows_top_to_bottom'])} colors × "
        f"{len(fixture_manifest['fixed_alpha_bytes'])} alpha patches × 3 intensities to {output_path}"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=OUT)
    parser.add_argument("--rd", help="Run identifier recorded in the output manifest.")
    parser.add_argument("--overwrite", action="store_true")
    args = parser.parse_args()
    if args.output.exists() and not args.overwrite:
        parser.error(f"output exists; choose another path or pass --overwrite: {args.output}")
    main(args.output, args.rd or args.output.parent.name)
