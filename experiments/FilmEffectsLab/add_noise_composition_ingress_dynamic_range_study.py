from __future__ import annotations

import hashlib
import json
import platform
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent.parent
PLAN = ROOT / "plans/add-noise-composition-ingress-hdr-alpha-r1.json"
ADD_NOISE = REPO / "Artifact/src/Effects/AddNoise/AddNoiseEffect.cpp"
LAYER_SHADER = REPO / "ArtifactCore/include/Graphics/Shader/Compute/LayerBlendComputeShader.ixx"
CONTROLLER = REPO / "Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cpp"
OUT = ROOT / "captures/add-noise-composition-ingress-hdr-alpha-r1"
OUT.mkdir(parents=True, exist_ok=True)
F32 = np.float32
U32 = np.uint32

ADD_TEXT = ADD_NOISE.read_text(encoding="utf-8")
LAYER_TEXT = LAYER_SHADER.read_text(encoding="utf-8")
CONTROLLER_TEXT = CONTROLLER.read_text(encoding="utf-8")
MARKERS = {
    "cpu_numeric_seed": "seed = static_cast<uint32_t>(seed_)" in ADD_TEXT,
    "cpu_floor_size": "std::floor(static_cast<float>(x) / size_)" in ADD_TEXT,
    "cpu_rgb_clamp": "std::clamp(p[0] + noiseAt" in ADD_TEXT and "std::clamp(p[2] + noiseAt" in ADD_TEXT,
    "cpu_alpha_unmodified": "dst = src;" in ADD_TEXT and "p[2] = std::clamp" in ADD_TEXT,
    "resident_numeric_seed": "(uint)max(g_P4, 0.0)" in ADD_TEXT,
    "resident_size_floor": "max(g_P1, 0.1)" in ADD_TEXT,
    "resident_rgb_clamp": "pixel.rgb = saturate" in ADD_TEXT,
    "gpu_ingress_unpremultiply": "src.rgb / alpha" in LAYER_TEXT,
    "gpu_ingress_saturate": "saturate(straight)" in LAYER_TEXT and "float alpha = saturate(src.a)" in LAYER_TEXT,
    "controller_uses_conversion": "renderer_->convertLayerToFloat" in CONTROLLER_TEXT,
    "controller_prepares_effect_layer": "prepareGpuLayerForBlend" in CONTROLLER_TEXT,
}
if not all(MARKERS.values()):
    raise RuntimeError(MARKERS)

ALPHAS = (0.0, 0.01, 0.05, 0.1, 0.25, 0.5, 0.75, 1.0)
TONES = (0.01, 0.05, 0.18, 0.5, 0.95, 1.0, 1.5, 4.0)
AMOUNTS = (0.0, 0.2, 0.4)
MODES = ("color", "monochrome", "luminance")
BACKGROUNDS = {"black": F32(0), "gray": F32(0.18), "white": F32(1)}
SEED = 73
SIZE = F32(1.5)
PATCH = 64
GRID = len(ALPHAS)
WIDTH = HEIGHT = PATCH * GRID


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def hash_noise(x: np.ndarray, y: np.ndarray, channel: int, seed: int) -> np.ndarray:
    with np.errstate(over="ignore"):
        value = (x.astype(U32) * U32(73856093) ^
                 y.astype(U32) * U32(19349663) ^
                 U32(channel) * U32(83492791) ^ U32(seed))
        value ^= value >> U32(16)
        value *= U32(0x7FEB352D)
        value ^= value >> U32(15)
        value *= U32(0x846CA68B)
        value ^= value >> U32(16)
    unit = (value & U32(0x00FFFFFF)).astype(F32) / F32(16777215.0)
    return unit * F32(2.0) - F32(1.0)


def make_fixture() -> tuple[np.ndarray, np.ndarray, dict]:
    source = np.zeros((HEIGHT, WIDTH, 4), dtype=F32)
    alpha_grid = np.zeros((HEIGHT, WIDTH), dtype=F32)
    records = {}
    for alpha_index, alpha_value in enumerate(ALPHAS):
        for tone_index, tone in enumerate(TONES):
            y0, x0 = alpha_index * PATCH, tone_index * PATCH
            y1, x1 = y0 + PATCH, x0 + PATCH
            alpha = F32(alpha_value)
            premul = F32(tone) * alpha
            source[y0:y1, x0:x1, :3] = premul
            source[y0:y1, x0:x1, 3] = alpha
            alpha_grid[y0:y1, x0:x1] = alpha
            records[(alpha_index, tone_index)] = {
                "alpha": float(alpha),
                "straight_tone": float(F32(tone)),
                "premultiplied_stored_rgb": float(premul),
                "roi_xyxy": [x0, y0, x1, y1],
            }
    return source, alpha_grid, records


def gpu_layer_to_float(source: np.ndarray) -> np.ndarray:
    alpha = np.clip(source[..., 3], F32(0), F32(1))
    straight = np.zeros_like(source[..., :3])
    valid = alpha > F32(1e-6)
    straight[valid] = source[..., :3][valid] / alpha[valid, None]
    straight = np.clip(straight, F32(0), F32(1))
    return np.dstack((straight, alpha)).astype(F32)


def noise_fields(width: int, height: int) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    yy, xx = np.mgrid[0:height, 0:width].astype(F32)
    sample_x = np.floor(xx / np.maximum(SIZE, F32(0.1))).astype(U32)
    sample_y = np.floor(yy / np.maximum(SIZE, F32(0.1))).astype(U32)
    return tuple(hash_noise(sample_x, sample_y, channel, SEED) for channel in (1, 2, 3))


def apply_add_noise(source_bgra: np.ndarray, amount: float, mode: str,
                    noises: tuple[np.ndarray, np.ndarray, np.ndarray]) -> np.ndarray:
    out = source_bgra.copy()
    n1, n2, n3 = noises
    strength = F32(amount)
    if mode == "monochrome":
        n = n1 * strength
        out[..., :3] = np.clip(source_bgra[..., :3] + n[..., None], F32(0), F32(1))
    elif mode == "color":
        out[..., 0] = np.clip(source_bgra[..., 0] + n3 * strength, F32(0), F32(1))
        out[..., 1] = np.clip(source_bgra[..., 1] + n2 * strength, F32(0), F32(1))
        out[..., 2] = np.clip(source_bgra[..., 2] + n1 * strength, F32(0), F32(1))
    elif mode == "luminance":
        luma = (source_bgra[..., 0] * F32(0.114) +
                source_bgra[..., 1] * F32(0.587) +
                source_bgra[..., 2] * F32(0.299))
        new_luma = np.clip(luma + n1 * strength, F32(0), F32(1))
        diff = new_luma - luma
        out[..., :3] = np.clip(source_bgra[..., :3] + diff[..., None], F32(0), F32(1))
    else:
        raise ValueError(mode)
    out[..., 3] = source_bgra[..., 3]
    return out


def composite(stored: np.ndarray, background: float, association: str) -> np.ndarray:
    alpha = stored[..., 3:4]
    rgb = stored[..., :3][..., ::-1]
    bg = F32(background)
    if association == "premultiplied":
        return rgb + bg * (F32(1) - alpha)
    return rgb * alpha + bg * (F32(1) - alpha)


def metric(delta: np.ndarray) -> dict:
    values = np.abs(delta.astype(np.float64)).reshape(-1)
    return {
        "mae": float(values.mean()),
        "rmse": float(np.sqrt(np.mean(values * values))),
        "p99_abs": float(np.quantile(values, 0.99)),
        "max_abs": float(values.max(initial=0.0)),
    }


def region_stats(output: np.ndarray, source: np.ndarray) -> dict:
    rgb = output[..., :3]
    return {
        "stored_rgb_min": [float(rgb[..., c].min()) for c in range(3)],
        "stored_rgb_max": [float(rgb[..., c].max()) for c in range(3)],
        "rgb_gt_alpha_fraction": float(np.mean(rgb > output[..., 3:4] + F32(1e-6))),
        "alpha_max_abs_delta": float(np.max(np.abs(output[..., 3] - source[..., 3]))),
        "nonfinite_count": int(np.count_nonzero(~np.isfinite(output))),
    }


def encode_preview(rgb: np.ndarray) -> Image.Image:
    clipped = np.clip(rgb[..., ::-1], F32(0), F32(1))
    return Image.fromarray(np.uint8(clipped * F32(255) + F32(0.5)), mode="RGB")


source, alpha_grid, patches = make_fixture()
gpu_input = gpu_layer_to_float(source)
noises = noise_fields(WIDTH, HEIGHT)
results = []
preview_images = {}

cpu_off = source.copy()
gpu_off = gpu_input.copy()
for mode in MODES:
    for amount in AMOUNTS:
        cpu_out = apply_add_noise(source, amount, mode, noises)
        gpu_out = apply_add_noise(gpu_input, amount, mode, noises)
        cpu_base_composites = {name: composite(cpu_off, bg, "premultiplied") for name, bg in BACKGROUNDS.items()}
        gpu_base_composites = {name: composite(gpu_off, bg, "straight") for name, bg in BACKGROUNDS.items()}
        cpu_composites = {name: composite(cpu_out, bg, "premultiplied") for name, bg in BACKGROUNDS.items()}
        gpu_composites = {name: composite(gpu_out, bg, "straight") for name, bg in BACKGROUNDS.items()}
        for alpha_index, alpha_value in enumerate(ALPHAS):
            for tone_index, tone in enumerate(TONES):
                x0, y0 = tone_index * PATCH, alpha_index * PATCH
                roi = np.s_[y0:y0 + PATCH, x0:x0 + PATCH]
                a = alpha_grid[roi]
                expected_straight = (
                    source[roi][..., :3] / F32(alpha_value)
                    if alpha_value > 0 else np.zeros_like(source[roi][..., :3])
                )
                row = {
                    "alpha": float(alpha_value),
                    "straight_tone": float(tone),
                    "premultiplied_input_value": float(F32(alpha_value) * F32(tone)),
                    "amount": float(amount),
                    "mode": mode,
                    "pixel_count": PATCH * PATCH,
                    "gpu_ingress_straight_rgb": [float(gpu_input[roi][0, 0, c]) for c in range(3)],
                    "gpu_ingress_straight_rgb_delta_from_unclamped": metric(
                        gpu_input[roi][..., :3] - expected_straight
                    ),
                    "gpu_ingress_clipped_channel_fraction": float(np.mean(
                        (source[roi][..., :3] / max(float(alpha_value), 1e-6) < 0) |
                        (source[roi][..., :3] / max(float(alpha_value), 1e-6) > 1)
                    )) if alpha_value > 0 else 0.0,
                    "cpu_output": region_stats(cpu_out[roi], source[roi]),
                    "gpu_output": region_stats(gpu_out[roi], gpu_input[roi]),
                    "stored_rgb_route_gap": metric(cpu_out[roi][..., :3] - gpu_out[roi][..., :3]),
                    "premultiplied_bound_violation_fraction_cpu": float(np.mean(cpu_out[roi][..., :3] > a[..., None] + F32(1e-6))),
                    "composited_cpu_gpu_gap": {
                        name: metric(cpu_composites[name][roi] - gpu_composites[name][roi])
                        for name in BACKGROUNDS
                    },
                    "route_effect_delta_gap": {
                        name: metric(
                            (cpu_composites[name][roi] - cpu_base_composites[name][roi]) -
                            (gpu_composites[name][roi] - gpu_base_composites[name][roi])
                        )
                        for name in BACKGROUNDS
                    },
                    "transparent_composited_gap_black": (
                        metric(cpu_composites["black"][roi] - gpu_composites["black"][roi])
                        if alpha_value == 0 else None
                    ),
                    "cpu_effect_delta_from_bypass_black": metric(cpu_composites["black"][roi] - cpu_base_composites["black"][roi]),
                    "gpu_effect_delta_from_bypass_black": metric(gpu_composites["black"][roi] - gpu_base_composites["black"][roi]),
                }
                results.append(row)
        if mode == "color" and amount == 0.4:
            for background in ("black", "white"):
                preview_images[(background, "cpu")] = encode_preview(cpu_composites[background])
                preview_images[(background, "gpu")] = encode_preview(gpu_composites[background])

# Save full route plate pairs, unmodified aside from clipping and channel encoding.
for background in ("black", "white"):
    sheet = Image.new("RGB", (WIDTH * 2, HEIGHT + 28), (28, 30, 34))
    draw = ImageDraw.Draw(sheet)
    sheet.paste(preview_images[(background, "cpu")], (0, 28))
    sheet.paste(preview_images[(background, "gpu")], (WIDTH, 28))
    draw.text((8, 7), f"CPU on stored premultiplied RGB / amount .4 / color / {background}", fill=(240, 240, 240))
    draw.text((WIDTH + 8, 7), f"GPU LayerToFloat then resident / amount .4 / color / {background}", fill=(240, 240, 240))
    sheet.save(OUT / f"route-pair-amount0p4-color-over-{background}.png")

# Aggregate by alpha, tone, mode and amount for a compact summary alongside the full rows.
summaries = {}
for row in results:
    key = f"a={row['alpha']:g}|tone={row['straight_tone']:g}|{row['mode']}|amount={row['amount']:g}"
    summaries[key] = {
        "cpu_rgba_gap_black_rmse": row["composited_cpu_gpu_gap"]["black"]["rmse"],
        "gpu_ingress_clipped_channel_fraction": row["gpu_ingress_clipped_channel_fraction"],
        "cpu_premul_bound_violation_fraction": row["premultiplied_bound_violation_fraction_cpu"],
        "effect_delta_gap_black_rmse": row["route_effect_delta_gap"]["black"]["rmse"],
    }

metrics = {
    "study": "Add Noise composition ingress HDR and alpha route model r1",
    "environment": {"python": platform.python_version(), "numpy": np.__version__},
    "input_sha256": {
        "plan": sha(PLAN),
        "add_noise_source": sha(ADD_NOISE),
        "layer_to_float_shader": sha(LAYER_SHADER),
        "composition_controller": sha(CONTROLLER),
        "runner": sha(Path(__file__)),
    },
    "source_markers": MARKERS,
    "conditions": {
        "patch_grid": [GRID, GRID],
        "patch_size": [PATCH, PATCH],
        "resolution": [WIDTH, HEIGHT],
        "alphas": list(ALPHAS),
        "straight_tones": list(TONES),
        "amounts": list(AMOUNTS),
        "modes": list(MODES),
        "backgrounds": {k: float(v) for k, v in BACKGROUNDS.items()},
        "seed": SEED,
        "size": float(SIZE),
        "cases": len(results),
        "seed_match": "CPU and resident both convert seed 73 numerically to uint; the dedicated wrapper float-asuint route is not modeled here.",
        "route_pair": "CPU applies Add Noise to premultiplied stored RGB and composites as premultiplied; GPU LayerToFloat unpremultiplies and saturates, then resident Add Noise operates on straight RGB and composites with alpha multiplication.",
    },
    "route_summary": summaries,
    "results": results,
    "limits": [
        "Conditional NumPy source-equation model, not C++/HLSL execution or evidence that Composition selects these routes for the same layer.",
        "Input is neutral and synthetic; no colored material, transfer-function or Viewer transform evidence.",
        "Preview PNGs clip RGB to [0,1]; raw Float32 metrics define the comparison.",
        "No GPU/backend, preview/final, temporal, performance or allocation measurement.",
    ],
}
(OUT / "metrics.json").write_text(json.dumps(metrics, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
checksums = {
    path.name: sha(path)
    for path in sorted(OUT.iterdir())
    if path.is_file() and path.name != "checksums.json"
}
(OUT / "checksums.json").write_text(json.dumps(checksums, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
print(json.dumps({
    "cases": len(results),
    "source_markers_all_true": all(MARKERS.values()),
    "alpha_exact": all(row["cpu_output"]["alpha_max_abs_delta"] == 0 and row["gpu_output"]["alpha_max_abs_delta"] == 0 for row in results),
    "nonfinite_total": sum(row["cpu_output"]["nonfinite_count"] + row["gpu_output"]["nonfinite_count"] for row in results),
    "png_count": sum(path.suffix.lower() == ".png" for path in OUT.iterdir()),
}, ensure_ascii=False))
