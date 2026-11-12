"""Compare matched installed Glow-family CPU and diagnostic-GPU captures."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import cv2
import numpy as np

from glow_product_capture_review import linear_luma, read_rgba
from glow_lab import add_label

ROIS = {
    "full_frame": (0.0, 0.0, 1.0, 1.0),
    "magenta_ring": (0.15, 0.18, 0.40, 0.58),
    "emissive_text": (0.36, 0.35, 0.58, 0.54),
    "compact_cyan_practical": (0.70, 0.28, 0.82, 0.48),
    "compact_amber_practical": (0.74, 0.43, 0.84, 0.56),
    "horizontal_tube": (0.08, 0.58, 0.90, 0.69),
}


def roi_slice(image: np.ndarray, rect: tuple[float, float, float, float]) -> np.ndarray:
    h, w = image.shape[:2]
    x0, y0, x1, y1 = rect
    return image[int(y0 * h):int(y1 * h), int(x0 * w):int(x1 * w)]


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(cpu_output: Path, gpu_output: Path, output_dir: Path) -> None:
    cpu_input = cpu_output.with_name(f"{cpu_output.stem}-fixture-input{cpu_output.suffix}")
    gpu_input = gpu_output.with_name(f"{gpu_output.stem}-fixture-input{gpu_output.suffix}")
    cpu_state = Path(str(cpu_output) + ".state.txt")
    gpu_state = Path(str(gpu_output) + ".state.txt")
    for path in (cpu_input, gpu_input, cpu_state, gpu_state):
        if not path.exists():
            raise RuntimeError(f"Missing matched capture artifact: {path}")
    cpu_state_text = cpu_state.read_text(encoding="utf-8")
    gpu_state_text = gpu_state.read_text(encoding="utf-8")
    if "compute=0" not in cpu_state_text or "captureSource=Installed CPU reference" not in cpu_state_text:
        raise RuntimeError(f"Unexpected CPU state sidecar: {cpu_state}")
    if "compute=1" not in gpu_state_text or "captureSource=Accum RT" not in gpu_state_text:
        raise RuntimeError(f"Unexpected GPU state sidecar: {gpu_state}")
    if sha256(cpu_input) != sha256(gpu_input):
        raise RuntimeError("CPU and GPU fixture input captures are not byte-identical")

    source = read_rgba(cpu_input)
    cpu = read_rgba(cpu_output)
    gpu = read_rgba(gpu_output)
    if source.shape != cpu.shape or source.shape != gpu.shape:
        raise RuntimeError(f"Dimension mismatch: source={source.shape}, CPU={cpu.shape}, GPU={gpu.shape}")

    source_rgb = source[..., :3].astype(np.float32) / 255.0
    cpu_rgb = cpu[..., :3].astype(np.float32) / 255.0
    gpu_rgb = gpu[..., :3].astype(np.float32) / 255.0
    cpu_delta = linear_luma(cpu[..., :3]) - linear_luma(source[..., :3])
    gpu_delta = linear_luma(gpu[..., :3]) - linear_luma(source[..., :3])
    delta_error = np.abs(cpu_rgb - gpu_rgb)
    newly_clipped_cpu = (cpu[..., :3] >= 254) & (source[..., :3] < 254)
    newly_clipped_gpu = (gpu[..., :3] >= 254) & (source[..., :3] < 254)

    diff = np.clip(np.abs(cpu.astype(np.int16) - gpu.astype(np.int16)) * 6, 0, 255).astype(np.uint8)
    panels = []
    for image, label in ((source, "INPUT"), (cpu, "INSTALLED CPU REFERENCE"),
                         (gpu, "GPU ACCUM RT"), (diff, "ABS CPU/GPU DIFF x6")):
        resized = cv2.resize(image, (480, 270), interpolation=cv2.INTER_AREA)
        panels.append(add_label(resized, label))
    sheet = np.hstack(panels)

    output_dir.mkdir(parents=True, exist_ok=True)
    image_path = output_dir / f"{cpu_output.stem}-cpu-gpu-review.jpg"
    cv2.imwrite(str(image_path), cv2.cvtColor(sheet, cv2.COLOR_RGBA2BGRA),
                [cv2.IMWRITE_JPEG_QUALITY, 95])
    roi_metrics: dict[str, object] = {}
    for roi_name, rect in ROIS.items():
        roi_source = roi_slice(source, rect)
        roi_cpu = roi_slice(cpu, rect)
        roi_gpu = roi_slice(gpu, rect)
        roi_cpu_delta = linear_luma(roi_cpu[..., :3]) - linear_luma(roi_source[..., :3])
        roi_gpu_delta = linear_luma(roi_gpu[..., :3]) - linear_luma(roi_source[..., :3])
        roi_metrics[roi_name] = {
            "normalized_rect": list(rect),
            "encoded_rgb_mae_cpu_gpu": float(np.abs(
                roi_cpu[..., :3].astype(np.float32) -
                roi_gpu[..., :3].astype(np.float32)).mean() / 255.0),
            "effect_delta_linear_luma_mae": float(np.abs(roi_cpu_delta - roi_gpu_delta).mean()),
        }

    metrics = {
        "capture_class": "matched ArtifactUiTest installed-effect CPU reference and diagnostic GPU Accum RT",
        "not_final_composition_viewer_output": True,
        "source_dimensions": [int(source.shape[1]), int(source.shape[0])],
        "source_sha256": sha256(cpu_input),
        "cpu_output_sha256": sha256(cpu_output),
        "gpu_output_sha256": sha256(gpu_output),
        "encoded_rgb_mae_cpu_gpu": float(delta_error.mean()),
        "encoded_rgb_p95_abs_error": float(np.percentile(delta_error, 95)),
        "cpu_effect_delta_linear_luma_mean": float(cpu_delta.mean()),
        "gpu_effect_delta_linear_luma_mean": float(gpu_delta.mean()),
        "effect_delta_linear_luma_mae": float(np.abs(cpu_delta - gpu_delta).mean()),
        "cpu_newly_clipped_channel_fraction": float(newly_clipped_cpu.mean()),
        "gpu_newly_clipped_channel_fraction": float(newly_clipped_gpu.mean()),
        "alpha_mae": float(np.abs(cpu[..., 3].astype(np.int16) -
                                  gpu[..., 3].astype(np.int16)).mean() / 255.0),
        "rois": roi_metrics,
        "cpu_state_sidecar": cpu_state.name,
        "gpu_state_sidecar": gpu_state.name,
        "limitations": [
            "GPU pixels are captured from diagnostic Accum RT, not final Composition Viewer presentation",
            "single effect, source plate, settings, and backend capture",
            "image error metrics do not establish perceptual quality or performance",
        ],
    }
    metrics_path = output_dir / f"{cpu_output.stem}-cpu-gpu-metrics.json"
    metrics_path.write_text(json.dumps(metrics, indent=2) + "\n", encoding="utf-8")
    print(f"Saved {image_path}")
    print(f"Saved {metrics_path}")
    print(f"encoded RGB MAE={metrics['encoded_rgb_mae_cpu_gpu']:.6f}; "
          f"effect ΔL MAE={metrics['effect_delta_linear_luma_mae']:.6f}; "
          f"alpha MAE={metrics['alpha_mae']:.6f}")
    for roi_name, roi_data in roi_metrics.items():
        print(f"{roi_name}: RGB MAE={roi_data['encoded_rgb_mae_cpu_gpu']:.6f}; "
              f"effect ΔL MAE={roi_data['effect_delta_linear_luma_mae']:.6f}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cpu-output", type=Path, required=True)
    parser.add_argument("--gpu-output", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    run(args.cpu_output, args.gpu_output, args.output_dir)


if __name__ == "__main__":
    main()
