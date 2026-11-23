#!/usr/bin/env python3
"""Compare alpha-preserving fog and volumetric-alpha compositing semantics."""

from __future__ import annotations

import json
from pathlib import Path


SOURCE_STRAIGHT = (1.25, 0.35, 0.08)  # Scene-linear HDR source color.
FOG_COLOR = (0.18, 0.42, 0.72)
ALPHA_VALUES = (0.0, 0.02, 0.1, 0.25, 0.5, 0.75, 1.0)
TRANSMITTANCE_VALUES = (0.9, 0.5, 0.1)
BACKGROUNDS = {
    "black": (0.0, 0.0, 0.0),
    "white": (1.0, 1.0, 1.0),
    "magenta": (0.8, 0.1, 0.7),
}


def add(a: tuple[float, float, float], b: tuple[float, float, float]) -> tuple[float, float, float]:
    return a[0] + b[0], a[1] + b[1], a[2] + b[2]


def scale(a: tuple[float, float, float], s: float) -> tuple[float, float, float]:
    return a[0] * s, a[1] * s, a[2] * s


def composite_premultiplied(
    rgb: tuple[float, float, float], alpha: float, background: tuple[float, float, float],
) -> tuple[float, float, float]:
    return add(rgb, scale(background, 1.0 - alpha))


def evaluate_case(alpha: float, transmittance: float) -> dict[str, object]:
    source_premultiplied = scale(SOURCE_STRAIGHT, alpha)
    fog_fraction = 1.0 - transmittance

    preserved_straight_rgb = add(scale(SOURCE_STRAIGHT, transmittance),
                                 scale(FOG_COLOR, fog_fraction))
    coverage_preserved = {
        "premultiplied_rgb": scale(preserved_straight_rgb, alpha),
        "alpha": alpha,
    }

    volume_alpha = {
        "premultiplied_rgb": add(scale(source_premultiplied, transmittance),
                                  scale(FOG_COLOR, fog_fraction)),
        "alpha": alpha * transmittance + fog_fraction,
    }

    # Models a fog RGB blend applied directly to premultiplied input while the
    # shader preserves the original alpha channel.
    naive_preserve_alpha = {
        "premultiplied_rgb": add(scale(source_premultiplied, transmittance),
                                  scale(FOG_COLOR, fog_fraction)),
        "alpha": alpha,
    }

    policies = {
        "coverage_preserved": coverage_preserved,
        "medium_alpha_included": volume_alpha,
        "naive_rgb_blend_alpha_preserved_on_premultiplied_input": naive_preserve_alpha,
    }
    output: dict[str, object] = {"input_alpha": alpha, "transmittance": transmittance,
                                 "policies": {}}
    for name, value in policies.items():
        premultiplied = value["premultiplied_rgb"]
        output["policies"][name] = {
            "premultiplied_rgb": premultiplied,
            "alpha": value["alpha"],
            "composite_over_backgrounds": {
                bg_name: composite_premultiplied(premultiplied, value["alpha"], background)
                for bg_name, background in BACKGROUNDS.items()
            },
        }
    return output


def main() -> None:
    cases = [evaluate_case(alpha, transmittance)
             for transmittance in TRANSMITTANCE_VALUES for alpha in ALPHA_VALUES]
    alpha_zero = [case for case in cases if case["input_alpha"] == 0.0]
    naive_alpha_zero_residual = max(
        max(abs(channel) for channel in case["policies"][
            "naive_rgb_blend_alpha_preserved_on_premultiplied_input"]["premultiplied_rgb"])
        for case in alpha_zero)
    result = {
        "schema": "fog-alpha-compositing-rd-v1",
        "input": {"representation": "scene-linear straight RGBA source; converted to premultiplied before policy evaluation",
                  "source_straight_rgb": SOURCE_STRAIGHT, "fog_rgb": FOG_COLOR,
                  "alpha_values": ALPHA_VALUES, "transmittance_values": TRANSMITTANCE_VALUES,
                  "backgrounds": BACKGROUNDS},
        "policies": {
            "coverage_preserved": "Fog the straight source color, keep coverage alpha, then premultiply. Fog contribution is limited by source coverage.",
            "medium_alpha_included": "Attenuate premultiplied source and add fog radiance; alpha becomes source_alpha*T + (1-T). This treats the medium as an occluding layer.",
            "naive_rgb_blend_alpha_preserved_on_premultiplied_input": "Blend fog into premultiplied RGB and retain source alpha, mirroring the shader's channel operation under a premultiplied-input assumption.",
        },
        "summary": {
            "naive_premultiplied_rgb_residual_at_zero_alpha_max": naive_alpha_zero_residual,
            "interpretation": "The two valid policies encode different layer semantics; this fixture cannot select the product contract. The naive policy leaks fog RGB with zero alpha when its input is premultiplied. Shader input alpha convention and downstream compositing are not confirmed.",
            "not_exercised": ["ArtifactStudio runtime", "GPU shader execution", "actual alpha-bearing renderer input", "color management", "temporal filtering"],
        },
        "cases": cases,
    }
    output_dir = Path("docs/worklog/artifacts/fog-alpha-compositing-rd-2026-11-12")
    output_dir.mkdir(parents=True, exist_ok=True)
    path = output_dir / "manifest.json"
    path.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Wrote {len(cases)} deterministic RGBA cases to {path}")
    print(f"Max naive premultiplied RGB residual at alpha=0: {naive_alpha_zero_residual:.6f}")


if __name__ == "__main__":
    main()
