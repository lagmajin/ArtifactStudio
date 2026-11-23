#!/usr/bin/env python3
"""Generate three-point, single-parameter Principled response fixtures."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


FIXTURES = {
    "metallic_zero.json": ("Metallic", 0.0),
    "metallic_half.json": ("Metallic", 0.5),
    "metallic_one.json": ("Metallic", 1.0),
    "roughness_floor.json": ("Roughness", 0.04),
    "roughness_half.json": ("Roughness", 0.5),
    "roughness_one.json": ("Roughness", 1.0),
    "emission_zero.json": ("EmissionStrength", 0.0),
    "emission_1_25.json": ("EmissionStrength", 1.25),
    "emission_2_5.json": ("EmissionStrength", 2.5),
}


def _graph(socket: str, value: float) -> dict[str, object]:
    nodes: list[dict[str, object]] = []
    links: list[dict[str, str]] = []
    if socket == "EmissionStrength":
        nodes.extend([
            {"id": "emission_color", "type": "RGB", "params": {
                "r": 0.25, "g": 0.5, "b": 1.0, "a": 1.0}},
            {"id": "parameter_value", "type": "Value", "params": {"value": value}},
        ])
        links.extend([
            {"from": "emission_color", "out": "Color", "to": "surface", "in": "EmissionColor"},
            {"from": "parameter_value", "out": "Value", "to": "surface", "in": "EmissionStrength"},
        ])
    else:
        nodes.append({"id": "parameter_value", "type": "Value", "params": {"value": value}})
        links.append({
            "from": "parameter_value", "out": "Value", "to": "surface", "in": socket,
        })
    nodes.extend([
        {"id": "surface", "type": "Principled", "params": {}},
        {"id": "output", "type": "MaterialOutput", "params": {}},
    ])
    links.append({"from": "surface", "out": "BSDF", "to": "output", "in": "Surface"})
    return {"version": 1, "kind": "artifact-material-graph", "nodes": nodes, "links": links}


def _expected_raster_surface_input(socket: str, value: float) -> dict[str, object]:
    if socket == "Metallic":
        return {"metallic": min(max(value, 0.0), 1.0)}
    if socket == "Roughness":
        return {"roughness": min(max(value, 0.04), 1.0)}
    strength = max(value, 0.0)
    return {"emissionRgbLinear": [0.25 * strength, 0.5 * strength, strength]}


def generate(project_root: Path) -> None:
    fixture_root = project_root / "tests" / "fixtures" / "shader-graph-rd"
    manifest_path = fixture_root / "evaluation_manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    entries = {entry["file"]: entry for entry in manifest["fixtures"]}
    coverage_rows = manifest["fixtureEvaluationCoverage"]
    generated: list[str] = []
    for filename, (socket, value) in FIXTURES.items():
        payload = _graph(socket, value)
        encoded = (json.dumps(payload, indent=2, ensure_ascii=False) + "\n").encode("utf-8")
        (fixture_root / filename).write_bytes(encoded)
        digest = hashlib.sha256(encoded).hexdigest().upper()
        entry = {
            "file": filename,
            "sha256": digest,
            "class": "positive-surface-parameter-sweep",
            "purpose": f"isolated Principled {socket} response at {value:g}",
            "expectedValidation": f"compile a single-surface-parameter graph with {socket}={value:g}",
            "expectedRasterSurfaceInput": _expected_raster_surface_input(socket, value),
            "expectedRasterSurfaceInputScope": (
                "linear shader input after current graph lowering and Raster epilogue; "
                "not a rendered-pixel prediction"
            ),
            "runtimeStatus": "source-added; not-built-or-run",
        }
        entries[filename] = entry
        generated.append(filename)

    manifest["fixtures"] = [
        entry for entry in manifest["fixtures"]
        if entry.get("file") not in FIXTURES
    ] + [entries[filename] for filename in FIXTURES]
    sweep_rows = [
        row for row in coverage_rows
        if isinstance(row, dict) and
        row.get("runtimeRoute") == "productionNumericParameterSweep"
    ]
    sweep_row = {
        "fixtures": list(FIXTURES),
        "cpuContract": "single-parameter-surface-fixture-structure",
        "runtimeRoute": "productionNumericParameterSweep",
        "runtimeComparison": (
            "three single-input graph samples compared at adjacent values under identical production-layer conditions; "
            "8-bit response smoke only, not curve-shape or HDR numeric acceptance"
        ),
        "evidenceState": "source-added-runtime-unverified",
    }
    if sweep_rows:
        first = coverage_rows.index(sweep_rows[0])
        coverage_rows[:] = [
            row for row in coverage_rows
            if not (isinstance(row, dict) and
                    row.get("runtimeRoute") == "productionNumericParameterSweep")
        ]
        coverage_rows.insert(first, sweep_row)
    else:
        coverage_rows.append(sweep_row)
    manifest_path.write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    print("Generated: " + ", ".join(generated))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path,
                        default=Path(__file__).resolve().parents[2])
    args = parser.parse_args()
    generate(args.project_root.resolve())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
