"""AI monocular depth map R&D lab.

Deterministic synthetic scenes with analytic ground-truth depth, classical
priors, and ONNX depth models are scored with scale-free ordering, alignment,
silhouette and temporal metrics. The pre-registered gates live in
EVALUATION_PLAN.md and are evaluated against the baselines of the same run.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import tempfile
import time
import zlib
from pathlib import Path
from urllib.request import urlopen

import cv2
import numpy as np


SEED = 20261116
FIXTURE_WIDTH = 640
FIXTURE_HEIGHT = 360
FOV_DEGREES = 52.0
CAMERA_ORIGIN = (0.0, 1.6, 0.0)
SKY_COLOR = (0.62, 0.70, 0.80)
FAR_DEPTH = 200.0

LAB_DIR = Path(__file__).resolve().parent
DEFAULT_MODEL_PATH = LAB_DIR / "models" / "depth-anything-v2-small-int8.onnx"

MODEL_REPOSITORY = "onnx-community/depth-anything-v2-small"
MODEL_REVISION = "4472b7362082ad9968fee890ca0f1e5aca36b93d"
MODEL_RELATIVE_PATH = "onnx/model_int8.onnx"
MODEL_SIZE_BYTES = 27_258_801
MODEL_SHA256 = "01aa7a23de3f4a0ee1a2bb9997e6918104c85a9f95dea46d27b9b3fb0c6b9001"
MODEL_URL = (f"https://huggingface.co/{MODEL_REPOSITORY}/resolve/"
             f"{MODEL_REVISION}/{MODEL_RELATIVE_PATH}")

PRESETS = {
    "da2_small": {"size": 518, "multiple": 14,
                  "mean": (0.485, 0.456, 0.406), "std": (0.229, 0.224, 0.225)},
    "midas_small": {"size": 384, "multiple": 32,
                    "mean": (0.5, 0.5, 0.5), "std": (0.5, 0.5, 0.5)},
    "generic_512": {"size": 512, "multiple": 32,
                    "mean": (0.0, 0.0, 0.0), "std": (1.0, 1.0, 1.0)},
}

# Pre-registered in EVALUATION_PLAN.md section 5. Do not edit after looking at
# candidate results without recording a revision in gates.json.
GATES = {
    "ordinal_accuracy": (">=", 0.90),
    "spearman": (">=", 0.75),
    "si_rmse": ("<=", 0.35),
    "delta1": (">=", 0.80),
    "edge_f1": (">=", 0.55),
    "boundary_band_mae": ("<=", 0.12),
    "foreground_leak": ("<=", 0.05),
    "background_intrusion": ("<=", 0.05),
}

TEMPORAL_GATE = ("temporal_flow_warped_mean_abs_delta", "<=", 0.05)


# ---------------------------------------------------------------------------
# Model download / verification


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def download_model(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.is_file() and path.stat().st_size == MODEL_SIZE_BYTES:
        if sha256_file(path) == MODEL_SHA256:
            print(f"Using verified cached model: {path}")
            return
        path.unlink()
    temporary_path: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(prefix="depthlab-", suffix=".part",
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


# ---------------------------------------------------------------------------
# Deterministic synthetic scenes with analytic z-depth


def camera_rays(width: int, height: int, fov_degrees: float = FOV_DEGREES) -> np.ndarray:
    focal = 0.5 * width / math.tan(0.5 * math.radians(fov_degrees))
    xs = (np.arange(width, dtype=np.float32) + 0.5 - 0.5 * width) / focal
    ys = -(np.arange(height, dtype=np.float32) + 0.5 - 0.5 * height) / focal
    grid_x, grid_y = np.meshgrid(xs, ys)
    directions = np.stack((grid_x, grid_y, -np.ones_like(grid_x)), axis=-1)
    directions /= np.linalg.norm(directions, axis=-1, keepdims=True)
    return directions


def surface_albedo(point: np.ndarray, phase: float) -> np.ndarray:
    x, y, z = point[..., 0], point[..., 1], point[..., 2]
    pattern = 0.55 + 0.18 * np.sin(4.3 * x + 1.7 * phase) * np.sin(3.1 * z + 2.1 * y + phase)
    pattern += 0.12 * np.sin(11.7 * x + 9.3 * z + 4.1 * phase) * np.sin(7.9 * y + 5.3 * phase)
    pattern += 0.06 * np.sin(57.31 * x + 91.7 * y + 43.13 * z + 13.7 * phase)
    return np.clip(pattern, 0.05, 1.0).astype(np.float32)


def _intersect_plane(directions: np.ndarray, origin: np.ndarray, y: float) -> np.ndarray:
    dy = directions[..., 1]
    with np.errstate(divide="ignore", invalid="ignore"):
        t = (y - origin[1]) / dy
    return np.where((t > 1e-3) & (dy < -1e-6), t, np.inf).astype(np.float32)


def _intersect_box(directions: np.ndarray, origin: np.ndarray,
                   center: np.ndarray, half: np.ndarray) -> np.ndarray:
    safe = np.where(np.abs(directions) < 1e-8, 1e-8, directions)
    inv = 1.0 / safe
    low = (center - half - origin) * inv
    high = (center + half - origin) * inv
    near = np.minimum(low, high).max(axis=-1)
    far = np.maximum(low, high).min(axis=-1)
    hit = far >= np.maximum(near, 0.0)
    t = np.where(near > 1e-3, near, far)
    return np.where(hit & (t > 1e-3), t, np.inf).astype(np.float32)


def _intersect_sphere(directions: np.ndarray, origin: np.ndarray,
                      center: np.ndarray, radius: float) -> np.ndarray:
    offset = origin - center
    b = 2.0 * np.sum(directions * offset, axis=-1)
    c = float(offset @ offset) - radius * radius
    discriminant = b * b - 4.0 * c
    root = np.sqrt(np.maximum(discriminant, 0.0))
    t_near = (-b - root) * 0.5
    t_far = (-b + root) * 0.5
    t = np.where(t_near > 1e-3, t_near, t_far)
    return np.where((discriminant >= 0.0) & (t > 1e-3), t, np.inf).astype(np.float32)


def render_scene(objects: list[dict], width: int = FIXTURE_WIDTH,
                 height: int = FIXTURE_HEIGHT) -> dict:
    directions = camera_rays(width, height)
    origin = np.asarray(CAMERA_ORIGIN, dtype=np.float32)
    best_t = np.full((height, width), np.inf, dtype=np.float32)
    best_id = np.full((height, width), -1, dtype=np.int32)
    for index, obj in enumerate(objects):
        if obj["kind"] == "plane":
            t = _intersect_plane(directions, origin, obj["y"])
        elif obj["kind"] == "box":
            t = _intersect_box(directions, origin, np.asarray(obj["center"], np.float32),
                               np.asarray(obj["half"], np.float32))
        elif obj["kind"] == "sphere":
            t = _intersect_sphere(directions, origin, np.asarray(obj["center"], np.float32),
                                  float(obj["radius"]))
        else:
            raise ValueError(f"unknown object kind: {obj['kind']}")
        closer = t < best_t
        best_t = np.where(closer, t, best_t)
        best_id = np.where(closer, index, best_id)

    rgb = np.empty((height, width, 3), dtype=np.float32)
    rgb[:] = np.asarray(SKY_COLOR, dtype=np.float32)
    light = np.asarray((0.35, 0.9, -0.25), dtype=np.float32)
    light /= np.linalg.norm(light)
    point = origin + directions * best_t[..., None]
    point[~np.isfinite(best_t)] = 0.0

    for index, obj in enumerate(objects):
        mask = best_id == index
        if not np.any(mask):
            continue
        if obj["kind"] == "plane":
            normal = np.zeros_like(point)
            normal[..., 1] = 1.0
        elif obj["kind"] == "sphere":
            normal = (point - np.asarray(obj["center"], np.float32)) / float(obj["radius"])
        else:
            center = np.asarray(obj["center"], np.float32)
            half = np.asarray(obj["half"], np.float32)
            local = (point - center) / half
            axis = np.abs(local).argmax(axis=-1)
            value = np.take_along_axis(local, axis[..., None], -1)[..., 0]
            normal = np.zeros_like(local)
            np.put_along_axis(normal, axis[..., None], np.sign(value)[..., None], -1)

        lambert = np.clip(np.sum(normal * light, axis=-1), 0.0, 1.0)
        shade = 0.28 + 0.72 * lambert
        albedo = surface_albedo(point, float(obj.get("phase", 0.0)))
        color = np.asarray(obj["color"], dtype=np.float32)
        rgb[mask] = np.clip(color * albedo[mask, None] * shade[mask, None], 0.0, 1.0)

    depth_z = np.where(np.isfinite(best_t), best_t * (-directions[..., 2]), FAR_DEPTH)
    depth_z = np.maximum(depth_z.astype(np.float32), 1e-3)
    disparity = 1.0 / depth_z
    image = np.round(rgb[..., ::-1] * 255.0).astype(np.uint8)
    return {
        "image": image,
        "depth_z": depth_z,
        "disparity": disparity,
        "near": normalize_near(disparity),
    }


def synthetic_scenes() -> dict[str, list[dict]]:
    floor = {"kind": "plane", "y": 0.0, "color": (0.42, 0.45, 0.50), "phase": 0.7}
    return {
        "scene_boxes": [
            floor,
            {"kind": "box", "center": (-1.05, 1.00, -2.60), "half": (0.65, 1.00, 0.45),
             "color": (0.10, 0.12, 0.18), "phase": 1.3},
            {"kind": "box", "center": (1.15, 0.75, -5.20), "half": (0.85, 0.75, 0.60),
             "color": (0.55, 0.50, 0.45), "phase": 2.1},
            {"kind": "sphere", "center": (0.10, 0.62, -3.90), "radius": 0.60,
             "color": (0.75, 0.55, 0.35), "phase": 3.7},
            {"kind": "box", "center": (0.0, 2.40, -9.50), "half": (3.40, 2.40, 0.30),
             "color": (0.92, 0.90, 0.82), "phase": 0.2},
        ],
        "scene_corridor": [
            floor,
            {"kind": "box", "center": (-1.90, 1.50, -2.00), "half": (0.28, 1.50, 0.22),
             "color": (0.70, 0.66, 0.58), "phase": 0.9},
            {"kind": "box", "center": (1.90, 1.50, -3.00), "half": (0.28, 1.50, 0.22),
             "color": (0.16, 0.18, 0.22), "phase": 1.6},
            {"kind": "box", "center": (-1.90, 1.50, -4.50), "half": (0.28, 1.50, 0.22),
             "color": (0.22, 0.24, 0.28), "phase": 2.4},
            {"kind": "box", "center": (1.90, 1.50, -6.00), "half": (0.28, 1.50, 0.22),
             "color": (0.80, 0.76, 0.68), "phase": 3.1},
            {"kind": "box", "center": (-1.90, 1.50, -7.50), "half": (0.28, 1.50, 0.22),
             "color": (0.34, 0.36, 0.40), "phase": 3.9},
            {"kind": "box", "center": (0.0, 2.20, -11.00), "half": (2.80, 2.20, 0.25),
             "color": (0.90, 0.88, 0.80), "phase": 0.4},
        ],
        "scene_occlusion": [
            floor,
            {"kind": "box", "center": (0.0, 2.00, -8.00), "half": (3.00, 2.00, 0.25),
             "color": (0.88, 0.88, 0.84), "phase": 0.5},
            {"kind": "sphere", "center": (-0.50, 0.80, -2.80), "radius": 0.70,
             "color": (0.12, 0.14, 0.20), "phase": 2.8},
            {"kind": "box", "center": (0.60, 1.00, -4.50), "half": (0.80, 1.00, 0.30),
             "color": (0.58, 0.50, 0.42), "phase": 1.1},
            {"kind": "box", "center": (1.00, 1.00, -2.20), "half": (0.05, 1.00, 0.05),
             "color": (0.30, 0.30, 0.32), "phase": 3.4},
            {"kind": "box", "center": (-1.60, 0.25, -6.50), "half": (0.40, 0.25, 0.30),
             "color": (0.85, 0.62, 0.40), "phase": 4.2},
        ],
        "scene_prior_trap": [
            floor,
            {"kind": "box", "center": (-0.75, 1.15, -1.80), "half": (0.50, 1.15, 0.05),
             "color": (0.08, 0.09, 0.11), "phase": 5.1},
            {"kind": "box", "center": (0.90, 0.70, -3.00), "half": (0.50, 0.70, 0.40),
             "color": (0.40, 0.40, 0.42), "phase": 2.2},
            {"kind": "box", "center": (0.0, 3.00, -9.00), "half": (4.00, 3.00, 0.20),
             "color": (0.98, 0.96, 0.90), "phase": 0.1},
        ],
    }


def build_fixtures() -> dict[str, dict]:
    fixtures = {}
    for name, objects in synthetic_scenes().items():
        rendered = render_scene(objects)
        rendered["source"] = "synthetic"
        rendered["objects"] = objects
        fixtures[name] = rendered
    return fixtures


# ---------------------------------------------------------------------------
# Classical baselines


PERCENTILE_MODES = {"p1_99": (1.0, 99.0), "p2_98": (2.0, 98.0), "p5_95": (5.0, 95.0)}
NORMALIZATIONS = ["p2_98", "p1_99", "p5_95", "median_iqr"]

REFINEMENT_PARAMETERS = {
    "none": {},
    "guided": {"radius": 4, "epsilon": 1e-4},
    "joint_bilateral": {"radius": 3, "sigma_space": 2.5, "sigma_color": 0.08},
}
REFINEMENTS = list(REFINEMENT_PARAMETERS)


def fit_normalization(values: np.ndarray, mode: str = "p2_98"):
    """Return (transform, parameters) so a sequence can share one scale."""
    if mode == "median_iqr":
        median = float(np.median(values))
        low, high = np.percentile(values, (25.0, 75.0))
        spread = float(high - low)
        if not (math.isfinite(median) and math.isfinite(spread)) or spread <= 1e-8:
            return (lambda array: np.zeros_like(array, dtype=np.float32)), {
                "mode": mode, "degenerate": True}
        parameters = {"mode": mode, "median": median, "iqr": spread}
        return (lambda array: np.clip((array - median) / (2.0 * spread) + 0.5,
                                     0.0, 1.0).astype(np.float32)), parameters
    low_percentile, high_percentile = PERCENTILE_MODES[mode]
    low, high = np.percentile(values, (low_percentile, high_percentile))
    if not (math.isfinite(low) and math.isfinite(high)) or high - low <= 1e-8:
        return (lambda array: np.zeros_like(array, dtype=np.float32)), {
            "mode": mode, "degenerate": True}
    parameters = {"mode": mode, "low_percentile": low_percentile,
                  "high_percentile": high_percentile,
                  "low_value": float(low), "high_value": float(high)}
    return (lambda array: np.clip((array - low) / (high - low), 0.0, 1.0).astype(np.float32)), parameters


def apply_normalization(values: np.ndarray, mode: str = "p2_98") -> np.ndarray:
    return fit_normalization(values, mode)[0](values)


def normalize_near(values: np.ndarray, low: float = 2.0, high: float = 98.0) -> np.ndarray:
    low_value, high_value = np.percentile(values, (low, high))
    if not np.isfinite(low_value) or not np.isfinite(high_value) or high_value - low_value <= 1e-8:
        return np.zeros_like(values, dtype=np.float32)
    return np.clip((values - low_value) / (high_value - low_value), 0.0, 1.0).astype(np.float32)


def guided_filter(near: np.ndarray, image: np.ndarray, radius: int = 4,
                  epsilon: float = 1e-4) -> np.ndarray:
    """Colour-guided filter (He et al.) applied after normalization."""
    guide = cv2.cvtColor(image, cv2.COLOR_BGR2RGB).astype(np.float32) / 255.0
    window = (radius * 2 + 1, radius * 2 + 1)
    mean_guide = cv2.blur(guide, window)
    mean_depth = cv2.blur(near, window)
    covariance_depth = cv2.blur(guide * near[..., None], window) - mean_guide * mean_depth[..., None]
    variance_guide = cv2.blur(guide * guide, window) - mean_guide * mean_guide
    slope = covariance_depth / (variance_guide + epsilon)
    intercept = mean_depth[..., None] - slope * mean_guide
    filtered = np.mean(cv2.blur(slope, window) * guide + cv2.blur(intercept, window), axis=-1)
    return np.clip(filtered, 0.0, 1.0).astype(np.float32)


def joint_bilateral(near: np.ndarray, image: np.ndarray, radius: int = 3,
                    sigma_space: float = 2.5, sigma_color: float = 0.08) -> np.ndarray:
    guide = cv2.cvtColor(image, cv2.COLOR_BGR2RGB).astype(np.float32) / 255.0
    height, width = near.shape
    padded_depth = cv2.copyMakeBorder(near, radius, radius, radius, radius, cv2.BORDER_REPLICATE)
    padded_guide = cv2.copyMakeBorder(guide, radius, radius, radius, radius, cv2.BORDER_REPLICATE)
    accumulated = np.zeros_like(near)
    weights_sum = np.zeros_like(near)
    for offset_y in range(-radius, radius + 1):
        for offset_x in range(-radius, radius + 1):
            shifted_depth = padded_depth[radius + offset_y:radius + offset_y + height,
                                         radius + offset_x:radius + offset_x + width]
            shifted_guide = padded_guide[radius + offset_y:radius + offset_y + height,
                                         radius + offset_x:radius + offset_x + width]
            spatial = math.exp(-(offset_x * offset_x + offset_y * offset_y) /
                               (2.0 * sigma_space * sigma_space))
            color = np.exp(-np.sum((shifted_guide - guide) ** 2, axis=-1) /
                           (2.0 * sigma_color * sigma_color))
            weight = spatial * color
            accumulated += weight * shifted_depth
            weights_sum += weight
    return (accumulated / np.maximum(weights_sum, 1e-8)).astype(np.float32)


def refine_depth(near: np.ndarray, image: np.ndarray, mode: str = "none") -> np.ndarray:
    if mode == "none":
        return near
    if mode == "guided":
        return guided_filter(near, image, **REFINEMENT_PARAMETERS[mode])
    if mode == "joint_bilateral":
        return joint_bilateral(near, image, **REFINEMENT_PARAMETERS[mode])
    raise ValueError(f"unknown refinement mode: {mode}")


def baseline_luma(image: np.ndarray) -> np.ndarray:
    luma = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY).astype(np.float32) / 255.0
    return normalize_near(cv2.GaussianBlur(luma, (0, 0), 2.0))


def baseline_vertical(image: np.ndarray) -> np.ndarray:
    height, width = image.shape[:2]
    rows = np.linspace(0.0, 1.0, height, dtype=np.float32)[:, None]
    return np.repeat(rows, width, axis=1)


# ---------------------------------------------------------------------------
# ONNX model wrapper


class OnnxDepthModel:
    def __init__(self, name: str, path: Path, preset_name: str):
        if preset_name not in PRESETS:
            raise ValueError(f"unknown preset {preset_name}; choose from {sorted(PRESETS)}")
        preset = PRESETS[preset_name]
        try:
            import onnxruntime as ort
        except ImportError as error:
            raise RuntimeError(
                "Depth inference requires onnxruntime; install experiments/DepthMapLab/requirements.txt"
            ) from error
        options = ort.SessionOptions()
        options.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_ALL
        self.session = ort.InferenceSession(str(path), sess_options=options,
                                            providers=["CPUExecutionProvider"])
        self.ort_version = ort.__version__
        self.input_name = self.session.get_inputs()[0].name
        self.output_name = self.session.get_outputs()[0].name
        self.name = name
        self.path = path
        self.preset_name = preset_name
        self.size = int(preset["size"])
        self.multiple = int(preset["multiple"])
        self.mean = np.asarray(preset["mean"], dtype=np.float32)
        self.std = np.asarray(preset["std"], dtype=np.float32)
        self.sha256 = sha256_file(path)
        self.invert = False

    def describe(self) -> dict:
        return {
            "name": self.name,
            "path": str(self.path),
            "sha256": self.sha256,
            "size_bytes": self.path.stat().st_size,
            "preset": self.preset_name,
            "model_input_short_side": self.size,
            "patch_multiple": self.multiple,
            "mean": self.mean.tolist(),
            "std": self.std.tolist(),
            "execution_provider": "CPUExecutionProvider",
            "onnxruntime": self.ort_version,
            "depth_orientation": "higher=near",
            "inverted_after_calibration": self.invert,
        }

    def _preprocess(self, rgb: np.ndarray) -> tuple[np.ndarray, tuple[int, int]]:
        height, width = rgb.shape[:2]
        scale = self.size / float(min(height, width))
        resized_width = max(self.size, int(round(width * scale / self.multiple)) * self.multiple)
        resized_height = max(self.size, int(round(height * scale / self.multiple)) * self.multiple)
        resized = cv2.resize(rgb, (resized_width, resized_height),
                             interpolation=cv2.INTER_CUBIC).astype(np.float32) / 255.0
        normalized = (resized - self.mean) / self.std
        tensor = np.ascontiguousarray(normalized.transpose(2, 0, 1)[None, ...], dtype=np.float32)
        return tensor, (resized_width, resized_height)

    def raw(self, image: np.ndarray) -> tuple[np.ndarray, float, tuple[int, int]]:
        rgb = cv2.cvtColor(image, cv2.COLOR_BGR2RGB)
        tensor, model_size = self._preprocess(rgb)
        started = time.perf_counter()
        output = self.session.run([self.output_name], {self.input_name: tensor})[0]
        elapsed = time.perf_counter() - started
        depth = np.asarray(output, dtype=np.float32).squeeze()
        if depth.ndim != 2 or not np.isfinite(depth).all():
            raise ValueError(f"{self.name}: unexpected/non-finite output shape {depth.shape}")
        height, width = image.shape[:2]
        depth = cv2.resize(depth, (width, height), interpolation=cv2.INTER_CUBIC)
        return depth, elapsed, model_size

    def near_map(self, image: np.ndarray, normalization: str = "p2_98") -> tuple[np.ndarray, float]:
        raw, elapsed, _ = self.raw(image)
        return apply_normalization(-raw if self.invert else raw, normalization), elapsed


# ---------------------------------------------------------------------------
# Metrics


def _rank(values: np.ndarray) -> np.ndarray:
    order = np.argsort(values, kind="stable")
    ranks = np.empty(values.size, dtype=np.float64)
    ranks[order] = np.arange(values.size, dtype=np.float64)
    return ranks


def spearman(left: np.ndarray, right: np.ndarray) -> float:
    a = _rank(left)
    b = _rank(right)
    a -= a.mean()
    b -= b.mean()
    denominator = math.sqrt(float((a * a).sum()) * float((b * b).sum()))
    return float((a * b).sum() / denominator) if denominator > 0.0 else 0.0


def _edge_mask(near: np.ndarray) -> np.ndarray:
    gx = cv2.Sobel(near, cv2.CV_32F, 1, 0, ksize=3)
    gy = cv2.Sobel(near, cv2.CV_32F, 0, 1, ksize=3)
    magnitude = np.hypot(gx, gy)
    return magnitude > float(np.percentile(magnitude, 96.0))


def _dilate(mask: np.ndarray, radius: int = 2) -> np.ndarray:
    kernel = np.ones((radius * 2 + 1, radius * 2 + 1), dtype=np.uint8)
    return cv2.dilate(mask.astype(np.uint8), kernel).astype(bool)


def score_depth(predicted: np.ndarray, truth: np.ndarray,
                fixture_name: str) -> dict:
    step = 4
    pred = predicted[::step, ::step].astype(np.float64).ravel()
    gt = truth[::step, ::step].astype(np.float64).ravel()
    rng = np.random.default_rng(SEED + zlib.crc32(fixture_name.encode("utf-8")) % 997)

    count = pred.size
    left = rng.integers(0, count, size=40_000)
    right = rng.integers(0, count, size=40_000)
    difference = gt[left] - gt[right]
    valid_pairs = np.abs(difference) > 0.03
    agreement = np.sign(pred[left] - pred[right]) == np.sign(difference)
    ordinal = float(agreement[valid_pairs].mean()) if np.any(valid_pairs) else float("nan")

    design = np.stack((pred, np.ones_like(pred)), axis=1)
    coefficients, *_ = np.linalg.lstsq(design, gt, rcond=None)
    residual = design @ coefficients - gt
    si_rmse = float(np.sqrt((residual ** 2).mean()) / (gt.std() + 1e-8))

    closeness_pred = pred + 0.10
    closeness_gt = gt + 0.10
    scale = float(np.median(closeness_gt / closeness_pred))
    ratio = np.maximum((closeness_gt / closeness_pred) / scale,
                       scale / (closeness_gt / closeness_pred))

    truth_edges = _edge_mask(truth)
    predicted_edges = _edge_mask(predicted)
    precision_denominator = int(predicted_edges.sum())
    recall_denominator = int(truth_edges.sum())
    precision = float((predicted_edges & _dilate(truth_edges)).sum() / precision_denominator) \
        if precision_denominator else 0.0
    recall = float((truth_edges & _dilate(predicted_edges)).sum() / recall_denominator) \
        if recall_denominator else 0.0
    edge_f1 = 2.0 * precision * recall / (precision + recall) if precision + recall > 0.0 else 0.0

    band = _dilate(truth_edges)
    interior = ~_dilate(truth_edges, radius=4)
    error = predicted - truth
    band_mae = float(np.abs(error[band]).mean()) if np.any(band) else float("nan")
    band_bias = float(error[band].mean()) if np.any(band) else float("nan")
    interior_mae = float(np.abs(error[interior]).mean()) if np.any(interior) else float("nan")

    near_truth = truth > 0.75
    far_truth = truth < 0.25
    foreground_leak = float(((predicted < 0.25) & near_truth).sum() / max(int(near_truth.sum()), 1))
    background_intrusion = float(((predicted > 0.75) & far_truth).sum() / max(int(far_truth.sum()), 1))

    regions = {}
    for label, mask in (("near", near_truth), ("mid", ~near_truth & ~far_truth), ("far", far_truth)):
        regions[label] = {
            "pixels": int(mask.sum()),
            "mae": float(np.abs(error[mask]).mean()) if np.any(mask) else None,
            "bias": float(error[mask].mean()) if np.any(mask) else None,
        }

    return {
        "ordinal_accuracy": ordinal,
        "spearman": spearman(pred, gt),
        "si_rmse": si_rmse,
        "delta1": float((ratio < 1.25).mean()),
        "delta2": float((ratio < 1.25 ** 2).mean()),
        "delta3": float((ratio < 1.25 ** 3).mean()),
        "median_abs_rel": float(np.median(np.abs(pred - gt) / (gt + 0.1))),
        "edge_precision": precision,
        "edge_recall": recall,
        "edge_f1": edge_f1,
        "boundary_band_mae": band_mae,
        "boundary_band_bias": band_bias,
        "interior_mae": interior_mae,
        "foreground_leak": foreground_leak,
        "background_intrusion": background_intrusion,
        "regions": regions,
        "distribution": {
            "predicted": {str(p): float(np.percentile(predicted, p)) for p in (0, 1, 5, 50, 95, 99, 100)},
            "truth": {str(p): float(np.percentile(truth, p)) for p in (0, 1, 5, 50, 95, 99, 100)},
            "predicted_clipped_low": float((predicted <= 0.0).mean()),
            "predicted_clipped_high": float((predicted >= 1.0).mean()),
        },
    }


# ---------------------------------------------------------------------------
# Output helpers


def save_json(path: Path, payload: dict) -> None:
    with path.open("w", encoding="utf-8", newline="\n") as stream:
        json.dump(payload, stream, indent=2, ensure_ascii=False, allow_nan=False)
        stream.write("\n")


def save_depth_png(path: Path, near: np.ndarray) -> None:
    encoded = np.round(np.clip(near, 0.0, 1.0) * 65535.0).astype(np.uint16)
    if not cv2.imwrite(str(path), encoded):
        raise OSError(f"Failed to write {path}")


def save_u8_png(path: Path, near: np.ndarray) -> None:
    encoded = np.round(np.clip(near, 0.0, 1.0) * 255.0).astype(np.uint8)
    if not cv2.imwrite(str(path), encoded):
        raise OSError(f"Failed to write {path}")


def turbo_preview(near: np.ndarray) -> np.ndarray:
    encoded = np.round(np.clip(near, 0.0, 1.0) * 255.0).astype(np.uint8)
    return cv2.applyColorMap(encoded, cv2.COLORMAP_TURBO)


def amplified_difference(predicted: np.ndarray, truth: np.ndarray, gain: float = 6.0) -> np.ndarray:
    diff = np.clip(np.abs(predicted - truth) * gain, 0.0, 1.0)
    return np.round(diff * 255.0).astype(np.uint8)


def build_contact_sheet(rows: list[tuple[str, list[tuple[str, np.ndarray]]]],
                        path: Path, tile=(320, 180), label_height=18) -> None:
    if not rows:
        return
    columns = max(len(tiles) for _, tiles in rows)
    sheet = np.full((len(rows) * (tile[1] + label_height), columns * tile[0], 3), 24, np.uint8)
    for row_index, (row_label, tiles) in enumerate(rows):
        y = row_index * (tile[1] + label_height)
        for column, (label, image) in enumerate(tiles):
            x = column * tile[0]
            if image.ndim == 2:
                image = cv2.cvtColor(image, cv2.COLOR_GRAY2BGR)
            sheet[y:y + tile[1], x:x + tile[0]] = cv2.resize(image, tile, interpolation=cv2.INTER_AREA)
            cv2.rectangle(sheet, (x, y + tile[1]), (x + tile[0], y + tile[1] + label_height),
                          (32, 32, 32), -1)
            cv2.putText(sheet, f"{row_label} | {label}", (x + 4, y + tile[1] + 13),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.36, (230, 230, 230), 1, cv2.LINE_AA)
    if not cv2.imwrite(str(path), sheet):
        raise OSError(f"Failed to write {path}")


# ---------------------------------------------------------------------------
# Gates


def aggregate(metrics: dict[str, dict], key: str) -> float | None:
    values = [fixture[key] for fixture in metrics.values()
              if isinstance(fixture.get(key), (int, float)) and math.isfinite(fixture[key])]
    return float(np.mean(values)) if values else None


def evaluate_gates(results: dict[str, dict]) -> dict:
    spec = {name: {"op": op, "value": value} for name, (op, value) in GATES.items()}
    candidates = {}
    for name, fixtures in results.items():
        aggregate_values = {key: aggregate(fixtures, key) for key in GATES}
        failed = []
        for key, (op, threshold) in GATES.items():
            value = aggregate_values[key]
            if value is None:
                failed.append(f"{key}:unmeasured")
                continue
            passed = value >= threshold if op == ">=" else value <= threshold
            if not passed:
                failed.append(f"{key}:{value:.4f}{op}{threshold}")
        candidates[name] = {
            "aggregate": aggregate_values,
            "failed_gates": failed,
            "passed": not failed,
        }
    return {"spec": spec, "calibrated_from": "classical baselines in the same run",
            "candidates": candidates}


# ---------------------------------------------------------------------------
# Video temporal study


def flow_fields(frames: list[np.ndarray]) -> list[np.ndarray]:
    flows = []
    for index in range(1, len(frames)):
        previous_gray = cv2.cvtColor(frames[index - 1], cv2.COLOR_BGR2GRAY)
        current_gray = cv2.cvtColor(frames[index], cv2.COLOR_BGR2GRAY)
        flows.append(cv2.calcOpticalFlowFarneback(previous_gray, current_gray, None,
                                                  0.5, 3, 15, 3, 5, 1.2, 0))
    return flows


def _temporal_metrics(maps: list[np.ndarray], frames: list[np.ndarray],
                      flows: list[np.ndarray]) -> dict:
    frame_delta, warped_delta, static_delta = [], [], []
    for index in range(1, len(maps)):
        previous = maps[index - 1]
        current = maps[index]
        frame_delta.append(float(np.abs(current - previous).mean()))
        flow = flows[index - 1]
        height, width = previous.shape
        grid_x, grid_y = np.meshgrid(np.arange(width, dtype=np.float32),
                                     np.arange(height, dtype=np.float32))
        warped = cv2.remap(previous, grid_x + flow[..., 0], grid_y + flow[..., 1],
                           cv2.INTER_LINEAR, borderMode=cv2.BORDER_REPLICATE)
        delta = np.abs(current - warped)
        magnitude = np.hypot(flow[..., 0], flow[..., 1])
        warped_delta.append(float(delta.mean()))
        static_mask = magnitude < 0.5
        if np.any(static_mask):
            static_delta.append(float(delta[static_mask].mean()))
    means = [float(method.mean()) for method in maps]
    return {
        "frames": len(maps),
        "temporal_mean_abs_delta": float(np.mean(frame_delta)) if frame_delta else None,
        "temporal_flow_warped_mean_abs_delta": float(np.mean(warped_delta)) if warped_delta else None,
        "temporal_static_region_mean_abs_delta": float(np.mean(static_delta)) if static_delta else None,
        "frame_mean_depth_std": float(np.std(means)),
        "frame_mean_depth_range": float(np.ptp(means)),
    }


def run_video(path: Path, models: list[OnnxDepthModel], output: Path,
              max_frames: int, normalizations: list[str],
              refinements: list[str]) -> dict:
    capture = cv2.VideoCapture(str(path))
    if not capture.isOpened():
        raise OSError(f"could not open video: {path}")
    frame_count = int(capture.get(cv2.CAP_PROP_FRAME_COUNT))
    stride = max(1, frame_count // max_frames) if frame_count > max_frames else 1
    frames: list[np.ndarray] = []
    index = 0
    while len(frames) < max_frames:
        ok, frame = capture.read()
        if not ok:
            break
        if index % stride == 0:
            scale = min(1.0, 640.0 / max(frame.shape[:2]))
            if scale < 1.0:
                frame = cv2.resize(frame, (round(frame.shape[1] * scale), round(frame.shape[0] * scale)),
                                   interpolation=cv2.INTER_AREA)
            frames.append(frame)
        index += 1
    capture.release()
    if len(frames) < 2:
        raise ValueError("video study needs at least two decoded frames")

    output.mkdir(parents=True, exist_ok=True)
    flows = flow_fields(frames)
    report = {"input": str(path.resolve()), "frames": len(frames), "models": {}}
    for model in models:
        raws: list[np.ndarray] = []
        seconds: list[float] = []
        for frame in frames:
            raw, elapsed, _ = model.raw(frame)
            raws.append(-raw if model.invert else raw)
            seconds.append(elapsed)
        pooled = np.concatenate([raw[::8, ::8].ravel() for raw in raws])
        variants = {}
        for normalization in normalizations:
            clip_transform, clip_parameters = fit_normalization(pooled, normalization)
            for refinement in refinements:
                per_frame = [refine_depth(apply_normalization(raw, normalization),
                                          frames[index], refinement)
                             for index, raw in enumerate(raws)]
                clip_wide = [refine_depth(clip_transform(raw), frames[index], refinement)
                             for index, raw in enumerate(raws)]
                variants[f"{normalization}|{refinement}"] = {
                    "per_frame_normalization": _temporal_metrics(per_frame, frames, flows),
                    "clip_wide_normalization": _temporal_metrics(clip_wide, frames, flows),
                    "clip_parameters": clip_parameters,
                }

        entry = {
            "inference_seconds_median": float(np.median(seconds)),
            "variants": variants,
        }
        report["models"][model.name] = entry
        save_json(output / f"temporal-{model.name}.json", entry)

        tile_per_frame = [apply_normalization(raw, normalizations[0]) for raw in raws]
        tile_clip = [fit_normalization(pooled, normalizations[0])[0](raw) for raw in raws]
        height, width = frames[0].shape[:2]
        writer = cv2.VideoWriter(str(output / f"depth-{model.name}.avi"),
                                 cv2.VideoWriter_fourcc(*"MJPG"), 12.0, (width * 3, height))
        for frame, frame_map, clip_map in zip(frames, tile_per_frame, tile_clip):
            tiles = np.hstack((frame, turbo_preview(frame_map), turbo_preview(clip_map)))
            writer.write(tiles)
        writer.release()

        picks = np.linspace(0, len(frames) - 1, 6).round().astype(int)
        rows = [(f"frame {p}", [("source", frames[p]),
                                ("per-frame", turbo_preview(tile_per_frame[p])),
                                ("clip-wide", turbo_preview(tile_clip[p]))]) for p in picks]
        build_contact_sheet(rows, output / f"temporal-contact-{model.name}.png")
    save_json(output / "temporal-report.json", report)
    return report


# ---------------------------------------------------------------------------
# Main


def parse_model_arguments(args: argparse.Namespace) -> list[OnnxDepthModel]:
    presets: dict[str, str] = {}
    for entry in args.onnx_preset or []:
        if "=" not in entry:
            raise ValueError("--onnx-preset must be NAME=PRESET")
        name, preset = entry.split("=", 1)
        if preset not in PRESETS:
            raise ValueError(f"unknown preset {preset}; choose from {sorted(PRESETS)}")
        presets[name] = preset
    models = []
    for entry in args.onnx or []:
        if "=" not in entry:
            raise ValueError("--onnx must be NAME=PATH")
        name, path_text = entry.split("=", 1)
        path = Path(path_text)
        if not path.is_absolute() and not path.is_file() and (LAB_DIR / path).is_file():
            path = LAB_DIR / path
        if not path.is_file():
            raise ValueError(f"model file does not exist: {path}")
        preset = presets.get(name)
        if preset is None:
            preset = "da2_small" if "da2" in name.lower() else "generic_512"
            print(f"note: {name} has no --onnx-preset; using {preset}")
        models.append(OnnxDepthModel(name, path, preset))
    return models


def calibrate_orientation(model: OnnxDepthModel, fixture: dict) -> dict:
    raw, _, _ = model.raw(fixture["image"])
    truth = fixture["disparity"][::4, ::4].astype(np.float64).ravel()
    predicted = raw[::4, ::4].astype(np.float64).ravel()
    rho = spearman(predicted, truth)
    model.invert = rho < 0.0
    return {
        "fixture": "scene_boxes",
        "spearman_raw_vs_gt_disparity": rho,
        "inverted": model.invert,
        "result": "higher=near" if not model.invert else "raw was higher=far",
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rd", default="rd0", help="run directory under captures/")
    parser.add_argument("--onnx", action="append", metavar="NAME=PATH",
                        help="add an ONNX depth model candidate")
    parser.add_argument("--onnx-preset", action="append", metavar="NAME=PRESET",
                        help=f"preprocessing preset per model ({'/'.join(sorted(PRESETS))})")
    parser.add_argument("--download-model", action="store_true",
                        help="download and verify the pinned Depth Anything V2 Small int8 model")
    parser.add_argument("--photo", action="append", type=Path,
                        help="extra still image, preview only (no ground truth)")
    parser.add_argument("--video", action="append", type=Path,
                        help="video for the temporal study (requires --onnx)")
    parser.add_argument("--max-frames", type=int, default=24)
    parser.add_argument("--save-raw", action="store_true", help="also save float32 .npy depth maps")
    parser.add_argument("--normalization", action="append", choices=NORMALIZATIONS,
                        help="normalization mode per variant (repeatable)")
    parser.add_argument("--refine", action="append", choices=REFINEMENTS,
                        help="edge-aware refinement per variant (repeatable)")
    args = parser.parse_args()
    args.normalization = args.normalization or ["p2_98"]
    args.refine = args.refine or ["none"]
    if "/" in args.rd or "\\" in args.rd or not args.rd:
        parser.error("--rd must be a plain directory name such as rd0")

    if args.download_model:
        download_model(DEFAULT_MODEL_PATH)
    try:
        models = parse_model_arguments(args)
    except ValueError as error:
        parser.error(str(error))

    fixtures = build_fixtures()
    output = LAB_DIR / "captures" / args.rd
    fixture_dir = output / "fixtures"
    truth_dir = output / "gt"
    maps_dir = output / "maps"
    review_dir = output / "review"
    for directory in (fixture_dir, truth_dir, maps_dir, review_dir):
        directory.mkdir(parents=True, exist_ok=True)

    calibration = {}
    for model in models:
        calibration[model.name] = calibrate_orientation(model, fixtures["scene_boxes"])
        print(f"{model.name}: orientation {calibration[model.name]['result']} "
              f"(spearman {calibration[model.name]['spearman_raw_vs_gt_disparity']:.3f})")

    fixture_records = {}
    for name, fixture in fixtures.items():
        image_path = fixture_dir / f"{name}.png"
        if not cv2.imwrite(str(image_path), fixture["image"]):
            raise OSError(f"Failed to write {image_path}")
        save_depth_png(truth_dir / f"{name}-gt-near.png", fixture["near"])
        if args.save_raw:
            np.save(truth_dir / f"{name}-depth-z.npy", fixture["depth_z"])
        fixture_records[name] = {
            "image_sha256": hashlib.sha256(image_path.read_bytes()).hexdigest(),
            "width": FIXTURE_WIDTH,
            "height": FIXTURE_HEIGHT,
            "source": "synthetic analytic renderer",
            "objects": fixture["objects"],
            "gt_depth_units": "camera z-depth",
        }

    single_default = (len(args.normalization) == 1 and args.normalization[0] == "p2_98"
                      and len(args.refine) == 1 and args.refine[0] == "none")
    variants = []
    for model in models:
        for normalization in args.normalization:
            for refinement in args.refine:
                variant = (model.name if single_default
                           else f"{model.name}__{normalization}__{refinement}")
                variants.append((variant, model, normalization, refinement))

    results: dict[str, dict] = {}
    timing: dict[str, list[float]] = {model.name: [] for model in models}
    raw_cache: dict[tuple[str, str], np.ndarray] = {}
    sheet_rows = []
    for name, fixture in fixtures.items():
        truth = fixture["near"]
        row_tiles = [("input", fixture["image"]),
                     ("gt near", np.round(truth * 255).astype(np.uint8))]
        candidates: dict[str, np.ndarray] = {
            "baseline_luma": baseline_luma(fixture["image"]),
            "baseline_vertical": baseline_vertical(fixture["image"]),
        }
        for variant, model, normalization, refinement in variants:
            key = (model.name, name)
            raw = raw_cache.get(key)
            if raw is None:
                raw, elapsed, _ = model.raw(fixture["image"])
                raw_cache[key] = raw
                timing[model.name].append(elapsed)
            near = apply_normalization(-raw if model.invert else raw, normalization)
            candidates[variant] = refine_depth(near, fixture["image"], refinement)
        for candidate_name, predicted in candidates.items():
            results.setdefault(candidate_name, {})[name] = score_depth(predicted, truth, name)
            candidate_dir = maps_dir / candidate_name
            candidate_dir.mkdir(parents=True, exist_ok=True)
            save_u8_png(candidate_dir / f"{name}-near.png", predicted)
            cv2.imwrite(str(candidate_dir / f"{name}-turbo.jpg"), turbo_preview(predicted),
                        [cv2.IMWRITE_JPEG_QUALITY, 92])
            cv2.imwrite(str(candidate_dir / f"{name}-diff-x6.png"),
                        amplified_difference(predicted, truth))
            if args.save_raw:
                np.save(candidate_dir / f"{name}-near.npy", predicted.astype(np.float32))
            if candidate_name.startswith("baseline"):
                row_tiles.append((candidate_name.replace("baseline_", ""),
                                  turbo_preview(predicted)))
                continue
            variant_entry = next((entry for entry in variants if entry[0] == candidate_name), None)
            if variant_entry is None:
                continue
            _, _, normalization, refinement = variant_entry
            if refinement == "none" or normalization == "p2_98":
                row_tiles.append((f"{normalization}/{refinement}", turbo_preview(predicted)))
            if normalization == "p2_98" and refinement == "none":
                row_tiles.append(("diff x6", amplified_difference(predicted, truth)))
        sheet_rows.append((name, row_tiles))

    photos = {}
    for photo in args.photo or []:
        image = cv2.imread(str(photo), cv2.IMREAD_COLOR)
        if image is None:
            print(f"warning: could not decode photo {photo}")
            continue
        photo_dir = maps_dir / "photos"
        photo_dir.mkdir(parents=True, exist_ok=True)
        save_u8_png(photo_dir / f"{photo.stem}-input.png", cv2.cvtColor(image, cv2.COLOR_BGR2GRAY))
        photos[photo.name] = {"path": str(photo.resolve()), "models": {}}
        for model in models:
            predicted, elapsed = model.near_map(image)
            save_u8_png(photo_dir / f"{photo.stem}-{model.name}-near.png", predicted)
            photos[photo.name]["models"][model.name] = {
                "inference_seconds": elapsed,
                "predicted_percentiles": {str(p): float(np.percentile(predicted, p))
                                          for p in (0, 1, 5, 50, 95, 99, 100)},
                "no_ground_truth": True,
            }

    for name, samples in timing.items():
        if samples:
            print(f"{name}: median inference {np.median(samples):.3f}s over {len(samples)} fixtures")

    gates = evaluate_gates(results)
    for name, entry in gates["candidates"].items():
        status = "PASS" if entry["passed"] else "FAIL"
        print(f"{status} {name}: " + (", ".join(entry["failed_gates"]) or "all gates met"))

    build_contact_sheet(sheet_rows, review_dir / "contact-sheet.png")
    save_json(output / "metrics.json", {"fixtures": fixture_records, "candidates": results})
    save_json(output / "gates.json", gates)
    save_json(output / "manifest.json", {
        "run": args.rd,
        "seed": SEED,
        "environment": {
            "python": ".".join(str(part) for part in __import__("sys").version_info[:3]),
            "numpy": np.__version__,
            "opencv": cv2.__version__,
            "platform": __import__("platform").platform(),
        },
        "fixtures": fixture_records,
        "models": [model.describe() for model in models],
        "orientation_calibration": calibration,
        "photos": photos,
        "gates_source": "pre-registered in EVALUATION_PLAN.md",
        "normalizations": args.normalization,
        "refinements": {mode: REFINEMENT_PARAMETERS[mode] for mode in args.refine},
        "normalization_contract": "disparity, near=1, refinement applied after normalization",
        "limitation": "Synthetic scenes have analytic GT but no lens, defocus, noise or haze; "
                      "monocular depth is relative, not metric.",
    })

    if args.video:
        if not models:
            parser.error("--video requires at least one --onnx model")
        for video in args.video:
            report = run_video(video, models, output / "video" / video.stem, args.max_frames,
                               args.normalization, args.refine)
            for model_name, entry in report["models"].items():
                for key, metrics in entry["variants"].items():
                    value = metrics["clip_wide_normalization"]["temporal_flow_warped_mean_abs_delta"]
                    status = "PASS" if value is not None and value <= TEMPORAL_GATE[2] else "FAIL"
                    print(f"{status} temporal {model_name} {key}: flow-warped delta "
                          f"{value:.4f} ({TEMPORAL_GATE[2]} gate)")
    print(f"Wrote {output}")


if __name__ == "__main__":
    main()
