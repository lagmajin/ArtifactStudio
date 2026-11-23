"""Model Adjustment Gaussian's source-level apply-or-original gate on a synthetic fixture.

The resident equation mirrors normalized shader weights and clamp border. The
final output model also mirrors accumulator presentation and the common normal
self-over blend used by the Adjustment layer. This is not an Artifact/GPU
capture or a quality acceptance test.
"""
from __future__ import annotations
import hashlib, json, math
from pathlib import Path
import cv2
import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
FIXTURES = (
    ROOT / "experiments/blur-rd/fixtures/gaussian-r1/opaque_asymmetric_impulses.npy",
    ROOT / "experiments/blur-rd/fixtures/gaussian-r1/premultiplied_alpha_edge_hdr.npy",
)
MANIFEST = ROOT / "experiments/blur-rd/fixtures/gaussian-r1/manifest.json"
CONTROLLER = ROOT / "Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cpp"
PIPELINE = ROOT / "Artifact/src/Render/ArtifactRenderLayerPipeline.cpp"
BLEND_SHADER = ROOT / "ArtifactCore/include/Graphics/Shader/Compute/LayerBlendComputeShader.ixx"
BLEND_PIPELINE = ROOT / "ArtifactCore/src/Graphics/LayerBlendPipeline.cpp"
SPRITE_SUBMITTER = ROOT / "Artifact/src/Render/DiligentBindlessSubmitter.cpp"
PRIMITIVE_2D = ROOT / "Artifact/src/Render/PrimitiveRenderer2D.cpp"
RENDERER = ROOT / "Artifact/src/Render/ArtifactIRenderer.cpp"
ADJUSTABLE_LAYER = ROOT / "Artifact/src/Layer/ArtifactAdjustableLayer.cpp"
OUT = ROOT / "experiments/blur-rd/results/gaussian_adjustment_gate_output_model_2026-11-12.json"


def sha(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def radii(sigma: float) -> tuple[int, int, int]:
    s = np.float32(sigma)
    gate_width = max(3, int(np.float32(s * np.float32(6))) | 1)
    gate = (gate_width - 1) // 2
    gaussian_width = max(1, int(np.float32(np.float32(6) * s + np.float32(1))))
    gaussian_width += gaussian_width % 2 == 0
    gaussian_cpu = (gaussian_width - 1) // 2
    resident = int(math.ceil(float(np.float32(max(np.float32(.1), s) * np.float32(3)))) )
    return gate, gaussian_cpu, resident


def resident_blur(source: np.ndarray, sigma: float) -> np.ndarray:
    s = np.float32(max(.1, sigma))
    radius = min(64, int(math.ceil(float(np.float32(s * np.float32(3))))))
    weights = np.zeros((129,), dtype=np.float32)
    total = np.float32(0)
    for offset in range(-radius, radius + 1):
        value = np.float32(np.exp(np.float32(-.5) * np.float32(offset * offset) / np.float32(s * s)))
        weights[offset + 64] = value
        total = np.float32(total + value)
    kernel = np.array([weights[i + 64] / total for i in range(-radius, radius + 1)], dtype=np.float32)
    tmp = cv2.sepFilter2D(source, -1, kernel, np.array([1], dtype=np.float32), borderType=cv2.BORDER_REPLICATE)
    return cv2.sepFilter2D(tmp, -1, np.array([1], dtype=np.float32), kernel, borderType=cv2.BORDER_REPLICATE)



def present_accumulator_and_over_self(accumulator: np.ndarray) -> np.ndarray:
    """Mirror the no-effect Adjustment path's BGRA present + normal over blend.

    Assumes successful presentation conversion and the default opacity-1 normal
    layer blend. The converted source straight RGB is clamped by the shader.
    """
    dst = accumulator.astype(np.float32, copy=False)
    dst_alpha = dst[:, :, 3]
    src_alpha = np.clip(dst_alpha, 0.0, 1.0)
    straight = np.zeros_like(dst[:, :, :3])
    visible = src_alpha > np.float32(1e-6)
    np.divide(dst[:, :, :3], src_alpha[:, :, None], out=straight, where=visible[:, :, None])
    straight = np.clip(straight, 0.0, 1.0)
    src_premul = straight * src_alpha[:, :, None]
    out = np.empty_like(dst)
    out[:, :, :3] = src_premul + dst[:, :, :3] * (1.0 - src_alpha[:, :, None])
    out[:, :, 3] = src_alpha + dst_alpha * (1.0 - src_alpha)
    return out


def stats(a: np.ndarray, b: np.ndarray) -> dict[str, float | int]:
    d = np.abs(a.astype(np.float64) - b.astype(np.float64))
    spatial = np.max(d[:, :, :3], axis=2)
    rgb_error = a[:, :, :3].astype(np.float64) - b[:, :, :3].astype(np.float64)
    alpha_error = a[:, :, 3].astype(np.float64) - b[:, :, 3].astype(np.float64)
    composite_rmse = {}
    for name, bg in {"black": (0.0, 0.0, 0.0), "gray18": (0.18, 0.18, 0.18), "white": (1.0, 1.0, 1.0), "green": (0.0, 1.0, 0.0)}.items():
        bg = np.asarray(bg, dtype=np.float64)
        comp_a = a[:, :, :3].astype(np.float64) + bg * (1.0 - a[:, :, 3:4].astype(np.float64))
        comp_b = b[:, :, :3].astype(np.float64) + bg * (1.0 - b[:, :, 3:4].astype(np.float64))
        composite_rmse[name] = float(np.sqrt(np.mean(np.square(comp_a - comp_b))))
    return {
        "rgb_mae": float(np.mean(np.abs(rgb_error))),
        "rgb_rmse": float(np.sqrt(np.mean(np.square(rgb_error)))),
        "rgb_max_abs": float(np.max(np.abs(rgb_error))),
        "rgb_changed_pixels_gt_1e-6": int(np.count_nonzero(spatial > 1e-6)),
        "rgb_changed_pixels_gt_1e-3": int(np.count_nonzero(spatial > 1e-3)),
        "alpha_mae": float(np.mean(np.abs(alpha_error))),
        "alpha_rmse": float(np.sqrt(np.mean(np.square(alpha_error)))),
        "alpha_max_abs": float(np.max(np.abs(alpha_error))),
        "alpha_changed_pixels_gt_1e-6": int(np.count_nonzero(np.abs(alpha_error) > 1e-6)),
        "composite_rmse_linear_by_background": composite_rmse,
        "source_rgb_energy": float(np.sum(np.abs(a[:, :, :3].astype(np.float64)))),
        "output_rgb_energy": float(np.sum(np.abs(b[:, :, :3].astype(np.float64)))),
    }



def alpha_edge_profile(frame: np.ndarray, y: int = 256) -> dict[str, float]:
    """Measure normalized 10-90 alpha edge spread across the authored vertical edge."""
    profile = frame[y, :, 3].astype(np.float64)
    low = float(np.median(profile[16:48]))
    high = float(np.median(profile[-48:-16]))
    amplitude = high - low
    if amplitude <= 1e-9:
        return {"low_alpha": low, "high_alpha": high, "width_10_90_px": 0.0, "crossing_50_x_px": 0.0}

    def crossing(fraction: float) -> float:
        target = low + amplitude * fraction
        indices = np.flatnonzero(profile >= target)
        if not len(indices):
            return float("nan")
        idx = int(indices[0])
        if idx == 0:
            return 0.0
        a, b = float(profile[idx - 1]), float(profile[idx])
        return float(idx - 1) + (target - a) / (b - a) if b != a else float(idx)

    x10, x50, x90 = crossing(.1), crossing(.5), crossing(.9)
    return {"low_alpha": low, "high_alpha": high, "crossing_10_x_px": x10, "crossing_50_x_px": x50, "crossing_90_x_px": x90, "width_10_90_px": x90 - x10, "profile_row_y_px": float(y)}


def linear_composite_preview(frame: np.ndarray, background: tuple[float, float, float]) -> Image.Image:
    logical_rgb = frame[:, :, 2::-1].astype(np.float64)
    alpha = frame[:, :, 3:4].astype(np.float64)
    bg = np.asarray(background, dtype=np.float64).reshape(1, 1, 3)
    linear = np.maximum(logical_rgb + bg * (1.0 - alpha), 0.0)
    clipped = np.clip(linear, 0.0, 1.0)
    encoded = np.where(clipped <= 0.0031308, clipped * 12.92, 1.055 * np.power(clipped, 1.0 / 2.4) - 0.055)
    return Image.fromarray(np.round(np.clip(encoded, 0.0, 1.0) * 255.0).astype(np.uint8), mode="RGB")


def write_adjustment_previews(no_effect: np.ndarray, output_03: np.ndarray, output_10: np.ndarray) -> dict[str, object]:
    preview_dir = ROOT / "experiments/blur-rd/results/previews"
    preview_dir.mkdir(parents=True, exist_ok=True)
    font = ImageFont.load_default()
    stages = (("No-effect Adjustment baseline", no_effect), ("Gaussian sigma 0.3 (gate accepted)", output_03), ("Gaussian sigma 1.0 (gate accepted)", output_10))
    backgrounds = (("Black", (0.0, 0.0, 0.0)), ("White", (1.0, 1.0, 1.0)))
    paths = {}
    for view_name, crop_box in (("full", None), ("edge_crop", (224, 224, 288, 288))):
        tile_size = 256
        margin, label_h = 12, 20
        canvas = Image.new("RGB", (margin + 3 * (tile_size + margin), margin + 2 * (tile_size + label_h + margin)), (238, 240, 243))
        draw = ImageDraw.Draw(canvas)
        for row, (bg_name, bg) in enumerate(backgrounds):
            for col, (stage_name, output) in enumerate(stages):
                x = margin + col * (tile_size + margin)
                y = margin + row * (tile_size + label_h + margin)
                title = f"{bg_name}: {stage_name}"
                draw.text((x, y), title, font=font, fill=(20, 24, 30))
                image = linear_composite_preview(output, bg)
                if crop_box is not None:
                    image = image.crop(crop_box).resize((tile_size, tile_size), Image.Resampling.NEAREST)
                else:
                    image = image.resize((tile_size, tile_size), Image.Resampling.BILINEAR)
                canvas.paste(image, (x, y + label_h))
        path = preview_dir / f"gaussian_adjustment_baseline_vs_gate_sigma_{view_name}_2026-11-12.png"
        canvas.save(path)
        paths[view_name] = {"path": str(path.relative_to(ROOT)).replace("\\", "/"), "sha256": sha(path)}
    return {"previews": paths, "crop_xyxy_px": [224, 224, 288, 288], "display": "linear composite over black/white, clipped to [0,1], simple sRGB OETF; diagnostic only, not app view transform", "stages": [name for name, _ in stages]}


def main() -> None:
    controller = CONTROLLER.read_text(encoding="utf-8")
    shader = PIPELINE.read_text(encoding="utf-8")
    adjustment_layer = ADJUSTABLE_LAYER.read_text(encoding="utf-8")
    blend_shader = BLEND_SHADER.read_text(encoding="utf-8")
    sprite_submitter = SPRITE_SUBMITTER.read_text(encoding="utf-8")
    primitive_2d = PRIMITIVE_2D.read_text(encoding="utf-8")
    renderer = RENDERER.read_text(encoding="utf-8")
    generic_draw_fallback = controller[controller.find("// Fallback for layer types without a direct surface accessor."):controller.find("// Project3D-style camera projection helper", controller.find("// Fallback for layer types without a direct surface accessor."))]
    source_checks = {
        "preflight_radius_gate_present": "static_cast<int>(std::ceil(std::max(0.1f, sigma) * 3.0f))" in controller,
        "preflight_failure_draws_original_accum": "drawBgraAccumulator(accumSRV);" in controller,
        "full_quality_generic_fallback_calls_layer_draw": "layer->draw(renderer);" in generic_draw_fallback,
        "adjustment_layer_draw_is_noop": "(void)renderer;" in adjustment_layer and "do not draw their own pixels" in adjustment_layer,
        "accum_presenter_unpremultiplies_and_clamps_rgb": "premultipliedBgra.bgr / alpha" in blend_shader and "saturate(straightRgb)" in blend_shader,
        "draw_sprite_routes_through_local_texture_packet": "primitiveRenderer_.drawTextureLocal(x, y, w, h, pSRV, opacity)" in renderer and "impl_->cmdBuf_->append(std::move(pkt));" in primitive_2d,
        "sprite_packet_uses_src_alpha_blending": "(sprite && submitSprite(*sprite" in sprite_submitter and "blend.SrcBlend = BLEND_FACTOR_SRC_ALPHA;" in sprite_submitter and "blend.DestBlendAlpha = BLEND_FACTOR_INV_SRC_ALPHA;" in sprite_submitter,
        "layer_conversion_unpremultiplies_and_clamps": "src.rgb / alpha" in blend_shader and "saturate(straight)" in blend_shader,
        "normal_blend_is_premultiplied_over": "float3 blended = srcRGB + dst.rgb * (1.0 - srcA);" in blend_shader and "float outA = srcA + dst.a * (1.0 - srcA);" in blend_shader,
        "common_adjustment_layer_blend_has_no_special_skip": "renderer_->blendLayers(blendPipeline_.get(), preparedBlendSRV" in controller and "if (layer->isAdjustmentLayer())" not in controller[controller.find("GpuLayerBlendResult blendGpuLayerIntoAccum"):controller.find("Diligent::ITextureView* finalizeGpuRenderToViewport")],
        "resident_shader_clamps_sample_coordinate": "clamp(p.x + i, 0, int(width) - 1)" in shader and "clamp(p.y + i, 0, int(height) - 1)" in shader,
        "resident_weights_normalized_from_exp_gaussian": "std::exp(-0.5f * static_cast<float>(offset * offset)" in shader and "params.weights[weightIndex] /= weightSum;" in shader,
    }
    if not all(source_checks.values()):
        raise SystemExit(f"source check failed: {source_checks}")
    fixture_runs = []
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    manifest_entries = {entry["path"]: entry["sha256"] for entry in manifest["fixtures"]}
    sigmas = (.2, .3, .4, .5, .9, 1.0, 1.2, 2.0, 2.2, 2.9)
    for fixture_path in FIXTURES:
        relative_fixture = str(fixture_path.relative_to(ROOT)).replace("\\", "/")
        actual_sha = sha(fixture_path)
        if manifest_entries.get(relative_fixture) != actual_sha:
            raise SystemExit(f"fixture hash does not match manifest: {relative_fixture}")
        fixture = np.load(fixture_path)
        if fixture.dtype != np.float32 or fixture.shape != (513, 513, 4) or not np.isfinite(fixture).all():
            raise SystemExit(f"fixture contract mismatch: {fixture_path}")
        rows = []
        no_effect_baseline = present_accumulator_and_over_self(fixture)
        outputs = {}
        resident_outputs = {}
        resident_composited_outputs = {}
        for sigma in sigmas:
            gate_radius, cpu_radius, resident_radius = radii(sigma)
            accepted = gate_radius == resident_radius
            blurred = resident_blur(fixture, sigma)
            effect_output = blurred if accepted else fixture.copy()
            composited_resident = present_accumulator_and_over_self(blurred)
            modeled_output = present_accumulator_and_over_self(effect_output)
            resident_outputs[sigma] = blurred
            resident_composited_outputs[sigma] = composited_resident
            outputs[sigma] = modeled_output
            rows.append({
                "sigma": sigma,
                "controller_blur_formula_radius": gate_radius,
                "gaussian_cpu_formula_radius": cpu_radius,
                "resident_radius": resident_radius,
                "preflight_accepts": accepted,
                "modeled_adjustment_output": "resident Gaussian result presented and normally composited over accumulation" if accepted else "unmodified accumulation presented and normally composited over itself",
                "resident_effect_stage_vs_original_fixture": stats(fixture, blurred),
                "resident_vs_original_fixture": stats(fixture, composited_resident),
                "resident_vs_no_effect_adjustment_baseline": stats(no_effect_baseline, composited_resident),
                "effect_stage_output_vs_original_fixture": stats(fixture, effect_output),
                "modeled_output_vs_original_fixture": stats(fixture, modeled_output),
                "modeled_output_vs_no_effect_adjustment_baseline": stats(no_effect_baseline, modeled_output),
            })
        adjacent = []
        for lower, upper in zip(sigmas, sigmas[1:]):
            adjacent.append({
                "sigma_pair": [lower, upper],
                "lower_preflight_accepts": rows[sigmas.index(lower)]["preflight_accepts"],
                "upper_preflight_accepts": rows[sigmas.index(upper)]["preflight_accepts"],
                "modeled_output_pair_difference": stats(outputs[lower], outputs[upper]),
                "resident_equation_pair_difference": stats(resident_outputs[lower], resident_outputs[upper]),
                "resident_composited_pair_difference": stats(resident_composited_outputs[lower], resident_composited_outputs[upper]),
            })
        preview_record = None
        edge_profiles = None
        if "premultiplied_alpha_edge_hdr" in fixture_path.name:
            preview_record = write_adjustment_previews(no_effect_baseline, outputs[0.3], outputs[1.0])
            edge_profiles = {"no_effect_adjustment_baseline": alpha_edge_profile(no_effect_baseline), "by_sigma": {str(sigma): alpha_edge_profile(outputs[sigma]) for sigma in (.3, .4, .5, .9, 1.0, 1.2)}}
        fixture_runs.append({
            "input_fixture": relative_fixture,
            "input_fixture_sha256": actual_sha,
            "manifest_hash_verified": True,
            "conditions": {"shape": list(fixture.shape), "dtype": str(fixture.dtype), "physical_order": "BGRA", "transfer": "synthetic linear numeric", "alpha_association": "opaque" if "opaque_" in fixture_path.name else "premultiplied", "display_transform": "none", "ROI": "full frame", "frame": 0},
            "no_effect_adjustment_baseline": {
                "definition": "unchanged accumulation presented into the cleared adjustment layer target and normally blended over the same accumulation",
                "output_vs_input": stats(fixture, no_effect_baseline),
            },
            "alpha_edge_profile": edge_profiles,
            "visual_previews": preview_record,
            "samples": rows,
            "adjacent_sigma_response_probe": adjacent,
        })
    result = {
        "schema": "artifact.blur-rd.gaussian-adjustment-gate-output-model.v1",
        "evidence_class": "synthetic fixtures + source-equation model; not Artifact capture",
        "fixture_manifest_sha256": sha(MANIFEST),
        "fixture_manifest_self_sha256_verified": bool(manifest.get("bundle_identity_sha256")),
        "source_files": {str(p.relative_to(ROOT)).replace("\\", "/"): sha(p) for p in (CONTROLLER, PIPELINE, BLEND_SHADER, BLEND_PIPELINE, SPRITE_SUBMITTER, PRIMITIVE_2D, RENDERER, ADJUSTABLE_LAYER)},
        "source_checks": source_checks,
        "formula": "The spatial gate first selects resident Gaussian result or the unchanged accumulation. In both cases the Adjustment Layer presents that premultiplied BGRA accumulator as clamped straight RGBA, the sprite blends it into the cleared layer target, the layer is converted back to straight RGBA, and the common normal blend composites that layer over the same accumulator. The no-effect adjustment baseline is the same self-over operation applied to the original accumulation. The model assumes opacity 1.0 and successful presentation; source anchors are checked below.",
        "fixtures": fixture_runs,
        "visual_evaluation_scope": "Diagnostic visual panels compare no-effect Adjustment baseline, accepted sigma .3, and accepted sigma 1.0 over black/white backgrounds; full frame and 64x64 edge crop. The display transform is explicitly a simple clipped sRGB OETF, not the app transform. Human visual acceptance remains pending.",
        "metrics": "RGB MAE/RMSE/max and changed-pixel counts at 1e-6 and 1e-3; alpha MAE/RMSE/max and changed-pixel count; premultiplied-linear composite RMSE on black, gray18, white, green backgrounds; absolute RGB energy.",
        "scope_limit": "OpenCV separable convolution with normalized float32 shader-weight equation and BORDER_REPLICATE plus a source-equation model of accumulator presentation and normal self-over compositing. Assumes successful conversion, opacity=1, Normal blend, no masks or transform; does not model texture quantization or rasterizer coverage. No GPU dispatch, adjustment-layer UI, runtime image, display transform, temporal sequence, performance, or visual acceptance. Metrics compare modeled final output with input, not a ground-truth blur.",
    }
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(OUT)

if __name__ == "__main__":
    main()
