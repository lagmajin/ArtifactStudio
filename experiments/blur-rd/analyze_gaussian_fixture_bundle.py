"""Run source-equation Gaussian comparisons on the fixed synthetic bundle."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_BUNDLE = Path(__file__).resolve().parent / "fixtures" / "gaussian-r1"
CPU_SOURCE = ROOT / "Artifact/src/Effects/Blur/GauusianBlur.cpp"
RESIDENT_SOURCE = ROOT / "Artifact/src/Render/ArtifactRenderLayerPipeline.cpp"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def gaussian_weights(sigma: float, resident: bool) -> np.ndarray:
    if resident:
        applied = np.float32(np.clip(sigma, 0.1, 64.0))
        radius = min(64, int(np.ceil(float(applied) * 3.0)))
        offsets = np.arange(-radius, radius + 1, dtype=np.float32)
        weights = np.exp(
            np.float32(-0.5) * offsets * offsets / (applied * applied)
        ).astype(np.float32)
        weights /= np.sum(weights, dtype=np.float32)
        return weights

    applied = np.float32(sigma)
    width = max(1, int(np.float32(np.float32(6.0) * applied + 1.0)))
    if width % 2 == 0:
        width += 1
    return cv2.getGaussianKernel(width, float(applied), cv2.CV_32F)[:, 0]


def blur_cpu(image: np.ndarray, sigma: float) -> np.ndarray:
    weights = gaussian_weights(sigma, resident=False)
    return cv2.sepFilter2D(
        image, -1, weights, weights, borderType=cv2.BORDER_REFLECT_101
    )


def blur_direct_rgba_control(image: np.ndarray, sigma: float) -> np.ndarray:
    """Direct RGBA convolution control; this does not mirror current HLSL alpha math."""
    weights = gaussian_weights(sigma, resident=True)
    radius = len(weights) // 2
    output = image.astype(np.float32, copy=True)
    for axis in (1, 0):
        source = output
        output = np.zeros_like(source)
        for index, weight in enumerate(weights):
            offset = index - radius
            coordinates = np.clip(
                np.arange(source.shape[axis]) + offset,
                0,
                source.shape[axis] - 1,
            )
            sampled = np.take(source, coordinates, axis=axis)
            output += sampled * weight
    return output


def wrapper_weights(sigma: float) -> np.ndarray:
    applied = np.float32(np.clip(sigma, 0.1, 64.0))
    radius = min(64, int(np.ceil(float(applied) * 3.0)))
    offsets = np.arange(-radius, radius + 1, dtype=np.float32)
    weights = np.exp(
        np.float32(-0.5) * offsets * offsets / (applied * applied)
    ).astype(np.float32)
    return weights


def straight_from_premultiplied(image: np.ndarray) -> np.ndarray:
    output = image.astype(np.float32, copy=True)
    alpha = output[..., 3:4]
    output[..., :3] = np.divide(
        output[..., :3], alpha,
        out=np.zeros_like(output[..., :3]),
        where=alpha > np.float32(1e-5),
    )
    return output


def blur_cpu_wrapper(straight_image: np.ndarray, sigma: float) -> np.ndarray:
    premultiplied = straight_image.astype(np.float32, copy=True)
    premultiplied[..., :3] *= premultiplied[..., 3:4]
    weights = gaussian_weights(sigma, resident=False)
    blurred = cv2.sepFilter2D(
        premultiplied, -1, weights, weights, borderType=cv2.BORDER_REFLECT_101
    )
    alpha = blurred[..., 3:4]
    blurred[..., :3] = np.divide(
        blurred[..., :3], alpha,
        out=np.zeros_like(blurred[..., :3]),
        where=alpha > np.float32(1e-5),
    )
    return blurred


def blur_standalone_gpu_wrapper(straight_image: np.ndarray, sigma: float) -> np.ndarray:
    weights = wrapper_weights(sigma)
    radius = len(weights) // 2
    output = straight_image.astype(np.float32, copy=True)
    for axis in (1, 0):
        source = output
        accumulated = np.zeros_like(source)
        weight_sum = np.float32(0.0)
        for index, weight in enumerate(weights):
            offset = index - radius
            coordinates = np.clip(
                np.arange(source.shape[axis]) + offset,
                0,
                source.shape[axis] - 1,
            )
            sample = np.take(source, coordinates, axis=axis)
            accumulated[..., :3] += sample[..., :3] * sample[..., 3:4] * weight
            accumulated[..., 3:4] += sample[..., 3:4] * weight
            weight_sum = np.float32(weight_sum + weight)
        accumulated /= np.maximum(weight_sum, np.float32(1e-4))
        alpha = accumulated[..., 3:4]
        accumulated[..., :3] = np.divide(
            accumulated[..., :3], alpha,
            out=np.zeros_like(accumulated[..., :3]),
            where=alpha > np.float32(1e-5),
        )
        output = accumulated
    return output


def visible_bgra(image: np.ndarray, alpha_association: str) -> np.ndarray:
    alpha = np.clip(image[..., 3:4], 0.0, 1.0)
    if alpha_association == "premultiplied":
        return image[..., :3]
    return image[..., :3] * alpha


def linear_to_srgb(values: np.ndarray) -> np.ndarray:
    values = np.clip(values, 0.0, 1.0)
    return np.where(
        values <= 0.0031308,
        values * 12.92,
        1.055 * np.power(values, 1.0 / 2.4) - 0.055,
    ).astype(np.float32)


def srgb_to_linear(values: np.ndarray) -> np.ndarray:
    values = np.clip(values, 0.0, 1.0)
    return np.where(
        values <= 0.04045,
        values / 12.92,
        np.power((values + 0.055) / 1.055, 2.4),
    ).astype(np.float32)


def active_rgb_roi(image: np.ndarray, margin: int) -> tuple[np.ndarray, list[int]]:
    active = np.any(image[..., :3] != 0.0, axis=2)
    ys, xs = np.nonzero(active)
    if len(xs) == 0:
        return image[:0, :0], [0, 0, 0, 0]
    x0 = max(0, int(xs.min()) - margin)
    y0 = max(0, int(ys.min()) - margin)
    x1 = min(image.shape[1], int(xs.max()) + 1 + margin)
    y1 = min(image.shape[0], int(ys.max()) + 1 + margin)
    return image[y0:y1, x0:x1], [x0, y0, x1, y1]


def metrics(
    reference: np.ndarray,
    candidate: np.ndarray,
    reference_alpha: str = "straight",
    candidate_alpha: str = "straight",
) -> dict[str, float | int]:
    raw = candidate.astype(np.float64) - reference.astype(np.float64)
    visible = (
        visible_bgra(candidate, candidate_alpha).astype(np.float64)
        - visible_bgra(reference, reference_alpha).astype(np.float64)
    )
    alpha_delta = candidate[..., 3].astype(np.float64) - reference[..., 3].astype(np.float64)
    finite = np.isfinite(candidate)
    return {
        "raw_rgba_rmse": float(np.sqrt(np.mean(raw * raw))),
        "raw_rgba_max_abs": float(np.max(np.abs(raw))),
        "visible_bgr_rmse": float(np.sqrt(np.mean(visible * visible))),
        "visible_bgr_max_abs": float(np.max(np.abs(visible))),
        "alpha_rmse": float(np.sqrt(np.mean(alpha_delta * alpha_delta))),
        "alpha_max_abs": float(np.max(np.abs(alpha_delta))),
        "candidate_nonfinite_components": int(candidate.size - np.count_nonzero(finite)),
        "candidate_components_outside_0_1": int(np.count_nonzero((candidate < 0.0) | (candidate > 1.0))),
    }


def save_alpha_route_preview(
    input_premultiplied: np.ndarray,
    cpu_straight: np.ndarray,
    cpu_misread_premultiplied: np.ndarray,
    standalone_straight: np.ndarray,
    standalone_misread_premultiplied: np.ndarray,
    direct_rgba_control: np.ndarray,
    sigma: float,
) -> str:
    backgrounds = (
        ("black", np.array((0.0, 0.0, 0.0), dtype=np.float32)),
        ("gray", np.array((0.18, 0.18, 0.18), dtype=np.float32)),
        ("white", np.array((1.0, 1.0, 1.0), dtype=np.float32)),
        ("green", np.array((0.08, 0.45, 0.12), dtype=np.float32)),
    )
    sources = (
        ("input", input_premultiplied, "premultiplied"),
        ("CPU wrapper", cpu_straight, "straight"),
        ("CPU misread", cpu_misread_premultiplied, "straight"),
        ("standalone GPU eq.", standalone_straight, "straight"),
        ("GPU misread", standalone_misread_premultiplied, "straight"),
        ("direct RGBA control (not HLSL)", direct_rgba_control, "premultiplied"),
    )
    center = input_premultiplied.shape[0] // 2
    x0, x1 = center - 48, center + 48
    y0, y1 = center - 48, center + 48
    crop_size = x1 - x0
    cell = crop_size * 2
    label_height = 24
    canvas = Image.new("RGB", (cell * len(sources), (cell + label_height) * len(backgrounds)), "#20242a")
    draw = ImageDraw.Draw(canvas)

    for row, (background_name, background_rgb) in enumerate(backgrounds):
        row_top = row * (cell + label_height)
        for column, (source_name, image, alpha_mode) in enumerate(sources):
            rgb = image[..., :3]
            alpha = np.clip(image[..., 3:4], 0.0, 1.0)
            associated = rgb if alpha_mode == "premultiplied" else rgb * alpha
            composited = associated + background_rgb * (1.0 - alpha)
            composited = np.clip(composited, 0.0, 1.0)
            srgb = np.where(
                composited <= 0.0031308,
                composited * 12.92,
                1.055 * np.power(composited, 1.0 / 2.4) - 0.055,
            )
            crop_rgb = np.rint(srgb[y0:y1, x0:x1, ::-1] * 255.0).astype(np.uint8)
            preview = Image.fromarray(crop_rgb, mode="RGB").resize(
                (cell, cell), Image.Resampling.NEAREST
            )
            left = column * cell
            top = row_top + label_height
            canvas.paste(preview, (left, top))
            draw.text(
                (left + 4, row_top + 4),
                f"{source_name} / {background_name}",
                fill="#ffffff",
            )

    path = DEFAULT_BUNDLE / "previews" / f"alpha-routes-sigma-{sigma:g}.png"
    path.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(path)
    return path.relative_to(ROOT).as_posix()


def save_transfer_preview(
    linear_input: np.ndarray,
    cpu_linear_output: np.ndarray,
    cpu_encoded_output: np.ndarray,
    sigma: float,
) -> str:
    center = linear_input.shape[0] // 2
    crop = (slice(center - 48, center + 48), slice(center - 48, center + 48))
    images = (
        ("input", linear_to_srgb(linear_input)),
        ("blur linear", linear_to_srgb(cpu_linear_output)),
        ("blur 8-bit sRGB", cpu_encoded_output),
    )
    cell = 192
    label_height = 24
    canvas = Image.new("RGB", (cell * len(images), cell + label_height), "#20242a")
    draw = ImageDraw.Draw(canvas)
    for index, (label, value) in enumerate(images):
        rgb = np.clip(value[crop][..., :3][..., ::-1], 0.0, 1.0)
        preview = Image.fromarray(np.rint(rgb * 255.0).astype(np.uint8), mode="RGB")
        preview = preview.resize((cell, cell), Image.Resampling.NEAREST)
        left = index * cell
        canvas.paste(preview, (left, label_height))
        draw.text((left + 4, 4), label, fill="#ffffff")
    path = DEFAULT_BUNDLE / "previews" / f"transfer-space-sigma-{sigma:g}.png"
    path.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(path)
    return path.relative_to(ROOT).as_posix()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bundle", type=Path, default=DEFAULT_BUNDLE)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    manifest_path = args.bundle / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    identity_payload = dict(manifest)
    recorded_identity = identity_payload.pop("bundle_identity_sha256", None)
    identity_bytes = json.dumps(
        identity_payload, sort_keys=True, separators=(",", ":"), ensure_ascii=False
    ).encode("utf-8")
    calculated_identity = hashlib.sha256(identity_bytes).hexdigest()
    if recorded_identity != calculated_identity:
        raise SystemExit("bundle identity SHA-256 mismatch")
    generator_path = Path(__file__).resolve().parent / "build_gaussian_fixture_bundle.py"
    if manifest.get("fixtures_source_sha256") != sha256(generator_path):
        raise SystemExit("fixture generator SHA-256 mismatch")
    rows = []
    transfer_rows = []
    alpha_route_preview = None
    transfer_preview = None
    for entry in manifest["fixtures"]:
        path = ROOT / entry["path"]
        if sha256(path) != entry["sha256"]:
            raise SystemExit(f"fixture SHA-256 mismatch: {path}")
        image = np.load(path, allow_pickle=False)
        if image.dtype != np.float32 or image.shape != tuple(entry["shape"]):
            raise SystemExit(f"fixture contract mismatch: {path}")
        if not np.isfinite(image).all():
            raise SystemExit(f"fixture contains non-finite values: {path}")
        input_alpha = entry["alpha_association"]
        for sigma in (0.1, 0.5, 3.0):
            cpu = blur_cpu(image, sigma)
            direct_rgba = blur_direct_rgba_control(image, sigma)
            straight_input = (
                straight_from_premultiplied(image)
                if input_alpha == "premultiplied"
                else image
            )
            cpu_wrapper = blur_cpu_wrapper(straight_input, sigma)
            standalone_wrapper = blur_standalone_gpu_wrapper(straight_input, sigma)
            cpu_wrapper_on_premult_as_straight = (
                blur_cpu_wrapper(image, sigma)
                if input_alpha == "premultiplied"
                else None
            )
            standalone_wrapper_on_premult_as_straight = (
                blur_standalone_gpu_wrapper(image, sigma)
                if input_alpha == "premultiplied"
                else None
            )
            cpu_radius = len(gaussian_weights(sigma, resident=False)) // 2
            shader_radius = len(gaussian_weights(sigma, resident=True)) // 2
            roi_margin = max(cpu_radius, shader_radius) + 2
            cpu_roi, roi_bounds = active_rgb_roi(cpu, roi_margin)
            direct_rgba_roi = direct_rgba[
                roi_bounds[1]:roi_bounds[3], roi_bounds[0]:roi_bounds[2]
            ]
            rows.append({
                "fixture": entry["name"],
                "input_sha256": entry["sha256"],
                "input_alpha_association": input_alpha,
                "requested_sigma_px": sigma,
                "cpu_kernel_vs_direct_rgba_control_not_current_hlsl": metrics(
                    cpu, direct_rgba, input_alpha, input_alpha
                ),
                "cpu_kernel_vs_direct_rgba_active_rgb_roi": {
                    "bounds_xyxy_px": roi_bounds,
                    "margin_px": roi_margin,
                    "rmse_metrics": metrics(
                        cpu_roi, direct_rgba_roi, input_alpha, input_alpha
                    ),
                },
                "input_vs_cpu_kernel_response_diagnostic": metrics(
                    image, cpu, input_alpha, input_alpha
                ),
                "input_vs_direct_rgba_response_diagnostic": metrics(
                    image, direct_rgba, input_alpha, input_alpha
                ),
                "cpu_wrapper_vs_direct_rgba_control_not_current_hlsl": metrics(
                    cpu_wrapper, direct_rgba, "straight", "premultiplied"
                ),
                "cpu_wrapper_vs_standalone_gpu_wrapper": metrics(
                    cpu_wrapper, standalone_wrapper, "straight", "straight"
                ),
                "standalone_gpu_wrapper_vs_direct_rgba_control_not_current_hlsl": metrics(
                    standalone_wrapper, direct_rgba, "straight", "premultiplied"
                ),
                "premultiplied_input_misread_as_straight": (
                    {
                        "cpu_wrapper_vs_correctly_unassociated_input": metrics(
                            cpu_wrapper, cpu_wrapper_on_premult_as_straight,
                            "straight", "straight"
                        ),
                        "cpu_wrapper_vs_direct_rgba_control_not_current_hlsl": metrics(
                            cpu_wrapper_on_premult_as_straight,
                            direct_rgba,
                            "straight",
                            "premultiplied",
                        ),
                        "standalone_gpu_wrapper_vs_correctly_unassociated_input": metrics(
                            standalone_wrapper,
                            standalone_wrapper_on_premult_as_straight,
                            "straight",
                            "straight",
                        ),
                        "standalone_gpu_wrapper_vs_direct_rgba_control_not_current_hlsl": metrics(
                            standalone_wrapper_on_premult_as_straight,
                            direct_rgba,
                            "straight",
                            "premultiplied",
                        ),
                    }
                    if input_alpha == "premultiplied"
                    else None
                ),
            })
            if entry["name"] == "premultiplied_alpha_edge_hdr" and sigma == 3.0:
                alpha_route_preview = save_alpha_route_preview(
                    image,
                    cpu_wrapper,
                    cpu_wrapper_on_premult_as_straight,
                    standalone_wrapper,
                    standalone_wrapper_on_premult_as_straight,
                    direct_rgba,
                    sigma,
                )
            if entry["name"] == "opaque_linear_srgb_transfer_edge":
                encoded = linear_to_srgb(image)
                quantized_encoded = np.rint(encoded * 255.0).astype(np.float32) / 255.0
                cpu_linear = blur_cpu_wrapper(image, sigma)
                cpu_encoded = blur_cpu_wrapper(quantized_encoded, sigma)
                cpu_encoded_decoded = srgb_to_linear(cpu_encoded)
                shader_linear = blur_standalone_gpu_wrapper(image, sigma)
                edge_center = image.shape[1] // 2
                edge_roi = (
                    slice(max(0, edge_center - 32), min(image.shape[0], edge_center + 32)),
                    slice(max(0, edge_center - 11), min(image.shape[1], edge_center + 12)),
                )
                transfer_rows.append({
                    "fixture": entry["name"],
                    "input_sha256": entry["sha256"],
                    "requested_sigma_px": sigma,
                    "encoded_input_quantization": "linear synthetic BGRA -> sRGB OETF -> round to 8-bit code values -> float32 normalization",
                    "output_decode_for_comparison": "sRGB EOTF after blur in encoded code values; diagnostic linear reconstruction",
                    "cpu_linear_wrapper_vs_encoded_wrapper_decoded": metrics(
                        cpu_linear, cpu_encoded_decoded, "straight", "straight"
                    ),
                    "edge_roi_xyxy_px": [edge_center - 11, edge_center - 32,
                                         edge_center + 12, edge_center + 32],
                    "cpu_linear_vs_encoded_decoded_edge_roi": metrics(
                        cpu_linear[edge_roi], cpu_encoded_decoded[edge_roi],
                        "straight", "straight"
                    ),
                    "cpu_encoded_wrapper_decoded_vs_hlsl_equation_linear": metrics(
                        cpu_encoded_decoded, shader_linear,
                        "straight", "straight"
                    ),
                    "cpu_linear_wrapper_vs_hlsl_equation_linear": metrics(
                        cpu_linear, shader_linear, "straight", "straight"
                    ),
                    "encoded_input_sha256": hashlib.sha256(
                        quantized_encoded.tobytes(order="C")
                    ).hexdigest(),
                })
                if sigma == 3.0:
                    transfer_preview = save_transfer_preview(
                        image, cpu_linear, cpu_encoded, sigma
                    )
    result = {
        "schema": "artifact.blur-rd.gaussian-fixture-analysis.v1",
        "evidence_class": "synthetic source-equation mirrors; not Artifact captures",
        "bundle_manifest_sha256": sha256(manifest_path),
        "analyzer_sha256": sha256(Path(__file__).resolve()),
        "source_files": {
            "gaussian_cpu_wrapper_and_kernel_setup": {
                "path": CPU_SOURCE.relative_to(ROOT).as_posix(),
                "sha256": sha256(CPU_SOURCE),
            },
            "gaussian_hlsl_kernel_and_dispatch": {
                "path": RESIDENT_SOURCE.relative_to(ROOT).as_posix(),
                "sha256": sha256(RESIDENT_SOURCE),
            },
        },
        "opencv_version": cv2.__version__,
        "numpy_version": np.__version__,
        "storage_and_metrics": {
            "input_storage": "float32 BGRA",
            "raw_metric": "all four stored channels, unclamped, not perceptual",
            "visible_metric": "compare straight output RGB multiplied by clipped alpha against premultiplied output RGB directly; diagnostic convention only",
            "boundary": "CPU OpenCV BORDER_REFLECT_101; HLSL equation coordinate clamp; direct RGBA control also uses clamp",
            "sigma_values_px": [0.1, 0.5, 3.0],
            "wrapper_input_assumption": "CPU and standalone wrapper source RGB is straight; premultiplied authored fixture is explicitly unassociated for this conditional mirror",
            "wrapper_output_assumption": "CPU and HLSL equation wrappers unpremultiply RGB when output alpha >1e-5; host copies source descriptor metadata unchanged, so runtime numeric association and descriptor agreement require capture",
            "hidden_rgb_case": "zero-alpha hidden-RGB fixture is an equation-level invariance control; direct-RGBA control is not a current HLSL route",
        },
        "measurements": rows,
        "transfer_space_diagnostics": {
            "evidence_class": "synthetic equation model; not a production capture",
            "description": "opaque midtone color edge blurred in linear values and in 8-bit-quantized sRGB code values; encoded output is decoded to linear for diagnostics",
            "measurements": transfer_rows,
            "display_preview": transfer_preview,
            "limitation": "The QImage input route and HLSL equation route may have different working-space contracts; this model does not prove which route a frame uses or what later color conversion is applied.",
        },
        "alpha_route_display_preview": {
            "path": alpha_route_preview,
            "description": "synthetic source-equation outputs and direct-RGBA control composited on black/gray/white/green, clipped to SDR, sRGB encoded, nearest-neighbor enlarged; not an Artifact capture or view-transform match",
        },
        "limitations": [
            "CPU operation mirrors only the OpenCV kernel equation, kernel-size formula, and border mode; it does not reproduce the Artifact wrapper's RGB-alpha multiply/filter/unmultiply or descriptor conversions.",
            "Direct RGBA control mirrors normalized float32 weights and clamp sampling only; it does not mirror current HLSL alpha multiplication/unassociation. The standalone GPU wrapper is the current HLSL equation mirror, not an actual GPU execution.",
            "Input-to-output differences measure blur response, not error against a ground truth.",
            "No visual review, runtime capture, performance, or product quality verdict is included.",
        ],
    }
    output_path = args.output or args.bundle / "equation_analysis.json"
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(
        json.dumps(result, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    print(output_path)


if __name__ == "__main__":
    main()
