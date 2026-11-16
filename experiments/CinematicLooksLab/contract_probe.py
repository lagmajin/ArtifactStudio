#!/usr/bin/env python3
"""Float-domain contract probe mirroring ColorLUT + LUTEffect source equations.

This is an analytical Python mirror, not a C++ differential test or product render.
"""

from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[1]
LUT_SOURCE = REPO / "ArtifactCore/src/Color/ColorLUT.cpp"
EFFECT_SOURCE = REPO / "Artifact/src/Effects/ColorCorrection/LUTEffect.cpp"
SOURCE_CONTRACT = ROOT / "SOURCE_CONTRACT.json"
OUT = ROOT / "captures/rd0/float_contract_probe.json"

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


def apply_effect(table: np.ndarray, rgba: tuple[float, float, float, float], intensity: float) -> list[float]:
    original = [float(v) for v in rgba[:3]]
    mapped = sample(table, tuple(original))
    if intensity >= 0.9999:
        rgb = mapped
    elif intensity > 0.0001:
        rgb = [a + (b - a) * intensity for a, b in zip(original, mapped)]
    else:
        rgb = original
    return [*rgb, float(rgba[3])]


def json_numbers(values: list[float]) -> list[float | str]:
    return [float(v) if math.isfinite(float(v)) else str(v) for v in values]


def main() -> None:
    source_contract = json.loads(SOURCE_CONTRACT.read_text(encoding="utf-8"))
    actual_hashes = {
        LUT_SOURCE.relative_to(REPO).as_posix(): sha256(LUT_SOURCE),
        EFFECT_SOURCE.relative_to(REPO).as_posix(): sha256(EFFECT_SOURCE),
    }
    if actual_hashes != source_contract["source_files"]:
        raise SystemExit(
            "LUT source changed since mirror review; inspect the source, update the "
            "Python mirror/evidence, then refresh SOURCE_CONTRACT.json."
        )
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
        "sources": {
            **actual_hashes,
        },
        "source_contract_sha256": sha256(SOURCE_CONTRACT),
        "format": "IEEE-like Python floats; 17^3 table and sampler arithmetic use numpy float32 where source stores float",
        "contract_notes": [
            "ColorLUT lookup sanitizes non-finite RGB to 0 and clamps lookup input/output to [0,1].",
            "LUTEffect intensity 0 leaves source RGB unchanged; intermediate values linearly mix source RGB with mapped RGB; intensity >= 0.9999 uses mapped RGB directly.",
            "LUTEffect copies RGBA then writes only RGB channels, so alpha is preserved by the inspected CPU implementation.",
            "HDR RGB is not preserved at full intensity. Intermediate intensity mixes HDR source values with the clamped LUT output.",
        ],
        "probes": {},
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
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with OUT.open("w", encoding="utf-8", newline="\n") as stream:
        json.dump(output, stream, ensure_ascii=False, indent=2, allow_nan=False)
        stream.write("\n")
    print(f"Wrote {len(LOOKS)} LUTs × {len(probes)} RGBA probes to {OUT}")


if __name__ == "__main__":
    main()
