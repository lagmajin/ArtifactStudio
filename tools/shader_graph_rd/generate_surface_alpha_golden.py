#!/usr/bin/env python3
"""Generate CPU-only candidate probes for the current material alpha path."""

from __future__ import annotations

import argparse
import json
import math
import struct
from pathlib import Path


def f32(value: float) -> float:
    result = struct.unpack(">f", struct.pack(">f", value))[0]
    if not math.isfinite(result):
        raise ValueError("surface alpha probe must remain finite binary32")
    return 0.0 if result == 0.0 else result


def material_alpha(base_alpha: float, opacity_alpha: float, graph_alpha: float) -> float:
    # Current HLSL path: materialAlpha = baseColor.a * opacitySample.a;
    # graph epilogue assigns graphBaseColor.a * graphAlpha.
    return f32(f32(base_alpha * opacity_alpha) * graph_alpha)


def blend_straight(src_rgb: list[float], src_alpha: float, dst_rgb: list[float], dst_alpha: float) -> dict:
    rgb = [f32(f32(src_rgb[index] * src_alpha) + f32(dst_rgb[index] * (1.0 - src_alpha))) for index in range(3)]
    alpha = f32(f32(src_alpha) + f32(dst_alpha * (1.0 - src_alpha)))
    return {"rgb": rgb, "alpha": alpha}


def premultiplied_input_to_current_blend(src_rgb_premultiplied: list[float], src_alpha: float, dst_rgb: list[float], dst_alpha: float) -> dict:
    # Feeding premultiplied RGB into SRC_ALPHA blending multiplies source RGB
    # by alpha a second time. This intentionally models the current PSO state.
    return blend_straight(src_rgb_premultiplied, src_alpha, dst_rgb, dst_alpha)


def build_golden() -> dict:
    probes = []
    for alpha in (0.0, 0.5, 1.0):
        probes.append({
            "caseId": f"graph-base-alpha-{alpha:g}",
            "baseColorAlpha": alpha,
            "opacitySampleAlpha": 0.4,
            "principledGraphAlpha": 0.5,
            "expectedMaterialAlpha": material_alpha(alpha, 0.4, 0.5),
            "formula": "((baseColor.a * opacitySample.a) from the fixed path is replaced by graphBaseColor.a * graphAlpha when the graph splice is active)",
        })
    straight_rgb = [0.8, 0.2, 0.1]
    alpha = 0.5
    backgrounds = {
        "black": [0.0, 0.0, 0.0],
        "white": [1.0, 1.0, 1.0],
    }
    straight = {}
    premultiplied_misinterpreted_as_straight = {}
    for name, background in backgrounds.items():
        straight[name] = blend_straight(straight_rgb, alpha, background, 1.0)
        premultiplied_rgb = [f32(component * alpha) for component in straight_rgb]
        premultiplied_misinterpreted_as_straight[name] = premultiplied_input_to_current_blend(premultiplied_rgb, alpha, background, 1.0)
    return {
        "schemaVersion": 1,
        "kind": "artifact-shader-graph-surface-alpha-golden",
        "status": "source-derived-cpu-formula-candidate-no-renderer-runtime",
        "binary32": "Each multiply/add intermediate is rounded to IEEE-754 binary32 by the host oracle; this is not a GPU rounding claim.",
        "sourceContract": {
            "fixedPath": "materialAlpha = baseColor.a * opacitySample.a",
            "graphOverride": "materialAlpha = graphBaseColor.a * graphAlpha",
            "transparentRgb": "srcRgb * srcAlpha + dstRgb * (1-srcAlpha)",
            "transparentAlpha": "srcAlpha + dstAlpha * (1-srcAlpha)",
        },
        "alphaCompositionCases": probes,
        "associationComparison": {
            "sourceStraightRgb": straight_rgb,
            "sourceAlpha": alpha,
            "backgrounds": backgrounds,
            "straightInputComposites": straight,
            "premultipliedInputPassedToCurrentStraightBlend": premultiplied_misinterpreted_as_straight,
            "purpose": "show the current straight-alpha blend equation darkens premultiplied RGB a second time; this does not select the product association policy",
        },
        "limitations": [
            "CPU formula candidate only; no HLSL compiler, GPU blend state, or image capture executed",
            "current source equations are observed behavior, not a product-approved color/alpha contract",
            "RGBA32F capture and black/white composite remain required to inspect actual backend output",
            "sRGB transfer and alpha association are independent policy dimensions",
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path("tests/fixtures/shader-graph-rd/surface_alpha_formula_golden.json"))
    args = parser.parse_args()
    args.output.write_text(json.dumps(build_golden(), ensure_ascii=False, indent=2, allow_nan=False) + "\n", encoding="utf-8", newline="\n")
    print(f"Wrote {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
