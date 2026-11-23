"""Build pending capture runs for Gaussian on Adjustment Layers.

This records reproducible proposed runs and source-derived gate expectations;
it does not capture or claim that any GPU route executed.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
BUNDLE = ROOT / "experiments/blur-rd/fixtures/gaussian-r1"
FIXTURE_MANIFEST = BUNDLE / "manifest.json"
EXR_ASSET_MANIFEST = BUNDLE / "exr-source" / "manifest.json"
GATE_RESULT = ROOT / "experiments/blur-rd/results/gaussian_route_eligibility_audit_2026-11-12.json"
MODEL_RESULT = ROOT / "experiments/blur-rd/results/gaussian_adjustment_gate_output_model_2026-11-12.json"
OUT = ROOT / "experiments/blur-rd/results/gaussian_adjustment_capture_manifest_2026-11-12.json"
CONTROLLER = ROOT / "Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cpp"
EFFECT = ROOT / "Artifact/src/Effects/Blur/GauusianBlur.cpp"
QUEUE = ROOT / "Artifact/src/Render/ArtifactRenderQueueService.cpp"
RENDERER = ROOT / "Artifact/src/Render/ArtifactIRenderer.cpp"
EXPORTER = ROOT / "ArtifactCore/src/IO/Image/ImageExporter.cpp"
IMAGE_LAYER = ROOT / "Artifact/src/Layer/ArtifactImageLayer.cpp"
IMAGE_IMPORTER = ROOT / "ArtifactCore/src/IO/Image/ImageImporter.cpp"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def adjustment_radii(sigma: float) -> tuple[int, int]:
    value = np.float32(sigma)
    gate_width = max(3, int(np.float32(value * np.float32(6.0))) | 1)
    gate_radius = (gate_width - 1) // 2
    shader_sigma = max(np.float32(0.1), value)
    resident_radius = int(np.ceil(np.float32(shader_sigma * np.float32(3.0))))
    return gate_radius, resident_radius


def main() -> None:
    fixture_manifest = json.loads(FIXTURE_MANIFEST.read_text(encoding="utf-8"))
    exr_asset_manifest = json.loads(EXR_ASSET_MANIFEST.read_text(encoding="utf-8"))
    gate_audit = json.loads(GATE_RESULT.read_text(encoding="utf-8"))
    model = json.loads(MODEL_RESULT.read_text(encoding="utf-8"))
    fixture_by_name = {Path(row["path"]).stem: row for row in fixture_manifest["fixtures"]}
    exr_assets = {row["name"]: row for row in exr_asset_manifest["assets"]}
    model_fixtures = {Path(row["input_fixture"]).stem: row for row in model["fixtures"]}
    if model.get("fixture_manifest_sha256") != sha(FIXTURE_MANIFEST):
        raise SystemExit("Adjustment model references a different fixture manifest")
    audited_sources = gate_audit["source_files"]
    for audit_key, source_path in (("effect_implementation", EFFECT),
                                   ("composition_controller", CONTROLLER)):
        if audited_sources[audit_key]["sha256"] != sha(source_path):
            raise SystemExit(f"route audit is stale for {source_path}")
    for fixture in fixture_manifest["fixtures"]:
        fixture_path = ROOT / fixture["path"]
        if sha(fixture_path) != fixture["sha256"]:
            raise SystemExit(f"fixture hash mismatch: {fixture_path}")
    for asset in exr_asset_manifest["assets"]:
        exr_path = ROOT / asset["exr"]
        sidecar_path = ROOT / asset["sidecar"]
        pixel_npy_path = ROOT / asset["asset_pixel_npy"]
        if (sha(exr_path) != asset["exr_sha256"] or
                sha(sidecar_path) != asset["sidecar_sha256"] or
                sha(pixel_npy_path) != asset["asset_pixel_npy_sha256"]):
            raise SystemExit(f"EXR asset hash mismatch: {asset['name']}")
        sidecar = json.loads(sidecar_path.read_text(encoding="utf-8"))
        if sidecar.get("exr_sha256") != asset["exr_sha256"] or not sidecar["numeric_roundtrip"]["exact_pixel_match"]:
            raise SystemExit(f"EXR source sidecar mismatch: {asset['name']}")
    import_fingerprints = exr_asset_manifest["artifact_image_import_source"]["files"]
    if (import_fingerprints["image_layer"]["sha256"] != sha(IMAGE_LAYER) or
            import_fingerprints["generic_importer"]["sha256"] != sha(IMAGE_IMPORTER)):
        raise SystemExit("EXR source asset manifest has stale Artifact import fingerprints")
    audit_candidates = {
        float(row["sigma"]): bool(row["source_gate_accepts_sigma"])
        for row in gate_audit["adjustment_layer_resident_route"]["candidate_rows"]
    }
    if not all(model.get("source_checks", {}).values()):
        raise SystemExit("Adjustment output model has a failed source anchor")
    queue_text = QUEUE.read_text(encoding="utf-8")
    renderer_text = RENDERER.read_text(encoding="utf-8")
    exporter_text = EXPORTER.read_text(encoding="utf-8")
    multi_frame = queue_text[queue_text.find("bool renderSingleFrameNoLock("):]
    export_writer = exporter_text[exporter_text.find("ImageExportResult ImageExporter::writeMultiChannel("):]
    alpha_attr_marker = 'spec.attribute("oiio:UnassociatedAlpha"'
    channel_branch_start = multi_frame.find("if (requiresChannels)")
    channel_readback = multi_frame.find("output.channels = renderer->readbackToMultiChannelImage();", channel_branch_start)
    channel_branch_else = multi_frame.find("} else {", channel_readback)
    final_effect_call = multi_frame.find("applyCompositionFinalEffectsToBuffer(", channel_branch_else)
    capture_source_checks = {
        "render_queue_draws_layers_before_multichannel_readback": (
            "drawLayerForCompositionView" in multi_frame and
            "output.channels = renderer->readbackToMultiChannelImage();" in multi_frame
        ),
        "multichannel_branch_bypasses_composition_final_effects": (
            channel_branch_start >= 0 and channel_readback > channel_branch_start and
            channel_branch_else > channel_readback and final_effect_call > channel_branch_else
        ),
        "multichannel_exr_declares_linear_colorspace": 'imgOpts.colorSpace = QStringLiteral("Linear");' in queue_text,
        "multichannel_export_honors_requested_datatype": "imgOpts.dataType = bitDepthToTypeDesc(job.bitDepth);" in queue_text,
        "renderer_reads_beauty_float_and_swizzles_bgra": (
            "readFloat(impl_->activeColorView()" in renderer_text and
            "SurfaceChannelOrder::BGRA" in renderer_text and
            "std::swap(row[x * 4], row[x * 4 + 2]);" in renderer_text
        ),
        "multichannel_writer_writes_float_input": "out->write_image(TypeDesc::FLOAT, interleaved.data())" in export_writer,
        "multichannel_writer_has_no_explicit_alpha_association_marker": alpha_attr_marker not in export_writer,
    }
    failed_capture_checks = [name for name, passed in capture_source_checks.items() if not passed]
    if failed_capture_checks:
        raise SystemExit(f"capture route source check failed: {failed_capture_checks}")
    run_cases: list[dict[str, object]] = []

    def add_run(case_id: str, fixture_name: str, mode: str, sigma: float | None,
                reason: str, alpha_background: str) -> None:
        fixture = fixture_by_name[fixture_name]
        exr_asset = exr_assets.get(fixture_name)
        if not exr_asset or not exr_asset["roundtrip_exact"]:
            raise SystemExit(f"verified float32 EXR source asset missing: {fixture_name}")
        model_fixture = model_fixtures.get(fixture_name)
        gate_radius = resident_radius = None
        accepted = None
        if mode == "gaussian_adjustment":
            assert sigma is not None
            gate_radius, resident_radius = adjustment_radii(sigma)
            accepted = sigma < 3.0 and gate_radius == resident_radius
            if sigma not in audit_candidates or audit_candidates[sigma] != accepted:
                raise SystemExit(f"route audit mismatch at sigma={sigma}")
        run_cases.append({
            "case_id": case_id,
            "status": "pending_capture",
            "capture_mode": mode,
            "fixture": {
                "name": fixture_name,
                "path": fixture["path"],
                "sha256": fixture["sha256"],
                "shape": fixture["shape"],
                "dtype": fixture["dtype"],
                "physical_order": fixture["storage_order"],
                "logical_channel_mapping": fixture["logical_channel_mapping"],
                "authored_alpha_association": fixture["alpha_association"],
                "transfer": fixture["transfer"],
            },
            "sigma": sigma,
            "source_gate_expectation": {
                "controller_radius": gate_radius,
                "resident_radius": resident_radius,
                "expects_adjustment_resident_gate_acceptance": accepted,
                "evidence": "float32 source-equation model cross-checked against route audit; replace with runtime route evidence",
            } if sigma is not None else None,
            "paired_control": "same composition and frame with Gaussian disabled; preserve identical background, layer stack, view transform, and render resolution",
            "render_input_asset": {
                "exr": exr_asset["exr"],
                "exr_sha256": exr_asset["exr_sha256"],
                "asset_pixel_npy": exr_asset["asset_pixel_npy"],
                "asset_pixel_npy_sha256": exr_asset["asset_pixel_npy_sha256"],
                "sidecar": exr_asset["sidecar"],
                "source_npy_sha256": exr_asset["source_npy_sha256"],
                "import_asset_alpha_association": exr_asset["alpha_association"],
                "alpha_conversion_from_fixture": exr_asset["alpha_conversion_from_fixture"],
                "opencv_roundtrip_exact": exr_asset["roundtrip_exact"],
                "artifact_import_status": "pending; compare asymmetric sentinel and descriptor before capture",
            },
            "expected_route_scope": (
                "Adjustment Layer preflighted spatial resident path; record accepted or rejected and CPU fallback separately"
                if mode == "gaussian_adjustment" else
                "ordinary image-layer Gaussian rasterizer path; confirm resident GPU dispatch independently"
            ),
            "capture_route_candidate": {
                "route": "GPU Render Queue multi-channel EXR float Beauty after layer rendering",
                "source_basis": "Gaussian rasterizer effects execute during the layer pass; Render Queue reads multi-channel Beauty after layer drawing and before composition-level final-image effects",
                "constraints": [
                    "composition-level final-image effect stack must be empty or separately accounted for because this EXR branch does not apply it",
                    "select only Beauty R/G/B/A channels and EXR Float32 output",
                    "use an actually transparent composition background for alpha-edge runs",
                    "EXR records Linear colorSpace but does not declare Beauty alpha association in the inspected writer; preserve raw RGBA channels and treat association as unknown until an asymmetric alpha sentinel verifies it",
                    "record whether the Gaussian adjustment gate accepted and whether GPU dispatch occurred; file existence alone is not route evidence",
                ],
                "status": "candidate_static_source_only_not_runtime_verified",
            },
            "alpha_capture_condition": alpha_background,
            "capture_tooling_requirements": [
                "read float32 Beauty channels after the target layer effect and preserve raw EXR plus file metadata",
                "retain an explicit sidecar recording transfer, primaries, alpha association (unknown until validated), channel order, unclamped/HDR status",
                "normalize to canonical logical RGBA float32 NPY only after validating EXR channel names/order and alpha association",
                "record project/composition IDs, frame, dimensions, effect stack/order/enabled flags, sigma, LOD, interaction/downsample state, transforms, opacity, blend, masks/region/overscan, MSAA, backend/device/driver, executable hash",
                "measure effect stage separately from upload, queue, readback and file writing; do not count synchronous wrapper timing as resident-pass timing",
            ],
            "route_evidence": None,
            "output": None,
            "timing_samples_ms": None,
            "quality_acceptance": "pending: establish after no-effect baseline and before comparing candidate results",
            "known_model_reference": (
                model_fixture["no_effect_adjustment_baseline"]["output_vs_input"]
                if mode == "gaussian_adjustment" and model_fixture else None
            ),
            "reason": reason,
        })

    edge = "premultiplied_alpha_edge_hdr"
    add_run("adj_edge_no_effect_control", edge, "no_effect_adjustment_control", None,
            "Required final-output baseline for isolating Adjustment Layer self-over behavior.",
            "Transparent composition background required; verify actual clear alpha is zero before interpreting alpha edge.")
    for sigma in (0.3, 0.4, 0.5, 0.9, 1.0, 1.1, 1.2):
        accepted = adjustment_radii(sigma)[0] == adjustment_radii(sigma)[1]
        add_run(f"adj_edge_sigma_{sigma:.1f}", edge, "gaussian_adjustment", sigma,
                "Gate-accepted control" if accepted else "Gate-rejection probe; verify CPU Gaussian was not treated as fallback.",
                "Transparent composition background required; record pre/post-composite alpha and confirm alpha semantics.")

    add_run("adj_opaque_hdr_no_effect_control", "opaque_asymmetric_impulses",
            "no_effect_adjustment_control", None,
            "Opaque/HDR control isolates Adjustment presentation clipping without alpha-edge ambiguity.",
            "Opaque fixture; record composition background and verify surface alpha is one across ROI.")
    add_run("adj_opaque_hdr_sigma_1_0", "opaque_asymmetric_impulses",
            "gaussian_adjustment", 1.0,
            "Accepted Adjustment gate probe on asymmetric HDR impulses.",
            "Opaque fixture; record composition background and verify surface alpha is one across ROI.")

    add_run("ordinary_layer_hdr_sigma_64_full", "opaque_hdr_color_edge",
            "ordinary_layer_resident_gaussian", 64.0,
            "Reachable resident radius-cap probe at effect scale 1.0; separate from Adjustment sigma<3 gate study.",
            "Opaque fixture; full-resolution, high/full LOD; record output dimensions and effect-resolution scale.")
    add_run("ordinary_layer_hdr_sigma_64_medium", "opaque_hdr_color_edge",
            "ordinary_layer_resident_gaussian", 64.0,
            "Scale sensitivity probe; source model predicts applied sigma 32 and resident radius cap 64.",
            "Opaque fixture; medium LOD/effect scale 0.5; record actual backing dimensions.")
    add_run("ordinary_layer_hdr_sigma_64_low", "opaque_hdr_color_edge",
            "ordinary_layer_resident_gaussian", 64.0,
            "Scale control; source model predicts applied sigma 16, below resident radius cap.",
            "Opaque fixture; low/draft scale 0.25; record actual backing dimensions and reconstruction filter.")

    result = {
        "schema": "artifact.blur-rd.gaussian-adjustment-capture-manifest.v1",
        "created_date": "2026-11-12",
        "status": "artifact_project_capture_and_import_validation_required",
        "evidence_class": "pending Artifact captures; expected gate fields are source-equation predictions only",
        "fixture_manifest": FIXTURE_MANIFEST.relative_to(ROOT).as_posix(),
        "fixture_manifest_sha256": sha(FIXTURE_MANIFEST),
        "fixture_bundle_identity_sha256": fixture_manifest.get("bundle_identity_sha256"),
        "exr_source_asset_manifest": EXR_ASSET_MANIFEST.relative_to(ROOT).as_posix(),
        "exr_source_asset_manifest_sha256": sha(EXR_ASSET_MANIFEST),
        "source_equation_references": {
            "route_audit": GATE_RESULT.relative_to(ROOT).as_posix(),
            "route_audit_sha256": sha(GATE_RESULT),
            "adjustment_output_model": MODEL_RESULT.relative_to(ROOT).as_posix(),
            "adjustment_output_model_sha256": sha(MODEL_RESULT),
        },
        "current_source_fingerprints": {
            "composition_render_controller": {"path": CONTROLLER.relative_to(ROOT).as_posix(), "sha256": sha(CONTROLLER)},
            "gaussian_effect_implementation": {"path": EFFECT.relative_to(ROOT).as_posix(), "sha256": sha(EFFECT)},
            "render_queue": {"path": QUEUE.relative_to(ROOT).as_posix(), "sha256": sha(QUEUE)},
            "renderer_readback": {"path": RENDERER.relative_to(ROOT).as_posix(), "sha256": sha(RENDERER)},
            "multichannel_image_exporter": {"path": EXPORTER.relative_to(ROOT).as_posix(), "sha256": sha(EXPORTER)},
            "image_layer_import": {"path": IMAGE_LAYER.relative_to(ROOT).as_posix(), "sha256": sha(IMAGE_LAYER)},
            "generic_image_importer": {"path": IMAGE_IMPORTER.relative_to(ROOT).as_posix(), "sha256": sha(IMAGE_IMPORTER)},
        },
        "capture_route_source_checks": capture_source_checks,
        "model_scope": "The source model applies the no-effect Adjustment self-over conversion. It does not model app final output or prove its route. Compare candidate output with a captured no-effect Adjustment control, not only with the input fixture.",
        "required_evaluation_coverage": {
            "effect_response_and_continuity": "sigma 0.3/0.4/0.5/0.9/1.0/1.1/1.2 with observed runtime route",
            "full_frame_and_detail": "full 513x513 output plus fixed ROIs around edges and impulse points",
            "color_alpha": "unclamped linear RGBA values, alpha profile, premultiplied composites over black/18% gray/white/green, HDR and transparent-edge cases",
            "time_stability": "static fixture repeated at fixed frame and short unchanged-frame sequence to separate temporal noise from route/resource warm-up",
            "route_resolution_backend": "Adjustment vs ordinary layer, high/full vs medium vs low effect scale, actual dimensions, backend and dispatch status",
            "performance": "separate baseline, effect compute, upload, sync/readback, serialization; capture overhead is not product timing",
        },
        "cases": run_cases,
        "limits": [
            "No case has been captured; gate decisions are source-equation predictions, not measured routes.",
            "Existing multi-channel EXR is before composition final-image effects; ordinary final output is converted to RGBA8 sRGB.",
            "Alpha-edge cases require an actually transparent render surface; the default composition background may be opaque.",
            "Set numeric acceptance thresholds from paired no-effect baseline variability before reviewing candidate output.",
            "Visual review remains necessary and must use the app display transform in addition to diagnostic linear composites.",
        ],
    }
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    print(OUT)


if __name__ == "__main__":
    main()
