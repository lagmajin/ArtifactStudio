#!/usr/bin/env python3
"""Offline source mirrors for Color Balance and Tritone cinematic presets.

This is an analytical Python mirror, not a C++ differential test or product render.
See EVALUATION_PLAN.md and EFFECT_SOURCE_CONTRACT.json before interpreting output.
"""

from __future__ import annotations

import hashlib
import json
import random
import zlib
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

from cinematic_looks_lab import load_cases, roi_stats, sha256, write_json_lf

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[1]
OUT = ROOT / "captures" / "rd0_effect_extension"
CONTRACT = ROOT / "EFFECT_SOURCE_CONTRACT.json"
SOURCES = {
    "ArtifactCore/src/ImageProcessing/ColorTransform/ColorBalance.cpp": "color_balance_processor",
    "ArtifactCore/include/ImageProcessing/ColorTransform/ColorBalance.ixx": "color_balance_settings",
    "Artifact/src/Effects/ColorCorrection/ColorBalanceEffect.cpp": "color_balance_effect",
    "ArtifactCore/src/ImageProcessing/ColorTransform/Tritone.cpp": "tritone_processor",
    "ArtifactCore/include/ImageProcessing/ColorTransform/Tritone.ixx": "tritone_settings",
    "Artifact/src/Effects/ColorCorrection/TritoneEffect.cpp": "tritone_effect",
}
LUMA = np.array([0.2126, 0.7152, 0.0722], dtype=np.float32)


def verify_sources() -> dict[str, str]:
    expected = json.loads(CONTRACT.read_text(encoding="utf-8"))["source_files"]
    actual = {relative: sha256(REPO / relative) for relative in SOURCES}
    if actual != expected:
        raise SystemExit("Effect sources changed; inspect source equations and update the mirror/contract before regenerating.")
    return actual


def smoothstep(edge0: float, edge1: float, x: np.ndarray) -> np.ndarray:
    t = np.clip((x - np.float32(edge0)) / np.float32(edge1 - edge0), 0.0, 1.0)
    return t * t * (np.float32(3.0) - np.float32(2.0) * t)


def preserve_luma(source_luma: np.ndarray, rgb: np.ndarray, amount: float) -> np.ndarray:
    target_luma = rgb @ LUMA
    safe = target_luma > np.float32(1e-6)
    scale = np.ones_like(target_luma)
    np.divide(source_luma, target_luma, out=scale, where=safe)
    preserved = np.clip(rgb * scale[..., None], 0.0, 1.0)
    return rgb * np.float32(1.0 - amount) + preserved * np.float32(amount)


def color_balance(rgb: np.ndarray, strength: float) -> np.ndarray:
    """ColorBalanceSettings::cinematic + ColorBalanceProcessor::applyPixel."""
    source = rgb.astype(np.float32, copy=False)
    # ColorBalanceEffect::isNeutral short-circuits the effect for strength <= 0.
    if strength <= 0.0:
        return source.copy()
    lum = source @ LUMA
    shadow_w = 1.0 - smoothstep(0.20, 0.40, lum)
    highlight_w = smoothstep(0.60, 0.80, lum)
    mid_w = np.clip(1.0 - shadow_w - highlight_w, 0.0, 1.0)
    delta = np.stack((
        -0.01 * shadow_w + 0.02 * mid_w + 0.10 * highlight_w,
        0.03 * shadow_w + 0.01 * mid_w + 0.04 * highlight_w,
        0.10 * shadow_w - 0.01 * mid_w - 0.01 * highlight_w,
    ), axis=-1).astype(np.float32)
    result = source + delta * np.float32(strength)
    result = preserve_luma(lum, result, strength)
    return np.clip(result, 0.0, 1.0)


TRITONE_CINEMATIC = {
    "shadow": (0.09, 0.18, 0.28),
    "midtone": (0.54, 0.48, 0.40),
    "highlight": (0.96, 0.84, 0.60),
    "balance": 0.48,
    "softness": 0.62,
    "color_mix": 0.90,
}
TRITONE_TEAL_ORANGE = {
    "shadow": (0.10, 0.35, 0.42),
    "midtone": (0.53, 0.52, 0.50),
    "highlight": (0.96, 0.56, 0.24),
    "balance": 0.46,
    "softness": 0.58,
    "color_mix": 0.88,
}


def tritone(rgb: np.ndarray, strength: float, preset: dict = TRITONE_CINEMATIC) -> np.ndarray:
    """TritoneSettings preset + TritoneProcessor::applyPixel source mirror."""
    source = rgb.astype(np.float32, copy=False)
    lum = source @ LUMA
    balance = preset["balance"]
    softness = preset["softness"]
    shadow_w = 1.0 - smoothstep(balance - softness, balance, lum)
    highlight_w = smoothstep(balance, balance + softness, lum)
    mid_w = np.clip(1.0 - shadow_w - highlight_w, 0.0, 1.0)
    colors = np.array([preset["shadow"], preset["midtone"], preset["highlight"]], dtype=np.float32)
    # Cinematic and TealAndOrange source settings both use unit tonal strengths.
    target = (shadow_w[..., None] * colors[0] + mid_w[..., None] * colors[1] +
              highlight_w[..., None] * colors[2])
    target = np.clip(target, 0.0, 1.0)
    mix = np.float32(strength * preset["color_mix"])
    result = source * (np.float32(1.0) - mix) + target * mix
    result = preserve_luma(lum, result, strength)
    return np.clip(result, 0.0, 1.0)


def quantize(rgb: np.ndarray) -> np.ndarray:
    return np.clip(rgb * 255.0 + 0.5, 0, 255).astype(np.uint8)


def alpha_association_probe(transform, premultiplied_aware: bool) -> dict:
    colors = np.array([
        (0.80, 0.20, 0.10), (0.22, 0.54, 0.16), (0.86, 0.76, 0.48),
        (0.06, 0.08, 0.12), (0.92, 0.92, 0.92),
    ], dtype=np.float32)
    alphas = (1.0, 0.5, 0.25, 0.1)
    report = {}
    for color_index, color in enumerate(colors):
        straight_result = transform(color[None, :])[0]
        per_alpha = {}
        for alpha in alphas:
            premultiplied = color * np.float32(alpha)
            if premultiplied_aware:
                output_pm = transform((premultiplied / np.float32(alpha))[None, :])[0] * np.float32(alpha)
            else:
                output_pm = transform(premultiplied[None, :])[0]
            recovered = np.clip(output_pm / np.float32(alpha), 0.0, 1.0)
            per_alpha[str(alpha)] = {
                "max_rgb_delta_vs_straight": float(np.max(np.abs(recovered - straight_result))),
                "recovered_rgb": recovered.tolist(),
            }
        report[f"color_{color_index + 1}"] = {
            "source_rgb": color.tolist(), "straight_result": straight_result.tolist(), "premultiplied": per_alpha
        }
    maxima = [v["max_rgb_delta_vs_straight"] for c in report.values() for v in c["premultiplied"].values()]
    return {"colors": report, "max_rgb_delta": max(maxima, default=0.0)}


def number_list(values: np.ndarray) -> list[float | str]:
    return [float(x) if np.isfinite(x) else str(x) for x in values.reshape(-1)]


def float_contract_probe() -> dict:
    samples = {
        "black": (0.0, 0.0, 0.0),
        "mid_gray": (0.18, 0.18, 0.18),
        "white": (1.0, 1.0, 1.0),
        "hdr_positive": (1.5, 0.5, 0.1),
        "negative_and_overrange": (-0.2, 0.4, 1.4),
        "nan_input": (float("nan"), 0.2, 0.3),
        "infinite_input": (float("inf"), 0.2, 0.3),
    }
    results = {}
    for effect_name, transform in (
        ("color_balance_cinematic", lambda x, s: color_balance(x, s)),
        ("tritone_cinematic", lambda x, s: tritone(x, s)),
    ):
        effect_results = {}
        for sample_name, sample in samples.items():
            source = np.array(sample, dtype=np.float32)
            strengths = {}
            for strength in (0.0, 0.5, 1.0):
                with np.errstate(invalid="ignore", divide="ignore", over="ignore"):
                    output = transform(source[None, :], strength)[0]
                strengths[str(strength)] = {
                    "output_rgb": number_list(output),
                    "all_finite": bool(np.all(np.isfinite(output))),
                    "in_unit_range": bool(np.all(np.isfinite(output)) and np.all((output >= 0.0) & (output <= 1.0))),
                    "exact_input_match": bool(np.array_equal(output, source, equal_nan=True)),
                }
            effect_results[sample_name] = strengths
        results[effect_name] = effect_results
    return {
        "samples": results,
        "interpretation": "source equation mirror only. Unit-range inputs and HDR/negative inputs are kept separate; non-finite handling is not defined by these processors.",
    }


def main() -> None:
    source_hashes = verify_sources()
    cases, provenance = load_cases()
    OUT.mkdir(parents=True, exist_ok=True)
    images = OUT / "images"
    blind = OUT / "blind"
    images.mkdir(exist_ok=True)
    blind.mkdir(exist_ok=True)
    candidate_functions = {
        "lut_cinematic_s1": lambda rgb: np.asarray(__import__("cinematic_looks_lab").apply_look(quantize(rgb), "cinematic", 1.0), dtype=np.float32) / 255.0,
        "color_balance_cinematic_s0.5": lambda rgb: color_balance(rgb, 0.5),
        "color_balance_cinematic_s1": lambda rgb: color_balance(rgb, 1.0),
        "tritone_cinematic_s0.5": lambda rgb: tritone(rgb, 0.5),
        "tritone_cinematic_s1": lambda rgb: tritone(rgb, 1.0),
        "tritone_teal_orange_s1": lambda rgb: tritone(rgb, 1.0, TRITONE_TEAL_ORANGE),
    }
    stats: dict = {}
    review_key: dict = {}
    for case_name, rgb8 in cases.items():
        source = rgb8.astype(np.float32) / np.float32(255.0)
        rois = provenance[case_name].get("rois", {})
        baseline = rgb8
        candidate_outputs = {"baseline": baseline}
        for candidate, transform in candidate_functions.items():
            candidate_outputs[candidate] = quantize(transform(source))
        stats[case_name] = {}
        for candidate, result in candidate_outputs.items():
            Image.fromarray(result, "RGB").save(images / f"{case_name}__{candidate}.png")
            delta = np.abs(result.astype(np.int16) - baseline.astype(np.int16))
            regions = {"full": roi_stats(result, None)}
            for roi_name, roi in rois.items():
                regions[roi_name] = roi_stats(result, roi)
            stats[case_name][candidate] = {
                "regions": regions,
                "response_delta_code": {
                    "mae": float(delta.mean()),
                    "rmse": float(np.sqrt(np.mean(delta.astype(np.float32) ** 2))),
                    "p95": float(np.quantile(delta, 0.95)),
                    "max": int(delta.max()),
                    "changed_pixel_rate": float(np.mean(np.any(delta != 0, axis=-1))),
                    "channel_mae": delta.mean(axis=(0, 1)).tolist(),
                },
            }

        # Blind sheet includes control, LUT Cinematic, both main Cinematic effects,
        # and the plan's Teal/Orange comparison. No key is embedded in image names.
        ordered = list(candidate_outputs.items())
        seed = 20261116 ^ zlib.crc32(case_name.encode("utf-8"))
        random.Random(seed).shuffle(ordered)
        sheet = Image.new("RGB", (720, ((len(ordered) + 1) // 2) * 280), (24, 26, 30))
        draw = ImageDraw.Draw(sheet)
        case_key = {}
        for index, (candidate, result) in enumerate(ordered, start=1):
            code = f"{case_name[:2].upper()}X-{index:02d}"
            thumb = Image.fromarray(result, "RGB")
            thumb.thumbnail((360, 240))
            x, y = ((index - 1) % 2) * 360, ((index - 1) // 2) * 280
            sheet.paste(thumb, (x, y + 28))
            draw.text((x + 6, y + 6), code, fill=(245, 245, 245))
            case_key[code] = candidate
        sheet.save(blind / f"{case_name}_contact_sheet.png")
        review_key[case_name] = case_key

    # Alpha sweep distinguishes ColorBalance's premultiplied-aware effect wrapper
    # from Tritone's direct-RGB CPU wrapper. This remains source-equation evidence.
    alpha = {
        "color_balance_cinematic": alpha_association_probe(lambda x: color_balance(x, 1.0), True),
        "tritone_cinematic": alpha_association_probe(lambda x: tritone(x, 1.0), False),
        "tritone_teal_orange": alpha_association_probe(lambda x: tritone(x, 1.0, TRITONE_TEAL_ORANGE), False),
        "note": "Probe applies effect equations directly to premultiplied RGB, then unpremultiplies for comparison. It is not a C++ differential or runtime capture.",
    }
    report = {
        "rd": "rd0_effect_extension",
        "status": "source_mirror_response_characterization_only",
        "source_hashes": source_hashes,
        "harness_sha256": sha256(Path(__file__).resolve()),
        "candidate_model": "ColorBalanceProcessor::applyPixel and TritoneProcessor::applyPixel equations, normalized channel values; LUT Cinematic output reused via existing pinned mirror",
        "image_contract": "same 5 deterministic/CC0 RGB uint8 cases and stored ROIs as rd0; normalized sRGB code values used directly because source processors operate channel values without transfer conversion",
        "candidates": list(candidate_functions),
        "case_provenance": provenance,
        "metrics": stats,
        "alpha_association_probe": alpha,
        "float_contract_probe": float_contract_probe(),
        "quality_verdict": "not assigned; these transforms have no approved target images and no human blind review",
    }
    write_json_lf(OUT / "metrics.json", report)
    write_json_lf(OUT / "blind_review_key.json", review_key)
    form = [
        "# Color Effect rd0 Extension — blind review form", "", "**Status:** Blank review template; no aesthetic or quality verdict assigned.",
        "", "Review the contact sheets in `blind/` before opening `blind_review_key.json`. IDs are case-local; each case contains baseline plus six effect candidates.",
        "", "Scoring: technical quality 0–3/N/A; look consistency 1–4; preference 1–5. Record defects by region and severity. These effects have not been captured in Artifact runtime.",
        "", f"Reviewer: ______  Date: ______  Display conditions: ______", "",
    ]
    for case_name, case_key in review_key.items():
        form.extend([f"## {case_name}", "", f"Contact sheet: `blind/{case_name}_contact_sheet.png`", "", "| Blind ID | Technical 0–3/N/A | Consistency 1–4 | Preference 1–5 | Defect / region / severity |", "|---|---:|---:|---:|---|"])
        form.extend(f"| {code} | | | | |" for code in case_key)
        form.append("")
    (OUT / "BLIND_REVIEW_FORM.md").write_text("\n".join(form) + "\n", encoding="utf-8", newline="\n")
    print(f"Wrote source-mirror response characterization for {len(cases)} cases to {OUT}")


if __name__ == "__main__":
    main()
