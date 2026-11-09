"""Deterministic image-space R&D for a light flash that sweeps across a frame."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import time

import cv2
import numpy as np

from glow_lab import add_label, linear_to_srgb, make_fixture, srgb_to_linear, to_u8


DEFAULT_SETTINGS: dict[str, object] = {
    "time": 0.5,
    "duration_seconds": 1.0,
    "fps": 24.0,
    "strength": 5.0,
    "softness": 0.19,
    "direction_degrees": 168.0,
    "mode": "directional",
    "source_position": [0.08, 0.5],
    "radial_falloff": 3.0,
    "attack_duration": 0.78,
    "peak_time": 0.78,
    "decay_rate": 18.0,
    "scatter_strength": 1.4,
    "scatter_front_coupling": 0.0,
    "veil_gain": 0.025,
    "highlight_threshold": 0.28,
    "highlight_range": 0.52,
    "global_fill": 0.06,
    "front_duration": 0.72,
    "temperature": [1.0, 0.94, 0.82],
    "frame_count": 25,
}
SCATTER_SWEEP_SETTINGS = {
    "time": 0.12,
    "exposure_strength": 2.0,
    "global_fill": 0.0,
    "gains": [0.0, 2.0, 8.0, 20.0],
}


def load_settings(path: Path | None) -> dict[str, object]:
    settings = DEFAULT_SETTINGS.copy()
    if path is not None:
        with path.open("r", encoding="utf-8") as stream:
            preset = json.load(stream)
        if "settings" in preset:
            preset = preset["settings"]
        if not isinstance(preset, dict):
            raise ValueError("Preset must contain a settings object")
        settings.update(preset)
    settings["mode"] = str(settings["mode"])
    if settings["mode"] not in ("directional", "radial"):
        raise ValueError("Preset mode must be directional or radial")
    return settings


def write_json(path: Path, data: dict[str, object]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="\n") as stream:
        json.dump(data, stream, indent=2, ensure_ascii=False)
        stream.write("\n")


def timing_in_seconds(settings: dict[str, object]) -> dict[str, float]:
    duration = max(1.0e-6, float(settings["duration_seconds"]))
    return {
        "duration_seconds": duration,
        "front_duration_seconds": duration * float(settings["front_duration"]),
        "attack_duration_seconds": duration * float(settings["attack_duration"]),
        "peak_time_seconds": duration * float(settings["peak_time"]),
        "decay_rate_per_second": float(settings["decay_rate"]) / duration,
        "nominal_fps": float(settings["fps"]),
    }


def load_image(path: Path) -> np.ndarray:
    """Load an 8-bit RGB/RGBA photo as float sRGB while retaining its alpha."""
    image = cv2.imread(str(path), cv2.IMREAD_UNCHANGED)
    if image is None:
        raise ValueError(f"Could not read image: {path}")
    if image.dtype != np.uint8:
        raise ValueError("R&D input currently supports 8-bit PNG, JPEG, and BMP images")
    if image.ndim == 2:
        rgb = cv2.cvtColor(image, cv2.COLOR_GRAY2RGB)
        alpha = np.full(image.shape, 255, dtype=np.uint8)
    elif image.shape[2] == 3:
        rgb = cv2.cvtColor(image, cv2.COLOR_BGR2RGB)
        alpha = np.full(image.shape[:2], 255, dtype=np.uint8)
    elif image.shape[2] == 4:
        rgb = cv2.cvtColor(image[..., :3], cv2.COLOR_BGR2RGB)
        alpha = image[..., 3]
    else:
        raise ValueError(f"Unsupported image channel layout: {image.shape}")
    return np.dstack((rgb.astype(np.float32) / 255.0, alpha.astype(np.float32) / 255.0))


def save_image(path: Path, rgba: np.ndarray) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    bgra = cv2.cvtColor(to_u8(rgba), cv2.COLOR_RGBA2BGRA)
    if not cv2.imwrite(str(path), bgra):
        raise OSError(f"Could not write image: {path}")


def load_gsf_fit(path: Path, target_shape: tuple[int, ...]) -> tuple[tuple[float, float], ...]:
    """Fit a compact positive Gaussian mixture to a measured radial CSV tail."""
    with path.open("r", encoding="utf-8") as stream:
        first_line = stream.readline().strip()
    metadata = {key: float(value) for key, value in
                re.findall(r"([a-z_]+)=([0-9.eE+-]+)", first_line)}
    skip_rows = 2 if first_line.startswith("#") else 1
    data = np.loadtxt(path, delimiter=",", skiprows=skip_rows, ndmin=2)
    if data.shape[1] < 2 or data.shape[0] < 8:
        raise ValueError("GSF CSV needs at least 8 radius/profile samples")
    radii = data[:, 0].astype(np.float64)
    profile = np.maximum(data[:, 1].astype(np.float64), 0.0)
    source_height = metadata.get("height", 0.0)
    scale = target_shape[0] / source_height if source_height > 0.0 else 1.0
    radii *= scale
    max_dimension = float(max(target_shape[:2]))
    sigma_pixels = max_dimension * np.asarray((0.003, 0.014, 0.06, 0.18), dtype=np.float64)
    fit_mask = (radii >= max(2.0, max_dimension * 0.003)) & np.isfinite(profile)
    if int(fit_mask.sum()) < 4:
        raise ValueError("GSF profile has too few usable samples outside the direct source core")
    radius_samples = radii[fit_mask]
    measured = profile[fit_mask]
    basis = np.exp(-0.5 * (radius_samples[:, None] / sigma_pixels[None, :]) ** 2)
    weights = np.maximum(np.linalg.lstsq(basis, measured, rcond=None)[0], 0.0)
    if float(weights.sum()) <= 1.0e-10:
        raise ValueError("Could not fit positive scatter weights to the GSF profile")
    return tuple((float(weight), float(sigma))
                 for weight, sigma in zip(weights, sigma_pixels) if weight > 1.0e-6)


def scatter_psf(source_highlights: np.ndarray, width: int, height: int,
                gsf_fit: tuple[tuple[float, float], ...] | None) -> np.ndarray:
    """Spread highlight energy with a measured or broad analytic GSF proxy."""
    if gsf_fit:
        result = np.zeros_like(source_highlights)
        for weight, sigma in gsf_fit:
            result += cv2.GaussianBlur(
                source_highlights, (0, 0), sigmaX=sigma, sigmaY=sigma,
                borderType=cv2.BORDER_REPLICATE) * weight
        return result
    bloom_core = cv2.GaussianBlur(
        source_highlights, (0, 0), sigmaX=max(0.8, width * 0.003),
        sigmaY=max(0.8, height * 0.003), borderType=cv2.BORDER_REPLICATE)
    bloom_near = cv2.GaussianBlur(
        source_highlights, (0, 0), sigmaX=max(1.0, width * 0.014),
        sigmaY=max(1.0, height * 0.014), borderType=cv2.BORDER_REPLICATE)
    bloom_wide = cv2.GaussianBlur(
        source_highlights, (0, 0), sigmaX=max(2.0, width * 0.06),
        sigmaY=max(2.0, height * 0.06), borderType=cv2.BORDER_REPLICATE)
    return bloom_core * 0.48 + bloom_near * 0.34 + bloom_wide * 0.18


def flash_fill(rgba: np.ndarray, progress: float, direction: tuple[float, float] = (-1.0, 0.22),
               softness: float = 0.19, strength: float = 5.0,
               temperature: tuple[float, float, float] = (1.0, 0.94, 0.82),
               attack_duration: float = 0.78,
               peak_time: float = 0.78, decay_rate: float = 18.0,
               mode: str = "directional", source_position: tuple[float, float] = (0.08, 0.5),
               radial_falloff: float = 3.0,
               scatter_strength: float = 1.4,
               highlight_threshold: float = 0.28, highlight_range: float = 0.52,
               gsf_fit: tuple[tuple[float, float], ...] | None = None,
               global_fill: float = 0.06, front_duration: float = 0.72,
               depth_map: np.ndarray | None = None, depth_strength: float = 0.0,
               depth_near_is_bright: bool = True,
               depth_front_strength: float = 0.0,
               highlight_rolloff: float = 0.0,
               scatter_front_coupling: float = 0.0,
               veil_gain: float = 0.025,
               return_components: bool = False) -> np.ndarray | dict[str, np.ndarray]:
    """Sweep an exposure wave across the image, blooming bright regions in linear light."""
    h, w = rgba.shape[:2]
    dx, dy = direction
    length = max(float(np.hypot(dx, dy)), 1.0e-6)
    dx, dy = dx / length, dy / length
    y, x = np.mgrid[0:h, 0:w].astype(np.float32)
    nx = x / max(w - 1, 1) - 0.5
    ny = y / max(h - 1, 1) - 0.5
    time = float(np.clip(progress, 0.0, 1.0))
    spatial_time = min(1.0, time / max(float(front_duration), 1.0e-3))
    if mode == "directional":
        distance = nx * dx + ny * dy
        extent = 0.5 * (abs(dx) + abs(dy))
        front = (spatial_time * 2.0 - 1.0) * extent
    elif mode == "radial":
        sx = float(np.clip(source_position[0], 0.0, 1.0)) * max(w - 1, 1)
        sy = float(np.clip(source_position[1], 0.0, 1.0)) * max(h - 1, 1)
        scale = float(max(w, h))
        distance = np.hypot(x - sx, y - sy) / scale
        extent = float(max(np.hypot(cx - sx, cy - sy)
                           for cx in (0.0, float(w - 1))
                           for cy in (0.0, float(h - 1))) / scale)
        front = spatial_time * extent
    else:
        raise ValueError(f"Unsupported wave mode: {mode}")
    feather = max(float(softness), 1.0e-4)
    depth = None
    if depth_map is not None and (float(depth_strength) > 0.0 or
                                  float(depth_front_strength) > 0.0):
        if depth_map.shape != (h, w):
            raise ValueError(f"Depth map dimensions {depth_map.shape[::-1]} do not match image {(w, h)}")
        depth = np.clip(depth_map.astype(np.float32, copy=False), 0.0, 1.0)
        if depth_near_is_bright:
            depth = 1.0 - depth
    # A coherent, very-low-frequency distortion keeps the travelling boundary
    # from reading as a mathematically perfect wipe while remaining repeatable.
    noise = np.random.default_rng(271828).standard_normal((9, 13), dtype=np.float32)
    noise = cv2.resize(noise, (w, h), interpolation=cv2.INTER_CUBIC)
    noise = cv2.GaussianBlur(noise, (0, 0), sigmaX=max(2.0, w * 0.035),
                             sigmaY=max(2.0, h * 0.035))
    noise /= max(float(noise.std()), 1.0e-5)
    distance = distance + noise * feather * 0.16
    # A logistic front avoids the razor-like wipe produced by a smoothstep mask.
    if depth is not None and float(depth_front_strength) > 0.0 and mode == "radial":
        # Larger normalized depth means farther, so the traveling front reaches
        # farther regions later. Mean-centering preserves baseline timing;
        # positive distance delays those regions until the front travels farther.
        depth_front = depth - float(depth.mean())
        distance = distance + depth_front * float(depth_front_strength)
    spatial_wave = 1.0 / (1.0 + np.exp(np.clip((distance - front) * (8.0 / feather), -40.0, 40.0)))
    # Model a fast flash attack, short peak, and exponential exposure decay.
    peak = float(np.clip(peak_time, 0.0, 1.0))
    attack_span = max(1.0e-3, min(max(float(attack_duration), 1.0e-3), max(peak, 1.0e-3)))
    # Keep exposure rising to the peak; the spatial front already supplies
    # feathering, while a smoothstep here would flatten the temporal shoulder.
    attack = np.clip(time / attack_span, 0.0, 1.0)
    decay = np.exp(-np.maximum(time - peak, 0.0) *
                   max(0.0, decay_rate))
    pulse = attack * decay
    global_fill = float(np.clip(global_fill, 0.0, 1.0))
    wave = pulse * (spatial_wave * (1.0 - global_fill) + global_fill)
    if mode == "radial":
        # Approximate inverse-square illumination in normalized image space.
        # This art-direction control does not reconstruct scene depth.
        sx = float(np.clip(source_position[0], 0.0, 1.0))
        sy = float(np.clip(source_position[1], 0.0, 1.0))
        radius = np.hypot(x / max(w - 1, 1) - sx, y / max(h - 1, 1) - sy)
        max_radius = max(np.hypot(cx - sx, cy - sy)
                         for cx in (0.0, 1.0) for cy in (0.0, 1.0))
        normalized_radius_sq = (radius / max(max_radius, 1.0e-6)) ** 2
        attenuation = 1.0 / (1.0 + max(0.0, float(radial_falloff)) * normalized_radius_sq)
        if depth_map is not None and float(depth_strength) > 0.0:
            depth_exposure = depth - float(depth.mean())
            depth_gain = np.exp(np.clip(depth_exposure * float(depth_strength), -4.0, 4.0))
            attenuation *= depth_gain
        wave *= attenuation
    # Scattered light leaks ahead of the direct exposure front at lower intensity.
    near_haze = cv2.GaussianBlur(wave, (0, 0), sigmaX=max(2.0, w * 0.055),
                                 sigmaY=max(2.0, h * 0.055),
                                 borderType=cv2.BORDER_REPLICATE)

    source = srgb_to_linear(np.maximum(rgba[..., :3], 0.0))
    tint = np.asarray(temperature, dtype=np.float32)[None, None, :]
    exposure = np.exp2((wave * 0.88 + near_haze * 0.12)[..., None] * (strength / 2.0))
    tint_mix = 1.0 + (tint - 1.0) * np.clip(wave, 0.0, 1.0)[..., None]
    exposed = source * exposure * tint_mix
    # Approximate optical stray-light spread from actual scene highlights.
    # A small Gaussian mixture is a controllable stand-in for a measured lens GSF.
    source_luminance = (source[..., 0] * 0.2126 + source[..., 1] * 0.7152 +
                        source[..., 2] * 0.0722)
    highlight = np.clip((source_luminance - float(highlight_threshold)) /
                        max(float(highlight_range), 1.0e-4), 0.0, 1.0)
    highlight = highlight * highlight * (3.0 - 2.0 * highlight)
    hot_source = source * highlight[..., None]
    bloom = scatter_psf(hot_source, w, h, gsf_fit)
    # Global mode keeps the legacy pulse-wide bloom. Coupled mode gates each
    # highlight source by local flash energy before the PSF, allowing its light
    # to spill softly beyond the traveling front instead of clipping the bloom.
    scatter_front_coupling = float(np.clip(scatter_front_coupling, 0.0, 1.0))
    scatter_linear = bloom * (pulse * (1.0 - scatter_front_coupling))[..., None]
    if scatter_front_coupling > 0.0:
        lit_highlights = hot_source * wave[..., None]
        front_scatter = scatter_psf(lit_highlights, w, h, gsf_fit)
        scatter_linear += front_scatter * scatter_front_coupling
    veil_scale = max(0.0, float(strength)) * max(0.0, float(veil_gain))
    veil = ((wave * 0.72 + near_haze * 0.28)[..., None] * veil_scale *
            np.asarray((2.4, 2.32, 2.2), np.float32))
    result_linear = np.maximum(exposed + scatter_linear * max(0.0, scatter_strength) + veil, 0.0)
    rolloff = float(np.clip(highlight_rolloff, 0.0, 1.0))
    if rolloff > 0.0:
        luma = (result_linear[..., 0] * 0.2126 + result_linear[..., 1] * 0.7152 +
                result_linear[..., 2] * 0.0722)
        knee = 0.62
        shoulder_width = 1.0 - knee
        excess = np.maximum(luma - knee, 0.0)
        shoulder_luma = knee + shoulder_width * (
            1.0 - np.exp(-excess / (shoulder_width * (0.25 + rolloff * 0.75))))
        compressed_luma = np.where(luma > knee, shoulder_luma, luma)
        flash_mix = rolloff * np.clip(wave + near_haze, 0.0, 1.0)
        mapped_luma = luma + (compressed_luma - luma) * flash_mix
        scale = np.divide(mapped_luma, np.maximum(luma, 1.0e-6),
                          out=np.ones_like(luma), where=luma > 1.0e-6)
        result_linear *= scale[..., None]
    result = linear_to_srgb(result_linear)
    result_rgba = np.dstack((np.clip(result, 0.0, 1.0), rgba[..., 3]))
    if not return_components:
        return result_rgba

    # Expose additive pre-rolloff layers for look diagnosis. Their sum is the
    # linear result before the nonlinear highlight shoulder is applied.
    layers: dict[str, np.ndarray] = {"result": result_rgba}
    for name, rgb_linear in (
            ("exposure", exposed),
            ("scatter", source + scatter_linear * max(0.0, float(scatter_strength))),
            ("veil", source + veil)):
        srgb = linear_to_srgb(np.maximum(rgb_linear, 0.0))
        layers[name] = np.dstack((np.clip(srgb, 0.0, 1.0), rgba[..., 3]))
    combined_pre_rolloff = (source + (exposed - source) +
                            scatter_linear * max(0.0, float(scatter_strength)) + veil)
    combined_srgb = linear_to_srgb(np.maximum(combined_pre_rolloff, 0.0))
    layers["combined_pre_rolloff"] = np.dstack(
        (np.clip(combined_srgb, 0.0, 1.0), rgba[..., 3]))
    return layers


def run(output_dir: Path, source: np.ndarray, settings: dict[str, object],
        input_path: Path | None = None, gsf_profile: Path | None = None,
        gsf_fit: tuple[tuple[float, float], ...] | None = None,
        depth_map: np.ndarray | None = None, depth_path: Path | None = None) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    fixture = source
    frame_count = int(settings["frame_count"])
    if frame_count < 2:
        raise ValueError("Frame count must be at least 2")
    duration_seconds = float(settings["duration_seconds"])
    if duration_seconds <= 0.0:
        raise ValueError("Duration must be greater than zero seconds")
    actual_fps = (frame_count - 1) / duration_seconds
    times = np.linspace(0.0, 1.0, frame_count, dtype=np.float32)
    sample_indices = np.linspace(0, frame_count - 1, min(6, frame_count), dtype=np.int32)
    panels = [add_label(to_u8(fixture), "SOURCE")]
    angle = np.deg2rad(float(settings["direction_degrees"]))
    direction = (float(np.cos(angle)), float(np.sin(angle)))
    source_position = tuple(float(v) for v in settings["source_position"])
    temperature = tuple(float(v) for v in settings["temperature"])
    frame_delta_metrics = []
    previous_result: np.ndarray | None = None
    for index, progress in enumerate(times):
        result = flash_fill(
            fixture, float(progress), direction=direction,
            softness=float(settings["softness"]), strength=float(settings["strength"]),
            temperature=temperature, attack_duration=float(settings["attack_duration"]),
            peak_time=float(settings["peak_time"]),
            decay_rate=float(settings["decay_rate"]), mode=str(settings["mode"]),
            source_position=source_position,
            scatter_strength=float(settings["scatter_strength"]),
            scatter_front_coupling=float(settings.get("scatter_front_coupling", 0.0)),
            veil_gain=float(settings.get("veil_gain", 0.025)),
            highlight_threshold=float(settings["highlight_threshold"]),
            highlight_range=float(settings["highlight_range"]), gsf_fit=gsf_fit,
            global_fill=float(settings["global_fill"]),
            front_duration=float(settings["front_duration"]),
            radial_falloff=float(settings["radial_falloff"]),
            depth_map=depth_map,
            depth_strength=float(settings.get("depth_strength", 0.0)),
            depth_near_is_bright=bool(settings.get("depth_near_is_bright", True)),
            depth_front_strength=float(settings.get("depth_front_strength", 0.0)),
            highlight_rolloff=float(settings.get("highlight_rolloff", 0.0)))
        assert np.isfinite(result).all()
        assert np.array_equal(result[..., 3], fixture[..., 3])
        if previous_result is not None:
            visible = (result[..., 3] > 1.0e-5) & (previous_result[..., 3] > 1.0e-5)
            if np.any(visible):
                srgb_delta = np.abs(result[..., :3] - previous_result[..., :3])
                current_linear = srgb_to_linear(result[..., :3])
                previous_linear = srgb_to_linear(previous_result[..., :3])
                linear_delta = np.abs(current_linear - previous_linear)
                current_luma = (current_linear[..., 0] * 0.2126 +
                                current_linear[..., 1] * 0.7152 +
                                current_linear[..., 2] * 0.0722)
                previous_luma = (previous_linear[..., 0] * 0.2126 +
                                 previous_linear[..., 1] * 0.7152 +
                                 previous_linear[..., 2] * 0.0722)
                signed_luma_delta = current_luma - previous_luma
                linear_luma_delta = np.abs(signed_luma_delta)
                frame_delta_metrics.append({
                    "from_frame": index - 1,
                    "to_frame": index,
                    "from_time": float(times[index - 1]),
                    "to_time": float(progress),
                    "from_time_seconds": float(times[index - 1]) * duration_seconds,
                    "to_time_seconds": float(progress) * duration_seconds,
                    "mean_abs_linear_luma_delta": float(linear_luma_delta[visible].mean()),
                    "mean_signed_linear_luma_delta": float(signed_luma_delta[visible].mean()),
                    "max_abs_linear_luma_delta": float(linear_luma_delta[visible].max()),
                    "brightening_pixel_fraction": float(np.mean(signed_luma_delta[visible] > 1.0e-6)),
                    "darkening_pixel_fraction": float(np.mean(signed_luma_delta[visible] < -1.0e-6)),
                    "changed_pixel_fraction_srgb_gt_1_over_255": float(
                        np.mean(np.max(srgb_delta[visible], axis=1) > (1.0 / 255.0))),
                })
        save_image(output_dir / f"flash-frame-{index:04d}.png", result)
        if index in sample_indices:
            panels.append(add_label(to_u8(result),
                                    f"FLASH  {progress:.0%}  |  {float(progress) * duration_seconds:.2f}s"))
        previous_result = result
    panels = [cv2.resize(p, None, fx=0.5, fy=0.5, interpolation=cv2.INTER_AREA) for p in panels]
    sheet = np.vstack(panels)
    cv2.imwrite(str(output_dir / "flash-fill-progression.png"), cv2.cvtColor(sheet, cv2.COLOR_RGBA2BGRA))
    scatter_panels = []
    for amount in SCATTER_SWEEP_SETTINGS["gains"]:
        scatter_result = flash_fill(
            fixture, float(SCATTER_SWEEP_SETTINGS["time"]), direction=direction,
            softness=float(settings["softness"]),
            strength=float(SCATTER_SWEEP_SETTINGS["exposure_strength"]),
            temperature=temperature, attack_duration=float(settings["attack_duration"]),
            peak_time=float(settings["peak_time"]),
            decay_rate=float(settings["decay_rate"]), mode=str(settings["mode"]),
            source_position=source_position, scatter_strength=amount,
            scatter_front_coupling=float(settings.get("scatter_front_coupling", 0.0)),
            veil_gain=float(settings.get("veil_gain", 0.025)),
            highlight_threshold=float(settings["highlight_threshold"]),
            highlight_range=float(settings["highlight_range"]),
            gsf_fit=gsf_fit, global_fill=float(SCATTER_SWEEP_SETTINGS["global_fill"]),
            front_duration=float(settings["front_duration"]),
            radial_falloff=float(settings["radial_falloff"]))
        scatter_panels.append(add_label(to_u8(scatter_result), f"SCATTER  x{amount:.1f}"))
    scatter_panels = [cv2.resize(p, None, fx=0.5, fy=0.5, interpolation=cv2.INTER_AREA)
                      for p in scatter_panels]
    scatter_sheet = np.hstack(scatter_panels)
    cv2.imwrite(str(output_dir / "flash-scatter-sweep.png"),
                cv2.cvtColor(scatter_sheet, cv2.COLOR_RGBA2BGRA))
    sequence_metrics_path = output_dir / "flash-sequence-metrics.json"
    mean_deltas = [entry["mean_abs_linear_luma_delta"] for entry in frame_delta_metrics]
    write_json(sequence_metrics_path, {
        "schema_version": 1,
        "input_image": str(input_path.resolve()) if input_path else "<generated neon fixture>",
        "settings": settings,
        "frame_count": frame_count,
        "timing_seconds": timing_in_seconds(settings),
        "effective_sequence_fps": actual_fps,
        "frame_pair_count": len(frame_delta_metrics),
        "summary": {
            "mean_of_pair_mean_abs_linear_luma_delta":
                float(np.mean(mean_deltas)) if mean_deltas else 0.0,
            "max_pair_mean_abs_linear_luma_delta": max(mean_deltas, default=0.0),
            "max_single_pixel_abs_linear_luma_delta": max(
                (entry["max_abs_linear_luma_delta"] for entry in frame_delta_metrics), default=0.0),
        },
        "frame_pairs": frame_delta_metrics,
    })
    write_json(output_dir / "flash-run.json", {
        "schema_version": 1,
        "input_image": str(input_path.resolve()) if input_path else "<generated neon fixture>",
        "input_dimensions": {"width": int(fixture.shape[1]), "height": int(fixture.shape[0])},
        "depth_map": str(depth_path.resolve()) if depth_path else None,
        "gsf_profile": str(gsf_profile.resolve()) if gsf_profile else None,
        "settings": settings,
        "timeline": {
            **timing_in_seconds(settings),
            "frame_count": frame_count,
            "effective_sequence_fps": actual_fps,
        },
        "scatter_sweep_settings": SCATTER_SWEEP_SETTINGS,
        "gsf_fit_terms_weight_sigma_px": [list(term) for term in gsf_fit] if gsf_fit else None,
        "outputs": [f"flash-frame-{i:04d}.png" for i in range(frame_count)] +
                   ["flash-fill-progression.png", "flash-scatter-sweep.png",
                    sequence_metrics_path.name],
        "sequence_metrics": sequence_metrics_path.name,
    })
    print(f"Saved flash-fill captures to {output_dir.resolve()}")


def run_radial_sweep(output_dir: Path, source: np.ndarray, settings: dict[str, object],
                     progress: float, input_path: Path | None = None,
                     gsf_profile: Path | None = None,
                     gsf_fit: tuple[tuple[float, float], ...] | None = None) -> None:
    """Render a comparable grid of radial-source positions and falloff values."""
    source_x, source_y = (float(v) for v in settings["source_position"])
    source_y = float(np.clip(source_y, 0.0, 1.0))
    x_positions = []
    for x_position in (source_x, 0.5, 0.96):
        x_position = float(np.clip(x_position, 0.0, 1.0))
        if all(abs(x_position - old) > 0.02 for old in x_positions):
            x_positions.append(x_position)
    falloff_values = (0.0, 3.0, 6.0)
    angle = np.deg2rad(float(settings["direction_degrees"]))
    direction = (float(np.cos(angle)), float(np.sin(angle)))
    rows = []
    for falloff in falloff_values:
        cells = []
        for x_position in x_positions:
            position = (x_position, source_y)
            result = flash_fill(
                source, progress, direction=direction,
                softness=float(settings["softness"]), strength=float(settings["strength"]),
                temperature=tuple(float(v) for v in settings["temperature"]),
                attack_duration=float(settings["attack_duration"]),
                peak_time=float(settings["peak_time"]), decay_rate=float(settings["decay_rate"]),
                mode="radial", source_position=position, radial_falloff=falloff,
                scatter_strength=float(settings["scatter_strength"]),
                scatter_front_coupling=float(settings.get("scatter_front_coupling", 0.0)),
                veil_gain=float(settings.get("veil_gain", 0.025)),
                highlight_threshold=float(settings["highlight_threshold"]),
                highlight_range=float(settings["highlight_range"]), gsf_fit=gsf_fit,
                global_fill=float(settings["global_fill"]),
                front_duration=float(settings["front_duration"]))
            if not np.isfinite(result).all():
                raise FloatingPointError("Radial sweep produced a non-finite image")
            if not np.array_equal(result[..., 3], source[..., 3]):
                raise ValueError("Radial sweep changed the source alpha channel")
            cell = add_label(to_u8(result), f"SOURCE X {x_position:.2f}  |  FALLOFF {falloff:.1f}")
            cells.append(cv2.resize(cell, None, fx=0.5, fy=0.5, interpolation=cv2.INTER_AREA))
        rows.append(np.hstack(cells))
    sheet = np.vstack(rows)
    image_path = output_dir / "radial-source-falloff-sweep.png"
    if not cv2.imwrite(str(image_path), cv2.cvtColor(sheet, cv2.COLOR_RGBA2BGRA)):
        raise OSError(f"Could not write radial sweep image: {image_path}")
    metadata_path = output_dir / "radial-source-falloff-sweep.json"
    write_json(metadata_path, {
        "schema_version": 1,
        "input_image": str(input_path.resolve()) if input_path else "<generated neon fixture>",
        "input_dimensions": {"width": int(source.shape[1]), "height": int(source.shape[0])},
        "gsf_profile": str(gsf_profile.resolve()) if gsf_profile else None,
        "time": float(np.clip(progress, 0.0, 1.0)),
        "time_seconds": float(np.clip(progress, 0.0, 1.0)) *
                        float(settings["duration_seconds"]),
        "mode": "radial",
        "timing_seconds": timing_in_seconds(settings),
        "source_positions": [[x, source_y] for x in x_positions],
        "radial_falloff_values": list(falloff_values),
        "base_settings": settings,
        "gsf_fit_terms_weight_sigma_px": [list(term) for term in gsf_fit] if gsf_fit else None,
        "output_image": image_path.name,
    })
    manifest_path = output_dir / "flash-run.json"
    if manifest_path.exists():
        with manifest_path.open("r", encoding="utf-8") as stream:
            manifest = json.load(stream)
        outputs = manifest.setdefault("outputs", [])
        for name in (image_path.name, metadata_path.name):
            if name not in outputs:
                outputs.append(name)
        manifest["radial_sweep"] = {
            "time": float(np.clip(progress, 0.0, 1.0)),
            "time_seconds": float(np.clip(progress, 0.0, 1.0)) *
                            float(settings["duration_seconds"]),
            "source_positions": [[x, source_y] for x in x_positions],
            "radial_falloff_values": list(falloff_values),
        }
        write_json(manifest_path, manifest)
    print(f"Saved radial source/falloff sweep to {image_path.resolve()}")


def run_temporal_samples(output_dir: Path, source: np.ndarray, settings: dict[str, object],
                         sample_times: list[float], input_path: Path | None = None,
                         gsf_profile: Path | None = None,
                         gsf_fit: tuple[tuple[float, float], ...] | None = None) -> None:
    """Render exact normalized times under one shared flash configuration."""
    if not sample_times:
        raise ValueError("At least one temporal sample is required")
    if any(value < 0.0 or value > 1.0 for value in sample_times):
        raise ValueError("Temporal sample times must be between 0 and 1")
    angle = np.deg2rad(float(settings["direction_degrees"]))
    direction = (float(np.cos(angle)), float(np.sin(angle)))
    source_position = tuple(float(v) for v in settings["source_position"])
    temperature = tuple(float(v) for v in settings["temperature"])
    panels = []
    output_names = []
    measurements = []
    for index, sample_time in enumerate(sample_times):
        result = flash_fill(
            source, sample_time, direction=direction,
            softness=float(settings["softness"]), strength=float(settings["strength"]),
            temperature=temperature, attack_duration=float(settings["attack_duration"]),
            peak_time=float(settings["peak_time"]),
            decay_rate=float(settings["decay_rate"]), mode=str(settings["mode"]),
            source_position=source_position,
            radial_falloff=float(settings["radial_falloff"]),
            scatter_strength=float(settings["scatter_strength"]),
            scatter_front_coupling=float(settings.get("scatter_front_coupling", 0.0)),
            veil_gain=float(settings.get("veil_gain", 0.025)),
            highlight_threshold=float(settings["highlight_threshold"]),
            highlight_range=float(settings["highlight_range"]), gsf_fit=gsf_fit,
            global_fill=float(settings["global_fill"]),
            front_duration=float(settings["front_duration"]))
        if not np.isfinite(result).all():
            raise FloatingPointError(f"Temporal sample {sample_time:.3f} produced non-finite pixels")
        if not np.array_equal(result[..., 3], source[..., 3]):
            raise ValueError(f"Temporal sample {sample_time:.3f} changed source alpha")
        time_code = int(round(sample_time * 1000.0))
        filename = f"flash-sample-{index:02d}-t{time_code:03d}.png"
        save_image(output_dir / filename, result)
        output_names.append(filename)
        panels.append(add_label(to_u8(result),
                                f"TIME {sample_time:.3f}  |  {sample_time * float(settings['duration_seconds']):.2f}s"))
        rgb = np.clip(result[..., :3], 0.0, 1.0)
        luma = rgb[..., 0] * 0.2126 + rgb[..., 1] * 0.7152 + rgb[..., 2] * 0.0722
        visible = result[..., 3] > 1.0e-5
        measurements.append({
            "time": sample_time,
            "time_seconds": sample_time * float(settings["duration_seconds"]),
            "mean_srgb_luma_visible": float(luma[visible].mean()) if np.any(visible) else 0.0,
            "p95_srgb_luma_visible": float(np.percentile(luma[visible], 95.0)) if np.any(visible) else 0.0,
            "near_white_pixel_fraction_visible": float(np.mean(np.min(rgb[visible], axis=1) >= 0.999))
            if np.any(visible) else 0.0,
        })
    panels = [cv2.resize(panel, None, fx=0.5, fy=0.5, interpolation=cv2.INTER_AREA)
              for panel in panels]
    contact_path = output_dir / "flash-temporal-samples.png"
    if not cv2.imwrite(str(contact_path),
                       cv2.cvtColor(np.vstack(panels), cv2.COLOR_RGBA2BGRA)):
        raise OSError(f"Could not write temporal sample sheet: {contact_path}")
    metadata_path = output_dir / "flash-temporal-samples.json"
    write_json(metadata_path, {
        "schema_version": 1,
        "input_image": str(input_path.resolve()) if input_path else "<generated neon fixture>",
        "input_dimensions": {"width": int(source.shape[1]), "height": int(source.shape[0])},
        "gsf_profile": str(gsf_profile.resolve()) if gsf_profile else None,
        "mode": str(settings["mode"]),
        "settings": settings,
        "sample_times": sample_times,
        "sample_times_seconds": [value * float(settings["duration_seconds"])
                                 for value in sample_times],
        "timing_seconds": timing_in_seconds(settings),
        "measurements": measurements,
        "gsf_fit_terms_weight_sigma_px": [list(term) for term in gsf_fit] if gsf_fit else None,
        "outputs": output_names + [contact_path.name],
    })
    manifest_path = output_dir / "flash-run.json"
    if manifest_path.exists():
        with manifest_path.open("r", encoding="utf-8") as stream:
            manifest = json.load(stream)
        outputs = manifest.setdefault("outputs", [])
        for name in output_names + [contact_path.name, metadata_path.name]:
            if name not in outputs:
                outputs.append(name)
        manifest["temporal_samples"] = {
            "times": sample_times,
            "contact_sheet": contact_path.name,
            "metadata": metadata_path.name,
        }
        write_json(manifest_path, manifest)
    print(f"Saved exact-time flash samples to {contact_path.resolve()}")


def interactive(output_dir: Path, source: np.ndarray, settings: dict[str, object],
                input_path: Path | None = None, gsf_profile: Path | None = None,
                gsf_fit: tuple[tuple[float, float], ...] | None = None) -> None:
    """Open a live parameter study with sliders and save the current frame with S."""
    output_dir.mkdir(parents=True, exist_ok=True)
    window = "Flash Fill Lab | Space: play/pause | S: save | Esc: quit"
    cv2.namedWindow(window, cv2.WINDOW_NORMAL)
    cv2.resizeWindow(window, 960, 600)
    cv2.createTrackbar("Time %", window, int(float(settings["time"]) * 100), 100, lambda _value: None)
    cv2.createTrackbar("Strength x0.1", window, int(float(settings["strength"]) * 10), 80, lambda _value: None)
    cv2.createTrackbar("Softness x0.01", window, int(float(settings["softness"]) * 100), 50, lambda _value: None)
    cv2.createTrackbar("Direction deg", window, int(float(settings["direction_degrees"])), 359, lambda _value: None)
    cv2.createTrackbar("Wave mode 0 dir 1 radial", window, 1 if settings["mode"] == "radial" else 0,
                       1, lambda _value: None)
    cv2.createTrackbar("Source X %", window, int(float(settings["source_position"][0]) * 100), 100, lambda _value: None)
    cv2.createTrackbar("Source Y %", window, int(float(settings["source_position"][1]) * 100), 100, lambda _value: None)
    cv2.createTrackbar("Radial falloff x0.1", window,
                       int(np.clip(float(settings["radial_falloff"]), 0.0, 10.0) * 10),
                       100, lambda _value: None)
    cv2.createTrackbar("Peak time %", window, int(np.clip(float(settings["peak_time"]), 0.0, 1.0) * 100),
                       100, lambda _value: None)
    cv2.createTrackbar("Attack duration %", window,
                       int(np.clip(float(settings["attack_duration"]), 0.01, 1.0) * 100),
                       100, lambda _value: None)
    cv2.createTrackbar("Decay x0.1", window, int(np.clip(float(settings["decay_rate"]), 0.0, 30.0) * 10),
                       300, lambda _value: None)
    cv2.createTrackbar("Scatter x0.1", window,
                       int(np.clip(float(settings["scatter_strength"]), 0.0, 20.0) * 10),
                       200, lambda _value: None)
    cv2.createTrackbar("Scatter front %", window,
                       int(np.clip(float(settings.get("scatter_front_coupling", 0.0)), 0.0, 1.0) * 100),
                       100, lambda _value: None)
    cv2.createTrackbar("Veil gain x1000", window,
                       int(np.clip(float(settings.get("veil_gain", 0.025)), 0.0, 0.05) * 1000),
                       50, lambda _value: None)
    cv2.createTrackbar("Highlight threshold %", window,
                       int(np.clip(float(settings["highlight_threshold"]), 0.0, 1.0) * 100),
                       100, lambda _value: None)
    cv2.createTrackbar("Highlight range %", window,
                       max(1, int(np.clip(float(settings["highlight_range"]), 0.0, 1.0) * 100)),
                       100, lambda _value: None)
    cv2.createTrackbar("Global fill %", window,
                       int(np.clip(float(settings["global_fill"]), 0.0, 1.0) * 100),
                       100, lambda _value: None)
    cv2.createTrackbar("Front duration %", window,
                       int(np.clip(float(settings["front_duration"]), 0.05, 1.0) * 100),
                       100, lambda _value: None)
    cv2.createTrackbar("Duration x0.1 s", window,
                       int(np.clip(float(settings["duration_seconds"]), 0.1, 10.0) * 10),
                       100, lambda _value: None)
    cv2.createTrackbar("Playback fps", window,
                       int(np.clip(float(settings["fps"]), 1.0, 60.0)),
                       60, lambda _value: None)
    playing = False
    last_tick = time.perf_counter()
    playback_position = float(cv2.getTrackbarPos("Time %", window))
    while True:
        now = time.perf_counter()
        playback_fps = max(1, cv2.getTrackbarPos("Playback fps", window))
        duration_seconds = max(0.1, cv2.getTrackbarPos("Duration x0.1 s", window) / 10.0)
        if playing:
            visible_position = cv2.getTrackbarPos("Time %", window)
            if abs(visible_position - (int(round(playback_position)) % 100)) > 1:
                playback_position = float(visible_position)
            playback_position = (playback_position +
                                 (now - last_tick) * 100.0 / duration_seconds) % 100.0
            new_position = int(round(playback_position)) % 100
            if new_position != visible_position:
                cv2.setTrackbarPos("Time %", window, new_position)
            last_tick = now
        else:
            playback_position = float(cv2.getTrackbarPos("Time %", window))
            last_tick = now
        progress = cv2.getTrackbarPos("Time %", window) / 100.0
        strength = cv2.getTrackbarPos("Strength x0.1", window) / 10.0
        softness = max(0.01, cv2.getTrackbarPos("Softness x0.01", window) / 100.0)
        angle = np.deg2rad(cv2.getTrackbarPos("Direction deg", window))
        peak_time = cv2.getTrackbarPos("Peak time %", window) / 100.0
        attack_duration = max(0.01, cv2.getTrackbarPos("Attack duration %", window) / 100.0)
        decay_rate = cv2.getTrackbarPos("Decay x0.1", window) / 10.0
        mode = "radial" if cv2.getTrackbarPos("Wave mode 0 dir 1 radial", window) else "directional"
        source_position = (cv2.getTrackbarPos("Source X %", window) / 100.0,
                           cv2.getTrackbarPos("Source Y %", window) / 100.0)
        radial_falloff = cv2.getTrackbarPos("Radial falloff x0.1", window) / 10.0
        scatter_strength = cv2.getTrackbarPos("Scatter x0.1", window) / 10.0
        scatter_front_coupling = cv2.getTrackbarPos("Scatter front %", window) / 100.0
        veil_gain = cv2.getTrackbarPos("Veil gain x1000", window) / 1000.0
        highlight_threshold = cv2.getTrackbarPos("Highlight threshold %", window) / 100.0
        highlight_range = max(0.01, cv2.getTrackbarPos("Highlight range %", window) / 100.0)
        global_fill = cv2.getTrackbarPos("Global fill %", window) / 100.0
        front_duration = max(0.05, cv2.getTrackbarPos("Front duration %", window) / 100.0)
        direction = (float(np.cos(angle)), float(np.sin(angle)))
        temperature = tuple(float(v) for v in settings["temperature"])
        result = flash_fill(source, progress, direction=direction,
                            softness=softness, strength=strength,
                            attack_duration=attack_duration,
                            peak_time=peak_time, decay_rate=decay_rate,
                            mode=mode, source_position=source_position,
                            radial_falloff=radial_falloff,
                            scatter_strength=scatter_strength,
                            scatter_front_coupling=scatter_front_coupling,
                            veil_gain=veil_gain,
                            highlight_threshold=highlight_threshold,
                            highlight_range=highlight_range, gsf_fit=gsf_fit,
                            global_fill=global_fill, front_duration=front_duration,
                            temperature=temperature)
        preview = cv2.cvtColor(to_u8(result), cv2.COLOR_RGBA2BGR)
        playback_label = "PLAY" if playing else "PAUSE"
        cv2.putText(preview,
                    f"{playback_label}  {playback_fps} FPS  "
                    f"{progress * duration_seconds:.2f}/{duration_seconds:.1f}s  {progress:.0%}",
                    (18, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.72,
                    (245, 245, 245), 2, cv2.LINE_AA)
        cv2.imshow(window, preview)
        key = cv2.waitKey(max(1, int(round(1000.0 / playback_fps)))) & 0xFF
        if key in (27, ord("q")):
            break
        if key == ord(" "):
            playing = not playing
            last_tick = time.perf_counter()
            playback_position = float(cv2.getTrackbarPos("Time %", window))
        if key in (ord("s"), ord("S")):
            filename = f"flash-p{progress:.2f}-s{strength:.1f}-{mode}-a{int(np.rad2deg(angle))}.png"
            image_path = output_dir / filename
            save_image(image_path, result)
            saved_settings = {
                "time": progress,
                "duration_seconds": duration_seconds,
                "fps": float(playback_fps),
                "time_seconds": progress * duration_seconds,
                "strength": strength,
                "softness": softness,
                "direction_degrees": float(np.rad2deg(angle)),
                "mode": mode,
                "source_position": list(source_position),
                "radial_falloff": radial_falloff,
                "attack_duration": attack_duration,
                "peak_time": peak_time,
                "decay_rate": decay_rate,
                "scatter_strength": scatter_strength,
                "scatter_front_coupling": scatter_front_coupling,
                "veil_gain": veil_gain,
                "highlight_threshold": highlight_threshold,
                "highlight_range": highlight_range,
                "global_fill": global_fill,
                "front_duration": front_duration,
                "temperature": list(temperature),
                "frame_count": int(settings["frame_count"]),
            }
            write_json(Path(str(image_path) + ".json"), {
                "schema_version": 1,
                "input_image": str(input_path.resolve()) if input_path else "<generated neon fixture>",
                "gsf_profile": str(gsf_profile.resolve()) if gsf_profile else None,
                "settings": saved_settings,
                "gsf_fit_terms_weight_sigma_px": [list(term) for term in gsf_fit] if gsf_fit else None,
            })
            print(f"Saved preview and preset: {image_path}")
    cv2.destroyWindow(window)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--interactive", action="store_true",
                        help="open live controls for flash timing, shape, source, and appearance")
    parser.add_argument("--input", type=Path,
                        help="optional 8-bit PNG/JPEG/BMP image; uses the built-in fixture when omitted")
    parser.add_argument("--depth-map", type=Path,
                        help="optional grayscale depth map (white=near by default; radial mode only)")
    parser.add_argument("--depth-strength", type=float,
                        help="relative depth attenuation strength; 0 disables depth modulation")
    parser.add_argument("--depth-front-strength", type=float,
                        help="relative depth delay of the radial front; 0 disables depth timing")
    parser.add_argument("--depth-white-is-far", action="store_true",
                        help="interpret white depth-map values as farther instead of nearer")
    parser.add_argument("--frames", type=int,
                        help="number of PNG frames in the temporal progression (batch mode)")
    parser.add_argument("--duration-seconds", type=float,
                        help="real-time duration mapped to normalized flash time (default 1 second)")
    parser.add_argument("--fps", type=float,
                        help="nominal sequence/playback FPS; derives frame count unless --frames is set")
    parser.add_argument("--radial-sweep", action="store_true",
                        help="also render a batch contact sheet for radial source position and falloff")
    parser.add_argument("--temporal-samples", nargs="+", type=float,
                        help="also render exact normalized times under the shared run settings")
    parser.add_argument("--sweep-time", type=float,
                        help="normalized time used by --radial-sweep (default 0.60)")
    parser.add_argument("--peak-time", type=float,
                        help="flash peak time as a normalized value from 0 to 1")
    parser.add_argument("--attack-duration", type=float,
                        help="normalized rise duration before the flash peak (0..1)")
    parser.add_argument("--decay-rate", type=float,
                        help="exponential decay rate after the flash peak")
    parser.add_argument("--mode", choices=("directional", "radial"),
                        help="spatial shape of the advancing flash front")
    parser.add_argument("--scatter", type=float,
                        help="gain for source-highlight scatter through the approximate GSF")
    parser.add_argument("--scatter-front-coupling", type=float,
                        help="couple highlight scatter to the traveling front (0=global, 1=front-gated)")
    parser.add_argument("--veil-gain", type=float,
                        help="gain for the broad veiling-light component (default 0.025)")
    parser.add_argument("--highlight-threshold", type=float,
                        help="linear luminance threshold for pixels that seed lens scatter")
    parser.add_argument("--highlight-range", type=float,
                        help="linear luminance range above the threshold to reach full seed weight")
    parser.add_argument("--highlight-rolloff", type=float,
                        help="linear-light highlight shoulder amount (0 is hard clipping; 0..1)")
    parser.add_argument("--gsf-profile", type=Path,
                        help="measured radial GSF CSV from measure_gsf.py")
    parser.add_argument("--global-fill", type=float,
                        help="uniform veiling component mixed with the spatial front (0..1)")
    parser.add_argument("--radial-falloff", type=float,
                        help="radial exposure falloff amount (0 is uniform; radial mode only)")
    parser.add_argument("--front-duration", type=float,
                        help="normalized time taken for the front to cross the image")
    parser.add_argument("--strength", type=float, help="exposure gain in stops")
    parser.add_argument("--softness", type=float, help="front softness normalized to image extent")
    parser.add_argument("--direction-degrees", type=float, help="directional sweep angle")
    parser.add_argument("--source-position", nargs=2, type=float, metavar=("X", "Y"),
                        help="radial source point in normalized image coordinates")
    parser.add_argument("--temperature", nargs=3, type=float, metavar=("R", "G", "B"),
                        help="linear flash tint multipliers")
    parser.add_argument("--preset", type=Path,
                        help="load a preset JSON (S saves a compatible preset beside the preview)")
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parent / "captures")
    args = parser.parse_args()
    if args.interactive and (args.radial_sweep or args.temporal_samples is not None):
        parser.error("--radial-sweep and --temporal-samples are available in batch mode only")
    settings = load_settings(args.preset)
    preset_document: dict[str, object] = {}
    if args.preset is not None:
        with args.preset.open("r", encoding="utf-8") as stream:
            loaded_document = json.load(stream)
        if isinstance(loaded_document, dict):
            preset_document = loaded_document
    overrides = {
        "frame_count": args.frames,
        "duration_seconds": args.duration_seconds,
        "fps": args.fps,
        "peak_time": args.peak_time,
        "decay_rate": args.decay_rate,
        "attack_duration": args.attack_duration,
        "mode": args.mode,
        "scatter_strength": args.scatter,
        "scatter_front_coupling": args.scatter_front_coupling,
        "veil_gain": args.veil_gain,
        "highlight_threshold": args.highlight_threshold,
        "highlight_range": args.highlight_range,
        "highlight_rolloff": args.highlight_rolloff,
        "global_fill": args.global_fill,
        "radial_falloff": args.radial_falloff,
        "front_duration": args.front_duration,
        "strength": args.strength,
        "softness": args.softness,
        "direction_degrees": args.direction_degrees,
    }
    for field, override in overrides.items():
        if override is not None:
            settings[field] = override
    if float(settings["duration_seconds"]) <= 0.0:
        parser.error("--duration-seconds and preset duration must be greater than zero")
    if float(settings["fps"]) <= 0.0:
        parser.error("--fps and preset FPS must be greater than zero")
    if args.frames is None and (args.duration_seconds is not None or args.fps is not None):
        settings["frame_count"] = max(
            2, int(round(float(settings["duration_seconds"]) * float(settings["fps"]))) + 1)
    if args.source_position is not None:
        settings["source_position"] = args.source_position
    if args.depth_strength is not None:
        settings["depth_strength"] = args.depth_strength
    if args.depth_front_strength is not None:
        settings["depth_front_strength"] = args.depth_front_strength
    settings["depth_near_is_bright"] = not args.depth_white_is_far
    if args.radial_sweep and str(settings["mode"]) != "radial":
        parser.error("--radial-sweep requires radial mode (use --mode radial or a radial preset)")
    if args.temperature is not None:
        settings["temperature"] = args.temperature
    input_path = args.input
    if input_path is None:
        recorded_input = preset_document.get("input_image")
        if recorded_input and recorded_input != "<generated neon fixture>":
            input_path = Path(str(recorded_input))
            if not input_path.exists():
                raise FileNotFoundError(f"Preset input image no longer exists: {input_path}")
    gsf_profile = args.gsf_profile
    if gsf_profile is None and preset_document.get("gsf_profile"):
        gsf_profile = Path(str(preset_document["gsf_profile"]))
        if not gsf_profile.exists():
            raise FileNotFoundError(f"Preset GSF profile no longer exists: {gsf_profile}")
    source = load_image(input_path) if input_path else make_fixture()
    depth_map = None
    if args.depth_map is not None:
        depth_image = cv2.imread(str(args.depth_map), cv2.IMREAD_GRAYSCALE)
        if depth_image is None:
            raise FileNotFoundError(f"Could not read depth map: {args.depth_map}")
        if depth_image.shape != source.shape[:2]:
            parser.error(f"Depth map dimensions {depth_image.shape[::-1]} must match input image "
                         f"dimensions {(source.shape[1], source.shape[0])}")
        depth_map = depth_image.astype(np.float32) / 255.0
    if depth_map is not None and str(settings["mode"]) != "radial":
        parser.error("--depth-map currently requires radial mode")
    gsf_fit = load_gsf_fit(gsf_profile, source.shape) if gsf_profile else None
    if gsf_fit:
        print("Fitted GSF Gaussian terms (weight, sigma px): " +
              ", ".join(f"({weight:.4f}, {sigma:.1f})" for weight, sigma in gsf_fit))
    if args.interactive:
        interactive(args.output, source, settings, input_path,
                    gsf_profile, gsf_fit)
    else:
        run(args.output, source, settings, input_path, gsf_profile, gsf_fit,
            depth_map, args.depth_map)
        if args.radial_sweep:
            run_radial_sweep(args.output, source, settings,
                             0.60 if args.sweep_time is None else args.sweep_time,
                             input_path, gsf_profile, gsf_fit)
        if args.temporal_samples is not None:
            run_temporal_samples(args.output, source, settings, args.temporal_samples,
                                 input_path, gsf_profile, gsf_fit)
