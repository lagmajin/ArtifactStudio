#!/usr/bin/env python3
"""Static integer-schedule model for film effects; does not render pixels."""
from __future__ import annotations

import hashlib
import json
import platform
from fractions import Fraction
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PLAN = ROOT / "experiments/FilmEffectsLab/plans/temporal-seed-key-audit-r1.json"
GRUNGE_SRC = ROOT / "Artifact/src/Effect/ArtifactCreativeEffects.cpp"
DAMAGE_SRC = ROOT / "Artifact/src/Effects/Rasterizer/FilmDamageEffect.cpp"
EFFECT_HOST_SRC = ROOT / "Artifact/src/Effects/ArtifactAbstractEffect.cpp"
MASK = 0xFFFFFFFF
SEED = 1977
GRUNGE_MULT = 1013904223
DAMAGE_MULT = 0x9E3779B9


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def schedule(samples: list[Fraction], key_fn) -> dict:
    keys = [key_fn(x) for x in samples]
    deltas = [b[0]-a[0] for a, b in zip(keys, keys[1:])]
    return {
        "sample_count": len(samples),
        "unique_integer_key_count": len(set(keys)),
        "unique_uint32_seed_count": len({key_fn(x)[1] for x in samples}),
        "adjacent_same_key_pairs": sum(a == b for a, b in zip(keys, keys[1:])),
        "adjacent_changed_key_pairs": sum(a != b for a, b in zip(keys, keys[1:])),
        "adjacent_skipped_integer_key_pairs": sum(abs(d) > 1 for d in deltas),
        "key_delta_min_max": [min(deltas), max(deltas)] if deltas else [0, 0],
        "first_12_samples": [
            {"sample": str(t), "key": int(k), "seed_u32": int(seed)}
            for t, (k, seed) in list(zip(samples, keys))[:12]
        ],
        "last_sample": {"sample": str(samples[-1]), "key": int(keys[-1][0]), "seed_u32": int(keys[-1][1])},
    }


def grunge_key(frame: Fraction) -> tuple[int, int]:
    # Source casts frameSeed to uint32 after integer affine mapping.
    frame_i = int(frame)
    seed = ((SEED * 1664525) + (frame_i * GRUNGE_MULT)) & MASK
    return frame_i, seed


def damage_key(evolution: Fraction) -> tuple[int, int]:
    # Schedules are nonnegative rational values, so floor is exact here.
    frame_key = (evolution.numerator * 24) // evolution.denominator
    seed = (SEED ^ ((frame_key * DAMAGE_MULT) & MASK)) & MASK
    return frame_key, seed


def main() -> dict:
    plan = json.loads(PLAN.read_text(encoding="utf-8"))
    grunge_text, damage_text = GRUNGE_SRC.read_text(encoding="utf-8"), DAMAGE_SRC.read_text(encoding="utf-8")
    host_text = EFFECT_HOST_SRC.read_text(encoding="utf-8")
    markers = {
        "grunge_frame_seed": "1664525ull" in grunge_text and "1013904223ull" in grunge_text,
        "damage_evolution_quantizer": "std::floor(evolution * 24.0f)" in damage_text,
        "damage_seed_mix": "0x9e3779b9u" in damage_text,
        "damage_evolution_float_property": 'evolution.setType(PropertyType::Float)' in damage_text,
        "float_effect_properties_animatable": "supportsEffectPropertyAnimation(*property)" in host_text and "case PropertyType::Float:" in host_text,
        "keyframes_evaluated_at_composition_time": "const RationalTime time(context.compositionFrame, frameRate)" in host_text and "property->evaluateValue(" in host_text,
        "animated_value_applied_to_effect": "setPropertyValue(property->getName(), value)" in host_text,
    }
    if not all(markers.values()):
        raise RuntimeError(f"Source equation marker mismatch: {markers}")

    count = 120
    schedules = {
        "grunge_120_composition_frames": {"frames": 24, "property_slope_per_second": None,
            **schedule([Fraction(i) for i in range(count)], grunge_key)},
        "damage_1_over_24_at_24fps": {"timeline_fps": 24, "evolution_value_slope_per_second": 1.0,
            **schedule([Fraction(i, 24) for i in range(count)], damage_key)},
        "damage_1_over_30_at_30fps": {"timeline_fps": 30, "evolution_value_slope_per_second": 1.0,
            **schedule([Fraction(i, 30) for i in range(count)], damage_key)},
        "damage_1_over_60_at_60fps": {"timeline_fps": 60, "evolution_value_slope_per_second": 1.0,
            **schedule([Fraction(i, 60) for i in range(count)], damage_key)},
        "damage_1_over_48_at_24fps": {"timeline_fps": 24, "evolution_value_slope_per_second": .5,
            **schedule([Fraction(i, 48) for i in range(count)], damage_key)},
        "damage_1_over_12_at_24fps": {"timeline_fps": 24, "evolution_value_slope_per_second": 2.0,
            **schedule([Fraction(i, 12) for i in range(count)], damage_key)},
    }
    report = {
        "plan_id": plan["plan_id"],
        "plan_sha256": digest(PLAN),
        "script_sha256": digest(Path(__file__)),
        "source_sha256": {"grunge": digest(GRUNGE_SRC), "damage": digest(DAMAGE_SRC), "effect_host": digest(EFFECT_HOST_SRC)},
        "source_markers_verified": markers,
        "python": platform.python_version(),
        "base_seed": SEED,
        "semantics": {
            "uint32": "modulo 2^32",
            "Film Damage key": "floor(evolution * 24), then seed XOR uint32(key * 0x9e3779b9)",
            "Film Grunge key": "composition frame, then uint32(seed * 1664525 + frame * 1013904223)",
            "Evolution property": "animatable Float evaluated at RationalTime(compositionFrame, frameRate); property value is an arbitrary scalar, not intrinsically seconds or frames",
            "not_measured": "random engine output, rendered pixels, or perceived temporal stability",
        },
        "schedules": schedules,
        "limits": "Source-derived scheduling only. Evolution value slope is declared per schedule, but the value itself has no intrinsic time unit. Rational arithmetic does not model float32 boundary rounding. No pixel/effect runtime was executed.",
    }
    out_dir = ROOT / "experiments/FilmEffectsLab/captures/temporal-seed-key-audit-r1"
    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / "metrics.json"
    out_path.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    report["metrics_sha256"] = digest(out_path)
    (out_dir / "metrics.sha256").write_text(report["metrics_sha256"] + "  metrics.json\n", encoding="ascii", newline="\n")
    print(json.dumps({"output": str(out_path), "schedule_summaries": {
        name: {k: v for k, v in values.items() if k not in ("first_12_samples", "last_sample")}
        for name, values in report["schedules"].items()
    }}, indent=2))
    return report


if __name__ == "__main__":
    main()
