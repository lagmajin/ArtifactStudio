#!/usr/bin/env python3
"""Audit Film Damage's shared random-distribution call schedule from source."""
from __future__ import annotations

import hashlib
import json
import platform
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
PLAN = ROOT / "experiments/FilmEffectsLab/plans/film-damage-control-rng-coupling-r1.json"
SOURCE = ROOT / "Artifact/src/Effects/Rasterizer/FilmDamageEffect.cpp"
OUT = ROOT / "experiments/FilmEffectsLab/captures/film-damage-control-rng-coupling-r1"
SIZES = ((256, 256), (512, 256), (512, 512), (1024, 512), (1024, 1024))
DEFAULT_DUST = 0.08
DEFAULT_SCRATCHES = 0.10


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def f32_mul(a: float | np.float32, b: int | float) -> np.float32:
    return np.float32(np.float32(a) * np.float32(b))


def dust_count(dust: float, width: int, height: int) -> tuple[int, list[float]]:
    # Preserve C++ left-to-right float expression rounding before int truncation.
    value = f32_mul(dust, width)
    stages = [float(value)]
    value = f32_mul(value, height)
    stages.append(float(value))
    value = np.float32(value / np.float32(180.0))
    stages.append(float(value))
    return int(value), stages


def scratch_count(scratches: float) -> tuple[int, float]:
    product = f32_mul(scratches, 18.0)
    return int(product), float(product)


def ledger(dust: float, scratches: float, width: int, height: int) -> dict:
    d_count, d_stages = dust_count(dust, width, height)
    s_count, s_product = scratch_count(scratches)
    uniform_dust = d_count * 5
    uniform_scratch = s_count * 5
    normal_scratch = s_count
    uniform_pre_flicker = uniform_dust + uniform_scratch
    normal_pre_flicker = 2 + normal_scratch
    return {
        "width": width, "height": height,
        "dust": dust, "scratches": scratches,
        "dust_float32_stages": d_stages,
        "scratch_float32_product": s_product,
        "dust_count": d_count, "scratch_count": s_count,
        "distribution_invocations": {
            "normal_gate_weave": 2,
            "uniform_dust_marks": uniform_dust,
            "uniform_scratch_marks": uniform_scratch,
            "normal_scratch_marks": normal_scratch,
            "normal_flicker": 1,
            "uniform_burn_center": 2,
            "uniform_total": uniform_pre_flicker + 2,
            "normal_total": normal_pre_flicker + 1,
        },
        "high_level_cursor_before": {
            "flicker_gain": {"uniform_calls": uniform_pre_flicker, "normal_calls": normal_pre_flicker},
            "burn_center_x": {"uniform_calls": uniform_pre_flicker, "normal_calls": normal_pre_flicker + 1},
            "burn_center_y": {"uniform_calls": uniform_pre_flicker + 1, "normal_calls": normal_pre_flicker + 1},
        },
    }


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    source = SOURCE.read_text(encoding="utf-8")
    markers = {
        "dust_count_formula": "static_cast<int>(dust * width * height / 180.0f)" in source,
        "scratch_count_formula": "static_cast<int>(scratches * 18.0f)" in source,
        "five_dust_uniform_calls": "unit(rng) * width" in source and "unit(rng) * height" in source and "unit(rng) * 4.0f" in source and "unit(rng) > 0.55f" in source and "0.25f + unit(rng) * 0.5f" in source,
        "five_scratch_uniform_calls": "unit(rng) * height * 0.35f" in source and "0.25f + unit(rng) * 0.7f" in source and "unit(rng) > 0.45f" in source and "0.18f + unit(rng) * 0.45f" in source,
        "one_scratch_normal_call": "normal(rng) * 2.0f" in source,
        "flicker_draw_occurs": "const float flickerGain = 1.0f + normal(rng) * flicker;" in source,
        "burn_center_draws_after_damage": source.find("const float burnCenterX") > source.find("const int scratchCount"),
        "grain_uses_independent_row_rng": "std::mt19937 rowRng" in source and "rowNormal(rowRng) * grain" in source,
        "grain_row_rng_includes_y": "static_cast<std::uint32_t>(y) * 0x85ebca6bu" in source,
    }
    cases = []
    for width, height in SIZES:
        for dust in (0.0, 0.01, 0.08, 0.16, 1.0):
            cases.append({"sweep": "dust", **ledger(dust, DEFAULT_SCRATCHES, width, height)})
        for scratches in (0.0, 0.05, 0.0555556, 0.10, 0.25, 1.0):
            cases.append({"sweep": "scratches", **ledger(DEFAULT_DUST, scratches, width, height)})
        for label, dust, scratches in (("zero_damage", 0.0, 0.0),
                                       ("defaults", DEFAULT_DUST, DEFAULT_SCRATCHES),
                                       ("max_damage", 1.0, 1.0)):
            cases.append({"sweep": label, **ledger(dust, scratches, width, height)})

    baseline = ledger(DEFAULT_DUST, DEFAULT_SCRATCHES, 512, 512)
    for case in cases:
        inv = case["distribution_invocations"]
        base = baseline["distribution_invocations"]
        case["delta_vs_default_512_square"] = {
            "uniform_calls_before_flicker": case["high_level_cursor_before"]["flicker_gain"]["uniform_calls"] - baseline["high_level_cursor_before"]["flicker_gain"]["uniform_calls"],
            "normal_calls_before_flicker": case["high_level_cursor_before"]["flicker_gain"]["normal_calls"] - baseline["high_level_cursor_before"]["flicker_gain"]["normal_calls"],
            "uniform_calls_before_burn_center": case["high_level_cursor_before"]["burn_center_x"]["uniform_calls"] - baseline["high_level_cursor_before"]["burn_center_x"]["uniform_calls"],
            "total_uniform_distribution_calls": inv["uniform_total"] - base["uniform_total"],
            "total_normal_distribution_calls": inv["normal_total"] - base["normal_total"],
        }

    metrics = {
        "study": json.loads(PLAN.read_text(encoding="utf-8"))["plan_id"],
        "python": platform.python_version(),
        "numpy": np.__version__,
        "input_sha256": {"plan": digest(PLAN.read_bytes()), "source": digest(SOURCE.read_bytes()),
                         "runner": digest(Path(__file__).read_bytes())},
        "source_markers": markers,
        "ledger_semantics": {
            "distribution_invocations_are_not_raw_mt19937_draw_counts": True,
            "std_distribution_engine_consumption": "implementation-dependent and intentionally not modeled",
            "cursor_is_high_level_call_count_only": True,
            "film_burn_center_drawn_even_at_zero_film_burn": True,
            "row_grain_rng_is_separate_from_shared_overlay_rng": True,
        },
        "conditions": {"seed_label": 1977, "evolution": 0.0, "sizes_wh": SIZES,
                       "dust_sweep": [0.0, 0.01, 0.08, 0.16, 1.0],
                       "scratch_sweep": [0.0, 0.05, 0.0555556, 0.10, 0.25, 1.0],
                       "default_dust": DEFAULT_DUST, "default_scratches": DEFAULT_SCRATCHES},
        "baseline_512_square": baseline,
        "cases": cases,
        "limitations": ["No C++ or std::mt19937 execution", "No exact random values or rendered positions",
                        "No rendered images or perceptual temporal stability", "No runtime, backend or performance measurement"],
    }
    out_path = OUT / "metrics.json"
    out_path.write_text(json.dumps(metrics, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    checksums = {}
    for path in sorted(OUT.iterdir()):
        if path.is_file() and path.name != "checksums.json":
            checksums[str(path.relative_to(ROOT)).replace("\\", "/")] = digest(path.read_bytes())
    (OUT / "checksums.json").write_text(json.dumps(checksums, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps({"cases": len(cases), "all_source_markers": all(markers.values()),
                      "baseline": baseline["distribution_invocations"], "output": str(OUT)}, ensure_ascii=False))


if __name__ == "__main__":
    main()
