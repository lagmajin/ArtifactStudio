"""Estimate a relative near-is-bright depth map for GlowLab flash-front R&D."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import tempfile
import time
from urllib.request import urlopen

import cv2
import numpy as np


MODEL_REPOSITORY = "onnx-community/depth-anything-v2-small"
MODEL_REVISION = "4472b7362082ad9968fee890ca0f1e5aca36b93d"
MODEL_RELATIVE_PATH = "onnx/model_int8.onnx"
MODEL_SIZE_BYTES = 27_258_801
MODEL_SHA256 = "01aa7a23de3f4a0ee1a2bb9997e6918104c85a9f95dea46d27b9b3fb0c6b9001"
MODEL_URL = (f"https://huggingface.co/{MODEL_REPOSITORY}/resolve/"
             f"{MODEL_REVISION}/{MODEL_RELATIVE_PATH}")
INPUT_SIZE = 518
PATCH_MULTIPLE = 14
IMAGE_MEAN = np.asarray((0.485, 0.456, 0.406), dtype=np.float32)
IMAGE_STD = np.asarray((0.229, 0.224, 0.225), dtype=np.float32)


def default_model_path() -> Path:
    cache_root = Path(os.environ.get("LOCALAPPDATA", Path.home() / ".cache"))
    return cache_root / "GlowLab" / "models" / "depth-anything-v2-small-int8.onnx"


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def download_model(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and path.stat().st_size == MODEL_SIZE_BYTES:
        if sha256_file(path) == MODEL_SHA256:
            print(f"Using verified cached model: {path}")
            return
        path.unlink()
    temporary_path: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(prefix="glowlab-depth-", suffix=".part",
                                         dir=path.parent, delete=False) as temporary:
            temporary_path = Path(temporary.name)
            with urlopen(MODEL_URL, timeout=60) as response:
                while chunk := response.read(1024 * 1024):
                    temporary.write(chunk)
        size = temporary_path.stat().st_size
        if size != MODEL_SIZE_BYTES:
            raise OSError(f"Downloaded model size {size} does not match expected {MODEL_SIZE_BYTES}")
        if sha256_file(temporary_path) != MODEL_SHA256:
            raise OSError("Downloaded model SHA-256 does not match the pinned revision")
        temporary_path.replace(path)
    finally:
        if temporary_path is not None and temporary_path.exists():
            temporary_path.unlink()
    print(f"Downloaded pinned {MODEL_REPOSITORY} model to {path}")


def constrain_to_multiple(value: float, minimum: int) -> int:
    rounded = int(np.round(value / PATCH_MULTIPLE) * PATCH_MULTIPLE)
    if rounded < minimum:
        rounded = int(np.ceil(minimum / PATCH_MULTIPLE) * PATCH_MULTIPLE)
    return rounded


def preprocess(image_rgb: np.ndarray) -> tuple[np.ndarray, tuple[int, int]]:
    height, width = image_rgb.shape[:2]
    scale = max(INPUT_SIZE / width, INPUT_SIZE / height)
    resized_width = constrain_to_multiple(scale * width, INPUT_SIZE)
    resized_height = constrain_to_multiple(scale * height, INPUT_SIZE)
    resized = cv2.resize(image_rgb, (resized_width, resized_height),
                         interpolation=cv2.INTER_CUBIC).astype(np.float32) / 255.0
    normalized = (resized - IMAGE_MEAN) / IMAGE_STD
    tensor = np.ascontiguousarray(normalized.transpose(2, 0, 1)[None, ...], dtype=np.float32)
    return tensor, (resized_width, resized_height)


def create_session(model_path: Path):
    try:
        import onnxruntime as ort
    except ImportError as error:
        raise RuntimeError("Depth estimation requires optional onnxruntime; install requirements-depth.txt") from error
    options = ort.SessionOptions()
    options.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_ALL
    session = ort.InferenceSession(str(model_path), sess_options=options,
                                   providers=["CPUExecutionProvider"])
    return session, ort


def infer_raw_depth(session, image_bgr: np.ndarray) -> tuple[np.ndarray, tuple[int, int], float]:
    image_rgb = cv2.cvtColor(image_bgr, cv2.COLOR_BGR2RGB)
    tensor, resized_size = preprocess(image_rgb)
    model_input = session.get_inputs()[0]
    model_output = session.get_outputs()[0]
    started = time.perf_counter()
    prediction = session.run([model_output.name], {model_input.name: tensor})[0]
    inference_seconds = time.perf_counter() - started
    prediction = np.asarray(prediction, dtype=np.float32).squeeze()
    if prediction.ndim != 2 or not np.isfinite(prediction).all():
        raise ValueError(f"Unexpected/non-finite depth output shape: {prediction.shape}")
    height, width = image_bgr.shape[:2]
    prediction = cv2.resize(prediction, (width, height), interpolation=cv2.INTER_CUBIC)
    return np.maximum(prediction, 0.0), resized_size, inference_seconds


def normalize_relative_depth(raw_depth: np.ndarray, percentiles: tuple[float, float] = (2.0, 98.0),
                             invert: bool = False) -> tuple[np.ndarray, tuple[float, float]]:
    low, high = np.percentile(raw_depth, percentiles)
    if not np.isfinite((low, high)).all() or high - low <= 1.0e-8:
        raise ValueError("Depth prediction has insufficient range to normalize")
    normalized = np.clip((raw_depth - low) / (high - low), 0.0, 1.0)
    if invert:
        normalized = 1.0 - normalized
    return normalized, (float(low), float(high))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True, help="source image or video frame")
    parser.add_argument("--output", type=Path, required=True, help="output folder")
    parser.add_argument("--model", type=Path, default=default_model_path(),
                        help="local pinned ONNX model path")
    parser.add_argument("--download-model", action="store_true",
                        help="download the pinned 27 MB Apache-2.0 int8 model if missing")
    parser.add_argument("--invert", action="store_true",
                        help="invert model ordering if inspection shows far=bright for this model export")
    parser.add_argument("--percentile-range", nargs=2, type=float, default=(2.0, 98.0),
                        metavar=("LOW", "HIGH"),
                        help="robust relative-depth range mapped to black/white")
    args = parser.parse_args()
    if not args.input.is_file():
        parser.error(f"input image does not exist: {args.input}")
    if args.download_model:
        download_model(args.model)
    if not args.model.is_file():
        parser.error(f"model does not exist: {args.model}; pass --download-model to fetch the pinned model")
    if args.model.stat().st_size != MODEL_SIZE_BYTES or sha256_file(args.model) != MODEL_SHA256:
        parser.error("model file does not match the pinned revision; pass --download-model to replace it")
    low_percentile, high_percentile = args.percentile_range
    if not (0.0 <= low_percentile < high_percentile <= 100.0):
        parser.error("percentile range must satisfy 0 <= LOW < HIGH <= 100")

    bgr = cv2.imread(str(args.input), cv2.IMREAD_COLOR)
    if bgr is None:
        parser.error(f"could not decode input image: {args.input}")
    session, ort = create_session(args.model)
    raw_depth, resized_size, inference_seconds = infer_raw_depth(session, bgr)
    normalized, (low, high) = normalize_relative_depth(raw_depth,
                                                       (low_percentile, high_percentile), args.invert)
    height, width = bgr.shape[:2]
    depth_u8 = np.round(normalized * 255.0).astype(np.uint8)
    color_preview = cv2.applyColorMap(depth_u8, cv2.COLORMAP_TURBO)
    comparison = np.hstack((bgr, color_preview))

    args.output.mkdir(parents=True, exist_ok=True)
    depth_path = args.output / "depth-near-bright.png"
    preview_path = args.output / "depth-estimation-preview.jpg"
    raw_path = args.output / "depth-relative.npy"
    metrics_path = args.output / "depth-estimation-metrics.json"
    if not cv2.imwrite(str(depth_path), depth_u8):
        raise OSError(f"Failed to write {depth_path}")
    if not cv2.imwrite(str(preview_path), comparison, [cv2.IMWRITE_JPEG_QUALITY, 94]):
        raise OSError(f"Failed to write {preview_path}")
    np.save(raw_path, raw_depth.astype(np.float32))
    digest = sha256_file(args.model)
    report = {
        "input_image": str(args.input.resolve()),
        "model_repository": MODEL_REPOSITORY,
        "model_revision": MODEL_REVISION,
        "model_file_sha256": digest,
        "model_license": "Apache-2.0",
        "runtime": f"onnxruntime {ort.__version__}",
        "execution_provider": "CPUExecutionProvider",
        "input_dimensions": {"width": width, "height": height},
        "model_input_dimensions": {"width": resized_size[0], "height": resized_size[1]},
        "inference_seconds": inference_seconds,
        "depth_direction": "higher=far" if args.invert else "higher=near",
        "normalization_percentiles": [low_percentile, high_percentile],
        "raw_depth_percentiles": {str(p): float(v) for p, v in zip(
            (0, 1, 10, 50, 90, 99, 100), np.percentile(raw_depth, (0, 1, 10, 50, 90, 99, 100)))},
        "outputs": [depth_path.name, preview_path.name, raw_path.name],
        "limitation": "Monocular relative depth is not metric depth; inspect foreground/background ordering before use.",
    }
    with metrics_path.open("w", encoding="utf-8", newline="\n") as stream:
        json.dump(report, stream, indent=2, ensure_ascii=False)
        stream.write("\n")
    print(f"Saved near-bright depth map to {depth_path} ({inference_seconds:.3f}s inference)")


if __name__ == "__main__":
    main()
