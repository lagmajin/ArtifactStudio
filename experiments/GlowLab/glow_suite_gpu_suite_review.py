"""Build a matched CPU / diagnostic-GPU review sheet for GPU Glow families."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import cv2
import numpy as np

from glow_lab import add_label
from glow_product_capture_review import linear_luma, read_rgba


EFFECTS = (
    "glow", "edge_bloom", "reactive_glow", "chromatic_glow", "liquid_glow",
    "directional_glow", "luminescence_caustics", "builtin.volumetric_shine",
)
ROIS = {
    "magenta_ring": (0.15, 0.18, 0.40, 0.58),
    "emissive_text": (0.36, 0.35, 0.58, 0.54),
    "compact_cyan_practical": (0.70, 0.28, 0.82, 0.48),
    "compact_amber_practical": (0.74, 0.43, 0.84, 0.56),
    "horizontal_tube": (0.08, 0.58, 0.90, 0.69),
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def crop(image: np.ndarray, rect: tuple[float, float, float, float]) -> np.ndarray:
    h, w = image.shape[:2]
    x0, y0, x1, y1 = rect
    return image[int(y0 * h):int(y1 * h), int(x0 * w):int(x1 * w)]


def effect_paths(effect_id: str, cpu_dir: Path, gpu_dir: Path,
                 glow_dir: Path | None = None) -> tuple[Path, Path]:
    key = effect_id.replace(".", "_")
    cpu = cpu_dir / f"app-role-{key}-{effect_id}-cpu.png"
    actual_gpu_dir = glow_dir if effect_id == "glow" and glow_dir is not None else gpu_dir
    gpu_name = (f"glow-{effect_id}-gpu.png" if effect_id == "glow" and glow_dir is not None
                else f"gpu-role-{key}-{effect_id}-gpu.png")
    gpu = actual_gpu_dir / gpu_name
    return cpu, gpu


def review_effect(effect_id: str, cpu_path: Path, gpu_path: Path,
                  output_dir: Path) -> tuple[dict[str, object], np.ndarray]:
    cpu_source = cpu_path.with_name(f"{cpu_path.stem}-fixture-input{cpu_path.suffix}")
    gpu_source = gpu_path.with_name(f"{gpu_path.stem}-fixture-input{gpu_path.suffix}")
    cpu_state_path = Path(str(cpu_path) + ".state.txt")
    gpu_state_path = Path(str(gpu_path) + ".state.txt")
    for path in (cpu_path, gpu_path, cpu_source, gpu_source, cpu_state_path, gpu_state_path):
        if not path.exists():
            raise RuntimeError(f"Missing capture artifact: {path}")
    cpu_state = cpu_state_path.read_text(encoding="utf-8")
    gpu_state = gpu_state_path.read_text(encoding="utf-8")
    if "compute=0" not in cpu_state or "captureSource=Installed CPU reference" not in cpu_state:
        raise RuntimeError(f"Unexpected CPU route: {cpu_state_path}")
    if "compute=1" not in gpu_state or "captureSource=Accum RT" not in gpu_state:
        raise RuntimeError(f"Unexpected GPU route: {gpu_state_path}")
    if sha256(cpu_source) != sha256(gpu_source):
        raise RuntimeError(f"CPU/GPU input mismatch: {effect_id}")

    source = read_rgba(cpu_source)
    cpu = read_rgba(cpu_path)
    gpu = read_rgba(gpu_path)
    if source.shape != cpu.shape or source.shape != gpu.shape:
        raise RuntimeError(f"CPU/GPU dimension mismatch: {effect_id}")

    cpu_rgb = cpu[..., :3].astype(np.float32) / 255.0
    gpu_rgb = gpu[..., :3].astype(np.float32) / 255.0
    source_luma = linear_luma(source[..., :3])
    cpu_delta = linear_luma(cpu[..., :3]) - source_luma
    gpu_delta = linear_luma(gpu[..., :3]) - source_luma
    result: dict[str, object] = {
        "effect_id": effect_id,
        "source_sha256": sha256(cpu_source),
        "cpu_output_sha256": sha256(cpu_path),
        "gpu_output_sha256": sha256(gpu_path),
        "dimensions": [int(source.shape[1]), int(source.shape[0])],
        "encoded_rgb_mae_cpu_gpu": float(np.abs(cpu_rgb - gpu_rgb).mean()),
        "encoded_rgb_abs_error_p95": float(np.percentile(np.abs(cpu_rgb - gpu_rgb), 95)),
        "effect_delta_linear_luma_mae": float(np.abs(cpu_delta - gpu_delta).mean()),
        "alpha_mae": float(np.abs(cpu[..., 3].astype(np.int16) -
                                  gpu[..., 3].astype(np.int16)).mean() / 255.0),
        "rois": {},
        "cpu_state": cpu_state_path.name,
        "gpu_state": gpu_state_path.name,
    }
    for roi_name, rect in ROIS.items():
        roi_cpu = crop(cpu, rect)
        roi_gpu = crop(gpu, rect)
        roi_source = crop(source, rect)
        roi_cpu_delta = linear_luma(roi_cpu[..., :3]) - linear_luma(roi_source[..., :3])
        roi_gpu_delta = linear_luma(roi_gpu[..., :3]) - linear_luma(roi_source[..., :3])
        result["rois"][roi_name] = {
            "encoded_rgb_mae_cpu_gpu": float(np.abs(
                roi_cpu[..., :3].astype(np.float32) -
                roi_gpu[..., :3].astype(np.float32)).mean() / 255.0),
            "effect_delta_linear_luma_mae": float(np.abs(roi_cpu_delta - roi_gpu_delta).mean()),
        }

    tiles = []
    for image, title in ((cpu, "CPU REFERENCE"), (gpu, "GPU ACCUM RT")):
        resized = cv2.resize(image, (320, 180), interpolation=cv2.INTER_AREA)
        tiles.append(add_label(resized, title))
    diff = np.clip(np.abs(cpu.astype(np.int16) - gpu.astype(np.int16)) * 6,
                   0, 255).astype(np.uint8)
    diff_small = cv2.resize(diff, (320, 180), interpolation=cv2.INTER_AREA)
    tiles.append(add_label(diff_small, "ABS DIFF x6"))
    row = add_label(np.hstack(tiles),
                    f"{effect_id} | RGB MAE={result['encoded_rgb_mae_cpu_gpu']:.4f} "
                    f"| effect dL MAE={result['effect_delta_linear_luma_mae']:.4f}")
    return result, row


def run(cpu_dir: Path, gpu_dir: Path, output_dir: Path,
        glow_dir: Path | None = None) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    rows: list[np.ndarray] = []
    results: list[dict[str, object]] = []
    for effect_id in EFFECTS:
        cpu, gpu = effect_paths(effect_id, cpu_dir, gpu_dir, glow_dir)
        result, row = review_effect(effect_id, cpu, gpu, output_dir)
        results.append(result)
        rows.append(row)
        print(f"{effect_id}: RGB MAE={result['encoded_rgb_mae_cpu_gpu']:.6f}; "
              f"effect ΔL MAE={result['effect_delta_linear_luma_mae']:.6f}; "
              f"alpha MAE={result['alpha_mae']:.6f}")

    sheet_path = output_dir / "glow-suite-cpu-gpu-contact-sheet.jpg"
    if not cv2.imwrite(str(sheet_path), np.vstack(rows), [cv2.IMWRITE_JPEG_QUALITY, 95]):
        raise RuntimeError(f"Could not save review sheet: {sheet_path}")
    metrics_path = output_dir / "glow-suite-cpu-gpu-metrics.json"
    metrics_path.write_text(json.dumps({
        "capture_class": "matched installed CPU reference and diagnostic GPU Accum RT",
        "common_input_sha256": results[0]["source_sha256"],
        "effect_ids": list(EFFECTS),
        "results": results,
        "limitations": [
            "GPU output is diagnostic Accum RT, not final Composition Viewer output",
            "single synthetic neon plate and role-candidate settings",
            "metrics do not replace 1:1 aesthetic review or prove backend parity universally",
            "CPU-only Residual Glow, Physical Halation, Optical Glow, and Glint Star are outside this GPU set",
        ],
    }, indent=2) + "\n", encoding="utf-8")
    print(f"Saved {sheet_path}")
    print(f"Saved {metrics_path}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cpu-dir", type=Path, required=True)
    parser.add_argument("--gpu-dir", type=Path, required=True)
    parser.add_argument("--glow-dir", type=Path)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    run(args.cpu_dir, args.gpu_dir, args.output_dir, args.glow_dir)


if __name__ == "__main__":
    main()
