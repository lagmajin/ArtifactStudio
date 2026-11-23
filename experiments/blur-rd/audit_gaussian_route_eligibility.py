"""Create a static, hash-anchored Gaussian render-route eligibility audit."""

from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(__file__).resolve().parent / "results" / "gaussian_route_eligibility_audit_2026-11-12.json"
PLOT = Path(__file__).resolve().parent / "results" / "previews" / "gaussian_adjustment_radius_match.svg"
SOURCES = {
    "effect_interface": ROOT / "Artifact/include/Effects/GauusianBlur.ixx",
    "effect_implementation": ROOT / "Artifact/src/Effects/Blur/GauusianBlur.cpp",
    "composition_controller": ROOT / "Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cpp",
    "resident_pipeline": ROOT / "Artifact/src/Render/ArtifactRenderLayerPipeline.cpp",
    "effect_service": ROOT / "Artifact/src/Service/ArtifactEffectService.cpp",
}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def line_of(text: str, fragment: str) -> int | None:
    for index, line in enumerate(text.splitlines(), 1):
        if fragment in line:
            return index
    return None


def adjustment_radii(sigma: float) -> tuple[int, int]:
    # Mirror the C++ float32 operations and integer truncation in the controller.
    value = np.float32(sigma)
    cpu_width = max(3, int(np.float32(value * np.float32(6.0))) | 1)
    cpu_radius = (cpu_width - 1) // 2
    shader_sigma = max(np.float32(0.1), value)
    resident_radius = int(math.ceil(float(np.float32(shader_sigma * np.float32(3.0)))))
    return cpu_radius, resident_radius


def gaussian_cpu_radius(sigma: float) -> int:
    # Mirror GaussianBlurCPUImpl::applyCPU at noninteractive/full resolution.
    value = np.float32(sigma)
    width = max(1, int(np.float32(np.float32(6.0) * value + np.float32(1.0))))
    if width % 2 == 0:
        width += 1
    return (width - 1) // 2


def write_radius_plot() -> None:
    width, height = 820, 510
    left, top, plot_width, plot_height = 78, 50, 690, 380

    def px(sigma: float) -> float:
        return left + (sigma / 3.0) * plot_width

    def py(radius: float) -> float:
        return top + plot_height - (radius / 10.0) * plot_height

    def path_for(which: int) -> str:
        chunks = []
        previous_radius = None
        previous_x = None
        for index in range(3001):
            sigma = np.float32(index / 1000.0)
            radius = adjustment_radii(float(sigma))[which]
            x = px(float(sigma))
            y = py(radius)
            if index == 0:
                chunks.append(f"M{x:.2f},{y:.2f}")
            elif radius == previous_radius:
                chunks.append(f"H{x:.2f}")
            else:
                chunks.append(f"H{previous_x:.2f} V{y:.2f} H{x:.2f}")
            previous_radius = radius
            previous_x = x
        return " ".join(chunks)

    cpu_path = path_for(0)
    resident_path = path_for(1)
    ui_values = [i / 10.0 for i in range(1, 30)]
    ui_match = [value for value in ui_values if adjustment_radii(value)[0] == adjustment_radii(value)[1]]
    grid = []
    for radius in range(0, 11, 2):
        y = py(radius)
        grid.append(f'<line x1="{left}" y1="{y:.2f}" x2="{left + plot_width}" y2="{y:.2f}" stroke="#d6dbe2"/>')
        grid.append(f'<text x="{left - 14}" y="{y + 5:.2f}" text-anchor="end">{radius}</text>')
    for index in range(0, 31, 5):
        sigma = index / 10.0
        x = px(sigma)
        grid.append(f'<line x1="{x:.2f}" y1="{top}" x2="{x:.2f}" y2="{top + plot_height}" stroke="#e5e8ed"/>')
        grid.append(f'<text x="{x:.2f}" y="{top + plot_height + 22}" text-anchor="middle">{sigma:.1f}</text>')
    # Put a marker row above the response curves for UI-step values that pass the source gate.
    marks = " ".join(
        f'<circle cx="{px(value):.2f}" cy="{py(9.5):.2f}" r="4" fill="#1769aa"><title>UI sigma {value:.1f}: radius match</title></circle>'
        for value in ui_match
    )
    svg = f'''<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">
<rect width="100%" height="100%" fill="#ffffff"/><style>text{{font:14px sans-serif;fill:#222}}.small{{font-size:12px}}</style>
<text x="{width/2}" y="26" text-anchor="middle" style="font-size:18px;font-weight:600">Adjustment Gaussian: CPU vs resident support radius</text>
{''.join(grid)}
<rect x="{left}" y="{py(10):.2f}" width="{px(1/3)-left:.2f}" height="{py(0)-py(10):.2f}" fill="#58a76b" opacity=".10"/>
<path d="{cpu_path}" fill="none" stroke="#c44e52" stroke-width="2.5"/>
<path d="{resident_path}" fill="none" stroke="#3976af" stroke-width="2.5"/>
{marks}
<text x="{left + plot_width/2}" y="{height - 14}" text-anchor="middle">Requested sigma</text>
<text transform="translate(20 {top + plot_height/2}) rotate(-90)" text-anchor="middle">Support radius (pixels)</text>
<line x1="{left + 18}" y1="{height - 54}" x2="{left + 48}" y2="{height - 54}" stroke="#c44e52" stroke-width="3"/><text x="{left + 55}" y="{height - 49}">CPU radius</text>
<line x1="{left + 180}" y1="{height - 54}" x2="{left + 210}" y2="{height - 54}" stroke="#3976af" stroke-width="3"/><text x="{left + 218}" y="{height - 49}">resident radius</text>
<circle cx="{left + 390}" cy="{height - 54}" r="4" fill="#1769aa"/><text x="{left + 400}" y="{height - 49}">default UI .1-step values accepted: {', '.join(map(str, ui_match))}</text>
<rect x="{width - 156}" y="{height - 66}" width="12" height="12" fill="#58a76b" opacity=".4"/><text x="{width - 138}" y="{height - 55}" class="small">match band</text>
</svg>'''
    PLOT.parent.mkdir(parents=True, exist_ok=True)
    PLOT.write_text(svg, encoding="utf-8", newline="\n")


def main() -> None:
    texts = {key: path.read_text(encoding="utf-8") for key, path in SOURCES.items()}
    interface = texts["effect_interface"]
    implementation = texts["effect_implementation"]
    controller = texts["composition_controller"]
    pipeline = texts["resident_pipeline"]
    effect_service = texts["effect_service"]

    checks = {
        "effect_supports_gpu": "bool supportsGPU() const override" in interface,
        "effect_domain_spatial": "return GpuRasterEffectDomain::Spatial;" in interface,
        "effect_node_marks_sigma_resolution_scaled": "node.resolutionScaledParameterMask = 1u << 0;" in interface,
        "effect_default_compute_mode_gpu": "setComputeMode(ComputeMode::GPU);" in implementation,
        "property_hard_max_64": "sigmaProp.setHardRange(0.0, 64.0);" in implementation,
        "normal_plan_rejects_adjustment_layers": "layer->isAdjustmentLayer()" in controller[controller.find("bool buildGpuRasterEffectPlan"):controller.find("// Identifies a cached adjustment-mask")],
        "normal_plan_rejects_cpu_compute": "effect->computeMode() == ComputeMode::CPU" in controller[controller.find("bool buildGpuRasterEffectPlan"):controller.find("// Identifies a cached adjustment-mask")],
        "normal_plan_requires_spatial_nodes": "GpuRasterEffectDomain::Spatial" in controller[controller.find("bool buildGpuRasterEffectPlan"):controller.find("// Identifies a cached adjustment-mask")],
        "normal_plan_has_no_sigma_below_3_gate": "sigma >= 3.0f" not in controller[controller.find("bool buildGpuRasterEffectPlan"):controller.find("// Identifies a cached adjustment-mask")],
        "adjustment_path_requires_sigma_below_3": "primarySigma >= 3.0f" in controller,
        "adjustment_path_checks_radius_match": "std::ceil(std::max(0.1f, sigma) * 3.0f)" in controller,
        "resident_radius_cap_64": "min(64, (int)ceil(sigma * 3.0))" in pipeline,
    }
    missing = [name for name, passed in checks.items() if not passed]
    if missing:
        raise SystemExit(f"source contract check failed: {missing}")

    resident_scales = {
        "high_or_full": 1.0,
        "medium": 0.5,
        "low_or_interactive_draft": 0.25,
    }
    cap_applied_sigma = 64.0 / 3.0
    normal_layer_thresholds = {}
    for label, scale in resident_scales.items():
        requested_threshold = cap_applied_sigma / scale
        normal_layer_thresholds[label] = {
            "effect_resolution_scale": scale,
            "requested_sigma_strictly_above_to_hit_radius_cap": requested_threshold,
            "reachable_with_property_hard_max_64": requested_threshold < 64.0,
            "requested_sigma_64_applied_sigma": 64.0 * scale,
            "requested_sigma_64_resident_radius": min(64, math.ceil(3.0 * min(64.0, 64.0 * scale))),
        }

    adjustment_values = [round(i / 10.0, 1) for i in range(1, 30)]
    adjustment_candidates = []
    for sigma in adjustment_values:
        gate_cpu_radius, resident_radius = adjustment_radii(sigma)
        gaussian_radius = gaussian_cpu_radius(sigma)
        adjustment_candidates.append({
            "sigma": sigma,
            "controller_gate_cpu_radius_blur_formula": gate_cpu_radius,
            "gaussian_effect_cpu_radius_formula": gaussian_radius,
            "resident_shader_radius": resident_radius,
            "gate_radius_matches_resident": gate_cpu_radius == resident_radius,
            "gaussian_cpu_radius_matches_resident": gaussian_radius == resident_radius,
            "source_gate_accepts_sigma": sigma < 3.0 and gate_cpu_radius == resident_radius,
            "accepted_but_gaussian_cpu_radius_differs": sigma < 3.0 and gate_cpu_radius == resident_radius and gaussian_radius != resident_radius,
            "rejected_but_gaussian_cpu_radius_matches": sigma < 3.0 and gate_cpu_radius != resident_radius and gaussian_radius == resident_radius,
        })

    sweep_step = 0.0001
    sweep_count = 29999
    matching_samples = []
    radius_pairs: dict[str, int] = {}
    for index in range(1, sweep_count + 1):
        sigma = np.float32(index * sweep_step)
        cpu_radius, resident_radius = adjustment_radii(float(sigma))
        pair = f"cpu_{cpu_radius}_resident_{resident_radius}"
        radius_pairs[pair] = radius_pairs.get(pair, 0) + 1
        if cpu_radius == resident_radius:
            matching_samples.append(float(sigma))

    float32_neighborhoods = []
    for index in range(1, 9):
        boundary = np.float32(index / 3.0)
        neighbors = [
            np.nextafter(boundary, np.float32(-np.inf), dtype=np.float32),
            boundary,
            np.nextafter(boundary, np.float32(np.inf), dtype=np.float32),
        ]
        float32_neighborhoods.append({
            "nominal_boundary_sigma": index / 3.0,
            "values_and_radii": [
                {"sigma_float32": float(value),
                 "cpu_radius": adjustment_radii(float(value))[0],
                 "resident_radius": adjustment_radii(float(value))[1],
                 "eligible_by_radius_match": adjustment_radii(float(value))[0] == adjustment_radii(float(value))[1]}
                for value in neighbors
            ],
        })

    source_map = {}
    for name, path in SOURCES.items():
        text = texts[name]
        source_map[name] = {
            "path": path.relative_to(ROOT).as_posix(),
            "sha256": sha(path),
            "anchors": {
                "effect_supports_gpu": line_of(text, "bool supportsGPU() const override") if name == "effect_interface" else None,
                "normal_plan_builder": line_of(text, "bool buildGpuRasterEffectPlan(") if name == "composition_controller" else None,
                "adjustment_sigma_gate": line_of(text, "primarySigma >= 3.0f") if name == "composition_controller" else None,
                "resident_radius_cap": line_of(text, "min(64, (int)ceil(sigma * 3.0))") if name == "resident_pipeline" else None,
            },
        }

    result = {
        "schema": "artifact.blur-rd.gaussian-route-eligibility-audit.v1",
        "runner": Path(__file__).relative_to(ROOT).as_posix(),
        "runner_sha256": sha(Path(__file__).resolve()),
        "radius_plot": PLOT.relative_to(ROOT).as_posix(),
        "evidence_class": "static source contract with derived parameter thresholds; no runtime dispatch or build",
        "source_files": source_map,
        "source_checks": checks,
        "normal_layer_resident_route": {
            "static_requirements": [
                "non-adjustment layer with an enabled rasterizer effect",
                "composition render has blend pipeline and immediate GPU context",
                "every enabled effect is rasterizer stage and not ComputeMode::CPU",
                "every effect in the stack can produce a supported pointwise or spatial GPU plan",
                "Gaussian Blur defaults to ComputeMode::GPU and declares spatial node support",
            ],
            "sigma_gate": "No sigma<3 gate in buildGpuRasterEffectPlan; sigma is multiplied by effectResolutionScale before dispatch.",
            "effect_resolution_scale_thresholds": normal_layer_thresholds,
            "unknown_at_static_stage": [
                "whether a specific layer/effect stack passes all conditions in a live composition",
                "whether effectResolutionScale also changes the actual input/output texture dimensions",
                "GPU device/backend dispatch success and final displayed-space reconstruction",
            ],
        },
        "adjustment_layer_resident_route": {
            "effect_kind_scope_audit": {
                "factory_registers_gaussian_blur": "if (effectId == QStringLiteral(\"effect.blur.gaussian\"))" in effect_service,
                "layer_add_effect_service_has_no_adjustment_type_guard": "isAdjustmentLayer" not in effect_service[effect_service.find("EffectServiceResult ArtifactEffectService::addEffectToLayer"):effect_service.find("EffectServiceResult ArtifactEffectService::removeEffectFromLayer")],
                "gaussian_emits_same_node_kind_as_blur": "node.kind = GpuSpatialEffectKind::SeparableGaussianBlur;" in interface,
                "controller_rejection_draws_original_accumulator": "drawBgraAccumulator(accumSRV);" in controller,
                "interpretation": "The route gate can receive GaussianBlur in principle. A mismatch describes the gate's support model, not proof of output error: route failure draws the original accumulated backdrop rather than calling Gaussian CPU.",
                "default_ui_step_0_1_accepted_with_gaussian_cpu_radius_mismatch": [row["sigma"] for row in adjustment_candidates if row["accepted_but_gaussian_cpu_radius_differs"]],
                "default_ui_step_0_1_rejected_although_gaussian_cpu_radius_matches_resident": [row["sigma"] for row in adjustment_candidates if row["rejected_but_gaussian_cpu_radius_matches"]],
            },
            "static_requirements": [
                "high LOD, not interacting, not downsampled",
                "full composition-sized render pipeline, no MSAA target",
                "identity transform, no layer/effect masks or effect region, opacity 1, normal blend",
                "spatial rasterizer effects only, effect mix 1, no overscan",
                "each primary sigma <3 and every node passes the CPU/resident radius equality check",
            ],
            "default_property_step_0_1_radius_match_values": [row["sigma"] for row in adjustment_candidates if row["source_gate_accepts_sigma"]],
            "default_property_step_0_1_rejected_radius_values": [row["sigma"] for row in adjustment_candidates if not row["source_gate_accepts_sigma"]],
            "candidate_rows": adjustment_candidates,
            "continuous_float32_sweep": {
                "range_exclusive_upper": [0.0, 3.0],
                "uniform_step": sweep_step,
                "sample_count": sweep_count,
                "radius_match_sample_count": len(matching_samples),
                "radius_match_fraction_of_samples": len(matching_samples) / sweep_count,
                "matching_sample_min_max": [matching_samples[0], matching_samples[-1]],
                "matching_samples_after_initial_interval": [value for value in matching_samples if value > 0.3333],
                "cpu_resident_radius_pair_sample_counts": radius_pairs,
                "float32_one_ulp_neighborhoods_around_sigma_k_over_3": float32_neighborhoods,
                "interpretation": "Finite 1e-4 grid is a density probe, not an exact interval proof; the one-ULP table exposes float-rounding behavior at nominal support breakpoints.",
            },
            "implication": "High-sigma radius-cap mismatch is excluded from this specialized adjustment-layer resident path by the sigma<3 gate; this does not imply all adjustment cases are GPU eligible.",
        },
        "recommended_capture_candidates": [
            {"case": "ordinary non-adjustment image layer, only Gaussian Blur enabled, high LOD/full scale, sigma=64", "reason": "strongest resident radius-cap condition; capture route and output support"},
            {"case": "same ordinary layer, medium LOD scale=.5, sigma=64", "reason": "applied sigma=32 crosses cap and helps isolate scale sensitivity"},
            {"case": "same ordinary layer, low/interactive scale=.25, sigma=64", "reason": "applied sigma=16 stays below cap; control condition"},
            {"case": "Adjustment Layer, sigma=.1..2.9 at default .1 steps", "reason": "verify source radius-match eligibility versus established fallback; separate from high-sigma study"},
        ],
        "scope_limit": "Static source analysis only. No build, test, executable, frame capture, GPU dispatch, visual acceptance, or performance measurement.",
    }
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8", newline="\n")
    write_radius_plot()
    print(OUT)


if __name__ == "__main__":
    main()
