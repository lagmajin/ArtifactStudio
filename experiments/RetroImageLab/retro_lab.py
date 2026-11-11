"""Small, standalone still-image retro effect playground."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont


PRESETS = {
    "handheld": {"colors": 8, "scale": 0.5, "dither": 0.0, "scanlines": 0.12, "aberration": 0.6, "grain": 0.025},
    "arcade": {"colors": 24, "scale": 0.75, "dither": 0.35, "scanlines": 0.2, "aberration": 1.2, "grain": 0.015},
    "print": {"colors": 12, "scale": 1.0, "dither": 0.7, "scanlines": 0.0, "aberration": 0.0, "grain": 0.035},
    "vhs": {"colors": 48, "scale": 1.0, "dither": 0.0, "scanlines": 0.1, "aberration": 2.0, "grain": 0.04},
}

BAYER_4 = np.array(
    [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]],
    dtype=np.float32,
) / 16.0 - 0.5
BAYER_8 = np.array(
    [[0, 48, 12, 60, 3, 51, 15, 63], [32, 16, 44, 28, 35, 19, 47, 31],
     [8, 56, 4, 52, 11, 59, 7, 55], [40, 24, 36, 20, 43, 27, 39, 23],
     [2, 50, 14, 62, 1, 49, 13, 61], [34, 18, 46, 30, 33, 17, 45, 29],
     [10, 58, 6, 54, 9, 57, 5, 53], [42, 26, 38, 22, 41, 25, 37, 21]],
    dtype=np.float32,
) / 64.0 - 0.5

PALETTES = {
    "gameboy": np.array([[15, 56, 15], [48, 98, 48], [139, 172, 15], [155, 188, 15]], dtype=np.float32) / 255.0,
    "cga": np.array([[0, 0, 0], [85, 255, 255], [255, 85, 255], [255, 255, 255]], dtype=np.float32) / 255.0,
    "pico8": np.array([[0, 0, 0], [29, 43, 83], [126, 37, 83], [0, 135, 81], [171, 82, 54], [95, 87, 79], [194, 195, 199], [255, 241, 232], [255, 0, 77], [255, 163, 0], [255, 236, 39], [0, 228, 54], [41, 173, 255], [131, 118, 156], [255, 119, 168], [255, 204, 170]], dtype=np.float32) / 255.0,
}


def ordered_quantize(rgb: np.ndarray, levels: int, matrix: np.ndarray, strength: float) -> np.ndarray:
    height, width, _ = rgb.shape
    threshold = np.tile(matrix, (height // matrix.shape[0] + 1, width // matrix.shape[1] + 1))[:height, :width]
    step = 1.0 / max(levels - 1, 1)
    return np.clip(np.floor(np.clip(rgb + threshold[:, :, None] * step * strength, 0, 1) / step + 0.5) * step, 0, 1)


def ordered_quantize_domain(rgb: np.ndarray, levels: int, matrix: np.ndarray,
                            strength: float, domain: str) -> np.ndarray:
    if domain == "srgb":
        return ordered_quantize(rgb, levels, matrix, strength)
    linear = srgb_to_linear(rgb)
    height, width, _ = rgb.shape
    threshold = np.tile(
        matrix,
        (height // matrix.shape[0] + 1, width // matrix.shape[1] + 1),
    )[:height, :width]
    step = 1.0 / max(levels - 1, 1)
    quantized = np.floor(
        np.clip(linear + threshold[:, :, None] * step * strength, 0.0, 1.0)
        / step + 0.5
    ) * step
    return linear_to_srgb(np.clip(quantized, 0.0, 1.0))


def error_diffuse_quantize(rgb: np.ndarray, levels: int, domain: str) -> np.ndarray:
    """Floyd–Steinberg RGB error diffusion; offline reference implementation."""
    work = srgb_to_linear(rgb).copy() if domain == "linear" else rgb.copy()
    height, width, _ = work.shape
    scale = max(levels - 1, 1)
    result = np.empty_like(work)
    for y in range(height):
        for x in range(width):
            old = np.clip(work[y, x], 0.0, 1.0)
            new = np.round(old * scale) / scale
            result[y, x] = new
            error = old - new
            if x + 1 < width:
                work[y, x + 1] += error * (7.0 / 16.0)
            if y + 1 < height:
                if x > 0:
                    work[y + 1, x - 1] += error * (3.0 / 16.0)
                work[y + 1, x] += error * (5.0 / 16.0)
                if x + 1 < width:
                    work[y + 1, x + 1] += error * (1.0 / 16.0)
    return linear_to_srgb(result) if domain == "linear" else np.clip(result, 0.0, 1.0)


def make_banding_fixture(width: int = 512, height: int = 128) -> Image.Image:
    x = np.linspace(0.0, 1.0, width, dtype=np.float32)
    gray = np.broadcast_to(x[None, :, None], (height // 2, width, 3))
    color = np.stack((
        np.broadcast_to(x[None, :], (height // 2, width)),
        np.broadcast_to((0.18 + 0.72 * np.sqrt(x))[None, :], (height // 2, width)),
        np.broadcast_to((1.0 - x)[None, :], (height // 2, width)),
    ), axis=2)
    pixels = np.concatenate((gray, color), axis=0)
    return Image.fromarray(np.uint8(np.clip(pixels * 255.0 + 0.5, 0, 255)), "RGB")


def horizontal_run_metrics(gray: np.ndarray) -> tuple[float, float, float]:
    quantized = np.uint8(np.clip(gray * 255.0 + 0.5, 0, 255))
    mean_unique = float(np.mean([np.unique(row).size for row in quantized]))
    longest_runs: list[int] = []
    reverse_steps: list[float] = []
    for row in quantized:
        changes = np.flatnonzero(row[1:] != row[:-1]) + 1
        boundaries = np.concatenate(([0], changes, [row.size]))
        longest_runs.append(int(np.max(np.diff(boundaries))))
        reverse_steps.append(float(np.mean(np.diff(row.astype(np.int16)) < 0)))
    return mean_unique, float(np.mean(longest_runs)), float(np.mean(reverse_steps))


def quantization_banding_study(output_gray: Path, output_color: Path,
                               fixture_path: Path) -> dict[str, float]:
    image = make_banding_fixture()
    fixture_path.parent.mkdir(parents=True, exist_ok=True)
    image.save(fixture_path)
    source = np.asarray(image, dtype=np.float32) / 255.0
    gray_source = source[:source.shape[0] // 2]
    color_source = source[source.shape[0] // 2:]
    gray_linear = srgb_to_linear(gray_source)
    color_linear = srgb_to_linear(color_source)
    cases: list[tuple[str, int, float, np.ndarray, np.ndarray]] = []
    metrics: dict[str, float] = {}
    for domain in ("srgb", "linear"):
        for levels in (4, 8, 16):
            for strength in (0.0, 0.5, 1.0):
                gray_result = ordered_quantize_domain(
                    gray_source, levels, BAYER_4, strength, domain,
                )
                color_result = ordered_quantize_domain(
                    color_source, levels, BAYER_4, strength, domain,
                )
                key = f"{domain}/{levels}levels/dither{strength:.1f}"
                unique_gray, longest_run, reverse_ratio = horizontal_run_metrics(
                    gray_result[:, :, 0],
                )
                metrics[f"{key} gray linear RGB RMSE"] = float(np.mean(
                    (srgb_to_linear(gray_result) - gray_linear) ** 2,
                ) ** 0.5)
                metrics[f"{key} color linear RGB RMSE"] = float(np.mean(
                    (srgb_to_linear(color_result) - color_linear) ** 2,
                ) ** 0.5)
                metrics[f"{key} mean unique gray codes per row"] = unique_gray
                metrics[f"{key} mean longest gray run px"] = longest_run
                metrics[f"{key} horizontal reverse-step ratio"] = reverse_ratio
                cases.append((domain, levels, strength, gray_result, color_result))

    def save_sheet(path: Path, colored: bool) -> None:
        thumb_width = source.shape[1]
        thumb_height = source.shape[0] // 2
        label_height = 22
        columns = 3
        rows = (len(cases) + columns - 1) // columns
        sheet = Image.new(
            "RGB", (columns * thumb_width, rows * (thumb_height + label_height)),
            (28, 28, 32),
        )
        draw = ImageDraw.Draw(sheet)
        for index, (domain, levels, strength, gray_result, color_result) in enumerate(cases):
            result = color_result if colored else gray_result
            top = (index // columns) * (thumb_height + label_height)
            left = (index % columns) * thumb_width
            section = "color ramp" if colored else "gray ramp"
            draw.text(
                (left + 4, top + 4), f"{domain} · {levels} levels · dither {strength:.1f} · {section}",
                fill=(245, 245, 245), font=ImageFont.load_default(),
            )
            thumb = Image.fromarray(np.uint8(np.clip(result * 255.0, 0, 255)), "RGB")
            sheet.paste(thumb, (left, top + label_height))
        path.parent.mkdir(parents=True, exist_ok=True)
        sheet.save(path)

    save_sheet(output_gray, False)
    save_sheet(output_color, True)
    return metrics


def make_natural_quantization_fixture(width: int = 384, height: int = 256,
                                      seed: int = 23) -> Image.Image:
    """Create a deterministic landscape chart with gradients and fine detail."""
    rng = np.random.default_rng(seed)
    yy, xx = np.mgrid[0:height, 0:width].astype(np.float32)
    x = xx / max(width - 1, 1)
    y = yy / max(height - 1, 1)
    x_row = np.linspace(0.0, 1.0, width, dtype=np.float32)[None, :]
    y_column = np.linspace(0.0, 1.0, height, dtype=np.float32)[:, None]
    horizon = 0.50 + 0.025 * np.sin(x_row * np.pi * 2.0)
    sky_mix = np.clip(y_column / horizon, 0.0, 1.0)
    sky_top = np.array((0.12, 0.25, 0.48), dtype=np.float32)
    sky_bottom = np.array((0.94, 0.62, 0.34), dtype=np.float32)
    sky = np.broadcast_to(
        sky_top[None, None, :] * (1.0 - sky_mix[:, :, None])
        + sky_bottom[None, None, :] * sky_mix[:, :, None],
        (height, width, 3),
    ).copy()

    hill_a = 0.59 + 0.045 * np.sin(x_row * 7.0 + 0.4) + 0.018 * np.sin(x_row * 19.0)
    hill_b = 0.69 + 0.035 * np.sin(x_row * 5.0 + 2.2) + 0.014 * np.sin(x_row * 23.0 + 1.0)
    ground_a = np.array((0.24, 0.43, 0.31), dtype=np.float32)
    ground_b = np.array((0.12, 0.29, 0.23), dtype=np.float32)
    ground_c = np.array((0.34, 0.31, 0.23), dtype=np.float32)
    pixels = sky.copy()
    mask_a = y_column >= hill_a
    pixels[mask_a] = ground_a
    mask_b = y_column >= hill_b
    pixels[mask_b] = ground_b
    foreground = y_column >= 0.82 + 0.018 * np.sin(x_row * 9.0)
    pixels[foreground] = ground_c

    # A broad sun glow and soft cloud bands provide low-frequency gradients.
    glow = np.exp(-(((x - 0.76) / 0.22) ** 2 + ((y - 0.26) / 0.18) ** 2) * 2.0)
    pixels += glow[:, :, None] * np.array((0.18, 0.11, 0.035), dtype=np.float32)
    cloud = np.exp(-((y - (0.22 + 0.025 * np.sin(x * 12.0))) / 0.018) ** 2)
    pixels += cloud[:, :, None] * np.array((0.025, 0.025, 0.02), dtype=np.float32)

    # Fine deterministic grass strokes expose detail loss and dither texture.
    draw_base = Image.fromarray(np.uint8(np.clip(pixels * 255.0 + 0.5, 0, 255)), "RGB")
    draw = ImageDraw.Draw(draw_base)
    for _ in range(520):
        px = int(rng.integers(0, width))
        py = int(rng.integers(int(height * 0.58), height))
        length = int(rng.integers(2, 10))
        color = (47, 69, 39) if py < height * 0.82 else (65, 57, 43)
        draw.line((px, py, px + int(rng.integers(-2, 3)), py - length), fill=color, width=1)
    # Small high-contrast house and window edges anchor a detail crop.
    draw.polygon(((274, 154), (313, 154), (313, 190), (274, 190)), fill=(184, 125, 77))
    draw.polygon(((268, 156), (293, 134), (319, 156)), fill=(73, 43, 38))
    draw.rectangle((282, 165, 290, 173), fill=(43, 77, 102))
    draw.rectangle((299, 165, 307, 173), fill=(43, 77, 102))
    draw.rectangle((290, 177, 298, 190), fill=(74, 48, 38))
    return draw_base


def make_retro_suite_fixture(width: int = 384, height: int = 256) -> Image.Image:
    """Create a license-free synthetic evaluation plate with RGB and alpha cases."""
    image = make_natural_quantization_fixture(width, height)
    pixels = np.asarray(image, dtype=np.uint8).copy()
    rgba = np.concatenate((pixels, np.full((height, width, 1), 255, dtype=np.uint8)), axis=2)
    result = Image.fromarray(rgba, "RGBA")
    draw = ImageDraw.Draw(result)
    draw.ellipse((18, 174, 206, 362), fill=(225, 74, 113, 210), outline=(255, 236, 181, 255), width=2)
    draw.ellipse((54, 210, 170, 326), fill=(30, 82, 144, 92), outline=(255, 255, 255, 180), width=1)
    draw.rectangle((334, 16, width - 8, 116), fill=(252, 235, 90, 255))
    draw.line((326, 120, width - 8, 240), fill=(248, 248, 248, 255), width=1)
    return result


def natural_quantization_study(fixture_path: Path, output_sheet: Path,
                               output_crop: Path) -> dict[str, float]:
    image = make_natural_quantization_fixture()
    fixture_path.parent.mkdir(parents=True, exist_ok=True)
    image.save(fixture_path)
    source = np.asarray(image, dtype=np.float32) / 255.0
    source_linear = srgb_to_linear(source)
    cases: list[tuple[str, int, float, np.ndarray]] = [("source baseline", 0, 0.0, source)]
    metrics: dict[str, float] = {}
    for domain in ("srgb", "linear"):
        for levels in (8, 16):
            for strength in (0.0, 0.5, 1.0):
                result = ordered_quantize_domain(source, levels, BAYER_4, strength, domain)
                key = f"{domain}/{levels}levels/dither{strength:.1f}"
                result_linear = srgb_to_linear(result)
                error = result_linear - source_linear
                source_luma = np.mean(source_linear, axis=2)
                result_luma = np.mean(result_linear, axis=2)
                source_gradient = np.hypot(
                    np.diff(source_luma, axis=1, append=source_luma[:, -1:]),
                    np.diff(source_luma, axis=0, append=source_luma[-1:, :]),
                )
                result_gradient = np.hypot(
                    np.diff(result_luma, axis=1, append=result_luma[:, -1:]),
                    np.diff(result_luma, axis=0, append=result_luma[-1:, :]),
                )
                metrics[f"{key} linear RGB RMSE"] = float(np.mean(error ** 2) ** 0.5)
                metrics[f"{key} detail gradient MAE"] = float(np.mean(np.abs(result_gradient - source_gradient)))
                metrics[f"{key} changed pixel ratio"] = float(np.mean(np.any(np.abs(result - source) > 1e-6, axis=2)))
                cases.append((domain, levels, strength, result))

    thumb_w, thumb_h, label_h = image.width, image.height, 24
    columns = 3
    rows = (len(cases) + columns - 1) // columns
    sheet = Image.new("RGB", (columns * thumb_w, rows * (thumb_h + label_h)), (28, 28, 32))
    draw = ImageDraw.Draw(sheet)
    for index, (domain, levels, strength, result) in enumerate(cases):
        row, column = divmod(index, columns)
        left, top = column * thumb_w, row * (thumb_h + label_h)
        label = "source baseline" if levels == 0 else f"{domain} · {levels} levels · dither {strength:.1f}"
        draw.text((left + 5, top + 5), label, fill=(245, 245, 245), font=ImageFont.load_default())
        sheet.paste(Image.fromarray(np.uint8(np.clip(result * 255.0 + 0.5, 0, 255)), "RGB"), (left, top + label_h))
    output_sheet.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output_sheet)

    crop = image.crop((240, 118, 336, 214))
    crop_w, crop_h, crop_label_h, crop_scale = crop.width, crop.height, 22, 2
    crop_sheet = Image.new(
        "RGB",
        (columns * crop_w * crop_scale, rows * (crop_h * crop_scale + crop_label_h)),
        (28, 28, 32),
    )
    crop_draw = ImageDraw.Draw(crop_sheet)
    for index, (domain, levels, strength, result) in enumerate(cases):
        row, column = divmod(index, columns)
        left = column * crop_w * crop_scale
        top = row * (crop_h * crop_scale + crop_label_h)
        label = "source baseline" if levels == 0 else f"{domain} · {levels} levels · dither {strength:.1f}"
        crop_draw.text((left + 4, top + 4), label, fill=(245, 245, 245), font=ImageFont.load_default())
        crop_pixels = np.uint8(np.clip(result[118:214, 240:336] * 255.0 + 0.5, 0, 255))
        crop_sheet.paste(
            Image.fromarray(crop_pixels, "RGB").resize(
                (crop_w * crop_scale, crop_h * crop_scale), Image.Resampling.NEAREST,
            ),
            (left, top + crop_label_h),
        )
    output_crop.parent.mkdir(parents=True, exist_ok=True)
    crop_sheet.save(output_crop)
    return metrics


def dither_pattern_study(fixture_path: Path, output_sheet: Path,
                         output_crop: Path) -> dict[str, float]:
    """Compare periodic ordered patterns and Floyd–Steinberg on one fixed scene."""
    image = make_natural_quantization_fixture()
    fixture_path.parent.mkdir(parents=True, exist_ok=True)
    image.save(fixture_path)
    source = np.asarray(image, dtype=np.float32) / 255.0
    levels = 8
    cases: list[tuple[str, str, np.ndarray]] = [("source baseline", "none", source)]
    metrics: dict[str, float] = {}
    settings: list[tuple[str, str, np.ndarray]] = []
    for domain in ("srgb", "linear"):
        settings.append((domain, "no dither", ordered_quantize_domain(source, levels, BAYER_4, 0.0, domain)))
        for matrix_name, matrix in (("Bayer 4x4", BAYER_4), ("Bayer 8x8", BAYER_8)):
            for strength in (0.5, 1.0):
                settings.append((domain, f"{matrix_name} · {strength:.1f}", ordered_quantize_domain(source, levels, matrix, strength, domain)))
        settings.append((domain, "Floyd–Steinberg", error_diffuse_quantize(source, levels, domain)))
    cases.extend(settings)

    source_linear = srgb_to_linear(source)
    regions = {
        "whole": (0, 0, image.width, image.height),
        "sky": (0, 0, image.width, 125),
        "hill boundary": (0, 132, image.width, 214),
        "house and grass detail": (240, 118, 336, 214),
    }
    for domain, method, result in settings:
        key = f"{domain}/{method}"
        result_linear = srgb_to_linear(result)
        for region, (left, top, right, bottom) in regions.items():
            source_region = source_linear[top:bottom, left:right]
            result_region = result_linear[top:bottom, left:right]
            metrics[f"{key} {region} linear RGB RMSE"] = float(np.mean((result_region - source_region) ** 2) ** 0.5)
            source_luma = np.mean(source_region, axis=2)
            result_luma = np.mean(result_region, axis=2)
            source_gradient = np.hypot(
                np.diff(source_luma, axis=1, append=source_luma[:, -1:]),
                np.diff(source_luma, axis=0, append=source_luma[-1:, :]),
            )
            result_gradient = np.hypot(
                np.diff(result_luma, axis=1, append=result_luma[:, -1:]),
                np.diff(result_luma, axis=0, append=result_luma[-1:, :]),
            )
            metrics[f"{key} {region} luma gradient MAE"] = float(np.mean(np.abs(result_gradient - source_gradient)))

    columns = 4
    rows = (len(cases) + columns - 1) // columns
    label_height = 24
    sheet = Image.new("RGB", (columns * image.width, rows * (image.height + label_height)), (28, 28, 32))
    draw = ImageDraw.Draw(sheet)
    for index, (domain, method, result) in enumerate(cases):
        row, column = divmod(index, columns)
        left, top = column * image.width, row * (image.height + label_height)
        label = "source baseline" if domain == "source baseline" else f"{domain} · {method}"
        draw.text((left + 5, top + 5), label, fill=(245, 245, 245), font=ImageFont.load_default())
        sheet.paste(Image.fromarray(np.uint8(np.clip(result * 255.0 + 0.5, 0, 255)), "RGB"), (left, top + label_height))
    output_sheet.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output_sheet)

    crop_box = (240, 118, 336, 214)
    crop_width, crop_height, scale, crop_label_height = 96, 96, 3, 22
    crop_sheet = Image.new(
        "RGB",
        (columns * crop_width * scale, rows * (crop_height * scale + crop_label_height)),
        (28, 28, 32),
    )
    crop_draw = ImageDraw.Draw(crop_sheet)
    for index, (domain, method, result) in enumerate(cases):
        row, column = divmod(index, columns)
        left, top = column * crop_width * scale, row * (crop_height * scale + crop_label_height)
        label = "source baseline" if domain == "source baseline" else f"{domain} · {method}"
        crop_draw.text((left + 4, top + 4), label, fill=(245, 245, 245), font=ImageFont.load_default())
        crop = Image.fromarray(np.uint8(np.clip(result[crop_box[1]:crop_box[3], crop_box[0]:crop_box[2]] * 255.0 + 0.5, 0, 255)), "RGB")
        crop_sheet.paste(crop.resize((crop_width * scale, crop_height * scale), Image.Resampling.NEAREST), (left, top + crop_label_height))
    output_crop.parent.mkdir(parents=True, exist_ok=True)
    crop_sheet.save(output_crop)
    return metrics


def ordered_quantize_with_origin(rgb: np.ndarray, levels: int, matrix: np.ndarray,
                                 strength: float, origin_x: int, origin_y: int = 0,
                                 domain: str = "srgb") -> np.ndarray:
    height, width, _ = rgb.shape
    yy, xx = np.mgrid[0:height, 0:width]
    threshold = matrix[
        ((yy + origin_y) % matrix.shape[0]),
        ((xx + origin_x) % matrix.shape[1]),
    ]
    if domain == "linear":
        working = srgb_to_linear(rgb)
    else:
        working = rgb
    step = 1.0 / max(levels - 1, 1)
    quantized = np.floor(
        np.clip(working + threshold[:, :, None] * step * strength, 0.0, 1.0)
        / step + 0.5
    ) * step
    quantized = np.clip(quantized, 0.0, 1.0)
    return linear_to_srgb(quantized) if domain == "linear" else quantized


def translate_frame_clamped(rgb: np.ndarray, offset_x: int) -> np.ndarray:
    height, width, _ = rgb.shape
    source_x = np.clip(np.arange(width) - offset_x, 0, width - 1)
    return rgb[:, source_x]


def temporal_dither_study(fixture_path: Path, output_sheet: Path,
                          output_crop: Path) -> dict[str, float]:
    """Measure dither-pattern changes after aligning translated still frames."""
    image = make_natural_quantization_fixture()
    fixture_path.parent.mkdir(parents=True, exist_ok=True)
    image.save(fixture_path)
    base = np.asarray(image, dtype=np.float32) / 255.0
    offsets = tuple(range(8))
    levels = 8
    methods = (
        ("no dither", "none", None, "screen"),
        ("Bayer 4x4 screen-locked", "bayer", BAYER_4, "screen"),
        ("Bayer 4x4 content-locked", "bayer", BAYER_4, "content"),
        ("Bayer 8x8 screen-locked", "bayer", BAYER_8, "screen"),
        ("Bayer 8x8 content-locked", "bayer", BAYER_8, "content"),
        ("Floyd-Steinberg per-frame", "floyd", None, "screen"),
    )
    outputs: dict[str, list[np.ndarray]] = {}
    metrics: dict[str, float] = {}
    for label, kind, matrix, lock in methods:
        rendered: list[np.ndarray] = []
        for offset in offsets:
            frame = translate_frame_clamped(base, offset)
            if kind == "none":
                result = ordered_quantize_domain(frame, levels, BAYER_4, 0.0, "srgb")
            elif kind == "bayer":
                # Content locking compensates for the image's +offset translation.
                phase_origin = -offset if lock == "content" else 0
                result = ordered_quantize_with_origin(
                    frame, levels, matrix, 0.5, phase_origin, "srgb",
                )
            else:
                result = error_diffuse_quantize(frame, levels, "srgb")
            rendered.append(result)
        outputs[label] = rendered

        frame_to_frame: list[float] = []
        aligned_to_first: list[float] = []
        for index in range(1, len(offsets)):
            previous = srgb_to_linear(rendered[index - 1])
            current = srgb_to_linear(rendered[index])
            # Remove the known one-pixel content motion before comparing texture.
            frame_to_frame.append(float(np.mean(np.abs(current[:, 1:-16] - previous[:, :-17]))))
            displacement = offsets[index]
            reference = srgb_to_linear(rendered[0])
            aligned_to_first.append(float(np.mean(np.abs(
                current[:, displacement + 8:base.shape[1] - 8]
                - reference[:, 8:base.shape[1] - displacement - 8],
            ))))
        metrics[f"{label} mean aligned frame-to-frame linear MAE"] = float(np.mean(frame_to_frame))
        metrics[f"{label} max aligned frame-to-frame linear MAE"] = float(np.max(frame_to_frame))
        metrics[f"{label} mean aligned drift from frame 0 linear MAE"] = float(np.mean(aligned_to_first))
        metrics[f"{label} max aligned drift from frame 0 linear MAE"] = float(np.max(aligned_to_first))

    columns = len(offsets)
    tile_width, tile_height, scale, label_height = 192, 128, 2, 22
    sheet = Image.new(
        "RGB",
        (columns * tile_width, len(methods) * (tile_height + label_height)),
        (28, 28, 32),
    )
    draw = ImageDraw.Draw(sheet)
    crop_box = (240, 118, 336, 214)
    crop_width, crop_height, crop_scale, crop_label_height = 96, 96, 2, 22
    crop_sheet = Image.new(
        "RGB",
        (columns * crop_width * crop_scale,
         len(methods) * (crop_height * crop_scale + crop_label_height)),
        (28, 28, 32),
    )
    crop_draw = ImageDraw.Draw(crop_sheet)
    for row, (label, _, _, _) in enumerate(methods):
        for column, (offset, result) in enumerate(zip(offsets, outputs[label])):
            left, top = column * tile_width, row * (tile_height + label_height)
            draw.text((left + 4, top + 4), f"{label} · dx {offset}px", fill=(245, 245, 245), font=ImageFont.load_default())
            tile = Image.fromarray(np.uint8(np.clip(result * 255.0 + 0.5, 0, 255)), "RGB")
            sheet.paste(tile.resize((tile_width, tile_height), Image.Resampling.NEAREST), (left, top + label_height))

            crop_left = column * crop_width * crop_scale
            crop_top = row * (crop_height * crop_scale + crop_label_height)
            crop_draw.text((crop_left + 4, crop_top + 4), f"{label} · dx {offset}px", fill=(245, 245, 245), font=ImageFont.load_default())
            crop = Image.fromarray(
                np.uint8(np.clip(result[crop_box[1]:crop_box[3], crop_box[0]:crop_box[2]] * 255.0 + 0.5, 0, 255)),
                "RGB",
            )
            crop_sheet.paste(
                crop.resize((crop_width * crop_scale, crop_height * crop_scale), Image.Resampling.NEAREST),
                (crop_left, crop_top + crop_label_height),
            )
    output_sheet.parent.mkdir(parents=True, exist_ok=True)
    output_crop.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output_sheet)
    crop_sheet.save(output_crop)
    return metrics


def recursive_bayer_matrix(size: int) -> np.ndarray:
    ranks = np.zeros((size, size), dtype=np.int32)
    bit = 0
    while (1 << bit) < size:
        x_bits = (np.arange(size)[None, :] >> bit) & 1
        y_bits = (np.arange(size)[:, None] >> bit) & 1
        ranks = ranks * 4 + ((x_bits ^ y_bits) * 2 + y_bits)
        bit += 1
    return ranks


def product_bayer_quantize(rgb: np.ndarray, size: int, color_count: int,
                           amount: float, pattern_scale: float,
                           implementation: str) -> np.ndarray:
    height, width, _ = rgb.shape
    yy, xx = np.mgrid[0:height, 0:width]
    levels = max(2.0, float(np.sqrt(color_count)))
    if implementation in ("cpu", "resident"):
        bx = (xx * pattern_scale / max(1.0, width * 0.01) * size).astype(np.int32) % size
        by = (yy * pattern_scale / max(1.0, height * 0.01) * size).astype(np.int32) % size
        if implementation == "cpu":
            rank_table = recursive_bayer_matrix(size)
        elif size == 2:
            rank_table = np.array([[0, 2], [3, 1]], dtype=np.int32)
        else:
            rank_table = np.array(
                [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]],
                dtype=np.int32,
            )
        ranks = rank_table[by, bx].astype(np.float32)
        matrix = size * size
        threshold = ranks / matrix + 0.5 / matrix
        threshold = (threshold - 0.5) * (1.0 - amount) * 2.0 + 0.5
    elif implementation == "standalone":
        if size not in (2, 4):
            # Source HLSL only transforms algorithm IDs 0 and 1.
            return np.clip(rgb, 0.0, 1.0)
        bx = xx % size
        by = yy % size
        if size == 2:
            flat_bayer4 = np.array(
                [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5],
                dtype=np.float32,
            )
            indices = ((by & 1) * 2 + (bx & 1)) % 16
            matrix_values = flat_bayer4[indices]
        else:
            matrix_values = np.array(
                [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5],
                dtype=np.float32,
            )[(by * 4 + bx) % 16]
        threshold = matrix_values / float(size * size) + 0.5 / float(size * size)
        threshold = (threshold - 0.5) * (1.0 - amount) * 2.0 + 0.5
    else:
        raise ValueError(f"unsupported product dither model: {implementation}")

    result = np.floor(rgb * levels + threshold[:, :, None]) / levels
    return np.clip(result, 0.0, 1.0)


def product_dither_source_audit(fixture_path: Path, output_sheet: Path,
                                amount: float = 0.0, pattern_scale: float = 1.0,
                                color_count: int = 16) -> dict[str, float]:
    """Model the three source Bayer formulations on identical pixels/settings."""
    image = make_natural_quantization_fixture()
    fixture_path.parent.mkdir(parents=True, exist_ok=True)
    image.save(fixture_path)
    source = np.asarray(image, dtype=np.float32) / 255.0
    cases: list[tuple[str, np.ndarray]] = []
    metrics: dict[str, float] = {}
    for size in (2, 4):
        cpu = product_bayer_quantize(source, size, color_count, amount, pattern_scale, "cpu")
        resident = product_bayer_quantize(source, size, color_count, amount, pattern_scale, "resident")
        standalone = product_bayer_quantize(source, size, color_count, amount, pattern_scale, "standalone")
        cases.extend(((f"CPU Bayer {size}x{size}", cpu),
                      (f"resident HLSL Bayer {size}x{size}", resident),
                      (f"standalone HLSL Bayer {size}x{size}", standalone)))
        for label, result in (("resident", resident), ("standalone", standalone)):
            difference = np.abs(result - cpu)
            key = f"Bayer {size}x{size} {label} vs CPU"
            metrics[f"{key} differing pixel ratio"] = float(np.mean(np.any(difference > 1e-6, axis=2)))
            metrics[f"{key} mean absolute RGB code difference"] = float(np.mean(difference))
            metrics[f"{key} max absolute RGB code difference"] = float(np.max(difference))
    for size in (8, 16):
        cpu = product_bayer_quantize(source, size, color_count, amount, pattern_scale, "cpu")
        unsupported = product_bayer_quantize(source, size, color_count, amount, pattern_scale, "standalone")
        cases.extend(((f"CPU Bayer {size}x{size}", cpu),
                      (f"standalone HLSL ID {2 if size == 8 else 3}", unsupported)))
        difference = np.abs(unsupported - cpu)
        key = f"Bayer {size}x{size} unsupported standalone ID vs CPU"
        metrics[f"{key} differing pixel ratio"] = float(np.mean(np.any(difference > 1e-6, axis=2)))
        metrics[f"{key} mean absolute RGB code difference"] = float(np.mean(difference))
        metrics[f"{key} max absolute RGB code difference"] = float(np.max(difference))

    columns, rows, label_height = 4, (len(cases) + 3) // 4, 24
    sheet = Image.new(
        "RGB", (columns * image.width, rows * (image.height + label_height)),
        (28, 28, 32),
    )
    draw = ImageDraw.Draw(sheet)
    for index, (label, result) in enumerate(cases):
        row, column = divmod(index, columns)
        left, top = column * image.width, row * (image.height + label_height)
        draw.text((left + 5, top + 5), label, fill=(245, 245, 245), font=ImageFont.load_default())
        tile = Image.fromarray(np.uint8(np.clip(result * 255.0 + 0.5, 0, 255)), "RGB")
        sheet.paste(tile, (left, top + label_height))
    output_sheet.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output_sheet)
    metrics["modeled color count"] = float(color_count)
    metrics["modeled levels per channel"] = float(max(2.0, np.sqrt(color_count)))
    metrics["modeled amount"] = float(amount)
    metrics["modeled pattern scale"] = float(pattern_scale)
    return metrics


def retro_suite(image: Image.Image, mode: str, seed: int = 23,
                alpha_resize: str = "straight") -> Image.Image:
    """Apply the actual named RetroImageLab preset and preserve RGBA alpha."""
    if mode not in PRESETS:
        raise ValueError(f"unknown retro suite mode: {mode}")
    return apply_retro(image, **PRESETS[mode], seed=seed, alpha_resize=alpha_resize)


def prepare_eval_corpus(source_dir: Path, corpus_dir: Path,
                        provenance_path: Path | None = None) -> tuple[Path, list[dict[str, object]]]:
    """Copy user-provided images with a manifest; files are never fetched externally."""
    from shutil import copy2

    corpus_dir.mkdir(parents=True, exist_ok=True)
    provenance = {}
    if provenance_path is not None and provenance_path.exists():
        provenance = json.loads(provenance_path.read_text(encoding="utf-8"))
    supported = {".png", ".jpg", ".jpeg", ".tif", ".tiff", ".webp"}
    manifest: list[dict[str, object]] = []
    for source_path in sorted(p for p in source_dir.rglob("*") if p.is_file() and p.suffix.lower() in supported):
        digest = hashlib.sha256(source_path.read_bytes()).hexdigest()
        destination = corpus_dir / f"{digest[:16]}{source_path.suffix.lower()}"
        if not destination.exists():
            copy2(source_path, destination)
        with Image.open(destination) as opened:
            image = opened.convert("RGBA")
            alpha = np.asarray(image.getchannel("A"), dtype=np.uint8)
            manifest.append({
                "file": destination.name,
                "source_name": source_path.name,
                "sha256": digest,
                "width": image.width,
                "height": image.height,
                "original_mode": opened.mode,
                "alpha_present": "A" in opened.getbands(),
                "non_opaque_alpha_ratio": float(np.mean(alpha < 255)),
                "source_url_or_origin": provenance.get(source_path.name, {}).get("source_url_or_origin", "unknown"),
                "license_or_permission": provenance.get(source_path.name, {}).get("license_or_permission", "unknown"),
            })
    manifest_path = corpus_dir / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return manifest_path, manifest


def alpha_hidden_rgb_suite_study(output_path: Path,
                                 fixture_path: Path) -> dict[str, float]:
    """Test whether transparent RGB affects visible output under real suite presets."""
    width, height = 192, 128
    base = np.zeros((height, width, 4), dtype=np.uint8)
    base[:, :, :3] = (232, 176, 92)
    yy, xx = np.mgrid[0:height, 0:width]
    signed = (xx - 72) * 0.82 + (yy - 58) * 0.38
    alpha = np.clip((signed + 2.5) / 5.0, 0.0, 1.0)
    alpha[(xx > 108) & (yy < 38)] = 0.0
    alpha[(xx > 130) & (yy > 92)] = 0.0
    base[:, :, 3] = np.uint8(alpha * 255.0 + 0.5)
    hidden_a = base.copy()
    hidden_b = base.copy()
    transparent = hidden_a[:, :, 3] == 0
    hidden_a[transparent, :3] = (255, 0, 255)
    hidden_b[transparent, :3] = (0, 255, 24)
    fixture_path.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(hidden_a, "RGBA").save(fixture_path)

    backgrounds = {
        "black": (0.0, 0.0, 0.0),
        "white": (1.0, 1.0, 1.0),
        "checker": (0.72, 0.72, 0.72),
    }
    edge = (alpha > 0.02) & (alpha < 0.98)
    cases: list[tuple[str, np.ndarray]] = []
    metrics: dict[str, float] = {}
    for mode in PRESETS:
        result_a = retro_suite(Image.fromarray(hidden_a, "RGBA"), mode, 23)
        result_b = retro_suite(Image.fromarray(hidden_b, "RGBA"), mode, 23)
        rgb_a = np.asarray(result_a, dtype=np.float32)[:, :, :3] / 255.0
        rgb_b = np.asarray(result_b, dtype=np.float32)[:, :, :3] / 255.0
        out_alpha = np.asarray(result_a, dtype=np.float32)[:, :, 3] / 255.0
        key = mode
        metrics[f"{key} hidden RGB raw edge MAE"] = float(np.mean(np.abs(rgb_a[edge] - rgb_b[edge])))
        metrics[f"{key} alpha exact preservation"] = float(np.array_equal(
            np.asarray(result_a)[:, :, 3], base[:, :, 3],
        ))
        for bg_name, background in backgrounds.items():
            comp_a = composite_rgba(rgb_a, out_alpha, background)
            comp_b = composite_rgba(rgb_b, out_alpha, background)
            delta = np.abs(comp_a - comp_b)
            metrics[f"{key} {bg_name} full visible hidden RGB MAE"] = float(np.mean(delta))
            metrics[f"{key} {bg_name} edge visible hidden RGB MAE"] = float(np.mean(delta[edge]))
        if mode in ("handheld", "arcade"):
            for bg_name, background in ("black", backgrounds["black"]), ("white", backgrounds["white"]):
                cases.append((f"{mode} hidden A / {bg_name}", composite_rgba(rgb_a, out_alpha, background)))
                cases.append((f"{mode} hidden B / {bg_name}", composite_rgba(rgb_b, out_alpha, background)))

    columns, tile_w, tile_h, label_h = 2, width * 2, height * 2, 22
    rows = (len(cases) + columns - 1) // columns
    sheet = Image.new("RGB", (columns * tile_w, rows * (tile_h + label_h)), (28, 28, 32))
    draw = ImageDraw.Draw(sheet)
    for index, (label, pixels) in enumerate(cases):
        row, column = divmod(index, columns)
        left, top = column * tile_w, row * (tile_h + label_h)
        draw.text((left + 5, top + 4), label, fill=(245, 245, 245), font=ImageFont.load_default())
        tile = Image.fromarray(np.uint8(np.clip(pixels * 255.0 + 0.5, 0, 255)), "RGB")
        sheet.paste(tile.resize((tile_w, tile_h), Image.Resampling.NEAREST), (left, top + label_h))
    output_path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output_path)
    return metrics


def alpha_resize_suite_study(output_path: Path,
                             fixture_path: Path) -> dict[str, float]:
    """Compare straight and premultiplied source downsampling in scale presets."""
    width, height = 192, 128
    rgba = np.zeros((height, width, 4), dtype=np.uint8)
    rgba[:, :, :3] = (244, 184, 104)
    yy, xx = np.mgrid[0:height, 0:width]
    alpha = np.clip(((xx - 72) * 0.82 + (yy - 58) * 0.38 + 2.5) / 5.0, 0.0, 1.0)
    rgba[:, :, 3] = np.uint8(alpha * 255.0 + 0.5)
    rgba[(xx > 108) & (yy < 38), 3] = 0
    rgba[(xx > 130) & (yy > 92), 3] = 0
    source = Image.fromarray(rgba, "RGBA")
    fixture_path.parent.mkdir(parents=True, exist_ok=True)
    source.save(fixture_path)
    source_rgb = np.asarray(source, dtype=np.float32)[:, :, :3] / 255.0
    source_alpha = np.asarray(source, dtype=np.float32)[:, :, 3] / 255.0
    edge = (source_alpha > 0.02) & (source_alpha < 0.98)
    backgrounds = {"black": (0.0, 0.0, 0.0), "white": (1.0, 1.0, 1.0)}
    metrics: dict[str, float] = {}
    cases: list[tuple[str, np.ndarray]] = []
    for mode in ("handheld", "arcade"):
        straight = retro_suite(source, mode, 23, "straight")
        premul = retro_suite(source, mode, 23, "premultiplied")
        straight_rgb = np.asarray(straight, dtype=np.float32)[:, :, :3] / 255.0
        premul_rgb = np.asarray(premul, dtype=np.float32)[:, :, :3] / 255.0
        out_alpha = np.asarray(straight, dtype=np.float32)[:, :, 3] / 255.0
        delta = np.abs(straight_rgb - premul_rgb)
        metrics[f"{mode} RGB edge MAE straight vs premultiplied"] = float(np.mean(delta[edge]))
        metrics[f"{mode} alpha exact preservation both"] = float(
            np.array_equal(np.asarray(straight)[:, :, 3], rgba[:, :, 3])
            and np.array_equal(np.asarray(premul)[:, :, 3], rgba[:, :, 3])
        )
        for bg_name, bg in backgrounds.items():
            comp_straight = composite_rgba(straight_rgb, out_alpha, bg)
            comp_premul = composite_rgba(premul_rgb, out_alpha, bg)
            metrics[f"{mode} {bg_name} composite edge MAE"] = float(
                np.mean(np.abs(comp_straight[edge] - comp_premul[edge]))
            )
            cases.extend(((f"{mode} straight / {bg_name}", comp_straight),
                          (f"{mode} premultiplied / {bg_name}", comp_premul)))
        for alpha_mode in ("straight", "premultiplied"):
            resized = apply_retro(
                source, colors=256, scale=PRESETS[mode]["scale"], dither=0.0,
                scanlines=0.0, aberration=0.0, grain=0.0, seed=23,
                alpha_resize=alpha_mode,
            )
            resized_rgb = np.asarray(resized, dtype=np.float32)[:, :, :3] / 255.0
            metrics[f"{mode} resize-only {alpha_mode} edge RGB MAE vs source"] = float(
                np.mean(np.abs(resized_rgb[edge] - source_rgb[edge]))
            )

    columns, tile_w, tile_h, label_h = 2, width * 2, height * 2, 22
    rows = (len(cases) + columns - 1) // columns
    sheet = Image.new("RGB", (columns * tile_w, rows * (tile_h + label_h)), (28, 28, 32))
    draw = ImageDraw.Draw(sheet)
    for index, (label, pixels) in enumerate(cases):
        row, column = divmod(index, columns)
        left, top = column * tile_w, row * (tile_h + label_h)
        draw.text((left + 5, top + 4), label, fill=(245, 245, 245), font=ImageFont.load_default())
        tile = Image.fromarray(np.uint8(np.clip(pixels * 255.0 + 0.5, 0, 255)), "RGB")
        sheet.paste(tile.resize((tile_w, tile_h), Image.Resampling.NEAREST), (left, top + label_h))
    output_path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output_path)
    return metrics


def alpha_rgb_dilation_study(output_path: Path,
                             fixture_path: Path) -> dict[str, float]:
    """Check bounded hidden-RGB dilation as a transparent-edge alternative."""
    width, height = 192, 128
    base = np.zeros((height, width, 4), dtype=np.uint8)
    base[:, :, :3] = (232, 176, 92)
    yy, xx = np.mgrid[0:height, 0:width]
    base[:, :, 0] = np.uint8(48 + 190 * xx / max(width - 1, 1))
    base[:, :, 1] = np.uint8(214 - 155 * xx / max(width - 1, 1))
    base[:, :, 2] = np.uint8(58 + 0.45 * yy)
    alpha = np.clip((((xx - 72) * 0.82 + (yy - 58) * 0.38) + 2.5) / 5.0, 0.0, 1.0)
    alpha[(xx > 108) & (yy < 38)] = 0.0
    alpha[(xx > 130) & (yy > 92)] = 0.0
    base[:, :, 3] = np.uint8(alpha * 255.0 + 0.5)
    hidden_a = base.copy()
    hidden_b = base.copy()
    transparent = base[:, :, 3] == 0
    hidden_a[transparent, :3] = (255, 0, 255)
    hidden_b[transparent, :3] = (0, 255, 24)
    fixture_path.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(hidden_a, "RGBA").save(fixture_path)
    edge = (alpha > 0.02) & (alpha < 0.98)
    metrics: dict[str, float] = {}
    backgrounds = {"black": (0.0, 0.0, 0.0), "white": (1.0, 1.0, 1.0)}
    cases: list[tuple[str, np.ndarray]] = []
    for mode in ("handheld", "arcade"):
        straight_reference = retro_suite(Image.fromarray(hidden_a, "RGBA"), mode, 23, "straight")
        straight_reference_rgb = np.asarray(straight_reference, dtype=np.float32)[:, :, :3] / 255.0
        for alpha_mode in ("straight", "nearest", "dilated", "premultiplied"):
            result_a = retro_suite(Image.fromarray(hidden_a, "RGBA"), mode, 23, alpha_mode)
            result_b = retro_suite(Image.fromarray(hidden_b, "RGBA"), mode, 23, alpha_mode)
            pixels_a = np.asarray(result_a, dtype=np.float32) / 255.0
            pixels_b = np.asarray(result_b, dtype=np.float32) / 255.0
            alpha_out = pixels_a[:, :, 3]
            metrics[f"{mode} {alpha_mode} RGB edge delta vs straight baseline"] = float(
                np.mean(np.abs(pixels_a[:, :, :3][edge] - straight_reference_rgb[edge]))
            )
            for bg_name, background in backgrounds.items():
                comp_a = composite_rgba(pixels_a[:, :, :3], alpha_out, background)
                comp_b = composite_rgba(pixels_b[:, :, :3], alpha_out, background)
                metrics[f"{mode} {alpha_mode} {bg_name} hidden RGB edge MAE"] = float(
                    np.mean(np.abs(comp_a[edge] - comp_b[edge]))
                )
                if bg_name == "black":
                    cases.extend(((f"{mode} {alpha_mode} hidden A", comp_a),
                                  (f"{mode} {alpha_mode} hidden B", comp_b)))
            metrics[f"{mode} {alpha_mode} alpha exact"] = float(
                np.array_equal(np.asarray(result_a)[:, :, 3], base[:, :, 3])
            )

    columns, tile_w, tile_h, label_h = 2, width * 2, height * 2, 22
    rows = (len(cases) + columns - 1) // columns
    sheet = Image.new("RGB", (columns * tile_w, rows * (tile_h + label_h)), (28, 28, 32))
    draw = ImageDraw.Draw(sheet)
    for index, (label, pixels) in enumerate(cases):
        row, column = divmod(index, columns)
        left, top = column * tile_w, row * (tile_h + label_h)
        draw.text((left + 5, top + 4), label, fill=(245, 245, 245), font=ImageFont.load_default())
        tile = Image.fromarray(np.uint8(np.clip(pixels * 255.0 + 0.5, 0, 255)), "RGB")
        sheet.paste(tile.resize((tile_w, tile_h), Image.Resampling.NEAREST), (left, top + label_h))
    output_path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output_path)
    return metrics


def alpha_dilation_stress_study(output_path: Path,
                                fixture_dir: Path) -> dict[str, float]:
    """Stress nearest-color ownership across narrow transparent gaps and strands."""
    width, height = 192, 128
    fixture_dir.mkdir(parents=True, exist_ok=True)
    metrics: dict[str, float] = {}
    cases: list[tuple[str, np.ndarray]] = []
    backgrounds = {"black": (0.0, 0.0, 0.0), "white": (1.0, 1.0, 1.0)}
    for gap in (1, 2, 4, 8):
        scale = 2
        alpha_hi = Image.new("L", (width * scale, height * scale), 0)
        draw = ImageDraw.Draw(alpha_hi)
        draw.rectangle((24 * scale, 18 * scale, 78 * scale - 1, 108 * scale - 1), fill=255)
        right_start = 79 + gap
        draw.rectangle((right_start * scale, 18 * scale, 154 * scale - 1, 108 * scale - 1), fill=255)
        # One-pixel strands stress whether dilation jumps across disconnected fine features.
        for x in (39, 48, 57, 112, 124, 136):
            draw.line((x * scale, 8 * scale, x * scale, 118 * scale), fill=255, width=scale)
        alpha = np.asarray(alpha_hi.resize((width, height), Image.Resampling.LANCZOS), dtype=np.uint8)
        base = np.zeros((height, width, 4), dtype=np.uint8)
        base[:, :, 3] = alpha
        opaque_left = (alpha > 0) & (np.indices((height, width))[1] < right_start)
        base[opaque_left, :3] = (224, 48, 35)
        base[(alpha > 0) & ~opaque_left, :3] = (24, 72, 232)
        # A small gold strand is distinct from both large objects.
        strand_mask = (alpha > 0) & (np.indices((height, width))[0] < 18)
        base[strand_mask, :3] = (250, 205, 48)
        hidden_a = base.copy()
        hidden_b = base.copy()
        hidden = alpha == 0
        hidden_a[hidden, :3] = (255, 0, 255)
        hidden_b[hidden, :3] = (0, 255, 24)
        fixture_path = fixture_dir / f"alpha-gap-{gap}px.png"
        Image.fromarray(hidden_a, "RGBA").save(fixture_path)
        gap_region = (np.indices((height, width))[1] >= 78) & (np.indices((height, width))[1] <= 80 + gap)
        edge_region = (alpha > 0) & (alpha < 255)
        for mode in ("handheld", "arcade"):
            straight_a = retro_suite(Image.fromarray(hidden_a, "RGBA"), mode, 23, "straight")
            straight_b = retro_suite(Image.fromarray(hidden_b, "RGBA"), mode, 23, "straight")
            nearest_a = retro_suite(Image.fromarray(hidden_a, "RGBA"), mode, 23, "nearest")
            nearest_b = retro_suite(Image.fromarray(hidden_b, "RGBA"), mode, 23, "nearest")
            for method, first, second in (("straight", straight_a, straight_b), ("nearest", nearest_a, nearest_b)):
                p = np.asarray(first, dtype=np.float32) / 255.0
                q = np.asarray(second, dtype=np.float32) / 255.0
                a = p[:, :, 3]
                comp_p = composite_rgba(p[:, :, :3], a, backgrounds["black"])
                comp_q = composite_rgba(q[:, :, :3], a, backgrounds["black"])
                key = f"gap{gap}px {mode} {method}"
                metrics[f"{key} hidden RGB edge composite MAE"] = float(np.mean(np.abs(comp_p[edge_region] - comp_q[edge_region])))
                metrics[f"{key} hidden RGB gap composite MAE"] = float(np.mean(np.abs(comp_p[gap_region] - comp_q[gap_region])))
                metrics[f"{key} alpha exact"] = float(np.array_equal(np.asarray(first)[:, :, 3], alpha))
                if mode == "handheld":
                    cases.append((f"gap {gap}px · {method} · {mode}", comp_p))
    columns, tile_w, tile_h, label_h = 2, width * 2, height * 2, 22
    rows = (len(cases) + columns - 1) // columns
    sheet = Image.new("RGB", (columns * tile_w, rows * (tile_h + label_h)), (28, 28, 32))
    draw = ImageDraw.Draw(sheet)
    for index, (label, pixels) in enumerate(cases):
        row, column = divmod(index, columns)
        left, top = column * tile_w, row * (tile_h + label_h)
        draw.text((left + 5, top + 4), label, fill=(245, 245, 245), font=ImageFont.load_default())
        tile = Image.fromarray(np.uint8(np.clip(pixels * 255.0 + 0.5, 0, 255)), "RGB")
        sheet.paste(tile.resize((tile_w, tile_h), Image.Resampling.NEAREST), (left, top + label_h))
    output_path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output_path)
    return metrics


def retro_suite_corpus_study(source_dir: Path, corpus_dir: Path, output_dir: Path,
                             seed: int = 23,
                             provenance_path: Path | None = None) -> dict[str, float]:
    """Evaluate fixed retro presets over a provenance-tracked user corpus."""
    manifest_path, manifest = prepare_eval_corpus(source_dir, corpus_dir, provenance_path)
    images = [entry for entry in manifest if entry["width"] and entry["height"]]
    if not images:
        raise ValueError(f"No supported user-provided images found in {source_dir}")
    output_dir.mkdir(parents=True, exist_ok=True)
    metrics: dict[str, float] = {}
    for mode in ("handheld", "arcade", "print", "vhs"):
        tile_w, tile_h, label_h = 256, 192, 42
        columns = min(3, len(images))
        rows = (len(images) + columns - 1) // columns
        sheet = Image.new("RGB", (columns * tile_w, rows * (tile_h + label_h)), (28, 28, 32))
        draw = ImageDraw.Draw(sheet)
        for index, entry in enumerate(images):
            source_path = corpus_dir / str(entry["file"])
            with Image.open(source_path) as opened:
                original = opened.convert("RGBA")
            result = retro_suite(original, mode, seed)
            original_rgb = np.asarray(original.convert("RGB"), dtype=np.float32) / 255.0
            result_rgb = np.asarray(result.convert("RGB"), dtype=np.float32) / 255.0
            key = f"{entry['file']} {mode}"
            metrics[f"{key} RGB RMSE"] = float(np.mean((result_rgb - original_rgb) ** 2) ** 0.5)
            metrics[f"{key} changed pixel ratio"] = float(np.mean(np.any(np.abs(result_rgb - original_rgb) > 1e-6, axis=2)))
            metrics[f"{key} alpha changed pixel ratio"] = float(np.mean(
                np.asarray(result.getchannel("A"), dtype=np.uint8) != np.asarray(original.getchannel("A"), dtype=np.uint8),
            ))
            def composite_preview(image: Image.Image) -> Image.Image:
                preview_bg = Image.new("RGBA", image.size, (36, 40, 48, 255))
                preview_bg.alpha_composite(image)
                preview = preview_bg.convert("RGB")
                preview.thumbnail((tile_w // 2, tile_h), Image.Resampling.LANCZOS)
                return preview
            baseline_preview = composite_preview(original)
            result_preview = composite_preview(result)
            row, column = divmod(index, columns)
            left, top = column * tile_w, row * (tile_h + label_h)
            label = f"{entry['file'][:12]} · {mode} · {entry['width']}×{entry['height']}"
            draw.text((left + 4, top + 4), label, fill=(245, 245, 245), font=ImageFont.load_default())
            draw.text((left + 4, top + 20), f"Origin: {entry['source_url_or_origin']} | License: {entry['license_or_permission']}", fill=(200, 200, 205), font=ImageFont.load_default())
            sheet.paste(baseline_preview, (left, top + label_h))
            sheet.paste(result_preview, (left + tile_w // 2, top + label_h))
            draw.text((left + 4, top + label_h + 3), "baseline", fill=(255, 255, 255), font=ImageFont.load_default())
            draw.text((left + tile_w // 2 + 4, top + label_h + 3), mode, fill=(255, 255, 255), font=ImageFont.load_default())
        sheet_path = output_dir / f"suite-{mode}.png"
        sheet.save(sheet_path)
    (output_dir / "metrics.json").write_text(json.dumps(metrics, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    metrics["corpus image count"] = float(len(images))
    metrics["manifest entry count"] = float(len(manifest))
    print(f"Corpus manifest: {manifest_path}")
    print(f"Suite sheets and metrics: {output_dir}")
    return metrics


def suite_detail_crop_study(image_path: Path, crop_box: tuple[int, int, int, int],
                            output_path: Path, seed: int = 23) -> dict[str, float]:
    """Render one reproducible ROI from the baseline and each actual lab preset."""
    with Image.open(image_path) as opened:
        source = opened.convert("RGBA")
    left, top, right, bottom = crop_box
    if left < 0 or top < 0 or right > source.width or bottom > source.height or left >= right or top >= bottom:
        raise ValueError(f"crop box {crop_box} is outside {source.width}x{source.height}")
    baseline = source.crop(crop_box)
    cases = [("baseline", baseline)]
    metrics: dict[str, float] = {}
    base_rgb = np.asarray(baseline.convert("RGB"), dtype=np.float32) / 255.0
    for mode in PRESETS:
        effected = retro_suite(source, mode, seed).crop(crop_box)
        cases.append((mode, effected))
        rgb = np.asarray(effected.convert("RGB"), dtype=np.float32) / 255.0
        metrics[f"{mode} ROI RGB RMSE vs baseline"] = float(np.mean((rgb - base_rgb) ** 2) ** 0.5)
        metrics[f"{mode} ROI changed pixel ratio"] = float(np.mean(np.any(rgb != base_rgb, axis=2)))
    scale = max(1, min(3, 720 // max(baseline.width, baseline.height)))
    label_h = 24
    sheet = Image.new("RGB", (len(cases) * baseline.width * scale,
                               baseline.height * scale + label_h), (28, 28, 32))
    draw = ImageDraw.Draw(sheet)
    for index, (label, image) in enumerate(cases):
        left_px = index * baseline.width * scale
        draw.text((left_px + 4, 4), label, fill=(245, 245, 245), font=ImageFont.load_default())
        tile = image.convert("RGB").resize((baseline.width * scale, baseline.height * scale), Image.Resampling.NEAREST)
        sheet.paste(tile, (left_px, label_h))
    output_path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output_path)
    return metrics


def texture_frequency_suite_study(image_path: Path, crop_box: tuple[int, int, int, int],
                                  output_path: Path, seed: int = 23) -> dict[str, float]:
    """Describe full-plane spatial-frequency and orientation changes across presets."""
    import cv2

    with Image.open(image_path) as opened:
        source = opened.convert("RGB")
    x, y, width, height = crop_box
    if x < 0 or y < 0 or width <= 0 or height <= 0 or x + width > source.width or y + height > source.height:
        raise ValueError(f"crop box {crop_box} is outside {source.width}x{source.height} input")
    window = np.hanning(height)[:, None] * np.hanning(width)[None, :]
    fy = np.fft.fftfreq(height)[:, None]
    fx = np.fft.rfftfreq(width)[None, :]
    radial_frequency = np.sqrt(fy * fy + fx * fx)
    one_sided_weights = np.full((1, fx.shape[1]), 2.0, dtype=np.float64)
    one_sided_weights[0, 0] = 1.0
    if width % 2 == 0:
        one_sided_weights[0, -1] = 1.0
    theta = np.arctan2(fy, fx)
    directional_band = (radial_frequency >= 0.08) & (radial_frequency < 0.5)
    bands = (("low", 0.0, 0.08), ("mid", 0.08, 0.20),
             ("fine", 0.20, 0.35), ("very-high", 0.35, 0.500001))
    roi_specs = (("primary", x, y),
                 ("upper-left", 64, 64),
                 ("lower-right", source.width - width - 64, source.height - height - 64))
    for roi_name, roi_x, roi_y in roi_specs:
        if roi_x < 0 or roi_y < 0 or roi_x + width > source.width or roi_y + height > source.height:
            raise ValueError(f"secondary ROI {roi_name} does not fit {source.width}x{source.height}")
    metrics: dict[str, float] = {"crop width": float(width), "crop height": float(height),
                                 "seed": float(seed)}
    montage_scale = max(1, min(2, 512 // max(width, height)))
    header_height = 24
    roi_cell_height = height * montage_scale + header_height
    montage = Image.new("RGB", (5 * width * montage_scale, 3 * roi_cell_height), (28, 28, 32))
    draw = ImageDraw.Draw(montage)
    for roi_index, (roi_name, roi_x, roi_y) in enumerate(roi_specs):
        crop = (roi_x, roi_y, roi_x + width, roi_y + height)
        metrics[f"{roi_name} crop x"] = float(roi_x)
        metrics[f"{roi_name} crop y"] = float(roi_y)
        baseline_image = source.crop(crop)
        cases: list[tuple[str, Image.Image]] = [("baseline", baseline_image)]
        for mode in PRESETS:
            no_grain_settings = PRESETS[mode].copy()
            no_grain_settings["grain"] = 0.0
            no_grain = apply_retro(source, **no_grain_settings, seed=seed).crop(crop)
            cases.append((f"{mode} grain-off", no_grain))
            actual = retro_suite(source, mode, seed).crop(crop)
            cases.append((f"{mode} actual", actual))
            panel_index = 1 + tuple(PRESETS).index(mode)
            left = panel_index * width * montage_scale
            top = roi_index * roi_cell_height
            draw.text((left + 4, top + 4), f"{roi_name} | {mode}", fill=(245, 245, 245), font=ImageFont.load_default())
            tile = actual.convert("RGB").resize((width * montage_scale, height * montage_scale), Image.Resampling.NEAREST)
            montage.paste(tile, (left, top + header_height))
        draw.text((4, roi_index * roi_cell_height + 4), f"{roi_name} | baseline ({roi_x},{roi_y})", fill=(245, 245, 245), font=ImageFont.load_default())
        baseline_tile = baseline_image.resize((width * montage_scale, height * montage_scale), Image.Resampling.NEAREST)
        montage.paste(baseline_tile, (0, roi_index * roi_cell_height + header_height))

        baseline_power: np.ndarray | None = None
        baseline_axis_angle: float | None = None
        baseline_gradient_threshold: float | None = None
        baseline_luminance: np.ndarray | None = None
        baseline_chroma: np.ndarray | None = None
        for label, image in cases:
            prefix = f"{roi_name} {label}"
            rgb = np.asarray(image.convert("RGB"), dtype=np.float32) / 255.0
            linear = srgb_to_linear(rgb)
            luminance = np.sum(linear * np.array((0.2126, 0.7152, 0.0722), dtype=np.float32), axis=2)
            chroma = np.stack((linear[:, :, 0] - linear[:, :, 1],
                               linear[:, :, 2] - 0.5 * (linear[:, :, 0] + linear[:, :, 1])), axis=2)
            centered = luminance - float(np.mean(luminance))
            spectrum = np.fft.rfft2(centered * window)
            power = np.abs(spectrum) ** 2 * one_sided_weights
            chroma_power = np.zeros_like(power)
            for channel in range(2):
                chroma_spectrum = np.fft.rfft2((chroma[:, :, channel] - float(np.mean(chroma[:, :, channel]))) * window)
                chroma_power += np.abs(chroma_spectrum) ** 2 * one_sided_weights
            total_power = float(np.sum(power))
            total_chroma_power = float(np.sum(chroma_power))
            metrics[f"{prefix} linear luminance variance"] = float(np.var(luminance))
            grad_y, grad_x = np.gradient(luminance)
            gradient_energy = grad_y ** 2 + grad_x ** 2
            gradient_magnitude = np.sqrt(gradient_energy)
            metrics[f"{prefix} gradient energy"] = float(np.mean(gradient_energy))
            metrics[f"{prefix} windowed full-plane spectral power sum"] = total_power
            chroma_high = (radial_frequency >= 0.20) & (radial_frequency < 0.500001)
            metrics[f"{prefix} linear opponent chroma RMS"] = float(np.sqrt(np.mean(chroma ** 2)))
            metrics[f"{prefix} chroma high-band power fraction"] = float(
                np.sum(chroma_power[chroma_high]) / max(total_chroma_power, 1e-20)
            )
            metrics[f"{prefix} chroma directional anisotropy"] = float(
                np.abs(np.sum(chroma_power * np.exp(2j * theta) * directional_band) /
                       max(float(np.sum(chroma_power * directional_band)), 1e-20))
            )
            for band_name, minimum, maximum in bands:
                mask = (radial_frequency >= minimum) & (radial_frequency < maximum)
                fraction = float(np.sum(power[mask]) / max(total_power, 1e-20))
                metrics[f"{prefix} {band_name} band power fraction"] = fraction
            directional_power = power * directional_band
            directional_total = float(np.sum(directional_power))
            directional_moment = np.sum(directional_power * np.exp(2j * theta)) / max(directional_total, 1e-20)
            metrics[f"{prefix} directional anisotropy"] = float(np.abs(directional_moment))
            metrics[f"{prefix} dominant frequency-axis degrees"] = float((np.degrees(np.angle(directional_moment) * 0.5) + 180.0) % 180.0)
            if label == "baseline":
                baseline_power = power
                baseline_axis_angle = float(np.angle(directional_moment) * 0.5)
                baseline_gradient_threshold = float(np.quantile(gradient_magnitude, 0.90))
                baseline_luminance = luminance.copy()
                baseline_chroma = chroma.copy()
            else:
                assert baseline_power is not None
                assert baseline_luminance is not None and baseline_chroma is not None
                metrics[f"{prefix} normalized spectrum L1 vs baseline"] = float(
                    np.sum(np.abs(power / max(total_power, 1e-20) - baseline_power / max(float(np.sum(baseline_power)), 1e-20)))
                )
                chroma_delta = chroma - baseline_chroma
                luminance_delta = np.abs(luminance - baseline_luminance)
                metrics[f"{prefix} baseline linear luminance delta MAE"] = float(np.mean(luminance_delta))
                metrics[f"{prefix} baseline linear luminance delta P95"] = float(np.percentile(luminance_delta, 95))
                metrics[f"{prefix} baseline opponent chroma delta RMS"] = float(np.sqrt(np.mean(chroma_delta ** 2)))
                metrics[f"{prefix} baseline opponent chroma delta P95"] = float(np.percentile(np.sqrt(np.sum(chroma_delta ** 2, axis=2)), 95))
            assert baseline_axis_angle is not None and baseline_gradient_threshold is not None
            gradient_angle = np.arctan2(grad_y, grad_x)
            angle_delta = (gradient_angle - baseline_axis_angle + np.pi / 2.0) % np.pi - np.pi / 2.0
            aligned_edges = np.abs(angle_delta) <= np.pi / 8.0
            metrics[f"{prefix} baseline-axis gradient energy fraction"] = float(
                np.sum(gradient_energy[aligned_edges]) / max(float(np.sum(gradient_energy)), 1e-20)
            )
            continuity_mask = aligned_edges & (gradient_magnitude >= baseline_gradient_threshold)
            run_lengths: list[int] = []
            for column in range(width):
                transitions = np.diff(np.pad(continuity_mask[:, column].astype(np.int8), (1, 1)))
                starts = np.flatnonzero(transitions == 1)
                ends = np.flatnonzero(transitions == -1)
                run_lengths.extend((ends - starts).tolist())
            runs = np.asarray(run_lengths, dtype=np.float32)
            metrics[f"{prefix} baseline-axis edge mask pixel fraction"] = float(np.mean(continuity_mask))
            metrics[f"{prefix} column run count"] = float(runs.size)
            metrics[f"{prefix} column run mean length"] = float(np.mean(runs)) if runs.size else 0.0
            metrics[f"{prefix} column run p90 length"] = float(np.percentile(runs, 90)) if runs.size else 0.0
            metrics[f"{prefix} column-aligned pixels in runs >=8 ratio"] = float(
                np.sum(runs[runs >= 8.0]) / max(float(np.sum(runs)), 1.0)
            )
            # Rotate the thresholded edge mask so the inferred strand axis is
            # vertical before measuring column runs. Keep the original fixed-
            # column metric above as a historical comparator.
            strand_angle_degrees = (float(np.degrees(baseline_axis_angle)) + 90.0) % 180.0
            rotation_degrees = (strand_angle_degrees - 90.0 + 90.0) % 180.0 - 90.0
            metrics[f"{prefix} strand-axis alignment rotation degrees"] = rotation_degrees
            rotation = cv2.getRotationMatrix2D((width * 0.5, height * 0.5), rotation_degrees, 1.0)
            aligned_mask = cv2.warpAffine(continuity_mask.astype(np.uint8), rotation, (width, height),
                                          flags=cv2.INTER_NEAREST, borderMode=cv2.BORDER_CONSTANT)
            aligned_run_lengths: list[int] = []
            for column in range(width):
                transitions = np.diff(np.pad(aligned_mask[:, column].astype(np.int8), (1, 1)))
                starts = np.flatnonzero(transitions == 1)
                ends = np.flatnonzero(transitions == -1)
                aligned_run_lengths.extend((ends - starts).tolist())
            aligned_runs = np.asarray(aligned_run_lengths, dtype=np.float32)
            metrics[f"{prefix} strand-axis column run count"] = float(aligned_runs.size)
            metrics[f"{prefix} strand-axis column run mean length"] = float(np.mean(aligned_runs)) if aligned_runs.size else 0.0
            metrics[f"{prefix} strand-axis column run p90 length"] = float(np.percentile(aligned_runs, 90)) if aligned_runs.size else 0.0
            metrics[f"{prefix} strand-axis pixels in runs >=8 ratio"] = float(
                np.sum(aligned_runs[aligned_runs >= 8.0]) / max(float(np.sum(aligned_runs)), 1.0)
            )
            # A second ablation uses each output's own P90 gradient threshold.
            # This separates direction/continuity from absolute edge strength,
            # which quantization can lower below the fixed baseline threshold.
            adaptive_threshold = max(float(np.quantile(gradient_magnitude, 0.90)), 1e-8)
            adaptive_mask = aligned_edges & (gradient_magnitude >= adaptive_threshold)
            metrics[f"{prefix} per-image P90 gradient threshold"] = adaptive_threshold
            metrics[f"{prefix} per-image P90 directional edge fraction"] = float(np.mean(adaptive_mask))
            adaptive_aligned = cv2.warpAffine(adaptive_mask.astype(np.uint8), rotation, (width, height),
                                              flags=cv2.INTER_NEAREST, borderMode=cv2.BORDER_CONSTANT)
            adaptive_runs: list[int] = []
            for column in range(width):
                transitions = np.diff(np.pad(adaptive_aligned[:, column].astype(np.int8), (1, 1)))
                starts = np.flatnonzero(transitions == 1)
                ends = np.flatnonzero(transitions == -1)
                adaptive_runs.extend((ends - starts).tolist())
            adaptive_run_array = np.asarray(adaptive_runs, dtype=np.float32)
            metrics[f"{prefix} per-image P90 strand-axis run count"] = float(adaptive_run_array.size)
            metrics[f"{prefix} per-image P90 strand-axis run mean length"] = float(np.mean(adaptive_run_array)) if adaptive_run_array.size else 0.0
            metrics[f"{prefix} per-image P90 strand-axis run p90 length"] = float(np.percentile(adaptive_run_array, 90)) if adaptive_run_array.size else 0.0
            metrics[f"{prefix} per-image P90 strand-axis pixels in runs >=8 ratio"] = float(
                np.sum(adaptive_run_array[adaptive_run_array >= 8.0]) / max(float(np.sum(adaptive_run_array)), 1.0)
            )
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(metrics, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    montage.save(output_path.with_name("texture-frequency-suite-rois.png"))
    return metrics


def video_grain_temporal_study(frame_dir: Path, output_path: Path,
                               manifest_path: Path, seed: int = 23) -> dict[str, float]:
    """Compare temporally fixed and frame-varying suite grain on decoded footage."""
    frame_paths = sorted(frame_dir.glob("frame-*.png"))
    if len(frame_paths) < 3:
        raise ValueError(f"Expected at least 3 extracted frames in {frame_dir}")
    images = []
    for path in frame_paths:
        with Image.open(path) as opened:
            images.append(opened.convert("RGB"))
    source = np.stack([np.asarray(image, dtype=np.float32) / 255.0 for image in images])
    height, width = source.shape[1:3]
    if any(frame.size != images[0].size for frame in images):
        raise ValueError("All temporal study frames must have the same resolution")
    # Crop ROIs were fixed from the inspected source frame, not selected per preset.
    hair_roi = np.zeros((height, width), dtype=bool)
    hair_roi[190:min(445, height), 295:min(510, width)] = True
    green_masks = (source[:, :, :, 1] > source[:, :, :, 0] * 1.25) & (source[:, :, :, 1] > source[:, :, :, 2] * 1.12)
    stable_green = np.all(green_masks, axis=0)
    safe = (
        (source[:, :, :, 1] > 0.10) & (source[:, :, :, 1] < 0.995)
        & (source[:, :, :, 0] > 0.02) & (source[:, :, :, 0] < 0.96)
        & (source[:, :, :, 2] > 0.02) & (source[:, :, :, 2] < 0.96)
    )
    stable_green &= np.all(safe, axis=0)
    metrics: dict[str, float] = {
        "frame count": float(len(images)),
        "width": float(width),
        "height": float(height),
        "mean adjacent source frame RGB MAE": float(np.mean(np.abs(np.diff(source, axis=0)))),
        "stable green pixel ratio": float(np.mean(stable_green)),
    }
    cases: list[tuple[str, Image.Image]] = []
    # Evaluate coordinate policy on the raw grain field. The suite presets quantize
    # before adding grain; their output-minus-grain=0 residual is mostly zero due
    # to 8-bit quantization plateaus and cannot serve as a valid motion metric.
    grain_rng = np.random.default_rng(seed)
    raw_grain = grain_rng.normal(0.0, 0.025, (height, width, 1)).astype(np.float32)
    raw_grain = np.repeat(raw_grain, 3, axis=2)
    raw_base = source.copy()
    raw_content_grain = np.empty_like(raw_base)
    raw_content_grain[0] = raw_grain
    for index, (map_x, map_y) in enumerate(backward_maps, start=1):
        warped = cv2.remap(raw_content_grain[index - 1], map_x, map_y, cv2.INTER_LINEAR,
                           borderMode=cv2.BORDER_REFLECT_101)
        target_sigma = float(np.std(raw_grain))
        warped_sigma = float(np.std(warped))
        if warped_sigma > 1e-8:
            warped *= target_sigma / warped_sigma
        raw_content_grain[index] = warped
    for policy, residual in (("screen-fixed", np.broadcast_to(raw_grain, raw_base.shape)),
                             ("content-locked", raw_content_grain)):
        rendered = np.clip(raw_base + residual, 0.0, 1.0)
        actual_residual = rendered - raw_base
        screen_deltas = np.abs(np.diff(actual_residual, axis=0))
        for roi_name, roi_mask in (("hair ROI", roi_hair), ("stable green", roi_green)):
            if not np.any(roi_mask):
                continue
            metrics[f"raw {policy} screen-space adjacent grain MAE {roi_name}"] = float(
                np.mean(screen_deltas[:, roi_mask, :])
            )
            aligned_deltas = []
            aligned_counts = []
            for frame_index, (map_x, map_y) in enumerate(backward_maps):
                previous_aligned = cv2.remap(actual_residual[frame_index], map_x, map_y,
                                             cv2.INTER_LINEAR, borderMode=cv2.BORDER_CONSTANT)
                valid = flow_valid_masks[frame_index] & roi_mask
                safe = np.all((raw_base[frame_index + 1] > 0.04) & (raw_base[frame_index + 1] < 0.96), axis=2)
                valid &= safe
                if np.any(valid):
                    aligned_deltas.append(np.mean(np.abs(actual_residual[frame_index + 1][valid] - previous_aligned[valid])))
                    aligned_counts.append(int(np.count_nonzero(valid)))
            metrics[f"raw {policy} motion-aligned grain MAE {roi_name}"] = float(np.mean(aligned_deltas)) if aligned_deltas else 0.0
            metrics[f"raw {policy} motion-aligned valid pixels {roi_name}"] = float(np.mean(aligned_counts)) if aligned_counts else 0.0
        metrics[f"raw {policy} mean grain residual sigma"] = float(np.std(actual_residual))
    detail_box = (295, 190, min(510, width), min(445, height))
    first_index, last_index = 0, len(images) - 1
    for mode, settings in PRESETS.items():
        base_frames = []
        fixed_residual = []
        animated_residual = []
        fixed_outputs = []
        animated_outputs = []
        for index, image in enumerate(images):
            base = apply_retro(image, **{**settings, "grain": 0.0}, seed=seed)
            fixed = apply_retro(image, **settings, seed=seed)
            animated = apply_retro(image, **settings, seed=seed + index + 1)
            base_rgb = np.asarray(base, dtype=np.float32) / 255.0
            fixed_rgb = np.asarray(fixed, dtype=np.float32) / 255.0
            animated_rgb = np.asarray(animated, dtype=np.float32) / 255.0
            base_frames.append(base_rgb)
            fixed_residual.append(fixed_rgb - base_rgb)
            animated_residual.append(animated_rgb - base_rgb)
            fixed_outputs.append(fixed)
            animated_outputs.append(animated)
        fixed_residual_array = np.stack(fixed_residual)
        animated_residual_array = np.stack(animated_residual)
        base_array = np.stack(base_frames)
        for label, residual in (("fixed", fixed_residual_array), ("frame-varying", animated_residual_array)):
            adjacent = np.abs(np.diff(residual, axis=0))
            for roi_name, mask in (("whole", np.ones((height, width), dtype=bool)),
                                   ("stable green", stable_green), ("hair ROI", hair_roi)):
                if not np.any(mask):
                    continue
                metrics[f"{mode} {label} adjacent grain residual MAE {roi_name}"] = float(
                    np.mean(adjacent[:, mask, :])
                )
        metrics[f"{mode} fixed grain sigma"] = float(np.std(fixed_residual_array))
        metrics[f"{mode} frame-varying grain sigma"] = float(np.std(animated_residual_array))
        metrics[f"{mode} base adjacent RGB MAE stable green"] = float(
            np.mean(np.abs(np.diff(base_array, axis=0)[:, stable_green, :]))
        ) if np.any(stable_green) else 0.0
        for idx, frame_label in ((first_index, "first"), (last_index, "last")):
            for method, outputs in (("fixed", fixed_outputs), ("frame-varying", animated_outputs)):
                cases.append((f"{mode} {frame_label} {method}", outputs[idx].crop(detail_box)))

    tile_w, tile_h, label_h = 220, 255, 22
    columns = 4
    rows = (len(cases) + columns - 1) // columns
    sheet = Image.new("RGB", (columns * tile_w, rows * (tile_h + label_h)), (28, 28, 32))
    draw = ImageDraw.Draw(sheet)
    for index, (label, image) in enumerate(cases):
        row, column = divmod(index, columns)
        left, top = column * tile_w, row * (tile_h + label_h)
        draw.text((left + 4, top + 4), label, fill=(245, 245, 245), font=ImageFont.load_default())
        image.thumbnail((tile_w, tile_h), Image.Resampling.NEAREST)
        sheet.paste(image, (left, top + label_h))
    output_path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output_path)
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps({
        "frame_paths": [str(path) for path in frame_paths],
        "frame_sha256": {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in frame_paths},
        "frame_size": [width, height],
        "seed": seed,
        "seed_policy": {"fixed": seed, "frame-varying": "seed + frame_index + 1"},
        "frame_interval_seconds": 0.1,
        "roi_source_pixels": {"hair_roi": [295, 190, min(510, width), min(445, height)]},
        "grain_method": "actual apply_retro presets; compare same frame with grain=0 to isolate added grain residual",
        "metrics": metrics,
    }, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return metrics


def video_grain_space_study(frame_dir: Path, output_path: Path,
                            manifest_path: Path, seed: int = 23) -> dict[str, float]:
    """Compare screen-fixed and optical-flow-advected grain with constant seed."""
    import cv2

    frame_paths = sorted(frame_dir.glob("frame-*.png"))
    if len(frame_paths) < 3:
        raise ValueError(f"Expected at least 3 extracted frames in {frame_dir}")
    images = []
    for path in frame_paths:
        with Image.open(path) as opened:
            images.append(opened.convert("RGB"))
    source = np.stack([np.asarray(image, dtype=np.float32) / 255.0 for image in images])
    frame_count, height, width, _ = source.shape
    gray = [cv2.cvtColor(np.uint8(np.clip(frame * 255.0 + 0.5, 0, 255)), cv2.COLOR_RGB2GRAY) for frame in source]
    yy, xx = np.mgrid[0:height, 0:width].astype(np.float32)
    roi_hair = np.zeros((height, width), dtype=bool)
    roi_hair[190:min(445, height), 295:min(510, width)] = True
    green_masks = (source[:, :, :, 1] > source[:, :, :, 0] * 1.25) & (source[:, :, :, 1] > source[:, :, :, 2] * 1.12)
    roi_green = np.all(green_masks, axis=0)
    roi_green &= np.all(
        (source[:, :, :, 1] < 0.995)
        & (source[:, :, :, 0] > 0.02) & (source[:, :, :, 0] < 0.96)
        & (source[:, :, :, 2] > 0.02) & (source[:, :, :, 2] < 0.96),
        axis=0,
    )

    backward_maps: list[tuple[np.ndarray, np.ndarray]] = []
    flow_valid_masks: list[np.ndarray] = []
    flow_magnitudes: list[np.ndarray] = []
    for index in range(1, frame_count):
        backward = cv2.calcOpticalFlowFarneback(gray[index], gray[index - 1], None, 0.5, 3, 21, 3, 5, 1.2, 0)
        forward = cv2.calcOpticalFlowFarneback(gray[index - 1], gray[index], None, 0.5, 3, 21, 3, 5, 1.2, 0)
        map_x = xx + backward[:, :, 0]
        map_y = yy + backward[:, :, 1]
        forward_at_previous = cv2.remap(forward, map_x, map_y, cv2.INTER_LINEAR, borderMode=cv2.BORDER_CONSTANT)
        consistency = np.linalg.norm(backward + forward_at_previous, axis=2)
        inside = (map_x >= 1) & (map_x < width - 1) & (map_y >= 1) & (map_y < height - 1)
        valid = inside & (consistency < 1.5)
        backward_maps.append((map_x, map_y))
        flow_valid_masks.append(valid)
        flow_magnitudes.append(np.linalg.norm(backward, axis=2))

    metrics: dict[str, float] = {
        "frame count": float(frame_count),
        "width": float(width),
        "height": float(height),
        "mean flow magnitude hair ROI px": float(np.mean([np.mean(flow[roi_hair]) for flow in flow_magnitudes])),
        "mean flow magnitude green ROI px": float(np.mean([np.mean(flow[roi_green]) for flow in flow_magnitudes])) if np.any(roi_green) else 0.0,
        "mean forward-backward valid ratio hair ROI": float(np.mean([np.mean(mask[roi_hair]) for mask in flow_valid_masks])),
        "mean forward-backward valid ratio green ROI": float(np.mean([np.mean(mask[roi_green]) for mask in flow_valid_masks])) if np.any(roi_green) else 0.0,
        "stable green pixel ratio": float(np.mean(roi_green)),
    }
    cases: list[tuple[str, Image.Image]] = []
    # Preset grain is added after quantization. At 8-bit output, subtracting the
    # grain=0 preset often produces a zero residual, so compare policies using a
    # separate raw diagnostic field and retain presets as visual context only.
    grain_rng = np.random.default_rng(seed)
    raw_grain = np.repeat(grain_rng.normal(0.0, 0.025, (height, width, 1)).astype(np.float32), 3, axis=2)
    raw_base = source.copy()
    raw_content_grain = np.empty_like(raw_base)
    raw_content_grain[0] = raw_grain
    for index, (map_x, map_y) in enumerate(backward_maps, start=1):
        warped = cv2.remap(raw_content_grain[index - 1], map_x, map_y, cv2.INTER_LINEAR,
                           borderMode=cv2.BORDER_REFLECT_101)
        sigma = float(np.std(warped))
        if sigma > 1e-8:
            warped *= float(np.std(raw_grain)) / sigma
        raw_content_grain[index] = warped
    for policy, residual in (("screen-fixed", np.broadcast_to(raw_grain, raw_base.shape)),
                             ("content-locked", raw_content_grain)):
        actual = np.clip(raw_base + residual, 0.0, 1.0) - raw_base
        screen_deltas = np.abs(np.diff(actual, axis=0))
        for roi_name, roi_mask in (("hair ROI", roi_hair), ("stable green", roi_green)):
            if not np.any(roi_mask):
                continue
            metrics[f"raw {policy} screen-space adjacent grain MAE {roi_name}"] = float(np.mean(screen_deltas[:, roi_mask, :]))
            aligned_deltas = []
            aligned_counts = []
            for frame_index, (map_x, map_y) in enumerate(backward_maps):
                previous = cv2.remap(actual[frame_index], map_x, map_y, cv2.INTER_LINEAR,
                                     borderMode=cv2.BORDER_CONSTANT)
                valid = flow_valid_masks[frame_index] & roi_mask
                valid &= np.all((raw_base[frame_index + 1] > 0.04) & (raw_base[frame_index + 1] < 0.96), axis=2)
                if np.any(valid):
                    aligned_deltas.append(float(np.mean(np.abs(actual[frame_index + 1][valid] - previous[valid]))))
                    aligned_counts.append(int(np.count_nonzero(valid)))
            metrics[f"raw {policy} motion-aligned grain MAE {roi_name}"] = float(np.mean(aligned_deltas)) if aligned_deltas else 0.0
            metrics[f"raw {policy} motion-aligned valid pixels {roi_name}"] = float(np.mean(aligned_counts)) if aligned_counts else 0.0
        metrics[f"raw {policy} mean grain residual sigma"] = float(np.std(actual))

    for mode, settings in PRESETS.items():
        base_frames = []
        fixed_residual = []
        for image in images:
            base = apply_retro(image, **{**settings, "grain": 0.0}, seed=seed)
            fixed = apply_retro(image, **settings, seed=seed)
            base_frames.append(np.asarray(base, dtype=np.float32) / 255.0)
            fixed_residual.append(np.asarray(fixed, dtype=np.float32) / 255.0 - np.asarray(base, dtype=np.float32) / 255.0)
        base_array = np.stack(base_frames)
        fixed_array = np.stack(fixed_residual)
        content_array = np.empty_like(fixed_array)
        content_array[0] = fixed_array[0]
        for index, (map_x, map_y) in enumerate(backward_maps, start=1):
            warped = cv2.remap(content_array[index - 1], map_x, map_y, cv2.INTER_LINEAR,
                               borderMode=cv2.BORDER_REFLECT_101)
            target_sigma = float(np.std(fixed_array[0]))
            warped_sigma = float(np.std(warped))
            if warped_sigma > 1e-8:
                warped *= target_sigma / warped_sigma
            content_array[index] = warped

        output_arrays = {
            "screen-fixed": np.clip(base_array + fixed_array, 0.0, 1.0),
            "content-locked": np.clip(base_array + content_array, 0.0, 1.0),
        }
        residual_arrays = {
            "screen-fixed": output_arrays["screen-fixed"] - base_array,
            "content-locked": output_arrays["content-locked"] - base_array,
        }
        for policy, residual in residual_arrays.items():
            screen_deltas = np.abs(np.diff(residual, axis=0))
            for roi_name, roi_mask in (("hair ROI", roi_hair), ("stable green", roi_green)):
                if not np.any(roi_mask):
                    continue
                metrics[f"{mode} {policy} screen-space adjacent grain MAE {roi_name}"] = float(
                    np.mean(screen_deltas[:, roi_mask, :])
                )
                aligned_deltas = []
                aligned_counts = []
                for frame_index, (map_x, map_y) in enumerate(backward_maps):
                    previous_aligned = cv2.remap(residual[frame_index], map_x, map_y,
                                                 cv2.INTER_LINEAR, borderMode=cv2.BORDER_CONSTANT)
                    valid = flow_valid_masks[frame_index] & roi_mask
                    safe = np.all((base_array[frame_index + 1] > 0.04) & (base_array[frame_index + 1] < 0.96), axis=2)
                    valid &= safe
                    if np.any(valid):
                        aligned_deltas.append(np.mean(np.abs(residual[frame_index + 1][valid] - previous_aligned[valid])))
                        aligned_counts.append(int(np.count_nonzero(valid)))
                metrics[f"{mode} {policy} motion-aligned grain MAE {roi_name}"] = float(np.mean(aligned_deltas)) if aligned_deltas else 0.0
                metrics[f"{mode} {policy} motion-aligned valid pixels {roi_name}"] = float(np.mean(aligned_counts)) if aligned_counts else 0.0
            metrics[f"{mode} {policy} mean grain residual sigma"] = float(np.std(residual))

        crop_box = (295, 190, min(510, width), min(445, height))
        for frame_index, label in ((0, "first"), (frame_count - 1, "last")):
            for policy in ("screen-fixed", "content-locked"):
                rendered = Image.fromarray(np.uint8(np.clip(output_arrays[policy][frame_index] * 255.0 + 0.5, 0, 255)), "RGB")
                cases.append((f"{mode} {label} {policy}", rendered.crop(crop_box)))

    columns, tile_w, tile_h, label_h = 4, 220, 255, 22
    rows = (len(cases) + columns - 1) // columns
    sheet = Image.new("RGB", (columns * tile_w, rows * (tile_h + label_h)), (28, 28, 32))
    draw = ImageDraw.Draw(sheet)
    for index, (label, image) in enumerate(cases):
        row, column = divmod(index, columns)
        left, top = column * tile_w, row * (tile_h + label_h)
        draw.text((left + 4, top + 4), label, fill=(245, 245, 245), font=ImageFont.load_default())
        image.thumbnail((tile_w, tile_h), Image.Resampling.NEAREST)
        sheet.paste(image, (left, top + label_h))
    output_path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output_path)
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps({
        "frame_paths": [str(path) for path in frame_paths],
        "frame_sha256": {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in frame_paths},
        "frame_size": [width, height],
        "seed": seed,
        "flow": "OpenCV Farneback bidirectional flow, current-to-previous remap; forward-backward confidence threshold 1.5px",
        "content_lock": "advect a fixed-seed grayscale Gaussian grain field with backward flow, bilinear remap, normalize global residual sigma to first frame",
        "coordinate_policy_metric": "raw diagnostic grain added to source before clipping; separated from preset output because presets add grain after quantization and 8-bit grain=0 subtraction often yields empty residual",
        "roi_source_pixels": {"hair": [295, 190, min(510, width), min(445, height)], "green": "green key threshold intersection across all frames with clipping-safe range"},
        "metrics": metrics,
    }, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return metrics


def nearest_palette(rgb: np.ndarray, palette: np.ndarray, matrix: np.ndarray | None = None,
                    strength: float = 0.0, domain: str = "srgb") -> np.ndarray:
    height, width, _ = rgb.shape
    if domain == "linear":
        sample = srgb_to_linear(rgb)
        palette_space = srgb_to_linear(palette)
    else:
        sample = rgb
        palette_space = palette
    if matrix is not None and strength > 0:
        tiled = np.tile(matrix, (height // matrix.shape[0] + 1, width // matrix.shape[1] + 1))[:height, :width]
        if domain == "linear":
            sample = srgb_to_linear(np.clip(rgb, 0.0, 1.0)) + tiled[:, :, None] * strength / max(len(palette) ** (1 / 3), 1)
            sample = np.clip(sample, 0.0, 1.0)
        else:
            sample = np.clip(rgb + tiled[:, :, None] * strength / max(len(palette) ** (1 / 3), 1), 0, 1)
    # Fixed, small palette: explicit broadcasting keeps this method straightforward to inspect.
    distances = np.sum((sample[:, :, None, :] - palette_space[None, None, :, :]) ** 2, axis=3)
    indices = np.argmin(distances, axis=2)
    return palette[indices]


def palette_domain_study(image: Image.Image, output: Path) -> dict[str, float]:
    rgb = np.asarray(image.convert("RGB"), dtype=np.float32) / 255.0
    source_linear = srgb_to_linear(rgb)
    cases: list[tuple[str, np.ndarray]] = []
    metrics: dict[str, float] = {}
    for palette_name, palette in PALETTES.items():
        for domain in ("srgb", "linear"):
            baseline = nearest_palette(rgb, palette, domain=domain)
            baseline_key = f"{palette_name}/{domain}/0.0"
            for strength in (0.0, 0.5, 1.0):
                result = baseline if strength == 0.0 else nearest_palette(
                    rgb, palette, BAYER_4, strength, domain,
                )
                key = f"{palette_name}/{domain}/{strength:.1f}"
                rmse = float(np.mean((srgb_to_linear(result) - source_linear) ** 2) ** 0.5)
                changed = float(np.mean(np.any(result != baseline, axis=2)))
                palette_colors = np.unique(np.uint8(np.clip(result * 255.0 + 0.5, 0, 255)).reshape(-1, 3), axis=0).shape[0]
                metrics[f"{key} linear RGB RMSE"] = rmse
                metrics[f"{key} changed pixels vs {baseline_key}"] = changed
                metrics[f"{key} palette colors used"] = float(palette_colors)
                cases.append((f"{palette_name} · {domain} · dither {strength:.1f} · RMSE {rmse:.4f}", result))

    thumb_w = 256
    thumb_h = max(1, round(image.height * thumb_w / image.width))
    label_h = 24
    columns = 3
    rows = (len(cases) + columns - 1) // columns
    sheet = Image.new("RGB", (columns * thumb_w, rows * (thumb_h + label_h)), (28, 28, 32))
    draw = ImageDraw.Draw(sheet)
    for index, (label, result) in enumerate(cases):
        left = (index % columns) * thumb_w
        top = (index // columns) * (thumb_h + label_h)
        draw.text((left + 4, top + 5), label, fill=(245, 245, 245), font=ImageFont.load_default())
        thumb = Image.fromarray(np.uint8(np.clip(result * 255.0, 0, 255)), "RGB").resize(
            (thumb_w, thumb_h), Image.Resampling.NEAREST,
        )
        sheet.paste(thumb, (left, top + label_h))
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    return metrics


def channel_offset(rgb: np.ndarray, amount: float) -> np.ndarray:
    if amount == 0:
        return rgb
    result = rgb.copy()
    result[:, :, 0] = sample_shifted_channel(rgb[:, :, 0], amount, "bilinear", "clamp", "srgb")
    result[:, :, 2] = sample_shifted_channel(rgb[:, :, 2], -amount, "bilinear", "clamp", "srgb")
    return result


def apply_retro(image: Image.Image, *, colors: int, scale: float, dither: float,
                scanlines: float, aberration: float, grain: float, seed: int,
                alpha_resize: str = "straight") -> Image.Image:
    rgba = image.convert("RGBA")
    alpha = rgba.getchannel("A")
    rgb_image = rgba.convert("RGB")
    width, height = rgb_image.size

    if scale < 1.0:
        small_size = (max(1, round(width * scale)), max(1, round(height * scale)))
        if alpha_resize == "nearest":
            rgb_values = np.asarray(rgb_image, dtype=np.float32)
            known = np.asarray(alpha, dtype=np.uint8) > 0
            unknown = ~known
            best_distance = np.full((height, width), np.inf, dtype=np.float32)
            best_rgb = np.zeros_like(rgb_values)
            for dy in range(-8, 9):
                for dx in range(-8, 9):
                    distance = dx * dx + dy * dy
                    if distance > 64:
                        continue
                    src_y = slice(max(0, -dy), min(height, height - dy))
                    src_x = slice(max(0, -dx), min(width, width - dx))
                    dst_y = slice(max(0, dy), min(height, height + dy))
                    dst_x = slice(max(0, dx), min(width, width + dx))
                    valid = known[src_y, src_x] & unknown[dst_y, dst_x]
                    closer = valid & (distance < best_distance[dst_y, dst_x])
                    target_rgb = best_rgb[dst_y, dst_x]
                    source_rgb = rgb_values[src_y, src_x]
                    target_rgb[closer] = source_rgb[closer]
                    best_rgb[dst_y, dst_x] = target_rgb
                    target_distance = best_distance[dst_y, dst_x]
                    target_distance[closer] = distance
                    best_distance[dst_y, dst_x] = target_distance
            filled = unknown & np.isfinite(best_distance)
            rgb_values[filled] = best_rgb[filled]
            rgb_image = Image.fromarray(np.uint8(np.clip(rgb_values + 0.5, 0, 255)), "RGB")
            rgb_image = rgb_image.resize(small_size, Image.Resampling.BOX).resize((width, height), Image.Resampling.NEAREST)
        elif alpha_resize == "dilated":
            rgb_values = np.asarray(rgb_image, dtype=np.float32)
            known = np.asarray(alpha, dtype=np.uint8) > 0
            for _ in range(8):
                sums = np.zeros_like(rgb_values)
                counts = np.zeros((height, width), dtype=np.float32)
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        if dx == 0 and dy == 0:
                            continue
                        src_y = slice(max(0, -dy), min(height, height - dy))
                        src_x = slice(max(0, -dx), min(width, width - dx))
                        dst_y = slice(max(0, dy), min(height, height + dy))
                        dst_x = slice(max(0, dx), min(width, width + dx))
                        valid = known[src_y, src_x]
                        sums[dst_y, dst_x] += rgb_values[src_y, src_x] * valid[:, :, None]
                        counts[dst_y, dst_x] += valid
                fill = (~known) & (counts > 0)
                if not np.any(fill):
                    break
                rgb_values[fill] = sums[fill] / counts[fill, None]
                known[fill] = True
            rgb_image = Image.fromarray(np.uint8(np.clip(rgb_values + 0.5, 0, 255)), "RGB")
            rgb_image = rgb_image.resize(small_size, Image.Resampling.BOX).resize((width, height), Image.Resampling.NEAREST)
        elif alpha_resize == "premultiplied":
            rgba_array = np.asarray(rgba, dtype=np.float32) / 255.0
            alpha_values = rgba_array[:, :, 3:4]
            premul = np.concatenate((rgba_array[:, :, :3] * alpha_values, alpha_values), axis=2)
            premul_image = Image.fromarray(np.uint8(np.clip(premul * 255.0 + 0.5, 0, 255)), "RGBA")
            reduced = np.asarray(
                premul_image.resize(small_size, Image.Resampling.BOX)
                    .resize((width, height), Image.Resampling.NEAREST),
                dtype=np.float32,
            ) / 255.0
            reduced_alpha = reduced[:, :, 3:4]
            rgb_values = np.divide(
                reduced[:, :, :3], reduced_alpha,
                out=np.zeros_like(reduced[:, :, :3]), where=reduced_alpha > 1e-6,
            )
            rgb_image = Image.fromarray(np.uint8(np.clip(rgb_values * 255.0 + 0.5, 0, 255)), "RGB")
        elif alpha_resize == "straight":
            rgb_image = rgb_image.resize(small_size, Image.Resampling.BOX).resize((width, height), Image.Resampling.NEAREST)
        else:
            raise ValueError(f"unknown alpha resize mode: {alpha_resize}")

    rgb = np.asarray(rgb_image, dtype=np.float32) / 255.0
    rgb = ordered_quantize(rgb, max(2, colors), BAYER_4, min(max(dither, 0.0), 1.0))
    rgb = channel_offset(rgb, aberration)

    if scanlines > 0:
        row_gain = np.where(np.arange(height)[:, None, None] % 2 == 1, 1.0 - min(scanlines, 0.9), 1.0)
        rgb *= row_gain
    if grain > 0:
        rng = np.random.default_rng(seed)
        rgb += rng.normal(0.0, grain, size=(height, width, 1)).astype(np.float32)
    rgb = np.clip(rgb, 0.0, 1.0)

    output = Image.fromarray(np.uint8(rgb * 255.0), "RGB").convert("RGBA")
    output.putalpha(alpha)
    return output


def build_research_sheet(image: Image.Image, output: Path) -> dict[str, float]:
    rgb = np.asarray(image.convert("RGB"), dtype=np.float32) / 255.0
    variants: list[tuple[str, np.ndarray]] = []
    for name, matrix in (("Bayer 4x4", BAYER_4), ("Bayer 8x8", BAYER_8)):
        for strength in (0.0, 0.5, 1.0):
            result = ordered_quantize(rgb, 4, matrix, strength)
            variants.append((f"{name} · {strength:.1f}", result))
    for name, palette in PALETTES.items():
        variants.append((f"{name} · nearest", nearest_palette(rgb, palette)))
        variants.append((f"{name} · Bayer 4x4", nearest_palette(rgb, palette, BAYER_4, 1.0)))

    thumb_w = 320
    thumb_h = max(1, round(image.height * thumb_w / image.width))
    cell_h = thumb_h + 32
    columns = 3
    rows = (len(variants) + columns - 1) // columns
    sheet = Image.new("RGB", (columns * thumb_w, rows * cell_h), (32, 32, 36))
    draw = ImageDraw.Draw(sheet)
    for index, (label, variant) in enumerate(variants):
        x, y = (index % columns) * thumb_w, (index // columns) * cell_h
        thumb = Image.fromarray(np.uint8(np.clip(variant * 255, 0, 255))).resize((thumb_w, thumb_h), Image.Resampling.NEAREST)
        sheet.paste(thumb, (x, y + 24))
        draw.text((x + 6, y + 5), label, fill=(245, 245, 245), font=ImageFont.load_default())
        error = float(np.mean((variant - rgb) ** 2) ** 0.5)
        draw.text((x + 6, y + 24 + thumb_h - 16), f"RGB RMSE {error:.4f}", fill=(255, 255, 255), font=ImageFont.load_default(), stroke_width=2, stroke_fill=(0, 0, 0))
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    return {label: float(np.mean((variant - rgb) ** 2) ** 0.5) for label, variant in variants}


def composite_rgba(rgb: np.ndarray, alpha: np.ndarray, background: tuple[float, float, float]) -> np.ndarray:
    return rgb * alpha[:, :, None] + np.asarray(background, dtype=np.float32)[None, None, :] * (1.0 - alpha[:, :, None])


def make_alpha_fixture() -> Image.Image:
    width, height = 256, 128
    rgba = np.zeros((height, width, 4), dtype=np.uint8)
    # Intentionally saturated hidden RGB under zero alpha exposes edge contamination.
    rgba[:, :, :3] = (255, 0, 255)
    rgba[:, :, 3] = 0
    rgba[:, 16:112, :3] = (240, 40, 20)
    rgba[:, 16:112, 3] = 255
    rgba[:, 112:144, :3] = (255, 180, 20)
    rgba[:, 112:144, 3] = np.tile(np.linspace(0, 255, 32, dtype=np.uint8), (height, 1))
    rgba[32:96, 144:240, :3] = (20, 90, 240)
    rgba[32:96, 144:240, 3] = 180
    return Image.fromarray(rgba, "RGBA")


def make_photo_alpha_fixture(width: int = 384, height: int = 256, seed: int = 21) -> Image.Image:
    """Build a textured foliage cutout with antialiased organic transparency."""
    rng = np.random.default_rng(seed)
    yy, xx = np.mgrid[0:height, 0:width].astype(np.float32)
    x, y = xx / max(width - 1, 1), yy / max(height - 1, 1)
    dx = (x - 0.5) / 0.34
    dy = (y - 0.52) / 0.43
    radius = np.sqrt(dx * dx + dy * dy)
    contour = (
        0.040 * np.sin(31.0 * x + 4.0 * np.sin(13.0 * y))
        + 0.026 * np.sin(47.0 * y + 3.0 * np.sin(17.0 * x))
        + 0.014 * np.sin(83.0 * x + 21.0 * y)
    )
    # Signed distance in approximate pixel units gives a narrow antialiased edge.
    edge_distance = (1.0 - radius + contour) * (0.34 * min(width, height))
    alpha = np.clip(edge_distance + 0.5, 0.0, 1.0)

    broad_noise = rng.normal(0.0, 1.0, (height, width)).astype(np.float32)
    mid_noise = rng.normal(0.0, 1.0, (height, width)).astype(np.float32)
    texture = (
        0.035 * np.sin(38.0 * x + 6.0 * np.sin(11.0 * y))
        + 0.027 * np.sin(59.0 * y + 4.0 * np.sin(19.0 * x))
        + 0.025 * broad_noise + 0.018 * mid_noise
    )
    light = np.clip(0.72 + 0.18 * (1.0 - y) + 0.12 * np.sin(7.0 * x + 4.0 * y), 0.0, 1.0)
    foreground = np.stack((
        0.12 + 0.13 * light + texture * 0.65,
        0.20 + 0.43 * light + texture,
        0.08 + 0.20 * light + texture * 0.55,
    ), axis=2)
    foreground = np.clip(foreground, 0.0, 1.0)
    rgba = np.empty((height, width, 4), dtype=np.uint8)
    rgba[:, :, :3] = (255, 0, 255)  # adversarial hidden RGB outside the cutout
    rgba[:, :, :3] = np.where(
        (alpha[:, :, None] > 0.0), np.uint8(foreground * 255.0), rgba[:, :, :3],
    )
    rgba[:, :, 3] = np.uint8(alpha * 255.0)
    return Image.fromarray(rgba, "RGBA")


def alpha_edge_study(image: Image.Image, output: Path, seed: int = 7) -> dict[str, float]:
    rgba = np.asarray(image.convert("RGBA"), dtype=np.float32) / 255.0
    source_rgb, alpha = rgba[:, :, :3], rgba[:, :, 3]
    h, w, _ = source_rgb.shape

    # Compare filtering hidden RGB directly with filtering premultiplied color.
    filtered_straight = np.asarray(
        Image.fromarray(np.uint8(source_rgb * 255), "RGB").resize((w // 2, h // 2), Image.Resampling.BOX)
        .resize((w, h), Image.Resampling.BILINEAR), dtype=np.float32,
    ) / 255.0
    filtered_alpha = np.asarray(
        Image.fromarray(np.uint8(alpha * 255), "L").resize((w // 2, h // 2), Image.Resampling.BOX)
        .resize((w, h), Image.Resampling.BILINEAR), dtype=np.float32,
    ) / 255.0
    premultiplied = source_rgb * alpha[:, :, None]
    filtered_premultiplied = np.asarray(
        Image.fromarray(np.uint8(np.clip(premultiplied * 255, 0, 255)), "RGB").resize((w // 2, h // 2), Image.Resampling.BOX)
        .resize((w, h), Image.Resampling.BILINEAR), dtype=np.float32,
    ) / 255.0
    unpremultiplied = np.divide(
        filtered_premultiplied, filtered_alpha[:, :, None],
        out=np.zeros_like(filtered_premultiplied), where=filtered_alpha[:, :, None] > (1.0 / 255.0),
    )

    # RGB channel shifts can draw opaque color into transparent pixels unless alpha follows the same mapping.
    shift = 4
    shifted_rgb = source_rgb.copy()
    shifted_rgb[:, :, 0] = np.roll(source_rgb[:, :, 0], shift, axis=1)
    shifted_rgb[:, :, 2] = np.roll(source_rgb[:, :, 2], -shift, axis=1)
    shifted_alpha = np.roll(alpha, shift, axis=1)
    channel_alpha = np.stack((np.roll(alpha, shift, axis=1), alpha, np.roll(alpha, -shift, axis=1)), axis=2)

    rng = np.random.default_rng(seed)
    grain = np.clip(source_rgb + rng.normal(0, 0.1, source_rgb.shape).astype(np.float32), 0, 1)
    variants = [
        ("source", source_rgb, alpha),
        ("scale RGB straight", filtered_straight, filtered_alpha),
        ("scale premul / unpremul", unpremultiplied, filtered_alpha),
        ("RGB shift / original alpha", shifted_rgb, alpha),
        ("RGB shift / shared shifted alpha", shifted_rgb, shifted_alpha),
        ("RGB shift / per-channel alpha", shifted_rgb, channel_alpha),
        ("grain unmasked", grain, alpha),
    ]
    backgrounds = ((0.95, 0.95, 0.95), (0.04, 0.04, 0.04), (0.0, 0.7, 0.2))
    thumb_w, thumb_h, label_h = 320, 160, 26
    sheet = Image.new("RGB", (thumb_w * len(backgrounds), (thumb_h + label_h) * len(variants)), (28, 28, 30))
    draw = ImageDraw.Draw(sheet)
    metrics: dict[str, float] = {}
    for row, (name, rgb, a) in enumerate(variants):
        edge = (alpha > 0.01) & (alpha < 0.99)
        reference = composite_rgba(source_rgb, alpha, (0.95, 0.95, 0.95))
        if np.asarray(a).ndim == 3:
            composite = rgb * a + 0.95 * (1.0 - a)
        else:
            composite = composite_rgba(rgb, a, (0.95, 0.95, 0.95))
        edge_error = float(np.mean(np.abs(composite[edge] - reference[edge]))) if np.any(edge) else 0.0
        metrics[f"{name} white-edge MAE"] = edge_error
        label = f"{name} · white-edge MAE {edge_error:.4f}"
        if name.startswith("scale RGB") or name.startswith("scale premul"):
            scale_edge = (a > 0.05) & (a < 0.95)
            transition = np.zeros((h, w), dtype=bool)
            transition[:, 108:148] = True
            halo = float(np.mean(np.abs(rgb[:, :, 2][scale_edge & transition] - (20.0 / 255.0))))
            metrics[f"{name} orange-edge blue deviation"] = halo
            label += f" · blue halo {halo:.4f}"
        draw.text((5, row * (thumb_h + label_h) + 5), label, fill=(245, 245, 245), font=ImageFont.load_default())
        for col, background in enumerate(backgrounds):
            if np.asarray(a).ndim == 3:
                composite = rgb * a + np.asarray(background, dtype=np.float32)[None, None, :] * (1.0 - a)
            else:
                composite = composite_rgba(rgb, a, background)
            thumb = Image.fromarray(np.uint8(np.clip(composite * 255, 0, 255))).resize((thumb_w, thumb_h), Image.Resampling.NEAREST)
            sheet.paste(thumb, (col * thumb_w, row * (thumb_h + label_h) + label_h))
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    return metrics


def alpha_chromatic_study(image: Image.Image, output: Path, shift_amount: float = 3.5) -> dict[str, float]:
    """Compare hidden-RGB sensitivity when chromatic channels are resampled."""
    rgba = np.asarray(image.convert("RGBA"), dtype=np.float32) / 255.0
    rgb, alpha = rgba[:, :, :3], rgba[:, :, 3]
    height, width, _ = rgb.shape
    yy, xx = np.mgrid[0:height, 0:width].astype(np.float32)
    # A deliberately visible constant shift isolates alpha handling from the
    # radial field used by the production-style chromatic study.
    red_x = xx + shift_amount
    blue_x = xx - shift_amount

    straight = rgb.copy()
    straight[:, :, 0] = sample_coordinates(rgb[:, :, 0], red_x, yy, "bilinear", "srgb")
    straight[:, :, 2] = sample_coordinates(rgb[:, :, 2], blue_x, yy, "bilinear", "srgb")

    # Sanitize hidden color through premultiplication before resampling. Divide
    # by the sampled coverage to recover the shifted straight color, then keep
    # the original alpha so the effect does not move the silhouette.
    premultiplied = rgb * alpha[:, :, None]
    red_alpha = sample_coordinates(alpha, red_x, yy, "bilinear", "srgb")
    blue_alpha = sample_coordinates(alpha, blue_x, yy, "bilinear", "srgb")
    premul_aware = rgb.copy()
    shifted_red = sample_coordinates(premultiplied[:, :, 0], red_x, yy, "bilinear", "srgb")
    shifted_blue = sample_coordinates(premultiplied[:, :, 2], blue_x, yy, "bilinear", "srgb")
    premul_aware[:, :, 0] = np.divide(
        shifted_red, red_alpha, out=np.zeros_like(shifted_red), where=red_alpha > 1e-6,
    )
    premul_aware[:, :, 2] = np.divide(
        shifted_blue, blue_alpha, out=np.zeros_like(shifted_blue), where=blue_alpha > 1e-6,
    )
    premul_aware = np.clip(premul_aware, 0.0, 1.0)

    sanitized = rgb.copy()
    sanitized[alpha <= 0.0] = 0.0
    straight_sanitized = rgb.copy()
    straight_sanitized[:, :, 0] = sample_coordinates(
        sanitized[:, :, 0], red_x, yy, "bilinear", "srgb",
    )
    straight_sanitized[:, :, 2] = sample_coordinates(
        sanitized[:, :, 2], blue_x, yy, "bilinear", "srgb",
    )

    variants = (
        ("straight sampling", straight),
        ("premultiplied sampling", premul_aware),
        ("straight with zero-hidden-RGB", straight_sanitized),
    )
    backgrounds = ((0.95, 0.95, 0.95), (0.04, 0.04, 0.04), (0.0, 0.7, 0.2))
    edge = (alpha > 0.01) & (alpha < 0.99)
    thumb_w, thumb_h, label_h = 320, 160, 28
    sheet = Image.new("RGB", (thumb_w * len(backgrounds), (thumb_h + label_h) * len(variants)), (28, 28, 30))
    draw = ImageDraw.Draw(sheet)
    metrics: dict[str, float] = {}
    for row, (name, result) in enumerate(variants):
        comps = [composite_rgba(result, alpha, bg) for bg in backgrounds]
        reference = [composite_rgba(rgb, alpha, bg) for bg in backgrounds]
        edge_mae = float(np.mean([
            np.mean(np.abs(comp[edge] - ref[edge])) for comp, ref in zip(comps, reference)
        ]))
        metrics[f"{name} edge composite change MAE"] = edge_mae
        label = f"{name} · edge ΔMAE {edge_mae:.5f}"
        draw.text((5, row * (thumb_h + label_h) + 5), label, fill=(245, 245, 245), font=ImageFont.load_default())
        for col, comp in enumerate(comps):
            thumb = Image.fromarray(np.uint8(np.clip(comp * 255, 0, 255))).resize(
                (thumb_w, thumb_h), Image.Resampling.NEAREST,
            )
            sheet.paste(thumb, (col * thumb_w, row * (thumb_h + label_h) + label_h))

    hidden_variant = rgb.copy()
    hidden_variant[alpha <= 0.0] = (0.0, 1.0, 0.0)
    hidden_straight = hidden_variant.copy()
    hidden_straight[:, :, 0] = sample_coordinates(hidden_variant[:, :, 0], red_x, yy, "bilinear", "srgb")
    hidden_straight[:, :, 2] = sample_coordinates(hidden_variant[:, :, 2], blue_x, yy, "bilinear", "srgb")
    hidden_premul = hidden_variant * alpha[:, :, None]
    hidden_premul_result = hidden_variant.copy()
    for channel, sx, sample_alpha in ((0, red_x, red_alpha), (2, blue_x, blue_alpha)):
        channel_value = sample_coordinates(hidden_premul[:, :, channel], sx, yy, "bilinear", "srgb")
        hidden_premul_result[:, :, channel] = np.divide(
            channel_value, sample_alpha, out=np.zeros_like(channel_value), where=sample_alpha > 1e-6,
        )
    hidden_premul_result = np.clip(hidden_premul_result, 0.0, 1.0)
    metrics["straight visible hidden-RGB sensitivity"] = float(np.mean([
        np.mean(np.abs(
            composite_rgba(straight, alpha, background)
            - composite_rgba(hidden_straight, alpha, background)
        )) for background in backgrounds
    ]))
    metrics["premultiplied visible hidden-RGB sensitivity"] = float(np.mean([
        np.mean(np.abs(
            composite_rgba(premul_aware, alpha, background)
            - composite_rgba(hidden_premul_result, alpha, background)
        )) for background in backgrounds
    ]))
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    return metrics


def alpha_photo_sweep_study(output: Path, fixture_path: Path) -> dict[float, dict[str, float]]:
    """Run alpha/chromatic comparisons over several shifts on one organic cutout."""
    image = make_photo_alpha_fixture()
    fixture_path.parent.mkdir(parents=True, exist_ok=True)
    image.save(fixture_path)
    shifts = (0.5, 1.5, 3.5, 7.0)
    per_shift: dict[float, dict[str, float]] = {}
    sheets: list[Image.Image] = []
    output.parent.mkdir(parents=True, exist_ok=True)
    for amount in shifts:
        sheet_path = output.with_name(f"{output.stem}-{amount:g}px.png")
        per_shift[amount] = alpha_chromatic_study(image, sheet_path, amount)
        with Image.open(sheet_path) as sheet:
            sheets.append(sheet.convert("RGB"))
    width = max(sheet.width for sheet in sheets)
    height = sum(sheet.height + 26 for sheet in sheets)
    contact = Image.new("RGB", (width, height), (28, 28, 30))
    draw = ImageDraw.Draw(contact)
    y_offset = 0
    for amount, sheet in zip(shifts, sheets):
        draw.text((5, y_offset + 5), f"Shift amount: {amount:g}px", fill=(245, 245, 245), font=ImageFont.load_default())
        y_offset += 26
        contact.paste(sheet, (0, y_offset))
        y_offset += sheet.height
    contact.save(output)
    for amount in shifts:
        output.with_name(f"{output.stem}-{amount:g}px.png").unlink(missing_ok=True)
    return per_shift


def production_radial_nearest_rgba(
    rgba: np.ndarray, red_amount: float, blue_amount: float,
    center_x: float = 0.5, center_y: float = 0.5,
) -> np.ndarray:
    """Float model of the current radial integer-load/clamp channel mapping."""
    height, width, _ = rgba.shape
    yy, xx = np.mgrid[0:height, 0:width].astype(np.float32)
    dx, dy = xx - center_x * width, yy - center_y * height
    distance = np.sqrt(dx * dx + dy * dy)
    denominator = max(max(width, height) * 0.7, 1.0)
    direction_x = np.divide(dx, distance, out=np.zeros_like(dx), where=distance > 0.0)
    direction_y = np.divide(dy, distance, out=np.zeros_like(dy), where=distance > 0.0)
    red_offset = distance / denominator * np.clip(red_amount, 0.0, 50.0)
    blue_offset = distance / denominator * np.clip(blue_amount, 0.0, 50.0)
    # The C++ adapter adds 0.5, converts to integer, then clamps; the shader
    # uses the corresponding integer Load coordinates and clamp.
    red_x = np.clip(np.trunc(xx + direction_x * red_offset + 0.5).astype(np.int32), 0, width - 1)
    red_y = np.clip(np.trunc(yy + direction_y * red_offset + 0.5).astype(np.int32), 0, height - 1)
    blue_x = np.clip(np.trunc(xx - direction_x * blue_offset + 0.5).astype(np.int32), 0, width - 1)
    blue_y = np.clip(np.trunc(yy - direction_y * blue_offset + 0.5).astype(np.int32), 0, height - 1)
    result = rgba.copy()
    result[:, :, 0] = rgba[red_y, red_x, 0]
    result[:, :, 2] = rgba[blue_y, blue_x, 2]
    return result


def production_chromatic_alpha_study(output: Path, fixture_path: Path) -> dict[float, dict[str, float]]:
    """Measure hidden-RGB leakage under the current nearest radial effect model."""
    fixture = np.asarray(make_photo_alpha_fixture().convert("RGBA"), dtype=np.float32) / 255.0
    fixture_path.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(np.uint8(np.clip(fixture * 255.0, 0, 255)), "RGBA").save(fixture_path)
    hidden_rgb = fixture.copy()
    hidden_rgb[fixture[:, :, 3] == 0.0, :3] = (0.0, 1.0, 0.0)
    sanitized = fixture.copy()
    sanitized[fixture[:, :, 3] <= 1e-6, :3] = 0.0
    sanitized_hidden = hidden_rgb.copy()
    sanitized_hidden[hidden_rgb[:, :, 3] <= 1e-6, :3] = 0.0
    shifts = (2.0, 8.0, 16.0)
    backgrounds = ((0.95, 0.95, 0.95), (0.04, 0.04, 0.04), (0.0, 0.7, 0.2))
    alpha = fixture[:, :, 3]
    edge = (alpha > 0.01) & (alpha < 0.99)
    visible = alpha > 0.0
    rows: list[tuple[float, Image.Image]] = []
    metrics: dict[float, dict[str, float]] = {}
    for amount in shifts:
        result = production_radial_nearest_rgba(fixture, amount, amount)
        hidden_result = production_radial_nearest_rgba(hidden_rgb, amount, amount)
        sanitized_result = production_radial_nearest_rgba(sanitized, amount, amount)
        sanitized_hidden_result = production_radial_nearest_rgba(
            sanitized_hidden, amount, amount,
        )
        channel_delta = np.max(np.abs(result[:, :, (0, 2)] - hidden_result[:, :, (0, 2)]), axis=2)
        affected = visible & (channel_delta > (1.0 / 255.0))
        composite_sensitivity = float(np.mean([
            np.mean(np.abs(
                composite_rgba(result[:, :, :3], alpha, background)
                - composite_rgba(hidden_result[:, :, :3], alpha, background)
            )) for background in backgrounds
        ]))
        metrics[amount] = {
            "visible affected pixel ratio": float(np.count_nonzero(affected) / max(np.count_nonzero(visible), 1)),
            "edge affected pixel ratio": float(np.mean(affected[edge])) if np.any(edge) else 0.0,
            "max shifted R/B normalized delta": float(np.max(channel_delta[visible])) if np.any(visible) else 0.0,
            "visible composite hidden-RGB sensitivity": composite_sensitivity,
            "zero-hidden sanitization visible composite delta": float(np.mean([
                np.mean(np.abs(
                    composite_rgba(result[:, :, :3], alpha, background)
                    - composite_rgba(sanitized_result[:, :, :3], alpha, background)
                )) for background in backgrounds
            ])),
            "after alpha-zero cleanup hidden-RGB sensitivity": float(np.mean([
                np.mean(np.abs(
                    composite_rgba(sanitized_result[:, :, :3], alpha, background)
                    - composite_rgba(sanitized_hidden_result[:, :, :3], alpha, background)
                )) for background in backgrounds
            ])),
        }
        row_height = 180
        panels = []
        for variant in (result, sanitized_result, hidden_result):
            panel = np.concatenate([
                composite_rgba(variant[:, :, :3], alpha, background)
                for background in backgrounds
            ], axis=1)
            panels.append(Image.fromarray(np.uint8(np.clip(panel * 255.0, 0, 255))).resize(
                (960, row_height), Image.Resampling.NEAREST,
            ))
        row = Image.new("RGB", (960, 3 * (row_height + 23)), (28, 28, 30))
        draw = ImageDraw.Draw(row)
        labels = ("production nearest / hidden magenta", "production nearest / hidden RGB zero", "production nearest / hidden green")
        for index, (label, panel) in enumerate(zip(labels, panels)):
            y = index * (row_height + 23)
            draw.text((5, y + 4), label, fill=(245, 245, 245), font=ImageFont.load_default())
            row.paste(panel, (0, y + 23))
        rows.append((amount, row))

    total_height = sum(row.height + 27 for _, row in rows)
    sheet = Image.new("RGB", (960, total_height), (24, 24, 26))
    draw = ImageDraw.Draw(sheet)
    y_offset = 0
    for amount, row in rows:
        draw.text((5, y_offset + 5), f"Production-style radial nearest/clamp · shift {amount:g}px", fill=(245, 245, 245), font=ImageFont.load_default())
        y_offset += 27
        sheet.paste(row, (0, y_offset))
        y_offset += row.height
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    return metrics


def srgb_to_linear(rgb: np.ndarray) -> np.ndarray:
    return np.where(rgb <= 0.04045, rgb / 12.92, ((rgb + 0.055) / 1.055) ** 2.4)


def linear_to_srgb(rgb: np.ndarray) -> np.ndarray:
    rgb = np.clip(rgb, 0.0, 1.0)
    return np.where(rgb <= 0.0031308, rgb * 12.92, 1.055 * np.power(rgb, 1.0 / 2.4) - 0.055)


def map_sample_indices(indices: np.ndarray, width: int, edge: str) -> np.ndarray:
    if edge == "wrap":
        return np.mod(indices, width)
    if edge == "reflect":
        if width <= 1:
            return np.zeros_like(indices)
        period = 2 * (width - 1)
        folded = np.mod(indices, period)
        return np.where(folded < width, folded, period - folded)
    return np.clip(indices, 0, width - 1)


def sample_shifted_channel(channel: np.ndarray, shift: float, interpolation: str,
                           edge: str, domain: str) -> np.ndarray:
    height, width = channel.shape
    values = srgb_to_linear(channel) if domain == "linear" else channel
    source_x = np.broadcast_to(np.arange(width, dtype=np.float32)[None, :] + shift, (height, width))
    if edge == "clamp":
        source_x = np.clip(source_x, 0.0, width - 1.0)
    elif edge == "wrap":
        source_x = np.mod(source_x, width)
    elif width > 1:
        period = 2.0 * (width - 1)
        folded = np.mod(source_x, period)
        source_x = np.where(folded <= width - 1, folded, period - folded)
    if interpolation == "nearest":
        index = map_sample_indices(np.floor(source_x + 0.5).astype(np.int32), width, edge)
        sampled = np.take_along_axis(values, np.broadcast_to(index, values.shape), axis=1)
    else:
        left = np.floor(source_x).astype(np.int32)
        fraction = source_x - left
        right = left + 1
        left = map_sample_indices(left, width, edge)
        right = map_sample_indices(right, width, edge)
        first = np.take_along_axis(values, left, axis=1)
        second = np.take_along_axis(values, right, axis=1)
        sampled = first * (1.0 - fraction) + second * fraction
    return linear_to_srgb(sampled) if domain == "linear" else sampled


def chromatic_shift(rgb: np.ndarray, amount: float, interpolation: str, edge: str,
                    domain: str) -> np.ndarray:
    output = rgb.copy()
    output[:, :, 0] = sample_shifted_channel(rgb[:, :, 0], amount, interpolation, edge, domain)
    output[:, :, 2] = sample_shifted_channel(rgb[:, :, 2], -amount, interpolation, edge, domain)
    return output


def sample_coordinates(channel: np.ndarray, source_x: np.ndarray, source_y: np.ndarray,
                       interpolation: str, domain: str) -> np.ndarray:
    height, width = channel.shape
    values = srgb_to_linear(channel) if domain == "linear" else channel
    source_x = np.clip(source_x, 0.0, width - 1.0)
    source_y = np.clip(source_y, 0.0, height - 1.0)
    if interpolation == "nearest":
        ix = np.minimum(np.floor(source_x + 0.5).astype(np.int32), width - 1)
        iy = np.minimum(np.floor(source_y + 0.5).astype(np.int32), height - 1)
        sampled = values[iy, ix]
    else:
        x0 = np.floor(source_x).astype(np.int32)
        y0 = np.floor(source_y).astype(np.int32)
        x1 = np.minimum(x0 + 1, width - 1)
        y1 = np.minimum(y0 + 1, height - 1)
        fx = source_x - x0
        fy = source_y - y0
        top = values[y0, x0] * (1.0 - fx) + values[y0, x1] * fx
        bottom = values[y1, x0] * (1.0 - fx) + values[y1, x1] * fx
        sampled = top * (1.0 - fy) + bottom * fy
    return linear_to_srgb(sampled) if domain == "linear" else sampled


def radial_chromatic_shift(rgb: np.ndarray, red_amount: float, blue_amount: float,
                           center_x: float, center_y: float, interpolation: str,
                           domain: str) -> np.ndarray:
    height, width, _ = rgb.shape
    yy, xx = np.mgrid[0:height, 0:width].astype(np.float32)
    cx, cy = center_x * width, center_y * height
    dx, dy = xx - cx, yy - cy
    distance = np.sqrt(dx * dx + dy * dy)
    denominator = max(max(width, height) * 0.7, 1.0)
    normalized = distance / denominator
    direction_x = np.divide(dx, distance, out=np.zeros_like(dx), where=distance > 0)
    direction_y = np.divide(dy, distance, out=np.zeros_like(dy), where=distance > 0)
    red_distance = normalized * red_amount
    blue_distance = normalized * blue_amount
    output = rgb.copy()
    output[:, :, 0] = sample_coordinates(
        rgb[:, :, 0], xx + direction_x * red_distance, yy + direction_y * red_distance,
        interpolation, domain,
    )
    output[:, :, 2] = sample_coordinates(
        rgb[:, :, 2], xx - direction_x * blue_distance, yy - direction_y * blue_distance,
        interpolation, domain,
    )
    return output


def radial_offset_stats(width: int, height: int, amount: float,
                         center_x: float = 0.5, center_y: float = 0.5) -> tuple[float, float]:
    yy, xx = np.mgrid[0:height, 0:width].astype(np.float32)
    distance = np.sqrt((xx - center_x * width) ** 2 + (yy - center_y * height) ** 2)
    offset = distance / max(max(width, height) * 0.7, 1.0) * amount
    return float(np.mean(offset)), float(np.max(offset))


def make_radial_fixture() -> Image.Image:
    size = 256
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
    dx, dy = xx - size / 2.0, yy - size / 2.0
    radius = np.sqrt(dx * dx + dy * dy)
    hue = (np.arctan2(dy, dx) % (2.0 * np.pi)) / (2.0 * np.pi) * 255.0
    saturation = np.full_like(hue, 255.0)
    value = np.where((np.floor(radius / 12.0).astype(np.int32) % 2) == 0, 235.0, 150.0)
    hsv = np.stack((hue, saturation, value), axis=2).astype(np.uint8)
    image = Image.fromarray(hsv, "HSV").convert("RGB")
    draw = ImageDraw.Draw(image)
    draw.line((0, size // 2, size - 1, size // 2), fill=(245, 245, 245), width=1)
    draw.line((size // 2, 0, size // 2, size - 1), fill=(245, 245, 245), width=1)
    draw.ellipse((size // 2 - 3, size // 2 - 3, size // 2 + 3, size // 2 + 3), fill=(255, 255, 255))
    return image


def radial_chromatic_study(image: Image.Image, output: Path) -> dict[str, float]:
    source = np.asarray(image.convert("RGB"), dtype=np.float32) / 255.0
    height, width, _ = source.shape
    variants = [
        ("radial nearest / center 50-50 / sRGB", radial_chromatic_shift(source, 8, 8, 0.5, 0.5, "nearest", "srgb")),
        ("radial bilinear / center 50-50 / sRGB", radial_chromatic_shift(source, 8, 8, 0.5, 0.5, "bilinear", "srgb")),
        ("radial bilinear / center 50-50 / linear", radial_chromatic_shift(source, 8, 8, 0.5, 0.5, "bilinear", "linear")),
        ("radial bilinear / center 35-65 / sRGB", radial_chromatic_shift(source, 8, 8, 0.35, 0.65, "bilinear", "srgb")),
        ("directional bilinear / 8px / sRGB", chromatic_shift(source, 8, "bilinear", "clamp", "srgb")),
    ]
    baseline = variants[1][1]
    yy, xx = np.mgrid[0:height, 0:width].astype(np.float32)
    border = (xx < 8) | (xx >= width - 8) | (yy < 8) | (yy >= height - 8)
    metrics: dict[str, float] = {}
    sheet_w, sheet_h, label_h = width, height, 28
    cols, rows = 2, (len(variants) + 1) // 2
    sheet = Image.new("RGB", (sheet_w * cols, (sheet_h + label_h) * rows), (30, 30, 34))
    draw = ImageDraw.Draw(sheet)
    for index, (name, result) in enumerate(variants):
        delta = np.abs(srgb_to_linear(result) - srgb_to_linear(baseline))
        mean_delta = float(np.mean(delta))
        edge_delta = float(np.mean(delta[border]))
        metrics[f"{name} · mean linear delta vs radial bilinear"] = mean_delta
        metrics[f"{name} · edge linear delta vs radial bilinear"] = edge_delta
        x, y = (index % cols) * sheet_w, (index // cols) * (sheet_h + label_h)
        draw.text((x + 5, y + 6), f"{name} · Δ {mean_delta:.4f} · edge Δ {edge_delta:.4f}", fill=(245, 245, 245), font=ImageFont.load_default())
        thumb = Image.fromarray(np.uint8(np.clip(result * 255, 0, 255))).resize((sheet_w, sheet_h), Image.Resampling.NEAREST)
        sheet.paste(thumb, (x, y + label_h))
    for label, center_x, center_y in (("center 50-50", 0.5, 0.5), ("center 35-65", 0.35, 0.65)):
        mean_offset, max_offset = radial_offset_stats(width, height, 8, center_x, center_y)
        metrics[f"{label} · mean radial offset px"] = mean_offset
        metrics[f"{label} · max radial offset px"] = max_offset
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    return metrics


def make_grain_fixture() -> Image.Image:
    width, height = 320, 200
    levels = (0.04, 0.16, 0.32, 0.5, 0.68, 0.84, 0.96)
    image = np.zeros((height, width, 3), dtype=np.float32)
    band_height = height // len(levels)
    for index, level in enumerate(levels):
        y0 = index * band_height
        y1 = height if index == len(levels) - 1 else (index + 1) * band_height
        image[y0:y1] = level
    image[:, 224:272] = np.array((0.82, 0.18, 0.12), dtype=np.float32)
    image[:, 272:] = np.array((0.1, 0.38, 0.86), dtype=np.float32)
    return Image.fromarray(np.uint8(np.clip(image * 255, 0, 255)), "RGB")


def apply_grain_model(rgb: np.ndarray, amount: float, seed: int, model: str,
                      channels: str = "mono", block_size: int = 1) -> tuple[np.ndarray, np.ndarray]:
    height, width, _ = rgb.shape
    rng = np.random.default_rng(seed)
    channel_count = 1 if channels == "mono" else 3
    if block_size > 1:
        low_h = max(1, int(np.ceil(height / block_size)))
        low_w = max(1, int(np.ceil(width / block_size)))
        low = rng.normal(0.0, 1.0, (low_h, low_w, channel_count)).astype(np.float32)
        low_encoded = np.uint8(np.clip((low + 3.0) / 6.0 * 255, 0, 255))
        noise_image = Image.fromarray(low_encoded, "RGB") if channel_count == 3 else Image.fromarray(low_encoded[:, :, 0], "L")
        noise_image = noise_image.resize((width, height), Image.Resampling.BILINEAR)
        noise = np.asarray(noise_image, dtype=np.float32)
        if channel_count == 1:
            noise = noise[:, :, None] if noise.ndim == 3 else noise[:, :, None]
        noise = (noise / 255.0 * 6.0 - 3.0).astype(np.float32)
        noise -= np.mean(noise, axis=(0, 1), keepdims=True)
        noise /= np.maximum(np.std(noise, axis=(0, 1), keepdims=True), 1.0e-6)
    else:
        noise = rng.normal(0.0, 1.0, (height, width, channel_count)).astype(np.float32)

    luminance = np.sum(rgb * np.array((0.299, 0.587, 0.114), dtype=np.float32), axis=2)
    if model == "constant":
        scale = np.full((height, width), amount, dtype=np.float32)
    elif model == "midtone":
        scale = amount * np.clip(4.0 * luminance * (1.0 - luminance), 0.0, 1.0)
    elif model == "core-filmgrain":
        scale = (amount * amount) * np.clip(4.0 * luminance * (1.0 - luminance), 0.0, 1.0)
    else:
        raise ValueError(f"Unknown grain model: {model}")
    if channel_count == 1:
        perturbation = noise * scale[:, :, None]
    else:
        perturbation = noise * scale[:, :, None]
    result = np.clip(rgb + perturbation, 0.0, 1.0)
    return result, perturbation


def grain_study(image: Image.Image, output: Path, amount: float = 0.08) -> dict[str, float]:
    source = np.asarray(image.convert("RGB"), dtype=np.float32) / 255.0
    variants = [
        ("constant mono / white", "constant", "mono", 1),
        ("midtone weighted mono / white", "midtone", "mono", 1),
        ("Core FilmGrain equation / mono", "core-filmgrain", "mono", 1),
        ("midtone weighted RGB / white", "midtone", "rgb", 1),
        ("midtone weighted mono / coarse 4px", "midtone", "mono", 4),
        ("midtone weighted mono / coarse 8px", "midtone", "mono", 8),
    ]
    generated: list[tuple[str, np.ndarray, np.ndarray]] = []
    for index, (name, model, channels, block) in enumerate(variants):
        result, perturbation = apply_grain_model(source, amount, 700 + index, model, channels, block)
        generated.append((name, result, perturbation))

    # Temporal behavior for static and changing seeds on an unchanged source frame.
    fixed_a, fixed_noise_a = apply_grain_model(source, amount, 901, "midtone", "mono")
    fixed_b, fixed_noise_b = apply_grain_model(source, amount, 901, "midtone", "mono")
    moving_a, moving_noise_a = apply_grain_model(source, amount, 901, "midtone", "mono")
    moving_b, moving_noise_b = apply_grain_model(source, amount, 902, "midtone", "mono")
    generated.extend((
        ("fixed seed / frame A", fixed_a, fixed_noise_a),
        ("fixed seed / frame B", fixed_b, fixed_noise_b),
        ("animated seed / frame A", moving_a, moving_noise_a),
        ("animated seed / frame B", moving_b, moving_noise_b),
    ))

    height, width, _ = source.shape
    cell_w, cell_h, label_h = 320, height, 28
    cols = 2
    rows = (len(generated) + cols - 1) // cols
    sheet = Image.new("RGB", (cell_w * cols, (cell_h + label_h) * rows), (30, 30, 34))
    draw = ImageDraw.Draw(sheet)
    metrics: dict[str, float] = {}
    levels = (0.04, 0.16, 0.32, 0.5, 0.68, 0.84, 0.96)
    band_h = height // len(levels)
    for index, (name, result, perturbation) in enumerate(generated):
        row, col = divmod(index, cols)
        x0, y0 = col * cell_w, row * (cell_h + label_h)
        image_view = Image.fromarray(np.uint8(np.clip(result * 255, 0, 255)))
        if image_view.size != (cell_w, cell_h):
            image_view = image_view.resize((cell_w, cell_h), Image.Resampling.NEAREST)
        sheet.paste(image_view, (x0, y0 + label_h))
        draw.text((x0 + 5, y0 + 6), name, fill=(245, 245, 245), font=ImageFont.load_default())
        for band, level in enumerate(levels):
            y1 = band * band_h
            y2 = height if band == len(levels) - 1 else (band + 1) * band_h
            band_perturbation = perturbation[y1:y2, :220]
            sigma = float(np.std(band_perturbation))
            metrics[f"{name} · band {level:.2f} sigma"] = sigma

    def temporal_rms(first: np.ndarray, second: np.ndarray) -> float:
        return float(np.mean((first - second) ** 2) ** 0.5)

    metrics["fixed-seed frame delta"] = temporal_rms(fixed_a, fixed_b)
    metrics["animated-seed frame delta"] = temporal_rms(moving_a, moving_b)
    mid_y1, mid_y2 = 3 * band_h, 4 * band_h
    rgb_perturbation = generated[3][2][mid_y1:mid_y2, 24:220]
    metrics["independent RGB channel correlation R-G"] = float(np.corrcoef(rgb_perturbation[:, :, 0].ravel(), rgb_perturbation[:, :, 1].ravel())[0, 1])
    mono_perturbation = generated[1][2][mid_y1:mid_y2, 24:220, 0]
    metrics["white-noise spatial correlation horizontal"] = float(np.corrcoef(mono_perturbation[:, :-1].ravel(), mono_perturbation[:, 1:].ravel())[0, 1])
    coarse_perturbation = generated[4][2][mid_y1:mid_y2, 24:220, 0]
    metrics["coarse-4px spatial correlation horizontal"] = float(np.corrcoef(coarse_perturbation[:, :-1].ravel(), coarse_perturbation[:, 1:].ravel())[0, 1])
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    return metrics


def resize_rgb_array(rgb: np.ndarray, size: tuple[int, int], resampling: Image.Resampling) -> np.ndarray:
    image = Image.fromarray(np.uint8(np.clip(rgb * 255.0, 0, 255)), "RGB")
    return np.asarray(image.resize(size, resampling), dtype=np.float32) / 255.0


def pixelate_rgb(rgb: np.ndarray, factor: float = 0.5) -> np.ndarray:
    height, width, _ = rgb.shape
    reduced = (max(1, round(width * factor)), max(1, round(height * factor)))
    small = resize_rgb_array(rgb, reduced, Image.Resampling.BOX)
    return resize_rgb_array(small, (width, height), Image.Resampling.NEAREST)


def scanline_rgb(rgb: np.ndarray, gain: float = 0.65, dark_phase: int = 1,
                 origin_y: int = 0) -> np.ndarray:
    result = rgb.copy()
    rows = (np.arange(rgb.shape[0]) + origin_y) % 2 == dark_phase
    result[rows] *= gain
    return result


def add_mono_grain(rgb: np.ndarray, amount: float, seed: int) -> tuple[np.ndarray, np.ndarray]:
    height, width, _ = rgb.shape
    noise = np.random.default_rng(seed).normal(0.0, amount, (height, width, 1)).astype(np.float32)
    return np.clip(rgb + noise, 0.0, 1.0), noise


def dither_with_phase(rgb: np.ndarray, phase_x: int, phase_y: int, strength: float = 0.7) -> np.ndarray:
    matrix = np.roll(BAYER_4, (phase_y, phase_x), axis=(0, 1))
    return ordered_quantize(rgb, 12, matrix, strength)


def dither_with_origin(rgb: np.ndarray, origin_x: int, origin_y: int,
                       strength: float = 0.7) -> np.ndarray:
    height, width, _ = rgb.shape
    yy, xx = np.mgrid[0:height, 0:width]
    threshold = BAYER_4[(yy + origin_y) % 4, (xx + origin_x) % 4]
    step = 1.0 / 11.0
    return np.clip(np.floor(np.clip(rgb + threshold[:, :, None] * step * strength, 0, 1) / step + 0.5) * step, 0, 1)


def pipeline_order_study(image: Image.Image, output: Path, seed: int = 481) -> dict[str, float]:
    source = np.asarray(image.convert("RGB"), dtype=np.float32) / 255.0
    height, width, _ = source.shape
    pixelated = pixelate_rgb(source)
    full_grain = np.random.default_rng(seed).normal(0.0, 0.035, (height, width, 1)).astype(np.float32)

    current, _ = add_mono_grain(scanline_rgb(dither_with_phase(pixelated, 0, 0)), 0.035, seed)
    low_noise_rgb = np.broadcast_to(full_grain, (height, width, 3))
    grain_before_scale = pixelate_rgb(np.clip(source + low_noise_rgb, 0.0, 1.0))
    grain_after_scale, _ = add_mono_grain(dither_with_phase(pixelated, 0, 0), 0.035, seed)
    grain_image = Image.fromarray(np.uint8(np.clip((full_grain[:, :, 0] + 0.5) * 255.0, 0, 255)), "L")
    reduced_noise = grain_image.resize((width // 2, height // 2), Image.Resampling.BOX).resize((width, height), Image.Resampling.NEAREST)
    low_noise_reduced = (np.asarray(reduced_noise, dtype=np.float32) / 255.0 - 0.5)[:, :, None]

    ordered_variants: list[tuple[str, np.ndarray]] = [
        ("Current: scale -> dither -> scanline -> grain", current),
        ("Scanline before scale", add_mono_grain(dither_with_phase(pixelate_rgb(scanline_rgb(source)), 0, 0), 0.035, seed)[0]),
        ("Dither before scale", add_mono_grain(scanline_rgb(pixelate_rgb(dither_with_phase(source, 0, 0))), 0.035, seed)[0]),
        ("Grain before scale", scanline_rgb(dither_with_phase(grain_before_scale, 0, 0))),
        ("Grain before quantize", scanline_rgb(dither_with_phase(np.clip(pixelated + full_grain, 0.0, 1.0), 0, 0))),
        ("Scanline before quantize", add_mono_grain(dither_with_phase(scanline_rgb(pixelated), 0, 0), 0.035, seed)[0]),
        ("Grain before scanline", scanline_rgb(grain_after_scale)),
        ("Bayer phase x+1", add_mono_grain(scanline_rgb(dither_with_phase(pixelated, 1, 0)), 0.035, seed)[0]),
        ("Bayer phase y+1", add_mono_grain(scanline_rgb(dither_with_phase(pixelated, 0, 1)), 0.035, seed)[0]),
        ("Bayer phase x+2 y+2", add_mono_grain(scanline_rgb(dither_with_phase(pixelated, 2, 2)), 0.035, seed)[0]),
        ("Scanline dark phase even", add_mono_grain(scanline_rgb(dither_with_phase(pixelated, 0, 0), dark_phase=0), 0.035, seed)[0]),
    ]

    cols = 2
    rows = (len(ordered_variants) + cols - 1) // cols
    label_h = 28
    sheet = Image.new("RGB", (width * cols, (height + label_h) * rows), (30, 30, 34))
    draw = ImageDraw.Draw(sheet)
    metrics: dict[str, float] = {}
    current_linear = srgb_to_linear(current)
    row_means = np.mean(current, axis=(1, 2))
    metrics["current scanline even-odd encoded contrast"] = float(np.mean(row_means[::2]) - np.mean(row_means[1::2]))
    grain_large = np.std(full_grain)
    grain_reduced = float(np.std(low_noise_reduced[:, :, :1]))
    metrics["full-resolution grain sigma"] = float(grain_large)
    metrics["grain sigma after half-scale before upscale"] = grain_reduced

    for index, (name, result) in enumerate(ordered_variants):
        delta = np.abs(srgb_to_linear(result) - current_linear)
        mean_delta = float(np.mean(delta))
        changed_ratio = float(np.mean(np.max(np.abs(result - current), axis=2) > (1.0 / 255.0)))
        metrics[f"{name} · linear MAE vs current"] = mean_delta
        metrics[f"{name} · changed pixel ratio"] = changed_ratio
        y, x = (index // cols) * (height + label_h), (index % cols) * width
        draw.text((x + 5, y + 6), f"{name} | linear MAE {mean_delta:.4f} | changed {changed_ratio:.1%}", fill=(245, 245, 245), font=ImageFont.load_default())
        thumb = Image.fromarray(np.uint8(np.clip(result * 255.0, 0, 255)), "RGB")
        sheet.paste(thumb, (x, y + label_h))
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    return metrics


def tile_screen_effects(rgb: np.ndarray, tile_width: int, tile_height: int,
                        source_relative: bool) -> np.ndarray:
    height, width, _ = rgb.shape
    result = np.empty_like(rgb)
    for top in range(0, height, tile_height):
        for left in range(0, width, tile_width):
            bottom = min(height, top + tile_height)
            right = min(width, left + tile_width)
            tile = rgb[top:bottom, left:right]
            origin_x = left if source_relative else 0
            origin_y = top if source_relative else 0
            screened = dither_with_origin(tile, origin_x, origin_y)
            result[top:bottom, left:right] = scanline_rgb(screened, dark_phase=1, origin_y=origin_y)
    return result


def crop_phase_study(image: Image.Image, output: Path) -> dict[str, float]:
    # Odd output dimensions and odd tile dimensions deliberately make phase resets observable.
    source = np.asarray(image.convert("RGB"), dtype=np.float32)[:191, :383] / 255.0
    height, width, _ = source.shape
    tile_width, tile_height = 127, 63
    columns = 3
    variants_per_scale = 3
    label_h = 26
    sheet = Image.new("RGB", (width * columns, (height + label_h) * 2), (28, 28, 32))
    draw = ImageDraw.Draw(sheet)
    metrics: dict[str, float] = {}
    for scale_index, factor in enumerate((0.5, 0.75)):
        scaled = pixelate_rgb(source, factor)
        reference = scanline_rgb(dither_with_origin(scaled, 0, 0), dark_phase=1)
        local = tile_screen_effects(scaled, tile_width, tile_height, False)
        anchored = tile_screen_effects(scaled, tile_width, tile_height, True)
        reduced_size = (max(1, round(width * factor)), max(1, round(height * factor)))
        metrics[f"scale {factor:.2f} output width"] = float(width)
        metrics[f"scale {factor:.2f} output height"] = float(height)
        metrics[f"scale {factor:.2f} reduced width"] = float(reduced_size[0])
        metrics[f"scale {factor:.2f} reduced height"] = float(reduced_size[1])

        seam = np.zeros((height, width), dtype=bool)
        for x in range(tile_width, width, tile_width):
            seam[:, max(0, x - 2):min(width, x + 2)] = True
        for y in range(tile_height, height, tile_height):
            seam[max(0, y - 2):min(height, y + 2), :] = True
        for name, result in (("tile-local", local), ("source-relative", anchored)):
            delta = np.abs(result - reference)
            metrics[f"scale {factor:.2f} {name} seam code MAE"] = float(np.mean(delta[seam]))
            metrics[f"scale {factor:.2f} {name} whole code MAE"] = float(np.mean(delta))
            metrics[f"scale {factor:.2f} {name} changed pixel ratio"] = float(np.mean(np.max(delta, axis=2) > 1.0 / 255.0))
            metrics[f"scale {factor:.2f} {name} max code error"] = float(np.max(delta))

        row_y = scale_index * (height + label_h)
        for column, (name, result) in enumerate((("full-frame reference", reference), ("tile-local phase", local), ("source-relative phase", anchored))):
            x = column * width
            draw.text((x + 5, row_y + 5), f"scale {factor:.2f} | {name}", fill=(245, 245, 245), font=ImageFont.load_default())
            sheet.paste(Image.fromarray(np.uint8(np.clip(result * 255, 0, 255))), (x, row_y + label_h))
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    return metrics


def crop_resize_phase_study(output: Path, seed: int = 91) -> dict[str, float]:
    """Compare whole-image pixelation with independently resized crop tiles."""
    rng = np.random.default_rng(seed)
    yy, xx = np.mgrid[0:257, 0:515].astype(np.float32)
    # Analytic source coordinates provide a continuous reference across crop seams.
    source = np.stack((
        0.12 + 0.78 * xx / 514.0,
        0.10 + 0.72 * yy / 256.0,
        0.18 + 0.56 * (0.5 + 0.5 * np.sin(xx * 0.071 + yy * 0.039)),
    ), axis=2)
    source += rng.normal(0.0, 0.012, source.shape).astype(np.float32)
    source = np.clip(source, 0.0, 1.0)
    height, width, _ = source.shape
    matrix = BAYER_4
    level_count, strength, scan_gain = 12, 0.7, 0.65

    def resize_only(rgb: np.ndarray, factor: float) -> np.ndarray:
        reduced = (max(1, round(rgb.shape[1] * factor)), max(1, round(rgb.shape[0] * factor)))
        low = resize_rgb_array(rgb, reduced, Image.Resampling.BOX)
        return resize_rgb_array(low, (rgb.shape[1], rgb.shape[0]), Image.Resampling.NEAREST)

    def process(rgb: np.ndarray, factor: float, bayer_origin_x: int = 0, bayer_origin_y: int = 0,
                scanline_origin_y: int = 0) -> np.ndarray:
        high = resize_only(rgb, factor)
        result = ordered_quantize_with_origin(high, level_count, matrix, strength,
                                              bayer_origin_x, bayer_origin_y)
        result = scanline_rgb(result, gain=scan_gain, dark_phase=1,
                              origin_y=scanline_origin_y)
        return result

    tile_cases = ((173, 85), (172, 86))
    variants: dict[str, np.ndarray] = {}
    metrics: dict[str, float] = {"width": float(width), "height": float(height)}
    for factor, (tile_width, tile_height) in zip((0.5, 0.75), tile_cases):
        reference = process(source, factor)
        globally_resized = resize_only(source, factor)
        seam = np.zeros((height, width), dtype=bool)
        for left in range(tile_width, width, tile_width):
            seam[:, max(0, left - 2):min(width, left + 2)] = True
        for top in range(tile_height, height, tile_height):
            seam[max(0, top - 2):min(height, top + 2), :] = True
        tile_interior = np.ones((height, width), dtype=bool)
        tile_interior &= ~seam
        prefix = f"scale {factor:.2f} tile {tile_width}x{tile_height}"
        metrics[f"{prefix} tile count"] = float(((width + tile_width - 1) // tile_width) * ((height + tile_height - 1) // tile_height))
        metrics[f"{prefix} seam region pixel ratio"] = float(np.mean(seam))
        case_variants = {"whole reference": reference}
        for tile_mode, anchored, global_resize in (
            ("local phase", False, False),
            ("local phase", False, True),
            ("output-coordinate phase", True, True),
            ("output-coordinate phase", True, False),
        ):
            label = ("global resize" if global_resize else "tile-local resize") + " + " + tile_mode
            tiled = np.empty_like(source)
            for top in range(0, height, tile_height):
                for left in range(0, width, tile_width):
                    bottom, right = min(height, top + tile_height), min(width, left + tile_width)
                    if global_resize:
                        region = globally_resized[top:bottom, left:right]
                    else:
                        region = resize_only(source[top:bottom, left:right], factor)
                    origin_x, origin_y = (left, top) if anchored else (0, 0)
                    effected = ordered_quantize_with_origin(region, level_count, matrix,
                                                            strength, origin_x, origin_y)
                    tiled[top:bottom, left:right] = scanline_rgb(
                        effected, gain=scan_gain, dark_phase=1,
                        origin_y=top if anchored else 0
                    )
            case_variants[label] = tiled
            delta = np.abs(tiled - reference)
            metrics[f"{prefix} {label} seam code MAE"] = float(np.mean(delta[seam]))
            metrics[f"{prefix} {label} tile interior code MAE"] = float(np.mean(delta[tile_interior]))
            metrics[f"{prefix} {label} whole code MAE"] = float(np.mean(delta))
            metrics[f"{prefix} {label} changed pixel ratio"] = float(np.mean(np.max(delta, axis=2) > 1.0 / 255.0))
            metrics[f"{prefix} {label} max code error"] = float(np.max(delta))
        variants.update({f"{factor:.2f} {name}": result for name, result in case_variants.items()})

    columns, label_h = 4, 24
    rows = (len(variants) + columns - 1) // columns
    sheet = Image.new("RGB", (width * columns, (height + label_h) * rows), (28, 28, 32))
    draw = ImageDraw.Draw(sheet)
    for index, (name, result) in enumerate(variants.items()):
        left, top = (index % columns) * width, (index // columns) * (height + label_h)
        draw.text((left + 5, top + 5), name, fill=(245, 245, 245), font=ImageFont.load_default())
        image = Image.fromarray(np.uint8(np.clip(result * 255.0 + 0.5, 0, 255)))
        sheet.paste(image, (left, top + label_h))
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    return metrics


def opencv_crop_resize_study(output: Path) -> dict[str, float]:
    """Model Core float resize filters and compare global vs crop-local processing."""
    import cv2

    yy, xx = np.mgrid[0:257, 0:515].astype(np.float32)
    source = np.stack((
        0.08 + 0.84 * xx / 514.0,
        0.06 + 0.82 * yy / 256.0,
        0.10 + 0.72 * (0.5 + 0.5 * np.sin(xx * 0.093 + yy * 0.047)),
        np.ones_like(xx),
    ), axis=2).astype(np.float32)
    source[:, :, :3] += 0.018 * np.sin(xx[:, :, None] * 0.71 + yy[:, :, None] * 0.53)
    source[:, :, :3] = np.clip(source[:, :, :3], 0.0, 1.0)
    height, width = source.shape[:2]
    tile_cases = {0.5: (173, 85), 0.75: (172, 86)}
    metrics: dict[str, float] = {
        "width": float(width), "height": float(height),
        "input alpha min": float(np.min(source[:, :, 3])),
        "input alpha max": float(np.max(source[:, :, 3])),
        "comparison domain normalized code values": 1.0,
    }
    outputs: list[tuple[str, np.ndarray]] = []
    filters = (("INTER_AREA", cv2.INTER_AREA), ("INTER_LINEAR", cv2.INTER_LINEAR))

    def resize_stage(rgb: np.ndarray, factor: float, interpolation: int) -> np.ndarray:
        target = (max(1, round(rgb.shape[1] * factor)), max(1, round(rgb.shape[0] * factor)))
        reduced = cv2.resize(rgb, target, interpolation=interpolation)
        return cv2.resize(reduced, (rgb.shape[1], rgb.shape[0]), interpolation=cv2.INTER_NEAREST)

    def finish(rgb: np.ndarray, origin_x: int, origin_y: int) -> np.ndarray:
        # Identity surface phase audit: preserve alpha while applying deterministic
        # ordered color quantization and scanline modulation to RGB only.
        out = rgb.copy()
        out[:, :, :3] = ordered_quantize_with_origin(
            out[:, :, :3], 16, BAYER_4, 0.65, origin_x, origin_y,
        )
        scan_rows = ((np.arange(rgb.shape[0]) + origin_y) % 2) == 1
        out[scan_rows, :, :3] *= 0.72
        return out

    for factor, (tile_w, tile_h) in tile_cases.items():
        seam = np.zeros((height, width), dtype=bool)
        for left in range(tile_w, width, tile_w):
            seam[:, max(0, left - 2):min(width, left + 2)] = True
        for top in range(tile_h, height, tile_h):
            seam[max(0, top - 2):min(height, top + 2), :] = True
        interior = ~seam
        for filter_name, interpolation in filters:
            reference_resize = resize_stage(source, factor, interpolation)
            reference = finish(reference_resize, 0, 0)
            outputs.append((f"{factor:.2f} {filter_name} global reference", reference))
            metrics[f"scale {factor:.2f} tile {tile_w}x{tile_h} count"] = float(
                ((width + tile_w - 1) // tile_w) * ((height + tile_h - 1) // tile_h)
            )
            local_reset = np.empty_like(source)
            local_origin = np.empty_like(source)
            local_resize = np.empty_like(source)
            for top in range(0, height, tile_h):
                for left in range(0, width, tile_w):
                    bottom, right = min(height, top + tile_h), min(width, left + tile_w)
                    tile = source[top:bottom, left:right]
                    local_resize[top:bottom, left:right] = resize_stage(tile, factor, interpolation)
                    local_reset[top:bottom, left:right] = finish(
                        local_resize[top:bottom, left:right], 0, 0
                    )
                    local_origin[top:bottom, left:right] = finish(
                        local_resize[top:bottom, left:right], left, top
                    )
            resize_delta = np.abs(local_resize - reference_resize)
            metrics[f"scale {factor:.2f} {filter_name} tile resize-only whole float MAE"] = float(np.mean(resize_delta[:, :, :3]))
            metrics[f"scale {factor:.2f} {filter_name} tile resize-only max error"] = float(np.max(resize_delta[:, :, :3]))
            for policy, result in (("tile local phase", local_reset),
                                   ("tile output phase", local_origin)):
                outputs.append((f"{factor:.2f} {filter_name} {policy}", result))
                delta = np.abs(result - reference)
                metrics[f"scale {factor:.2f} {filter_name} {policy} RGB seam float MAE"] = float(np.mean(delta[seam, :3]))
                metrics[f"scale {factor:.2f} {filter_name} {policy} RGB interior float MAE"] = float(np.mean(delta[interior, :3]))
                metrics[f"scale {factor:.2f} {filter_name} {policy} RGB whole float MAE"] = float(np.mean(delta[:, :, :3]))
                metrics[f"scale {factor:.2f} {filter_name} {policy} alpha max error"] = float(np.max(np.abs(result[:, :, 3] - reference[:, :, 3])))
            local_phase_delta = np.abs(local_reset - local_origin)
            metrics[f"scale {factor:.2f} {filter_name} tile phase policy RGB whole float MAE"] = float(np.mean(local_phase_delta[:, :, :3]))

    area_refs = {name.split()[0]: image for name, image in outputs if "INTER_AREA global reference" in name}
    linear_refs = {name.split()[0]: image for name, image in outputs if "INTER_LINEAR global reference" in name}
    for factor in ("0.50", "0.75"):
        delta = np.abs(area_refs[factor] - linear_refs[factor])
        metrics[f"scale {factor} INTER_AREA vs INTER_LINEAR RGB whole float MAE"] = float(np.mean(delta[:, :, :3]))
        metrics[f"scale {factor} INTER_AREA vs INTER_LINEAR RGB max error"] = float(np.max(delta[:, :, :3]))

    thumb_w, thumb_h, label_h, columns = 515, 257, 22, 4
    rows = (len(outputs) + columns - 1) // columns
    sheet = Image.new("RGB", (thumb_w * columns, (thumb_h + label_h) * rows), (28, 28, 32))
    draw = ImageDraw.Draw(sheet)
    for index, (label, rgba) in enumerate(outputs):
        left, top = (index % columns) * thumb_w, (index // columns) * (thumb_h + label_h)
        draw.text((left + 4, top + 4), label, fill=(245, 245, 245), font=ImageFont.load_default())
        rgb8 = Image.fromarray(np.uint8(np.clip(rgba[:, :, :3] * 255.0 + 0.5, 0, 255)), "RGB")
        sheet.paste(rgb8, (left, top + label_h))
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    return metrics


def make_alpha_pixel_art_crop_fixture() -> Image.Image:
    """Create a deterministic pixel-art chart with hard/soft edges and hidden RGB."""
    width, height = 515, 257
    yy, xx = np.mgrid[0:height, 0:width].astype(np.float32)
    image = np.zeros((height, width, 4), dtype=np.float32)
    # Hidden colors deliberately alternate behind transparent pixels.
    checker = ((xx.astype(np.int32) // 5 + yy.astype(np.int32) // 5) & 1) == 0
    image[:, :, :3] = np.where(checker[:, :, None], (1.0, 0.0, 1.0), (0.0, 1.0, 0.25))
    image[:, :, 3] = 0.0

    def paint(mask: np.ndarray, color: tuple[float, float, float], alpha: np.ndarray | float = 1.0) -> None:
        image[mask, :3] = color
        image[mask, 3] = alpha if np.isscalar(alpha) else alpha[mask]

    # Pixel-art blocks, outlined panel, thin diagonals, and one-pixel features.
    paint((xx >= 35) & (xx < 190) & (yy >= 38) & (yy < 218), (0.9, 0.13, 0.08))
    paint((xx >= 42) & (xx < 183) & (yy >= 45) & (yy < 211), (0.06, 0.12, 0.72))
    paint((xx >= 55) & (xx < 92) & (yy >= 63) & (yy < 176), (0.98, 0.78, 0.08))
    paint((xx >= 92) & (xx < 129) & (yy >= 63) & (yy < 176), (0.08, 0.78, 0.25))
    paint((xx >= 129) & (xx < 166) & (yy >= 63) & (yy < 176), (0.92, 0.12, 0.65))
    for index in range(9):
        x = 50 + index * 15
        y0 = 190 - index * 8
        paint((np.abs(xx - x) < 0.7) & (np.abs(yy - y0) < 0.7), (1.0, 1.0, 1.0))

    distance = np.sqrt((xx - 330.0) ** 2 + (yy - 124.0) ** 2)
    circle_alpha = np.clip(1.0 - (distance - 46.0), 0.0, 1.0)
    circle = distance <= 46.5
    paint(circle, (0.05, 0.78, 0.94), circle_alpha)
    paint((distance >= 31.0) & (distance <= 35.0), (0.98, 0.98, 0.92))
    paint((xx >= 282) & (xx < 380) & (yy >= 119) & (yy < 122), (0.98, 0.98, 0.92))
    paint((xx >= 327) & (xx < 330) & (yy >= 76) & (yy < 169), (0.98, 0.98, 0.92))

    # Soft antialiased diagonal crossing the hard pixel blocks.
    signed = np.abs(yy - (224.0 - 0.24 * xx)) / np.sqrt(1.0 + 0.24**2)
    line_alpha = np.clip(1.5 - signed, 0.0, 1.0)
    line = signed < 1.5
    paint(line, (1.0, 0.94, 0.86), line_alpha)
    return Image.fromarray(np.uint8(np.clip(image * 255.0 + 0.5, 0, 255)), "RGBA")


def alpha_pixel_art_crop_study(output: Path, fixture_path: Path) -> dict[str, float]:
    """Compare crop-local OpenCV pixelation on alpha art over three backgrounds."""
    import cv2

    fixture = make_alpha_pixel_art_crop_fixture()
    fixture_path.parent.mkdir(parents=True, exist_ok=True)
    fixture.save(fixture_path)
    source_rgba = np.asarray(fixture, dtype=np.float32) / 255.0
    height, width = source_rgba.shape[:2]
    alpha = source_rgba[:, :, 3].copy()
    edge = (alpha > 0.0) & (alpha < 1.0)
    kernel = np.ones((5, 5), dtype=np.uint8)
    edge = cv2.dilate(edge.astype(np.uint8), kernel, iterations=1) > 0
    tile_w, tile_h = 173, 85
    factors = (0.5, 0.75)
    filters = (("AREA", cv2.INTER_AREA), ("LINEAR", cv2.INTER_LINEAR))
    backgrounds = {
        "black": np.array((0.0, 0.0, 0.0), dtype=np.float32),
        "mid-gray": np.array((0.5, 0.5, 0.5), dtype=np.float32),
        "white": np.array((1.0, 1.0, 1.0), dtype=np.float32),
    }
    metrics: dict[str, float] = {
        "width": float(width), "height": float(height),
        "fraction partially transparent pixels": float(np.mean((alpha > 0.0) & (alpha < 1.0))),
        "fraction transparent hidden rgb pixels": float(np.mean(alpha == 0.0)),
        "edge roi pixel ratio": float(np.mean(edge)),
    }
    sheet_groups: list[tuple[str, list[tuple[str, np.ndarray]]]] = []

    def resize_rgb(rgb: np.ndarray, factor: float, interpolation: int) -> np.ndarray:
        size = (max(1, round(rgb.shape[1] * factor)), max(1, round(rgb.shape[0] * factor)))
        low = cv2.resize(rgb, size, interpolation=interpolation)
        return cv2.resize(low, (rgb.shape[1], rgb.shape[0]), interpolation=cv2.INTER_NEAREST)

    def finish(rgb: np.ndarray, origin_x: int, origin_y: int) -> np.ndarray:
        result = ordered_quantize_with_origin(rgb, 12, BAYER_4, 0.55, origin_x, origin_y)
        dark_rows = ((np.arange(rgb.shape[0]) + origin_y) % 2) == 1
        result[dark_rows] *= 0.75
        return np.clip(result, 0.0, 1.0)

    def composite(rgb: np.ndarray, bg: np.ndarray) -> np.ndarray:
        return rgb * alpha[:, :, None] + bg[None, None, :] * (1.0 - alpha[:, :, None])

    hidden_alt = source_rgba.copy()
    hidden_alt[alpha == 0.0, :3] = (0.05, 0.95, 0.12)
    for factor in factors:
        for filter_name, interpolation in filters:
            label = f"scale {factor:.2f} {filter_name}"
            global_rgb = finish(resize_rgb(source_rgba[:, :, :3], factor, interpolation), 0, 0)
            variants = {"global": global_rgb}
            tile_local = np.empty_like(global_rgb)
            tile_origin = np.empty_like(global_rgb)
            hidden_global = finish(resize_rgb(hidden_alt[:, :, :3], factor, interpolation), 0, 0)
            hidden_tile = np.empty_like(tile_local)
            hidden_tile_origin = np.empty_like(tile_origin)
            for top in range(0, height, tile_h):
                for left in range(0, width, tile_w):
                    bottom, right = min(height, top + tile_h), min(width, left + tile_w)
                    rgb_tile = source_rgba[top:bottom, left:right, :3]
                    alt_tile = hidden_alt[top:bottom, left:right, :3]
                    resized = resize_rgb(rgb_tile, factor, interpolation)
                    resized_alt = resize_rgb(alt_tile, factor, interpolation)
                    tile_local[top:bottom, left:right] = finish(resized, 0, 0)
                    tile_origin[top:bottom, left:right] = finish(resized, left, top)
                    hidden_tile[top:bottom, left:right] = finish(resized_alt, 0, 0)
                    hidden_tile_origin[top:bottom, left:right] = finish(resized_alt, left, top)
            variants["tile-local"] = tile_local
            variants["tile-origin"] = tile_origin
            metrics[f"{label} tile count"] = float(((width + tile_w - 1) // tile_w) * ((height + tile_h - 1) // tile_h))
            for policy, result in variants.items():
                metrics[f"{label} {policy} alpha max error"] = 0.0  # The study restores original alpha by construction.
                background_images: list[tuple[str, np.ndarray]] = []
                for bg_name, bg in backgrounds.items():
                    image = composite(result, bg)
                    background_images.append((bg_name, image))
                    if policy != "global":
                        reference = composite(global_rgb, bg)
                        delta = np.abs(image - reference)
                        metrics[f"{label} {policy} {bg_name} composite whole MAE"] = float(np.mean(delta))
                        metrics[f"{label} {policy} {bg_name} composite edge MAE"] = float(np.mean(delta[edge]))
                        metrics[f"{label} {policy} {bg_name} changed edge ratio"] = float(np.mean(np.max(delta[edge], axis=1) > 1.0 / 255.0))
                sheet_groups.append((f"{label} {policy}", background_images))
            for policy, hidden_result in (("global", hidden_global),
                                          ("tile-local", hidden_tile),
                                          ("tile-origin", hidden_tile_origin)):
                normal = variants[policy]
                for bg_name, bg in backgrounds.items():
                    sensitivity = np.abs(composite(normal, bg) - composite(hidden_result, bg))
                    metrics[f"{label} {policy} hidden-rgb {bg_name} composite edge MAE"] = float(np.mean(sensitivity[edge]))

    columns, cell_w, cell_h, label_h = 3, width, height, 22
    sheet = Image.new("RGB", (columns * cell_w, len(sheet_groups) * (cell_h + label_h)), (28, 28, 32))
    draw = ImageDraw.Draw(sheet)
    for index, (label, cells) in enumerate(sheet_groups):
        top = index * (cell_h + label_h)
        for column, (bg_name, rgb) in enumerate(cells):
            left = column * cell_w
            draw.text((left + 4, top + 4), f"{label} | {bg_name}", fill=(245, 245, 245), font=ImageFont.load_default())
            sheet.paste(Image.fromarray(np.uint8(np.clip(rgb * 255.0 + 0.5, 0, 255)), "RGB"), (left, top + label_h))
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    return metrics


def alpha_resize_strategy_study(image_path: Path, output: Path, seed: int = 37) -> dict[str, float]:
    """Compare current RGBA resize strategies through the actual lab presets."""
    import cv2

    with Image.open(image_path) as opened:
        source_image = opened.convert("RGBA")
    source_rgba = np.asarray(source_image, dtype=np.uint8)
    source = source_rgba.astype(np.float32) / 255.0
    alpha = source[:, :, 3]
    edge = ((alpha > 0.0) & (alpha < 1.0)).astype(np.uint8)
    edge = cv2.dilate(edge, np.ones((5, 5), dtype=np.uint8), iterations=1) > 0
    alternate = source_rgba.copy()
    alternate[alpha == 0.0, :3] = (13, 243, 31)
    alternate_image = Image.fromarray(alternate, "RGBA")
    policies = ("straight", "nearest", "dilated", "premultiplied")
    backgrounds = {
        "black": np.array((0.0, 0.0, 0.0), dtype=np.float32),
        "mid-gray": np.array((0.5, 0.5, 0.5), dtype=np.float32),
        "white": np.array((1.0, 1.0, 1.0), dtype=np.float32),
    }
    metrics: dict[str, float] = {
        "input width": float(source.shape[1]),
        "input height": float(source.shape[0]),
        "input SHA256 match source note": 1.0 if hashlib.sha256(image_path.read_bytes()).hexdigest() == "5f5b7f7252380f7502962fa1f789802ce187ae6634e883428aa4674ead72000c" else 0.0,
        "edge roi pixel ratio": float(np.mean(edge)),
        "alpha code domain": 255.0,
        "seed": float(seed),
    }
    groups: list[tuple[str, list[tuple[str, np.ndarray]]]] = []

    def composite(rgba: np.ndarray, background: np.ndarray) -> np.ndarray:
        return rgba[:, :, :3] * rgba[:, :, 3:4] + background[None, None, :] * (1.0 - rgba[:, :, 3:4])

    for preset_name in ("handheld", "arcade"):
        settings = PRESETS[preset_name]
        base_rgba = np.asarray(apply_retro(source_image, **settings, seed=seed), dtype=np.float32) / 255.0
        base_alt = np.asarray(apply_retro(alternate_image, **settings, seed=seed), dtype=np.float32) / 255.0
        for policy in policies:
            image = np.asarray(apply_retro(source_image, **settings, seed=seed,
                                           alpha_resize=policy), dtype=np.float32) / 255.0
            image_alt = np.asarray(apply_retro(alternate_image, **settings, seed=seed,
                                               alpha_resize=policy), dtype=np.float32) / 255.0
            label = f"{preset_name} {policy}"
            metrics[f"{label} alpha max error vs input"] = float(np.max(np.abs(image[:, :, 3] - source[:, :, 3])))
            edge_rgb_delta = np.abs(image[:, :, :3] - base_rgba[:, :, :3])
            metrics[f"{label} vs straight edge RGB MAE"] = float(np.mean(edge_rgb_delta[edge]))
            metrics[f"{label} vs straight whole RGB MAE"] = float(np.mean(edge_rgb_delta))
            hidden_delta = np.abs(image[:, :, :3] - image_alt[:, :, :3])
            metrics[f"{label} hidden RGB edge MAE"] = float(np.mean(hidden_delta[edge]))
            metrics[f"{label} hidden RGB whole MAE"] = float(np.mean(hidden_delta))
            metrics[f"{label} output sigma"] = float(np.std(image[:, :, :3]))
            background_panels: list[tuple[str, np.ndarray]] = []
            for bg_name, bg in backgrounds.items():
                rendered = composite(image, bg)
                background_panels.append((bg_name, rendered))
                baseline_rendered = composite(base_rgba, bg)
                delta = np.abs(rendered - baseline_rendered)
                metrics[f"{label} {bg_name} composite edge MAE vs straight"] = float(np.mean(delta[edge]))
                hidden_composite = np.abs(composite(image, bg) - composite(image_alt, bg))
                metrics[f"{label} {bg_name} hidden RGB composite edge MAE"] = float(np.mean(hidden_composite[edge]))
            groups.append((label, background_panels))

    columns, cell_w, cell_h, label_h = 3, source.shape[1], source.shape[0], 22
    sheet = Image.new("RGB", (columns * cell_w, len(groups) * (cell_h + label_h)), (28, 28, 32))
    draw = ImageDraw.Draw(sheet)
    for index, (label, panels) in enumerate(groups):
        top = index * (cell_h + label_h)
        for column, (bg_name, rgb) in enumerate(panels):
            left = column * cell_w
            draw.text((left + 4, top + 4), f"{label} | {bg_name}", fill=(245, 245, 245), font=ImageFont.load_default())
            sheet.paste(Image.fromarray(np.uint8(np.clip(rgb * 255.0 + 0.5, 0, 255)), "RGB"), (left, top + label_h))
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    return metrics


def make_alpha_resize_suite_fixture() -> Image.Image:
    """Deterministic multicolor RGBA control with narrow gaps and translucent strands."""
    width, height = 480, 320
    y, x = np.mgrid[0:height, 0:width]
    rgb = np.empty((height, width, 3), dtype=np.uint8)
    rgb[:] = (0, 0, 0)
    alpha = np.zeros((height, width), dtype=np.uint8)

    # Contrasting silhouette pairs separated by deliberately small transparent gaps.
    panels = [
        (24, 44, 1, (245, 62, 36), (32, 205, 132)),
        (140, 44, 2, (252, 195, 44), (42, 115, 240)),
        (256, 44, 4, (225, 75, 194), (36, 220, 226)),
        (372, 44, 8, (245, 115, 38), (142, 215, 51)),
    ]
    for x0, y0, gap, color_a, color_b in panels:
        split = x0 + 46
        rgb[y0:y0 + 86, x0:split] = color_a
        alpha[y0:y0 + 86, x0:split] = 255
        rgb[y0:y0 + 86, split + gap:split + gap + 46] = color_b
        alpha[y0:y0 + 86, split + gap:split + gap + 46] = 255
        # Opaque one-pixel bars expose narrow structure at the same scale.
        for offset in (8, 13, 19, 27):
            bx = x0 + offset
            rgb[y0 + 96:y0 + 150, bx:bx + 1] = color_b
            alpha[y0 + 96:y0 + 150, bx:bx + 1] = 255

    # Smooth, translucent hair-like curves with varied chroma and alpha.
    for strand in range(22):
        base_y = 224 + strand * 3
        center_x = 18 + strand * 21
        curve_y = base_y + 9.0 * np.sin((x - center_x) / 25.0 + strand * 0.37)
        distance = np.abs(y - curve_y)
        strand_alpha = np.clip((1.65 - distance) * (92 + (strand % 5) * 34), 0, 210).astype(np.uint8)
        mask = (x >= center_x - 13) & (x < center_x + 92) & (strand_alpha > alpha)
        strand_color = np.asarray(((strand * 47 + 60) % 256, (strand * 83 + 95) % 256,
                                   (strand * 29 + 180) % 256), dtype=np.uint8)
        rgb[mask] = strand_color
        alpha[mask] = strand_alpha[mask]

    # Distinct hidden colors reveal whether resampling samples transparent RGB.
    hidden = alpha == 0
    checker = ((x // 7 + y // 5) % 2) == 0
    rgb[hidden & checker] = (250, 10, 210)
    rgb[hidden & ~checker] = (8, 245, 35)
    rgba = np.dstack((rgb, alpha))
    return Image.fromarray(rgba, "RGBA")


def alpha_resize_multicolor_study(output: Path, fixture_path: Path, seed: int = 53) -> dict[str, float]:
    """Measure four resize policies on all presets using a multicolor alpha chart."""
    import cv2

    fixture = make_alpha_resize_suite_fixture()
    fixture_path.parent.mkdir(parents=True, exist_ok=True)
    fixture.save(fixture_path)
    source_rgba = np.asarray(fixture, dtype=np.uint8)
    source = source_rgba.astype(np.float32) / 255.0
    source_alpha = source[:, :, 3]
    partial = (source_alpha > 0.0) & (source_alpha < 1.0)
    edge = cv2.dilate(partial.astype(np.uint8), np.ones((7, 7), dtype=np.uint8), iterations=1) > 0
    strand_roi = np.zeros_like(edge)
    strand_roi[210:300, :] = True
    strand_roi &= edge
    gap_rois: dict[str, np.ndarray] = {}
    for x0, y0, gap, *_ in (
        (24, 44, 1), (140, 44, 2), (256, 44, 4), (372, 44, 8),
    ):
        mask = np.zeros_like(edge)
        split = x0 + 46
        mask[y0:y0 + 86, split - 4:split + gap + 4] = True
        gap_rois[f"gap-{gap}px"] = mask

    alternate = source_rgba.copy()
    alternate[source_alpha == 0.0, :3] = (13, 243, 31)
    alternate_image = Image.fromarray(alternate, "RGBA")
    policies = ("straight", "nearest", "dilated", "premultiplied")
    backgrounds = {
        "black": np.array((0.0, 0.0, 0.0), dtype=np.float32),
        "mid-gray": np.array((0.5, 0.5, 0.5), dtype=np.float32),
        "white": np.array((1.0, 1.0, 1.0), dtype=np.float32),
    }
    metrics: dict[str, float] = {
        "input width": float(source.shape[1]),
        "input height": float(source.shape[0]),
        "input SHA256": float(int(hashlib.sha256(fixture_path.read_bytes()).hexdigest()[:12], 16)),
        "edge roi pixel ratio": float(np.mean(edge)),
        "strand roi pixel count": float(np.count_nonzero(strand_roi)),
        "seed": float(seed),
    }
    groups: list[tuple[str, list[tuple[str, np.ndarray]]]] = []

    def composite(rgba: np.ndarray, background: np.ndarray) -> np.ndarray:
        return rgba[:, :, :3] * rgba[:, :, 3:4] + background[None, None, :] * (1.0 - rgba[:, :, 3:4])

    for preset_name, settings in PRESETS.items():
        base = np.asarray(apply_retro(fixture, **settings, seed=seed), dtype=np.float32) / 255.0
        for policy in policies:
            result_image = apply_retro(fixture, **settings, seed=seed, alpha_resize=policy)
            alt_image = apply_retro(alternate_image, **settings, seed=seed, alpha_resize=policy)
            result = np.asarray(result_image, dtype=np.float32) / 255.0
            result_alt = np.asarray(alt_image, dtype=np.float32) / 255.0
            label = f"{preset_name} {policy}"
            metrics[f"{label} alpha max error"] = float(np.max(np.abs(result[:, :, 3] - source_alpha)))
            rgb_delta = np.abs(result[:, :, :3] - base[:, :, :3])
            metrics[f"{label} vs straight edge RGB MAE"] = float(np.mean(rgb_delta[edge]))
            metrics[f"{label} vs straight strand RGB MAE"] = float(np.mean(rgb_delta[strand_roi])) if np.any(strand_roi) else 0.0
            hidden_delta = np.abs(result[:, :, :3] - result_alt[:, :, :3])
            metrics[f"{label} hidden RGB edge MAE"] = float(np.mean(hidden_delta[edge]))
            for region_name, region in gap_rois.items():
                scaled = np.asarray(Image.fromarray(region.astype(np.uint8) * 255).resize(result.shape[1::-1], Image.Resampling.NEAREST)) > 0
                metrics[f"{label} hidden RGB {region_name} MAE"] = float(np.mean(hidden_delta[scaled]))
            panels: list[tuple[str, np.ndarray]] = []
            for background_name, background in backgrounds.items():
                panels.append((background_name, composite(result, background)))
                composited_hidden_delta = np.abs(composite(result, background) - composite(result_alt, background))
                metrics[f"{label} {background_name} hidden composite edge MAE"] = float(np.mean(composited_hidden_delta[edge]))
            groups.append((label, panels))

    columns, cell_w, cell_h, label_h = 3, source.shape[1], source.shape[0], 22
    sheet = Image.new("RGB", (columns * cell_w, len(groups) * (cell_h + label_h)), (28, 28, 32))
    draw = ImageDraw.Draw(sheet)
    for index, (label, panels) in enumerate(groups):
        top = index * (cell_h + label_h)
        for column, (background_name, rgb) in enumerate(panels):
            left = column * cell_w
            draw.text((left + 4, top + 4), f"{label} | {background_name}", fill=(245, 245, 245), font=ImageFont.load_default())
            sheet.paste(Image.fromarray(np.uint8(np.clip(rgb * 255.0 + 0.5, 0, 255)), "RGB"), (left, top + label_h))
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    metrics["fixture SHA256 numeric"] = float(int(hashlib.sha256(fixture_path.read_bytes()).hexdigest()[:12], 16))
    return metrics


def alpha_resize_gap_scale_study(output: Path, seed: int = 71) -> dict[str, float]:
    """Isolate BOX+NEAREST resize from the full preset across source gap widths."""
    fixture = make_alpha_resize_suite_fixture()
    source_rgba = np.asarray(fixture, dtype=np.uint8)
    source = source_rgba.astype(np.float32) / 255.0
    alpha = source[:, :, 3]
    alternate = source_rgba.copy()
    alternate[alpha == 0.0, :3] = (13, 243, 31)
    alternate_source = alternate.astype(np.float32) / 255.0
    factors = (0.25, 0.375, 0.5, 0.75, 1.0)
    policies = ("straight", "nearest", "dilated", "premultiplied")
    backgrounds = {
        "black": np.array((0.0, 0.0, 0.0), dtype=np.float32),
        "mid-gray": np.array((0.5, 0.5, 0.5), dtype=np.float32),
        "white": np.array((1.0, 1.0, 1.0), dtype=np.float32),
    }

    def resize_policy(values: np.ndarray, scale: float, policy: str) -> np.ndarray:
        height, width, _ = values.shape
        if scale >= 1.0:
            return values.copy()
        target = (max(1, round(width * scale)), max(1, round(height * scale)))
        rgba = Image.fromarray(np.uint8(np.clip(values * 255.0 + 0.5, 0, 255)), "RGBA")
        original_alpha = np.asarray(rgba.getchannel("A"), dtype=np.float32) / 255.0
        rgb = values[:, :, :3]
        if policy in ("nearest", "dilated"):
            known = original_alpha > 0.0
            if policy == "nearest":
                filled = rgb.copy()
                best_distance = np.full(known.shape, np.inf, dtype=np.float32)
                best_rgb = np.zeros_like(rgb)
                for dy in range(-8, 9):
                    for dx in range(-8, 9):
                        distance = dx * dx + dy * dy
                        if distance > 64:
                            continue
                        sy, sx = slice(max(0, -dy), min(height, height - dy)), slice(max(0, -dx), min(width, width - dx))
                        ty, tx = slice(max(0, dy), min(height, height + dy)), slice(max(0, dx), min(width, width + dx))
                        valid = known[sy, sx] & ~known[ty, tx]
                        closer = valid & (distance < best_distance[ty, tx])
                        view = best_rgb[ty, tx]
                        view[closer] = rgb[sy, sx][closer]
                        best_rgb[ty, tx] = view
                        dist_view = best_distance[ty, tx]
                        dist_view[closer] = distance
                        best_distance[ty, tx] = dist_view
                fill_mask = ~known & np.isfinite(best_distance)
                filled[fill_mask] = best_rgb[fill_mask]
            else:
                filled = rgb.copy()
                active = known.copy()
                for _ in range(8):
                    sums = np.zeros_like(filled)
                    counts = np.zeros(known.shape, dtype=np.float32)
                    for dy in (-1, 0, 1):
                        for dx in (-1, 0, 1):
                            if dx == 0 and dy == 0:
                                continue
                            sy, sx = slice(max(0, -dy), min(height, height - dy)), slice(max(0, -dx), min(width, width - dx))
                            ty, tx = slice(max(0, dy), min(height, height + dy)), slice(max(0, dx), min(width, width + dx))
                            sums[ty, tx] += filled[sy, sx] * active[sy, sx, None]
                            counts[ty, tx] += active[sy, sx]
                    fill_mask = ~active & (counts > 0)
                    if not np.any(fill_mask):
                        break
                    filled[fill_mask] = sums[fill_mask] / counts[fill_mask, None]
                    active[fill_mask] = True
            prepared = Image.fromarray(np.uint8(np.clip(filled * 255.0 + 0.5, 0, 255)), "RGB")
        elif policy == "straight":
            prepared = Image.fromarray(np.uint8(np.clip(rgb * 255.0 + 0.5, 0, 255)), "RGB")
        else:
            premul = rgb * original_alpha[:, :, None]
            premul_rgba = np.dstack((premul, original_alpha))
            premul_image = Image.fromarray(np.uint8(np.clip(premul_rgba * 255.0 + 0.5, 0, 255)), "RGBA")
            reduced = np.asarray(premul_image.resize(target, Image.Resampling.BOX).resize((width, height), Image.Resampling.NEAREST), dtype=np.float32) / 255.0
            rgb_out = np.divide(reduced[:, :, :3], reduced[:, :, 3:4], out=np.zeros_like(reduced[:, :, :3]), where=reduced[:, :, 3:4] > 1e-6)
            return np.dstack((np.clip(rgb_out, 0.0, 1.0), original_alpha))
        rgb_out = np.asarray(prepared.resize(target, Image.Resampling.BOX).resize((width, height), Image.Resampling.NEAREST), dtype=np.float32) / 255.0
        return np.dstack((rgb_out, original_alpha))

    metrics: dict[str, float] = {"seed": float(seed), "input width": 480.0, "input height": 320.0}
    partial = (alpha > 0.0) & (alpha < 1.0)
    alpha_support = alpha > 0.0
    support_image = Image.fromarray(alpha_support.astype(np.uint8) * 255)
    hard_edge = np.asarray(support_image.filter(ImageFilter.MaxFilter(3)), dtype=np.uint8) != np.asarray(
        support_image.filter(ImageFilter.MinFilter(3)), dtype=np.uint8,
    )
    partial_edge = np.asarray(Image.fromarray(partial.astype(np.uint8) * 255).filter(ImageFilter.MaxFilter(7)), dtype=np.uint8) > 0
    edge_roi = hard_edge | partial_edge
    source_gap_specs = ((24, 44, 1), (140, 44, 2), (256, 44, 4), (372, 44, 8))
    cell_width, cell_height = 240, 150
    sheet = Image.new("RGB", (8 * cell_width, len(factors) * cell_height), (25, 25, 30))
    draw = ImageDraw.Draw(sheet)
    for row, factor in enumerate(factors):
        reference = resize_policy(source, factor, "straight")
        results: dict[str, np.ndarray] = {}
        for policy in policies:
            result = resize_policy(source, factor, policy)
            result_alt = resize_policy(alternate_source, factor, policy)
            results[policy] = result
            delta = np.abs(result[:, :, :3] - reference[:, :, :3])
            hidden_delta = np.abs(result[:, :, :3] - result_alt[:, :, :3])
            metrics[f"scale {factor:.3f} {policy} edge RGB MAE vs straight"] = float(np.mean(delta[edge_roi]))
            metrics[f"scale {factor:.3f} {policy} hidden RGB edge MAE"] = float(np.mean(hidden_delta[edge_roi]))
            composite_result = result[:, :, :3] * result[:, :, 3:4]
            composite_alt = result_alt[:, :, :3] * result_alt[:, :, 3:4]
            composite_delta = np.abs(composite_result - composite_alt)
            metrics[f"scale {factor:.3f} {policy} hidden composite edge MAE"] = float(np.mean(composite_delta[edge_roi]))
            for x0, y0, gap in source_gap_specs:
                mask = np.zeros(alpha.shape, dtype=bool)
                split = x0 + 46
                mask[y0:y0 + 86, split - 4:split + gap + 4] = True
                metrics[f"scale {factor:.3f} {policy} gap-{gap}px hidden RGB MAE"] = float(np.mean(hidden_delta[mask]))
                metrics[f"scale {factor:.3f} {policy} gap-{gap}px hidden composite MAE"] = float(np.mean(composite_delta[mask]))
        for region_index, (region_name, roi) in enumerate((
            ("gap-1px", (18, 32, 90, 135)),
            ("strand", (0, 205, 480, 300)),
        )):
            x0, y0, x1, y1 = roi
            for policy_index, policy in enumerate(policies):
                crop = results[policy][y0:y1, x0:x1]
                composite = crop[:, :, :3] * crop[:, :, 3:4] + backgrounds["mid-gray"][None, None, :] * (1.0 - crop[:, :, 3:4])
                thumb = Image.fromarray(np.uint8(np.clip(composite * 255.0 + 0.5, 0, 255)), "RGB").resize((cell_width, cell_height - 22), Image.Resampling.NEAREST)
                column = region_index * 4 + policy_index
                left, top = column * cell_width, row * cell_height
                draw.text((left + 4, top + 4), f"scale={factor:.3f} {policy} | {region_name}", fill=(245, 245, 245), font=ImageFont.load_default())
                sheet.paste(thumb, (left, top + 22))
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    return metrics


def alpha_hidden_stage_attribution(output: Path, seed: int = 83) -> dict[str, float]:
    """Attribute scale-1 hidden-RGB leakage to successive preset stages."""
    import cv2

    fixture = make_alpha_resize_suite_fixture()
    source_rgba = np.asarray(fixture, dtype=np.uint8)
    alpha = source_rgba[:, :, 3].astype(np.float32) / 255.0
    alternate = source_rgba.copy()
    alternate[source_rgba[:, :, 3] == 0, :3] = (13, 243, 31)
    alternate_image = Image.fromarray(alternate, "RGBA")
    edge = cv2.dilate(((alpha > 0.0) & (alpha < 1.0)).astype(np.uint8), np.ones((7, 7), dtype=np.uint8), iterations=1) > 0
    stages = ("identity", "quantize", "channel-offset", "scanlines", "grain")
    backgrounds = {
        "black": np.array((0.0, 0.0, 0.0), dtype=np.float32),
        "mid-gray": np.array((0.5, 0.5, 0.5), dtype=np.float32),
        "white": np.array((1.0, 1.0, 1.0), dtype=np.float32),
    }
    metrics: dict[str, float] = {"seed": float(seed), "edge pixel count": float(np.count_nonzero(edge))}
    rows: list[tuple[str, np.ndarray]] = []

    def comp(image: np.ndarray, bg: np.ndarray) -> np.ndarray:
        return image[:, :, :3] * image[:, :, 3:4] + bg[None, None, :] * (1.0 - image[:, :, 3:4])

    for preset_name in ("print", "vhs"):
        settings = PRESETS[preset_name]
        stage_settings = (
            {"colors": 256, "scale": 1.0, "dither": 0.0, "scanlines": 0.0, "aberration": 0.0, "grain": 0.0},
            {"colors": settings["colors"], "scale": 1.0, "dither": settings["dither"], "scanlines": 0.0, "aberration": 0.0, "grain": 0.0},
            {"colors": settings["colors"], "scale": 1.0, "dither": settings["dither"], "scanlines": 0.0, "aberration": settings["aberration"], "grain": 0.0},
            {"colors": settings["colors"], "scale": 1.0, "dither": settings["dither"], "scanlines": settings["scanlines"], "aberration": settings["aberration"], "grain": 0.0},
            {"colors": settings["colors"], "scale": 1.0, "dither": settings["dither"], "scanlines": settings["scanlines"], "aberration": settings["aberration"], "grain": settings["grain"]},
        )
        prior_delta = 0.0
        for stage, options in zip(stages, stage_settings):
            base = np.asarray(apply_retro(fixture, **options, seed=seed), dtype=np.float32) / 255.0
            alt = np.asarray(apply_retro(alternate_image, **options, seed=seed), dtype=np.float32) / 255.0
            pair_delta = np.abs(comp(base, backgrounds["black"]) - comp(alt, backgrounds["black"]))
            edge_delta = float(np.mean(pair_delta[edge]))
            metrics[f"{preset_name} {stage} hidden composite edge MAE"] = edge_delta
            metrics[f"{preset_name} {stage} hidden RGB edge MAE"] = float(np.mean(np.abs(base[:, :, :3] - alt[:, :, :3])[edge]))
            metrics[f"{preset_name} {stage} stage-added hidden composite edge MAE"] = edge_delta - prior_delta
            metrics[f"{preset_name} {stage} alpha exact"] = float(
                np.array_equal(np.uint8(np.clip(base[:, :, 3] * 255.0 + 0.5, 0, 255)), source_rgba[:, :, 3])
            )
            prior_delta = edge_delta
            rows.append((f"{preset_name} {stage}", base))

    cell_w, cell_h, label_h, columns = source_rgba.shape[1], source_rgba.shape[0], 22, 3
    sheet = Image.new("RGB", (columns * cell_w, len(rows) * (cell_h + label_h)), (28, 28, 32))
    draw = ImageDraw.Draw(sheet)
    for index, (label, image) in enumerate(rows):
        left, top = 0, index * (cell_h + label_h)
        for column, (bg_name, background) in enumerate(backgrounds.items()):
            draw.text((column * cell_w + 4, top + 4), f"{label} | {bg_name}", fill=(245, 245, 245), font=ImageFont.load_default())
            composite_rgb = comp(image, background)
            sheet.paste(Image.fromarray(np.uint8(np.clip(composite_rgb * 255.0 + 0.5, 0, 255)), "RGB"), (column * cell_w, top + label_h))
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    return metrics


def alpha_chromatic_offset_sweep(output: Path, seed: int = 97) -> dict[str, float]:
    """Measure hidden-RGB edge leakage and visible change across VHS offset amounts."""
    import cv2

    fixture = make_alpha_resize_suite_fixture()
    source_rgba = np.asarray(fixture, dtype=np.uint8)
    source_alpha = source_rgba[:, :, 3]
    alternate = source_rgba.copy()
    alternate[source_alpha == 0, :3] = (13, 243, 31)
    alternate_image = Image.fromarray(alternate, "RGBA")
    edge = cv2.dilate(((source_alpha > 0) & (source_alpha < 255)).astype(np.uint8), np.ones((7, 7), dtype=np.uint8), iterations=1) > 0
    amounts = (0.0, 0.25, 0.5, 1.0, 2.0, 4.0, -2.0)
    settings = {"colors": PRESETS["vhs"]["colors"], "scale": 1.0, "dither": 0.0,
                "scanlines": 0.0, "grain": 0.0}
    background = np.array((0.5, 0.5, 0.5), dtype=np.float32)
    metrics: dict[str, float] = {"seed": float(seed), "edge pixel count": float(np.count_nonzero(edge))}
    cases: list[tuple[str, np.ndarray]] = []

    def composite(rgba: np.ndarray) -> np.ndarray:
        return rgba[:, :, :3] * rgba[:, :, 3:4] + background[None, None, :] * (1.0 - rgba[:, :, 3:4])

    reference: np.ndarray | None = None
    for amount in amounts:
        base = np.asarray(apply_retro(fixture, **settings, aberration=amount, seed=seed), dtype=np.float32) / 255.0
        alt = np.asarray(apply_retro(alternate_image, **settings, aberration=amount, seed=seed), dtype=np.float32) / 255.0
        if reference is None:
            reference = base
        base_composite = composite(base)
        alt_composite = composite(alt)
        hidden_delta = np.abs(base_composite - alt_composite)
        visible_delta = np.abs(base_composite - composite(reference))
        label = f"offset {amount:+.2f}px"
        metrics[f"{label} hidden composite edge MAE"] = float(np.mean(hidden_delta[edge]))
        metrics[f"{label} hidden composite full MAE"] = float(np.mean(hidden_delta))
        metrics[f"{label} visible composite edge MAE vs zero"] = float(np.mean(visible_delta[edge]))
        metrics[f"{label} edge RGB MAE vs zero"] = float(np.mean(np.abs(base[:, :, :3] - reference[:, :, :3])[edge]))
        metrics[f"{label} alpha exact"] = float(np.array_equal(base[:, :, 3], source_alpha.astype(np.float32) / 255.0))
        # Same 1px gap and hair-strand crops in every column, with visible and alternate hidden-RGB render.
        for pair_index, (pair_name, image) in enumerate((("visible", base_composite), ("alternate hidden RGB", alt_composite))):
            for region_index, (region_name, roi) in enumerate((
                ("1px gap", (18, 32, 90, 135)),
                ("strands", (0, 205, 480, 300)),
            )):
                x0, y0, x1, y1 = roi
                crop = image[y0:y1, x0:x1]
                thumb = Image.fromarray(np.uint8(np.clip(crop * 255.0 + 0.5, 0, 255)), "RGB").resize((240, 150), Image.Resampling.NEAREST)
                cases.append((f"{label} {pair_name} {region_name}", np.asarray(thumb)))
    cell_w, cell_h, label_h, columns = 240, 172, 22, 4
    rows = (len(cases) + columns - 1) // columns
    sheet = Image.new("RGB", (columns * cell_w, rows * cell_h), (28, 28, 32))
    draw = ImageDraw.Draw(sheet)
    for index, (label, pixels) in enumerate(cases):
        row, column = divmod(index, columns)
        left, top = column * cell_w, row * cell_h
        draw.text((left + 4, top + 4), label, fill=(245, 245, 245), font=ImageFont.load_default())
        sheet.paste(Image.fromarray(pixels, "RGB"), (left, top + label_h))
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    return metrics


def alpha_resize_real_corpus_study(corpus_dir: Path, output: Path, seed: int = 109) -> dict[str, float]:
    """Compare alpha resize policies on permission-cleared real cutout images."""
    import cv2

    provenance_path = corpus_dir / "provenance.json"
    if not provenance_path.is_file():
        raise ValueError(f"missing provenance manifest: {provenance_path}")
    provenance = json.loads(provenance_path.read_text(encoding="utf-8-sig"))
    image_paths = sorted(path for path in corpus_dir.glob("*.png") if path.name in provenance)
    if not image_paths:
        raise ValueError(f"no PNG inputs with provenance in {corpus_dir}")
    policies = ("straight", "nearest", "dilated", "premultiplied")
    backgrounds = {
        "black": np.array((0.0, 0.0, 0.0), dtype=np.float32),
        "mid-gray": np.array((0.5, 0.5, 0.5), dtype=np.float32),
        "white": np.array((1.0, 1.0, 1.0), dtype=np.float32),
    }
    metrics: dict[str, float] = {"seed": float(seed), "asset count": float(len(image_paths))}
    columns, cell_w, cell_h, label_h = 2 * len(policies), 220, 240, 22
    sheet = Image.new("RGB", (columns * cell_w, len(image_paths) * cell_h), (28, 28, 32))
    draw = ImageDraw.Draw(sheet)

    def composite(rgba: np.ndarray, bg: np.ndarray) -> np.ndarray:
        return rgba[:, :, :3] * rgba[:, :, 3:4] + bg[None, None, :] * (1.0 - rgba[:, :, 3:4])

    case_row = 0
    for image_index, image_path in enumerate(image_paths):
        with Image.open(image_path) as opened:
            source_image = opened.convert("RGBA")
        source_rgba = np.asarray(source_image, dtype=np.uint8)
        source = source_rgba.astype(np.float32) / 255.0
        alpha = source[:, :, 3]
        binary_edge = cv2.dilate((alpha > 0.0).astype(np.uint8), np.ones((3, 3), dtype=np.uint8), iterations=1) != cv2.erode((alpha > 0.0).astype(np.uint8), np.ones((3, 3), dtype=np.uint8), iterations=1)
        partial_edge = cv2.dilate(((alpha > 0.0) & (alpha < 1.0)).astype(np.uint8), np.ones((7, 7), dtype=np.uint8), iterations=1) > 0
        edge = binary_edge | partial_edge
        alternate = source_rgba.copy()
        alternate[alpha == 0.0, :3] = (13, 243, 31)
        alternate_image = Image.fromarray(alternate, "RGBA")
        source_hash = hashlib.sha256(image_path.read_bytes()).hexdigest()
        metrics[f"{image_path.name} width"] = float(source.shape[1])
        metrics[f"{image_path.name} height"] = float(source.shape[0])
        metrics[f"{image_path.name} alpha edge ratio"] = float(np.mean(edge))
        metrics[f"{image_path.name} transparent ratio"] = float(np.mean(alpha == 0.0))
        metrics[f"{image_path.name} partial-alpha ratio"] = float(np.mean((alpha > 0.0) & (alpha < 1.0)))
        metrics[f"{image_path.name} sha256 numeric"] = float(int(source_hash[:12], 16))
        if not provenance[image_path.name].get("license_or_permission"):
            raise ValueError(f"license missing for {image_path.name}")

        for preset_name in ("handheld", "arcade"):
            settings = PRESETS[preset_name]
            baseline = np.asarray(apply_retro(source_image, **settings, seed=seed), dtype=np.float32) / 255.0
            baseline_comp = composite(baseline, backgrounds["mid-gray"])
            for policy_index, policy in enumerate(policies):
                image = np.asarray(apply_retro(source_image, **settings, seed=seed, alpha_resize=policy), dtype=np.float32) / 255.0
                image_alt = np.asarray(apply_retro(alternate_image, **settings, seed=seed, alpha_resize=policy), dtype=np.float32) / 255.0
                label = f"{image_path.name} {preset_name} {policy}"
                alpha_bytes = np.uint8(np.clip(image[:, :, 3] * 255.0 + 0.5, 0, 255))
                metrics[f"{label} alpha exact"] = float(np.array_equal(alpha_bytes, source_rgba[:, :, 3]))
                metrics[f"{label} edge composite MAE vs straight"] = float(np.mean(np.abs(composite(image, backgrounds["mid-gray"])[edge] - baseline_comp[edge])))
                for background_name, background in backgrounds.items():
                    hidden = np.abs(composite(image, background) - composite(image_alt, background))
                    metrics[f"{label} {background_name} hidden composite edge MAE"] = float(np.mean(hidden[edge]))
                display = composite(image, backgrounds["mid-gray"])
                thumb_image = Image.fromarray(np.uint8(np.clip(display * 255.0 + 0.5, 0, 255)), "RGB")
                thumb_image.thumbnail((cell_w - 8, cell_h - label_h - 8), Image.Resampling.LANCZOS)
                panel = Image.new("RGB", (cell_w, cell_h - label_h), (82, 82, 88))
                panel.paste(thumb_image, ((cell_w - thumb_image.width) // 2, (panel.height - thumb_image.height) // 2))
                column = (0 if preset_name == "handheld" else len(policies)) + policy_index
                left, top = column * cell_w, case_row * cell_h
                draw.text((left + 4, top + 4), f"{preset_name} {policy}", fill=(245, 245, 245), font=ImageFont.load_default())
                sheet.paste(panel, (left, top + label_h))
        draw.text((0, case_row * cell_h + cell_h - 13), f"{image_path.name} | {source.shape[1]}×{source.shape[0]} | SHA256 {source_hash[:16]}", fill=(235, 235, 235), font=ImageFont.load_default())
        case_row += 1
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    return metrics


def alpha_hidden_source_residual_study(corpus_dir: Path, output: Path, seed: int = 127) -> dict[str, float]:
    """Measure leakage from RGB already present in a real asset's transparent pixels."""
    import cv2

    image_path = corpus_dir / "vegetation_clover_02a.png"
    provenance_path = corpus_dir / "provenance.json"
    if not image_path.is_file() or not provenance_path.is_file():
        raise ValueError("expected vegetation_clover_02a.png and provenance.json in corpus directory")
    provenance = json.loads(provenance_path.read_text(encoding="utf-8-sig"))
    if not provenance.get(image_path.name, {}).get("license_or_permission"):
        raise ValueError("license or permission must be recorded for the selected source")
    with Image.open(image_path) as opened:
        source_image = opened.convert("RGBA")
    source_rgba = np.asarray(source_image, dtype=np.uint8)
    source = source_rgba.astype(np.float32) / 255.0
    alpha_bytes = source_rgba[:, :, 3]
    alpha = source[:, :, 3]
    transparent = alpha_bytes == 0
    alt_array = source_rgba.copy()
    alt_array[transparent, :3] = 0
    zero_hidden_image = Image.fromarray(alt_array, "RGBA")
    support = (alpha_bytes > 0).astype(np.uint8)
    edge = (cv2.dilate(support, np.ones((3, 3), dtype=np.uint8), iterations=1) !=
            cv2.erode(support, np.ones((3, 3), dtype=np.uint8), iterations=1))
    edge |= cv2.dilate(((alpha_bytes > 0) & (alpha_bytes < 255)).astype(np.uint8),
                       np.ones((7, 7), dtype=np.uint8), iterations=1) > 0
    near_hidden = cv2.dilate(support, np.ones((17, 17), dtype=np.uint8), iterations=1) > 0
    # Restrict this residual profile to fully transparent samples, splitting close-to-object from deep padding.
    near_hidden &= transparent
    deep_hidden = transparent & ~near_hidden
    near_zero_array = source_rgba.copy()
    near_zero_array[near_hidden, :3] = 0
    near_zero_image = Image.fromarray(near_zero_array, "RGBA")
    deep_zero_array = source_rgba.copy()
    deep_zero_array[deep_hidden, :3] = 0
    deep_zero_image = Image.fromarray(deep_zero_array, "RGBA")
    hidden_rgb = source_rgba[:, :, :3][transparent]
    metrics: dict[str, float] = {
        "seed": float(seed),
        "source width": float(source.shape[1]),
        "source height": float(source.shape[0]),
        "source SHA256 numeric": float(int(hashlib.sha256(image_path.read_bytes()).hexdigest()[:12], 16)),
        "hidden RGB all-zero ratio": float(np.mean(np.all(hidden_rgb == 0, axis=1))),
        "hidden RGB unique colors": float(np.unique(hidden_rgb, axis=0).shape[0]),
        "hidden RGB mean red": float(np.mean(hidden_rgb[:, 0])),
        "hidden RGB mean green": float(np.mean(hidden_rgb[:, 1])),
        "hidden RGB mean blue": float(np.mean(hidden_rgb[:, 2])),
        "near-hidden pixel count": float(np.count_nonzero(near_hidden)),
        "deep-hidden pixel count": float(np.count_nonzero(deep_hidden)),
        "edge ROI pixel count": float(np.count_nonzero(edge)),
    }
    backgrounds = {
        "black": np.array((0.0, 0.0, 0.0), dtype=np.float32),
        "mid-gray": np.array((0.5, 0.5, 0.5), dtype=np.float32),
        "white": np.array((1.0, 1.0, 1.0), dtype=np.float32),
    }
    cases: list[tuple[str, np.ndarray]] = []

    def composite(rgba: np.ndarray, bg: np.ndarray) -> np.ndarray:
        return rgba[:, :, :3] * rgba[:, :, 3:4] + bg[None, None, :] * (1.0 - rgba[:, :, 3:4])

    for preset_name in ("handheld", "arcade", "print", "vhs"):
        settings = PRESETS[preset_name]
        for policy in ("straight", "nearest", "dilated", "premultiplied"):
            original = np.asarray(apply_retro(source_image, **settings, seed=seed, alpha_resize=policy), dtype=np.float32) / 255.0
            zero_hidden = np.asarray(apply_retro(zero_hidden_image, **settings, seed=seed, alpha_resize=policy), dtype=np.float32) / 255.0
            near_zero = np.asarray(apply_retro(near_zero_image, **settings, seed=seed, alpha_resize=policy), dtype=np.float32) / 255.0
            deep_zero = np.asarray(apply_retro(deep_zero_image, **settings, seed=seed, alpha_resize=policy), dtype=np.float32) / 255.0
            edge_delta = np.abs(composite(original, backgrounds["black"]) - composite(zero_hidden, backgrounds["black"]))
            near_delta = np.abs(composite(original, backgrounds["black"]) - composite(near_zero, backgrounds["black"]))
            deep_delta = np.abs(composite(original, backgrounds["black"]) - composite(deep_zero, backgrounds["black"]))
            label = f"{preset_name} {policy}"
            metrics[f"{label} hidden source-vs-zero edge composite MAE"] = float(np.mean(edge_delta[edge]))
            metrics[f"{label} hidden near-source-only edge composite MAE"] = float(np.mean(near_delta[edge]))
            metrics[f"{label} hidden deep-source-only edge composite MAE"] = float(np.mean(deep_delta[edge]))
            metrics[f"{label} alpha exact"] = float(np.array_equal(
                np.uint8(np.clip(original[:, :, 3] * 255.0 + 0.5, 0, 255)), alpha_bytes
            ))
            display = composite(original, backgrounds["mid-gray"])
            thumb = Image.fromarray(np.uint8(np.clip(display * 255.0 + 0.5, 0, 255)), "RGB")
            thumb.thumbnail((280, 320), Image.Resampling.LANCZOS)
            cases.append((label, np.asarray(thumb)))

    columns, cell_w, cell_h, label_h = 4, 280, 344, 22
    sheet = Image.new("RGB", (columns * cell_w, 2 * cell_h), (28, 28, 32))
    draw = ImageDraw.Draw(sheet)
    for index, (label, thumb) in enumerate(cases):
        row, column = divmod(index, columns)
        left, top = column * cell_w, row * cell_h
        draw.text((left + 4, top + 4), label, fill=(245, 245, 245), font=ImageFont.load_default())
        sheet.paste(Image.fromarray(thumb, "RGB"), (left + 4, top + label_h + 4))
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    return metrics


def core_chromatic_alpha_source_model(corpus_dir: Path, output: Path) -> dict[str, float]:
    """Model the current Core integer/clamp RGB channel shifts on synthetic and CC0 RGBA."""
    import cv2

    real_path = corpus_dir / "vegetation_clover_02a.png"
    provenance_path = corpus_dir / "provenance.json"
    if not real_path.is_file() or not provenance_path.is_file():
        raise ValueError("expected a provenance-tracked vegetation_clover_02a.png")
    provenance = json.loads(provenance_path.read_text(encoding="utf-8-sig"))
    if not provenance.get(real_path.name, {}).get("license_or_permission"):
        raise ValueError("license or permission must be recorded for the real input")
    sources = [("CC0 vegetation clover", Image.open(real_path).convert("RGBA")),
               ("synthetic alpha chart", make_alpha_resize_suite_fixture())]
    cases = ((0.0, 0.0), (1.0, 0.0), (2.0, 0.0), (4.0, 0.0), (2.0, 90.0))
    metrics: dict[str, float] = {}
    panels: list[tuple[str, np.ndarray]] = []

    def sample_rgb(rgb: np.ndarray, alpha: np.ndarray, amount: float, angle_deg: float,
                   suppress_zero_alpha_samples: bool) -> np.ndarray:
        h, w, _ = rgb.shape
        angle = angle_deg * np.pi / 180.0
        raw_shift_x = amount * np.cos(angle)
        raw_shift_y = amount * np.sin(angle)
        shift_x = int(np.floor(raw_shift_x + 0.5)) if raw_shift_x >= 0 else int(np.ceil(raw_shift_x - 0.5))
        shift_y = int(np.floor(raw_shift_y + 0.5)) if raw_shift_y >= 0 else int(np.ceil(raw_shift_y - 0.5))
        yy, xx = np.mgrid[0:h, 0:w]
        result = rgb.copy()
        for channel, direction in ((0, 1), (2, -1)):
            sx = np.clip(xx + direction * shift_x, 0, w - 1)
            sy = np.clip(yy + direction * shift_y, 0, h - 1)
            sampled = rgb[sy, sx, channel]
            if suppress_zero_alpha_samples:
                sampled = np.where(alpha[sy, sx] > 0.0, sampled, 0.0)
            result[:, :, channel] = sampled
        return np.clip(result, 0.0, 1.0)

    for source_name, image in sources:
        rgba = np.asarray(image, dtype=np.uint8).astype(np.float32) / 255.0
        alpha = rgba[:, :, 3]
        transparent = alpha == 0.0
        zero_hidden = rgba.copy()
        zero_hidden[transparent, :3] = 0.0
        edge = cv2.dilate(((alpha > 0.0) & (alpha < 1.0)).astype(np.uint8),
                          np.ones((7, 7), dtype=np.uint8), iterations=1) > 0
        support = (alpha > 0.0).astype(np.uint8)
        edge |= cv2.dilate(support, np.ones((3, 3), dtype=np.uint8), iterations=1) != cv2.erode(
            support, np.ones((3, 3), dtype=np.uint8), iterations=1
        )
        metrics[f"{source_name} edge ROI ratio"] = float(np.mean(edge))
        for amount, angle in cases:
            base = sample_rgb(rgba[:, :, :3], alpha, amount, angle, False)
            alt = sample_rgb(zero_hidden[:, :, :3], alpha, amount, angle, False)
            alpha_aware = sample_rgb(rgba[:, :, :3], alpha, amount, angle, True)
            alpha_aware_alt = sample_rgb(zero_hidden[:, :, :3], alpha, amount, angle, True)
            key = f"{source_name} amount={amount:.1f} angle={angle:.0f}"
            base_comp = base * alpha[:, :, None]
            alt_comp = alt * alpha[:, :, None]
            aware_comp = alpha_aware * alpha[:, :, None]
            aware_alt_comp = alpha_aware_alt * alpha[:, :, None]
            metrics[f"{key} Core-model hidden-pair composite edge MAE"] = float(np.mean(np.abs(base_comp - alt_comp)[edge]))
            metrics[f"{key} alpha-aware hidden-pair composite edge MAE"] = float(np.mean(np.abs(aware_comp - aware_alt_comp)[edge]))
            metrics[f"{key} visible composite edge MAE vs alpha-aware"] = float(np.mean(np.abs(base_comp - aware_comp)[edge]))
            metrics[f"{key} rounded shift X"] = float(np.floor(amount * np.cos(angle * np.pi / 180.0) + 0.5))
            metrics[f"{key} rounded shift Y"] = float(np.floor(amount * np.sin(angle * np.pi / 180.0) + 0.5))
            if (amount, angle) in ((1.0, 0.0), (4.0, 0.0), (2.0, 90.0)):
                display_rows = (
                    ("Core source RGB", base_comp),
                    ("Core hidden RGB zeroed", alt_comp),
                    ("alpha-aware source RGB", aware_comp),
                    ("alpha-aware pair", aware_comp),
                )
                for label, composite in display_rows:
                    thumb = Image.fromarray(np.uint8(np.clip(composite * 255.0 + 0.5, 0, 255)), "RGB")
                    thumb.thumbnail((220, 250), Image.Resampling.LANCZOS)
                    panels.append((f"{source_name} {amount:.0f}px/{angle:.0f}deg {label}", np.asarray(thumb)))

    cell_w, cell_h, label_h, columns = 220, 278, 22, 4
    rows = (len(panels) + columns - 1) // columns
    sheet = Image.new("RGB", (columns * cell_w, rows * cell_h), (28, 28, 32))
    draw = ImageDraw.Draw(sheet)
    for index, (label, thumb) in enumerate(panels):
        row, column = divmod(index, columns)
        left, top = column * cell_w, row * cell_h
        draw.text((left + 3, top + 4), label[:34], fill=(245, 245, 245), font=ImageFont.load_default())
        sheet.paste(Image.fromarray(thumb, "RGB"), (left + 3, top + label_h + 3))
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    return metrics


def rasterizer_chromatic_alpha_source_model(corpus_dir: Path, output: Path) -> dict[str, float]:
    """Model Artifact Rasterizer CPU/HLSL radial sample coordinates from source."""
    import cv2

    real_path = corpus_dir / "vegetation_clover_02a.png"
    provenance_path = corpus_dir / "provenance.json"
    if not real_path.is_file() or not provenance_path.is_file():
        raise ValueError("expected a provenance-tracked vegetation_clover_02a.png")
    provenance = json.loads(provenance_path.read_text(encoding="utf-8-sig"))
    if not provenance.get(real_path.name, {}).get("license_or_permission"):
        raise ValueError("license or permission must be recorded for the real input")
    sources = [("CC0 vegetation clover", Image.open(real_path).convert("RGBA")),
               ("synthetic alpha chart", make_alpha_resize_suite_fixture())]
    settings = ((0.0, 0.0, 0.5, 0.5), (2.0, 2.0, 0.5, 0.5),
                (8.0, 8.0, 0.5, 0.5), (4.0, 1.0, 0.5, 0.5),
                (2.0, 2.0, 0.0, 0.0))
    metrics: dict[str, float] = {}
    panels: list[tuple[str, np.ndarray]] = []

    def render(rgb: np.ndarray, alpha: np.ndarray, red_shift: float, blue_shift: float,
               center_x: float, center_y: float, suppress_zero_alpha_samples: bool) -> np.ndarray:
        height, width, _ = rgb.shape
        yy, xx = np.mgrid[0:height, 0:width]
        cx, cy = center_x * width, center_y * height
        dx, dy = xx.astype(np.float32) - cx, yy.astype(np.float32) - cy
        distance = np.sqrt(dx * dx + dy * dy)
        normalized = distance / max(max(width, height) * 0.7, 1.0)
        inverse = np.divide(1.0, distance, out=np.zeros_like(distance), where=distance > 0.0)
        direction_x, direction_y = dx * inverse, dy * inverse
        red_offset, blue_offset = normalized * red_shift, normalized * blue_shift
        red_x = np.clip((xx + direction_x * red_offset + 0.5).astype(np.int32), 0, width - 1)
        red_y = np.clip((yy + direction_y * red_offset + 0.5).astype(np.int32), 0, height - 1)
        blue_x = np.clip((xx - direction_x * blue_offset + 0.5).astype(np.int32), 0, width - 1)
        blue_y = np.clip((yy - direction_y * blue_offset + 0.5).astype(np.int32), 0, height - 1)
        red = rgb[red_y, red_x, 0]
        blue = rgb[blue_y, blue_x, 2]
        if suppress_zero_alpha_samples:
            red = np.where(alpha[red_y, red_x] > 0.0, red, 0.0)
            blue = np.where(alpha[blue_y, blue_x] > 0.0, blue, 0.0)
        result = rgb.copy()
        result[:, :, 0] = red
        result[:, :, 2] = blue
        return np.clip(result, 0.0, 1.0)

    backgrounds = {
        "black": np.array((0.0, 0.0, 0.0), dtype=np.float32),
        "midgray": np.array((0.5, 0.5, 0.5), dtype=np.float32),
        "white": np.array((1.0, 1.0, 1.0), dtype=np.float32),
        "green": np.array((0.05, 0.42, 0.16), dtype=np.float32),
    }
    for source_name, image in sources:
        rgba = np.asarray(image, dtype=np.uint8).astype(np.float32) / 255.0
        alpha = rgba[:, :, 3]
        alt = rgba.copy()
        alt[alpha == 0.0, :3] = 0.0
        binary_edge = cv2.dilate((alpha > 0.0).astype(np.uint8), np.ones((3, 3), dtype=np.uint8), iterations=1) != cv2.erode(
            (alpha > 0.0).astype(np.uint8), np.ones((3, 3), dtype=np.uint8), iterations=1
        )
        partial_edge = cv2.dilate(((alpha > 0.0) & (alpha < 1.0)).astype(np.uint8), np.ones((7, 7), dtype=np.uint8), iterations=1) > 0
        edge = binary_edge | partial_edge
        for red_shift, blue_shift, center_x, center_y in settings:
            original_output = render(rgba[:, :, :3], alpha, red_shift, blue_shift, center_x, center_y, False)
            alt_output = render(alt[:, :, :3], alpha, red_shift, blue_shift, center_x, center_y, False)
            aware_output = render(rgba[:, :, :3], alpha, red_shift, blue_shift, center_x, center_y, True)
            aware_alt = render(alt[:, :, :3], alpha, red_shift, blue_shift, center_x, center_y, True)
            original_comp = original_output * alpha[:, :, None]
            alt_comp = alt_output * alpha[:, :, None]
            aware_comp = aware_output * alpha[:, :, None]
            aware_alt_comp = aware_alt * alpha[:, :, None]
            label = f"{source_name} red={red_shift:g} blue={blue_shift:g} center={center_x:g},{center_y:g}"
            metrics[f"{label} current-model hidden pair edge composite MAE"] = float(np.mean(np.abs(original_comp - alt_comp)[edge]))
            metrics[f"{label} alpha-aware-reference hidden pair edge composite MAE"] = float(np.mean(np.abs(aware_comp - aware_alt_comp)[edge]))
            metrics[f"{label} visible edge composite MAE vs reference"] = float(np.mean(np.abs(original_comp - aware_comp)[edge]))
            visible = alpha > 0.0
            for background_name, background in backgrounds.items():
                original_bg = original_output * alpha[:, :, None] + background * (1.0 - alpha[:, :, None])
                alt_bg = alt_output * alpha[:, :, None] + background * (1.0 - alpha[:, :, None])
                aware_bg = aware_output * alpha[:, :, None] + background * (1.0 - alpha[:, :, None])
                aware_alt_bg = aware_alt * alpha[:, :, None] + background * (1.0 - alpha[:, :, None])
                metrics[f"{label} {background_name} hidden-pair edge MAE"] = float(np.mean(np.abs(original_bg - alt_bg)[edge]))
                metrics[f"{label} {background_name} hidden-pair visible MAE"] = float(np.mean(np.abs(original_bg - alt_bg)[visible]))
                metrics[f"{label} {background_name} alpha-aware visible change MAE"] = float(np.mean(np.abs(original_bg - aware_bg)[visible]))
            if (red_shift, blue_shift, center_x, center_y) in ((2.0, 2.0, 0.5, 0.5), (4.0, 1.0, 0.5, 0.5)):
                for suffix, composite in (("current source RGB", original_comp), ("zero hidden RGB", alt_comp),
                                          ("suppressed source", aware_comp), ("suppressed pair", aware_alt_comp)):
                    thumb = Image.fromarray(np.uint8(np.clip(composite * 255.0 + 0.5, 0, 255)), "RGB")
                    thumb.thumbnail((280, 320), Image.Resampling.LANCZOS)
                    panels.append((f"{source_name} r{red_shift:g}/b{blue_shift:g} {suffix}", np.asarray(thumb)))
    cell_w, cell_h, label_h, columns = 280, 344, 22, 4
    rows = (len(panels) + columns - 1) // columns
    sheet = Image.new("RGB", (columns * cell_w, rows * cell_h), (28, 28, 32))
    draw = ImageDraw.Draw(sheet)
    for index, (label, thumb) in enumerate(panels):
        row, column = divmod(index, columns)
        left, top = column * cell_w, row * cell_h
        draw.text((left + 4, top + 4), label[:42], fill=(245, 245, 245), font=ImageFont.load_default())
        sheet.paste(Image.fromarray(thumb, "RGB"), (left + 4, top + label_h + 4))
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    return metrics


def alpha_hidden_distance_residual_study(corpus_dir: Path, output: Path) -> dict[str, float]:
    """Measure transparent RGB residual by distance to visible alpha support."""
    import cv2

    image_path = corpus_dir / "vegetation_clover_02a.png"
    provenance_path = corpus_dir / "provenance.json"
    if not image_path.is_file() or not provenance_path.is_file():
        raise ValueError("expected a provenance-tracked vegetation_clover_02a.png")
    provenance = json.loads(provenance_path.read_text(encoding="utf-8-sig"))
    if not provenance.get(image_path.name, {}).get("license_or_permission"):
        raise ValueError("license or permission must be recorded for the real input")
    rgba = np.asarray(Image.open(image_path).convert("RGBA"), dtype=np.uint8)
    alpha = rgba[:, :, 3]
    visible = alpha > 0
    hidden = alpha == 0
    rgb = rgba[:, :, :3].astype(np.float32) / 255.0
    metrics: dict[str, float] = {}
    panels: list[tuple[str, np.ndarray]] = []
    for radius in (1, 2, 4, 8, 16, 32, 64):
        support = cv2.dilate(visible.astype(np.uint8), np.ones((3, 3), dtype=np.uint8), iterations=radius) > 0
        ring = hidden & support
        distance_values = rgb.mean(axis=2)
        metrics[f"alpha0 distance<={radius}px pixels"] = int(np.count_nonzero(ring))
        metrics[f"alpha0 distance<={radius}px changedRGB ratio"] = float(np.mean(distance_values[ring] > 0.0)) if np.any(ring) else 0.0
        metrics[f"alpha0 distance<={radius}px mean RGB residual"] = float(np.mean(distance_values[ring])) if np.any(ring) else 0.0
        metrics[f"alpha0 distance<={radius}px p95 RGB residual"] = float(np.percentile(distance_values[ring], 95)) if np.any(ring) else 0.0
        if radius in (1, 4, 8, 16, 32, 64):
            preview = np.zeros((*alpha.shape, 3), dtype=np.float32)
            preview[visible] = rgb[visible]
            preview[ring] = np.maximum(rgb[ring], np.array((0.95, 0.25, 0.1), dtype=np.float32))
            thumb = Image.fromarray(np.uint8(np.clip(preview * 255 + 0.5, 0, 255)), "RGB")
            thumb.thumbnail((240, 300), Image.Resampling.LANCZOS)
            panels.append((f"hidden RGB within {radius}px", np.asarray(thumb)))
    metrics["alpha0 total pixels"] = int(np.count_nonzero(hidden))
    metrics["alpha0 nonblack RGB ratio"] = float(np.mean(np.any(rgba[hidden, :3] != 0, axis=1))) if np.any(hidden) else 0.0
    metrics["alpha0 mean RGB code value"] = float(np.mean(rgba[hidden, :3])) if np.any(hidden) else 0.0
    cell_w, cell_h, label_h, columns = 240, 324, 24, 3
    rows = (len(panels) + columns - 1) // columns
    sheet = Image.new("RGB", (columns * cell_w, rows * cell_h), (30, 30, 34))
    draw = ImageDraw.Draw(sheet)
    for index, (label, thumb) in enumerate(panels):
        row, column = divmod(index, columns)
        left, top = column * cell_w, row * cell_h
        draw.text((left + 4, top + 4), label, fill=(245, 245, 245), font=ImageFont.load_default())
        sheet.paste(Image.fromarray(thumb, "RGB"), (left + 4, top + label_h + 4))
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    return metrics


def rasterizer_chromatic_transfer_domain_study(corpus_dir: Path, output: Path) -> dict[str, float]:
    """Compare sRGB-code and linear-light sampling for the same profiled cutout."""
    import cv2

    image_path = corpus_dir / "vegetation_clover_02a.png"
    provenance_path = corpus_dir / "provenance.json"
    if not image_path.is_file() or not provenance_path.is_file():
        raise ValueError("expected a provenance-tracked vegetation_clover_02a.png")
    provenance = json.loads(provenance_path.read_text(encoding="utf-8-sig"))
    if not provenance.get(image_path.name, {}).get("license_or_permission"):
        raise ValueError("license or permission must be recorded for the real input")
    from PIL import ImageCms
    with Image.open(image_path) as opened:
        rgba8 = np.asarray(opened.convert("RGBA"), dtype=np.uint8)
        icc = opened.info.get("icc_profile")
    profile_description = "missing"
    if icc:
        profile_description = ImageCms.getProfileDescription(
            ImageCms.ImageCmsProfile(__import__("io").BytesIO(icc))
        ).strip()
    rgba_srgb = rgba8.astype(np.float32) / 255.0
    rgba_linear = rgba_srgb.copy()
    rgba_linear[:, :, :3] = srgb_to_linear(rgba_linear[:, :, :3])
    alpha = rgba_srgb[:, :, 3]
    visible = alpha > 0.0
    kernel = np.ones((3, 3), dtype=np.uint8)
    binary_edge = cv2.dilate(visible.astype(np.uint8), kernel, iterations=1) != cv2.erode(
        visible.astype(np.uint8), kernel, iterations=1
    )
    partial_edge = cv2.dilate(((alpha > 0.0) & (alpha < 1.0)).astype(np.uint8), np.ones((7, 7), dtype=np.uint8), iterations=1) > 0
    edge = binary_edge | partial_edge
    backgrounds = {
        "black": np.array((0.0, 0.0, 0.0), dtype=np.float32),
        "midgray": np.array((0.5, 0.5, 0.5), dtype=np.float32),
        "white": np.array((1.0, 1.0, 1.0), dtype=np.float32),
    }
    metrics: dict[str, float] = {"ICC profile description recognized by Pillow": 1.0 if "sRGB IEC61966-2.1" in profile_description else 0.0}
    metrics["ICC profile bytes"] = float(len(icc) if icc else 0)
    panels: list[tuple[str, np.ndarray]] = []
    for domain, rgba in (("sRGB-code", rgba_srgb), ("linear-light", rgba_linear)):
        sanitized = rgba.copy()
        sanitized[alpha == 0.0, :3] = 0.0
        for shift in (2.0, 8.0):
            current = production_radial_nearest_rgba(rgba, shift, shift)
            candidate = production_radial_nearest_rgba(sanitized, shift, shift)
            for background_name, background in backgrounds.items():
                current_composite = current[:, :, :3] * alpha[:, :, None] + background * (1.0 - alpha[:, :, None])
                candidate_composite = candidate[:, :, :3] * alpha[:, :, None] + background * (1.0 - alpha[:, :, None])
                if domain == "linear-light":
                    current_display = linear_to_srgb(np.clip(current_composite, 0.0, 1.0))
                    candidate_display = linear_to_srgb(np.clip(candidate_composite, 0.0, 1.0))
                else:
                    current_display = current_composite
                    candidate_display = candidate_composite
                delta = np.abs(current_display - candidate_display)
                metrics[f"{domain} R{shift:g}/B{shift:g} {background_name} edge hidden-pair display MAE"] = float(np.mean(delta[edge]))
                metrics[f"{domain} R{shift:g}/B{shift:g} {background_name} visible suppression display MAE"] = float(np.mean(delta[visible]))
            if shift == 8.0:
                composite = current[:, :, :3] * alpha[:, :, None]
                if domain == "linear-light":
                    composite = linear_to_srgb(np.clip(composite, 0.0, 1.0))
                thumb = Image.fromarray(np.uint8(np.clip(composite * 255.0 + 0.5, 0, 255)), "RGB")
                thumb.thumbnail((280, 320), Image.Resampling.LANCZOS)
                panels.append((f"{domain} current R8/B8", np.asarray(thumb)))
    cell_w, cell_h, label_h = 280, 344, 24
    sheet = Image.new("RGB", (cell_w * len(panels), cell_h), (28, 28, 32))
    draw = ImageDraw.Draw(sheet)
    for index, (label, thumb) in enumerate(panels):
        x = index * cell_w
        draw.text((x + 4, 4), label, fill=(245, 245, 245), font=ImageFont.load_default())
        sheet.paste(Image.fromarray(thumb, "RGB"), (x + 4, label_h + 4))
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    return metrics


def image_layer_rasterizer_boundary_study(corpus_dir: Path, output: Path) -> dict[str, float]:
    """Model the ImageF32x4->QImage transparent-RGB cleanup before Rasterizer."""
    import cv2

    image_path = corpus_dir / "vegetation_clover_02a.png"
    provenance_path = corpus_dir / "provenance.json"
    if not image_path.is_file() or not provenance_path.is_file():
        raise ValueError("expected a provenance-tracked vegetation_clover_02a.png")
    provenance = json.loads(provenance_path.read_text(encoding="utf-8-sig"))
    if not provenance.get(image_path.name, {}).get("license_or_permission"):
        raise ValueError("license or permission must be recorded for the real input")
    rgba8 = np.asarray(Image.open(image_path).convert("RGBA"), dtype=np.uint8)
    source = rgba8.astype(np.float32) / 255.0
    alpha = source[:, :, 3]
    visible = alpha > 0.0
    edge = cv2.dilate(visible.astype(np.uint8), np.ones((3, 3), dtype=np.uint8), iterations=1) != cv2.erode(
        visible.astype(np.uint8), np.ones((3, 3), dtype=np.uint8), iterations=1
    )
    edge |= cv2.dilate(((alpha > 0.0) & (alpha < 1.0)).astype(np.uint8), np.ones((7, 7), dtype=np.uint8), iterations=1) > 0
    hidden_pair = source.copy()
    hidden_pair[alpha == 0.0, :3] = 0.0
    # ImageF32x4_RGBA::toQImage requests Rgba8SrgbStraight; the direct
    # sRGB-roundtrip branch zeros RGB at alpha <= 1e-6 even for unknown alpha.
    image_layer_boundary = source.copy()
    image_layer_boundary[alpha <= 1.0e-6, :3] = 0.0
    shift_settings = (2.0, 8.0)
    metrics: dict[str, float] = {
        "source alpha0 hidden pixels": int(np.count_nonzero(alpha == 0.0)),
        "source partial-alpha pixels": int(np.count_nonzero((alpha > 0.0) & (alpha < 1.0))),
        "toQImage boundary alpha0 RGB changed pixel ratio": float(np.mean(np.any(image_layer_boundary[alpha == 0.0, :3] != source[alpha == 0.0, :3], axis=1))),
    }
    panels: list[tuple[str, np.ndarray]] = []
    for shift in shift_settings:
        raw_output = production_radial_nearest_rgba(source, shift, shift)
        raw_pair_output = production_radial_nearest_rgba(hidden_pair, shift, shift)
        imported_output = production_radial_nearest_rgba(image_layer_boundary, shift, shift)
        no_shift_output = production_radial_nearest_rgba(image_layer_boundary, 0.0, 0.0)
        yy, xx = np.mgrid[0:rgba8.shape[0], 0:rgba8.shape[1]]
        dx, dy = xx.astype(np.float32) - rgba8.shape[1] * 0.5, yy.astype(np.float32) - rgba8.shape[0] * 0.5
        distance = np.sqrt(dx * dx + dy * dy)
        denom = max(max(rgba8.shape[1], rgba8.shape[0]) * 0.7, 1.0)
        ux = np.divide(dx, distance, out=np.zeros_like(dx), where=distance > 0.0)
        uy = np.divide(dy, distance, out=np.zeros_like(dy), where=distance > 0.0)
        rx = np.clip((xx + ux * (distance / denom * shift) + 0.5).astype(np.int32), 0, rgba8.shape[1] - 1)
        ry = np.clip((yy + uy * (distance / denom * shift) + 0.5).astype(np.int32), 0, rgba8.shape[0] - 1)
        bx = np.clip((xx - ux * (distance / denom * shift) + 0.5).astype(np.int32), 0, rgba8.shape[1] - 1)
        by = np.clip((yy - uy * (distance / denom * shift) + 0.5).astype(np.int32), 0, rgba8.shape[0] - 1)
        fallback_output = imported_output.copy()
        red_transparent_sample = rgba8[ry, rx, 3] == 0
        blue_transparent_sample = rgba8[by, bx, 3] == 0
        fallback_output[red_transparent_sample, 0] = image_layer_boundary[red_transparent_sample, 0]
        fallback_output[blue_transparent_sample, 2] = image_layer_boundary[blue_transparent_sample, 2]
        raw_pair_delta = np.abs(raw_output[:, :, :3] - raw_pair_output[:, :, :3]).mean(axis=2) * alpha
        boundary_delta = np.abs(raw_output[:, :, :3] - imported_output[:, :, :3]).mean(axis=2) * alpha
        effect_delta = np.abs(imported_output[:, :, :3] - no_shift_output[:, :, :3]).mean(axis=2) * alpha
        fallback_delta = np.abs(imported_output[:, :, :3] - fallback_output[:, :, :3]).mean(axis=2) * alpha
        metrics[f"R{shift:g}/B{shift:g} raw-source hidden-pair edge composite MAE"] = float(np.mean(raw_pair_delta[edge]))
        metrics[f"R{shift:g}/B{shift:g} after-toQImage hidden-pair edge composite MAE"] = float(np.mean(np.abs(
            imported_output[:, :, :3] - production_radial_nearest_rgba(hidden_pair, shift, shift)[:, :, :3]
        )[edge] * alpha[edge, None]))
        metrics[f"R{shift:g}/B{shift:g} hidden-RGB cleanup edge composite MAE"] = float(np.mean(boundary_delta[edge]))
        metrics[f"R{shift:g}/B{shift:g} effect-vs-no-shift edge composite MAE"] = float(np.mean(effect_delta[edge]))
        metrics[f"R{shift:g}/B{shift:g} transparent-sample fallback edge composite MAE"] = float(np.mean(fallback_delta[edge]))
        metrics[f"R{shift:g}/B{shift:g} fallback effect-vs-no-shift edge composite MAE"] = float(np.mean(np.abs(fallback_output[:, :, :3] - no_shift_output[:, :, :3]).mean(axis=2)[edge] * alpha[edge]))
        for channel_name, sample_alpha in (("red", rgba8[ry, rx, 3]), ("blue", rgba8[by, bx, 3])):
            samples = sample_alpha[edge & visible]
            metrics[f"R{shift:g}/B{shift:g} {channel_name} edge samples alpha0 ratio"] = float(np.mean(samples == 0))
            metrics[f"R{shift:g}/B{shift:g} {channel_name} edge samples partial-alpha ratio"] = float(np.mean((samples > 0) & (samples < 255)))
            metrics[f"R{shift:g}/B{shift:g} {channel_name} edge samples opaque ratio"] = float(np.mean(samples == 255))
        if shift == 8.0:
            for label, frame in (("raw source counterfactual", raw_output),
                                 ("ImageLayer toQImage boundary", imported_output),
                                 ("alpha0 sample fallback candidate", fallback_output),
                                 ("boundary no-shift", no_shift_output)):
                composite = frame[:, :, :3] * alpha[:, :, None]
                thumb = Image.fromarray(np.uint8(np.clip(composite * 255.0 + 0.5, 0, 255)), "RGB")
                thumb.thumbnail((280, 320), Image.Resampling.LANCZOS)
                panels.append((label, np.asarray(thumb)))
    cell_w, cell_h, label_h = 280, 344, 24
    sheet = Image.new("RGB", (cell_w * len(panels), cell_h), (28, 28, 32))
    draw = ImageDraw.Draw(sheet)
    for index, (label, thumb) in enumerate(panels):
        x = index * cell_w
        draw.text((x + 4, 4), label, fill=(245, 245, 245), font=ImageFont.load_default())
        sheet.paste(Image.fromarray(thumb, "RGB"), (x + 4, label_h + 4))
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    return metrics


def rasterizer_alpha_sample_policy_study(output: Path, fixture_path: Path) -> dict[str, float]:
    """Compare transparent/partial-alpha sample policies on an RGBA control."""
    import cv2

    fixture = make_alpha_resize_suite_fixture()
    fixture_path.parent.mkdir(parents=True, exist_ok=True)
    fixture.save(fixture_path)
    rgba8 = np.asarray(fixture, dtype=np.uint8)
    original = rgba8.astype(np.float32) / 255.0
    alpha = original[:, :, 3]
    visible = alpha > 0.0
    alpha0 = alpha == 0.0
    partial = (alpha > 0.0) & (alpha < 1.0)
    kernel3 = np.ones((3, 3), dtype=np.uint8)
    edge = (cv2.dilate(visible.astype(np.uint8), kernel3, iterations=1) !=
            cv2.erode(visible.astype(np.uint8), kernel3, iterations=1))
    edge |= cv2.dilate(partial.astype(np.uint8), np.ones((7, 7), dtype=np.uint8), iterations=1) > 0
    strand_roi = np.zeros_like(edge)
    strand_roi[205:300, :] = True
    strand_roi &= edge
    gap_rois: dict[str, np.ndarray] = {}
    for x0, y0, gap in ((24, 44, 1), (140, 44, 2), (256, 44, 4), (372, 44, 8)):
        roi = np.zeros_like(edge)
        split = x0 + 46
        roi[y0:y0 + 86, split - 5:split + gap + 5] = True
        gap_rois[f"gap{gap}px"] = roi & edge

    # Source model receives the same byte-like RGBA values after toQImage's
    # alpha-zero RGB cleanup. The alternative hidden-RGB pair is normalized
    # through the same boundary so it must be behaviorally identical.
    source = original.copy()
    source[alpha0, :3] = 0.0
    alternate = original.copy()
    alternate[alpha0, :3] = np.array((0.05, 0.95, 0.15), dtype=np.float32)
    alternate[alpha0, :3] = 0.0
    h, w, _ = source.shape
    yy, xx = np.mgrid[0:h, 0:w]
    dx = xx.astype(np.float32) - w * 0.5
    dy = yy.astype(np.float32) - h * 0.5
    distance = np.sqrt(dx * dx + dy * dy)
    denominator = max(max(w, h) * 0.7, 1.0)
    ux = np.divide(dx, distance, out=np.zeros_like(dx), where=distance > 0.0)
    uy = np.divide(dy, distance, out=np.zeros_like(dy), where=distance > 0.0)
    backgrounds = {
        "black": np.array((0.0, 0.0, 0.0), dtype=np.float32),
        "white": np.array((1.0, 1.0, 1.0), dtype=np.float32),
    }
    metrics: dict[str, float] = {
        "fixture width": float(w),
        "fixture height": float(h),
        "partial-alpha source pixels": int(np.count_nonzero(partial)),
        "alpha-zero source pixels": int(np.count_nonzero(alpha0)),
    }
    panels: list[tuple[str, np.ndarray]] = []
    for shift in (2.0, 4.0, 8.0):
        rx = np.clip((xx + ux * (distance / denominator * shift) + 0.5).astype(np.int32), 0, w - 1)
        ry = np.clip((yy + uy * (distance / denominator * shift) + 0.5).astype(np.int32), 0, h - 1)
        bx = np.clip((xx - ux * (distance / denominator * shift) + 0.5).astype(np.int32), 0, w - 1)
        by = np.clip((yy - uy * (distance / denominator * shift) + 0.5).astype(np.int32), 0, h - 1)
        red = source[ry, rx, 0]
        blue = source[by, bx, 2]
        red_alpha = alpha[ry, rx]
        blue_alpha = alpha[by, bx]
        current = source.copy()
        current[:, :, 0] = red
        current[:, :, 2] = blue

        transparent_fallback = current.copy()
        transparent_fallback[red_alpha == 0.0, 0] = source[red_alpha == 0.0, 0]
        transparent_fallback[blue_alpha == 0.0, 2] = source[blue_alpha == 0.0, 2]

        nonopaque_fallback = current.copy()
        nonopaque_fallback[red_alpha < 1.0, 0] = source[red_alpha < 1.0, 0]
        nonopaque_fallback[blue_alpha < 1.0, 2] = source[blue_alpha < 1.0, 2]

        coverage_blend = current.copy()
        coverage_blend[:, :, 0] = source[:, :, 0] + red_alpha * (red - source[:, :, 0])
        coverage_blend[:, :, 2] = source[:, :, 2] + blue_alpha * (blue - source[:, :, 2])
        no_shift = source.copy()

        variants = {
            "current": current,
            "alpha0-fallback": transparent_fallback,
            "partial-fallback": nonopaque_fallback,
            "coverage-blend": coverage_blend,
        }
        current_composites = {
            name: {bg_name: frame[:, :, :3] * alpha[:, :, None] + bg * (1.0 - alpha[:, :, None])
                   for bg_name, bg in backgrounds.items()}
            for name, frame in variants.items()
        }
        no_shift_composites = {
            bg_name: no_shift[:, :, :3] * alpha[:, :, None] + bg * (1.0 - alpha[:, :, None])
            for bg_name, bg in backgrounds.items()
        }

        for channel_name, sample_alpha in (("red", red_alpha), ("blue", blue_alpha)):
            sample_region = edge & visible
            samples = sample_alpha[sample_region]
            metrics[f"R{shift:g}/B{shift:g} {channel_name} edge alpha0 sample ratio"] = float(np.mean(samples == 0.0))
            metrics[f"R{shift:g}/B{shift:g} {channel_name} partial-alpha sample ratio"] = float(np.mean((samples > 0.0) & (samples < 1.0)))
            metrics[f"R{shift:g}/B{shift:g} {channel_name} opaque sample ratio"] = float(np.mean(samples == 1.0))

        for variant_name in ("alpha0-fallback", "partial-fallback", "coverage-blend"):
            for roi_name, roi in (("edge", edge), ("strand", strand_roi), *gap_rois.items()):
                if not np.any(roi):
                    continue
                deltas = [np.abs(current_composites["current"][bg_name] -
                                 current_composites[variant_name][bg_name])[roi]
                          for bg_name in backgrounds]
                values = np.concatenate([entry.reshape(-1) for entry in deltas])
                metrics[f"R{shift:g}/B{shift:g} {variant_name} vs current {roi_name} MAE"] = float(np.mean(values))
                metrics[f"R{shift:g}/B{shift:g} {variant_name} vs current {roi_name} p95"] = float(np.percentile(values, 95))
        for roi_name, roi in (("edge", edge), ("strand", strand_roi), *gap_rois.items()):
            if not np.any(roi):
                continue
            delta = np.abs(current_composites["current"]["black"] - no_shift_composites["black"])[roi]
            metrics[f"R{shift:g}/B{shift:g} current effect vs no-shift {roi_name} MAE"] = float(np.mean(delta))

        # Alpha-zero source RGB variation must be erased by the modeled
        # ImageF32x4->QImage boundary before any sampling policy is applied.
        alt_current = alternate.copy()
        alt_current[:, :, 0] = alternate[ry, rx, 0]
        alt_current[:, :, 2] = alternate[by, bx, 2]
        metrics[f"R{shift:g}/B{shift:g} after-boundary hidden-pair max RGB delta"] = float(np.max(np.abs(current[:, :, :3] - alt_current[:, :, :3])))

        if shift == 8.0:
            for variant_name in ("current", "alpha0-fallback", "partial-fallback", "coverage-blend", "no-shift"):
                frame = no_shift if variant_name == "no-shift" else variants[variant_name]
                row_panels = []
                for bg_name, background in backgrounds.items():
                    composite = frame[:, :, :3] * alpha[:, :, None] + background * (1.0 - alpha[:, :, None])
                    row_panels.append(np.uint8(np.clip(composite * 255.0 + 0.5, 0, 255)))
                panels.append((variant_name, np.concatenate(row_panels, axis=1)))

    cell_w, cell_h, label_h = w * len(backgrounds), h, 22
    sheet = Image.new("RGB", (cell_w, (cell_h + label_h) * len(panels)), (28, 28, 32))
    draw = ImageDraw.Draw(sheet)
    for index, (label, pixels) in enumerate(panels):
        top = index * (cell_h + label_h)
        draw.text((4, top + 4), f"R8/B8 | {label} | black then white", fill=(245, 245, 245), font=ImageFont.load_default())
        sheet.paste(Image.fromarray(pixels, "RGB"), (0, top + label_h))
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    return metrics


def natural_alpha_sample_policy_study(corpus_dir: Path, output: Path) -> dict[str, float]:
    """Run the radial sample-alpha policy matrix on each licensed cutout."""
    import cv2

    provenance_path = corpus_dir / "provenance.json"
    if not provenance_path.is_file():
        raise ValueError("expected provenance.json for the natural alpha corpus")
    provenance = json.loads(provenance_path.read_text(encoding="utf-8-sig"))
    asset_names = tuple(sorted(name for name in provenance if name.lower().endswith((".png", ".jpg", ".jpeg", ".webp"))))
    if not asset_names:
        raise ValueError("provenance.json must list at least one supported image input")
    for name in asset_names:
        if not (corpus_dir / name).is_file() or not provenance.get(name, {}).get("license_or_permission"):
            raise ValueError(f"missing licensed provenance-tracked input: {name}")
        expected_sha256 = provenance[name].get("source_sha256")
        if expected_sha256 and hashlib.sha256((corpus_dir / name).read_bytes()).hexdigest().lower() != expected_sha256.lower():
            raise ValueError(f"input hash does not match provenance record: {name}")

    metrics: dict[str, float] = {}
    panels: list[tuple[str, np.ndarray]] = []
    backgrounds = {
        "black": np.array((0.0, 0.0, 0.0), dtype=np.float32),
        "midgray": np.array((0.5, 0.5, 0.5), dtype=np.float32),
        "white": np.array((1.0, 1.0, 1.0), dtype=np.float32),
    }
    for asset_name in asset_names:
        image_path = corpus_dir / asset_name
        rgba8 = np.asarray(Image.open(image_path).convert("RGBA"), dtype=np.uint8)
        original = rgba8.astype(np.float32) / 255.0
        alpha = original[:, :, 3]
        alpha0 = alpha == 0.0
        partial = (alpha > 0.0) & (alpha < 1.0)
        visible = alpha > 0.0
        edge = (cv2.dilate(visible.astype(np.uint8), np.ones((3, 3), dtype=np.uint8), iterations=1) !=
                cv2.erode(visible.astype(np.uint8), np.ones((3, 3), dtype=np.uint8), iterations=1))
        edge |= cv2.dilate(partial.astype(np.uint8), np.ones((7, 7), dtype=np.uint8), iterations=1) > 0
        source = original.copy()
        source[alpha0, :3] = 0.0
        h, w = alpha.shape
        prefix = Path(asset_name).stem
        metrics[f"{prefix} width"] = float(w)
        metrics[f"{prefix} height"] = float(h)
        metrics[f"{prefix} edge roi pixels"] = int(np.count_nonzero(edge))
        metrics[f"{prefix} alpha0 pixels"] = int(np.count_nonzero(alpha0))
        metrics[f"{prefix} partial alpha pixels"] = int(np.count_nonzero(partial))
        metrics[f"{prefix} alpha0 nonblack ratio before boundary"] = float(np.mean(np.any(original[alpha0, :3] != 0.0, axis=1))) if np.any(alpha0) else 0.0

        yy, xx = np.mgrid[0:h, 0:w]
        dx = xx.astype(np.float32) - w * 0.5
        dy = yy.astype(np.float32) - h * 0.5
        distance = np.sqrt(dx * dx + dy * dy)
        denominator = max(max(w, h) * 0.7, 1.0)
        ux = np.divide(dx, distance, out=np.zeros_like(dx), where=distance > 0.0)
        uy = np.divide(dy, distance, out=np.zeros_like(dy), where=distance > 0.0)

        for shift in (2.0, 4.0, 8.0):
            rx = np.clip((xx + ux * (distance / denominator * shift) + 0.5).astype(np.int32), 0, w - 1)
            ry = np.clip((yy + uy * (distance / denominator * shift) + 0.5).astype(np.int32), 0, h - 1)
            bx = np.clip((xx - ux * (distance / denominator * shift) + 0.5).astype(np.int32), 0, w - 1)
            by = np.clip((yy - uy * (distance / denominator * shift) + 0.5).astype(np.int32), 0, h - 1)
            red, blue = source[ry, rx, 0], source[by, bx, 2]
            red_alpha, blue_alpha = alpha[ry, rx], alpha[by, bx]
            current = source.copy()
            current[:, :, 0], current[:, :, 2] = red, blue

            alpha0_fallback = current.copy()
            mask = red_alpha == 0.0
            alpha0_fallback[mask, 0] = source[mask, 0]
            mask = blue_alpha == 0.0
            alpha0_fallback[mask, 2] = source[mask, 2]

            partial_fallback = current.copy()
            mask = red_alpha < 1.0
            partial_fallback[mask, 0] = source[mask, 0]
            mask = blue_alpha < 1.0
            partial_fallback[mask, 2] = source[mask, 2]

            coverage_blend = current.copy()
            coverage_blend[:, :, 0] = source[:, :, 0] + red_alpha * (red - source[:, :, 0])
            coverage_blend[:, :, 2] = source[:, :, 2] + blue_alpha * (blue - source[:, :, 2])
            variants = {"alpha0-fallback": alpha0_fallback,
                        "partial-fallback": partial_fallback,
                        "coverage-blend": coverage_blend}
            for channel_name, sample_alpha in (("red", red_alpha), ("blue", blue_alpha)):
                samples = sample_alpha[edge & visible]
                key = f"{prefix} R{shift:g}/B{shift:g} {channel_name} sample alpha0 ratio"
                metrics[key] = float(np.mean(samples == 0.0))
                metrics[f"{prefix} R{shift:g}/B{shift:g} {channel_name} sample partial ratio"] = float(np.mean((samples > 0.0) & (samples < 1.0)))
                metrics[f"{prefix} R{shift:g}/B{shift:g} {channel_name} sample opaque ratio"] = float(np.mean(samples == 1.0))

            for variant_name, candidate in variants.items():
                for roi_name, roi in (("edge", edge), ("partial-edge", edge & cv2.dilate(partial.astype(np.uint8), np.ones((7, 7), dtype=np.uint8), iterations=1).astype(bool))):
                    if not np.any(roi):
                        continue
                    domain_deltas = []
                    for background in backgrounds.values():
                        current_comp = current[:, :, :3] * alpha[:, :, None] + background * (1.0 - alpha[:, :, None])
                        candidate_comp = candidate[:, :, :3] * alpha[:, :, None] + background * (1.0 - alpha[:, :, None])
                        domain_deltas.append(np.abs(current_comp - candidate_comp)[roi].reshape(-1))
                    values = np.concatenate(domain_deltas)
                    metrics[f"{prefix} R{shift:g}/B{shift:g} {variant_name} vs current {roi_name} MAE"] = float(np.mean(values))
                    metrics[f"{prefix} R{shift:g}/B{shift:g} {variant_name} vs current {roi_name} p95"] = float(np.percentile(values, 95))
            if shift == 8.0:
                for label, frame in (("current", current), ("alpha0 fallback", alpha0_fallback),
                                     ("partial fallback", partial_fallback), ("coverage blend", coverage_blend)):
                    composite = frame[:, :, :3] * alpha[:, :, None]
                    thumb = Image.fromarray(np.uint8(np.clip(composite * 255.0 + 0.5, 0, 255)), "RGB")
                    thumb.thumbnail((260, 260), Image.Resampling.LANCZOS)
                    panels.append((f"{prefix} | {label}", np.asarray(thumb)))
        del original, source, current, alpha0_fallback, partial_fallback, coverage_blend

    cell_w, cell_h, label_h, columns = 260, 284, 20, 4
    rows = (len(panels) + columns - 1) // columns
    sheet = Image.new("RGB", (columns * cell_w, rows * cell_h), (28, 28, 32))
    draw = ImageDraw.Draw(sheet)
    for index, (label, thumb) in enumerate(panels):
        row, column = divmod(index, columns)
        x, y = column * cell_w, row * cell_h
        draw.text((x + 3, y + 3), label[:38], fill=(245, 245, 245), font=ImageFont.load_default())
        sheet.paste(Image.fromarray(thumb, "RGB"), (x + 3, y + label_h + 3))
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    return metrics


def natural_alpha_sample_policy_detail_study(
    corpus_dir: Path, asset_name: str, crop_box: tuple[int, int, int, int], output: Path,
) -> dict[str, float]:
    """Save an unscaled crop of the R8 policy matrix over three backgrounds."""
    provenance_path = corpus_dir / "provenance.json"
    provenance = json.loads(provenance_path.read_text(encoding="utf-8-sig"))
    image_path = corpus_dir / asset_name
    entry = provenance.get(asset_name, {})
    if not image_path.is_file() or not entry.get("license_or_permission"):
        raise ValueError(f"missing provenance-tracked input: {asset_name}")
    expected_sha256 = entry.get("source_sha256")
    if expected_sha256 and hashlib.sha256(image_path.read_bytes()).hexdigest().lower() != expected_sha256.lower():
        raise ValueError(f"input hash does not match provenance record: {asset_name}")

    rgba8 = np.asarray(Image.open(image_path).convert("RGBA"), dtype=np.uint8)
    h, w, _ = rgba8.shape
    x0, y0, crop_w, crop_h = crop_box
    x1, y1 = x0 + crop_w, y0 + crop_h
    if x0 < 0 or y0 < 0 or crop_w <= 0 or crop_h <= 0 or x1 > w or y1 > h:
        raise ValueError(f"crop box {crop_box} is outside {w}x{h} input")
    source = rgba8.astype(np.float32) / 255.0
    alpha = source[:, :, 3]
    source[alpha == 0.0, :3] = 0.0
    yy, xx = np.mgrid[0:h, 0:w]
    dx = xx.astype(np.float32) - w * 0.5
    dy = yy.astype(np.float32) - h * 0.5
    distance = np.sqrt(dx * dx + dy * dy)
    denominator = max(max(w, h) * 0.7, 1.0)
    ux = np.divide(dx, distance, out=np.zeros_like(dx), where=distance > 0.0)
    uy = np.divide(dy, distance, out=np.zeros_like(dy), where=distance > 0.0)
    shift = 8.0
    rx = np.clip((xx + ux * (distance / denominator * shift) + 0.5).astype(np.int32), 0, w - 1)
    ry = np.clip((yy + uy * (distance / denominator * shift) + 0.5).astype(np.int32), 0, h - 1)
    bx = np.clip((xx - ux * (distance / denominator * shift) + 0.5).astype(np.int32), 0, w - 1)
    by = np.clip((yy - uy * (distance / denominator * shift) + 0.5).astype(np.int32), 0, h - 1)
    red, blue = source[ry, rx, 0], source[by, bx, 2]
    red_alpha, blue_alpha = alpha[ry, rx], alpha[by, bx]
    current = source.copy()
    current[:, :, 0], current[:, :, 2] = red, blue
    alpha0_fallback = current.copy()
    alpha0_fallback[red_alpha == 0.0, 0] = source[red_alpha == 0.0, 0]
    alpha0_fallback[blue_alpha == 0.0, 2] = source[blue_alpha == 0.0, 2]
    partial_fallback = current.copy()
    partial_fallback[red_alpha < 1.0, 0] = source[red_alpha < 1.0, 0]
    partial_fallback[blue_alpha < 1.0, 2] = source[blue_alpha < 1.0, 2]
    coverage_blend = current.copy()
    coverage_blend[:, :, 0] = source[:, :, 0] + red_alpha * (red - source[:, :, 0])
    coverage_blend[:, :, 2] = source[:, :, 2] + blue_alpha * (blue - source[:, :, 2])
    variants = (("current", current), ("alpha0 fallback", alpha0_fallback),
                ("partial fallback", partial_fallback), ("coverage blend", coverage_blend))
    backgrounds = (("black", np.zeros(3, dtype=np.float32)),
                   ("gray", np.full(3, 0.5, dtype=np.float32)),
                   ("white", np.ones(3, dtype=np.float32)))
    cell_height = crop_h + 42
    sheet = Image.new("RGB", (crop_w * len(variants), cell_height * len(backgrounds)), (28, 28, 32))
    draw = ImageDraw.Draw(sheet)
    crop_alpha = alpha[y0:y1, x0:x1, None]
    metrics: dict[str, float] = {"crop x": float(x0), "crop y": float(y0),
                                 "crop width": float(crop_w), "crop height": float(crop_h),
                                 "crop partial-alpha pixel count": float(np.count_nonzero((alpha[y0:y1, x0:x1] > 0.0) & (alpha[y0:y1, x0:x1] < 1.0)))}
    for row, (background_name, background) in enumerate(backgrounds):
        draw.text((4, row * cell_height + 3), f"Background: {background_name} | 1:1 source pixels", fill=(245, 245, 245), font=ImageFont.load_default())
        base_crop = current[y0:y1, x0:x1, :3] * crop_alpha + background * (1.0 - crop_alpha)
        for column, (variant_name, frame) in enumerate(variants):
            frame_crop = frame[y0:y1, x0:x1, :3] * crop_alpha + background * (1.0 - crop_alpha)
            draw.text((column * crop_w + 4, row * cell_height + 20), variant_name, fill=(245, 245, 245), font=ImageFont.load_default())
            pixels = np.uint8(np.clip(frame_crop * 255.0 + 0.5, 0, 255))
            sheet.paste(Image.fromarray(pixels, "RGB"), (column * crop_w, row * cell_height + 42))
            if variant_name != "current":
                delta = np.abs(frame_crop - base_crop).reshape(-1)
                metrics[f"{background_name} {variant_name} crop MAE"] = float(np.mean(delta))
                metrics[f"{background_name} {variant_name} crop p95"] = float(np.percentile(delta, 95))
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    return metrics


def make_chromatic_fixture() -> Image.Image:
    width, height = 320, 160
    x = np.linspace(0, 1, width, dtype=np.float32)
    image = np.zeros((height, width, 3), dtype=np.float32)
    image[:] = (0.025, 0.035, 0.055)
    image[:, :1] = (0.95, 0.92, 0.82)
    image[:, -1:] = (0.82, 0.9, 0.98)
    image[24:136, 26:84] = (0.95, 0.16, 0.08)
    image[24:136, 84:142] = (0.05, 0.8, 0.32)
    image[24:136, 142:200] = (0.06, 0.24, 0.96)
    image[24:136, 200:258] = (0.95, 0.8, 0.08)
    image[24:136, 258:294] = x[258:294][None, :, None] * np.array([0.2, 0.75, 0.95], dtype=np.float32)
    image[::2, :] *= 0.6
    return Image.fromarray(np.uint8(np.clip(image * 255, 0, 255)), "RGB")


def chromatic_study(image: Image.Image, output: Path) -> dict[str, float]:
    source = np.asarray(image.convert("RGB"), dtype=np.float32) / 255.0
    variants = [
        ("nearest 2.0px / clamp / sRGB", chromatic_shift(source, 2.0, "nearest", "clamp", "srgb")),
        ("nearest 2.49px / clamp / sRGB", chromatic_shift(source, 2.49, "nearest", "clamp", "srgb")),
        ("nearest 2.5px / clamp / sRGB", chromatic_shift(source, 2.5, "nearest", "clamp", "srgb")),
        ("bilinear 2.5px / clamp / sRGB", chromatic_shift(source, 2.5, "bilinear", "clamp", "srgb")),
        ("bilinear 2.5px / clamp / linear", chromatic_shift(source, 2.5, "bilinear", "clamp", "linear")),
        ("bilinear 2.5px / wrap / sRGB", chromatic_shift(source, 2.5, "bilinear", "wrap", "srgb")),
        ("bilinear 2.5px / reflect / sRGB", chromatic_shift(source, 2.5, "bilinear", "reflect", "srgb")),
    ]
    baseline = next(result for name, result in variants if name == "bilinear 2.5px / clamp / sRGB")
    h, w, _ = source.shape
    thumb_w, thumb_h, label_h = 320, 160, 28
    columns = 2
    rows = (len(variants) + columns - 1) // columns
    sheet = Image.new("RGB", (thumb_w * columns, (thumb_h + label_h) * rows), (30, 30, 34))
    draw = ImageDraw.Draw(sheet)
    metrics: dict[str, float] = {}
    border = np.zeros((h, w), dtype=bool)
    border[:, :8] = True
    border[:, -8:] = True
    for index, (name, result) in enumerate(variants):
        delta = np.abs(srgb_to_linear(result) - srgb_to_linear(baseline))
        all_delta = float(np.mean(delta))
        border_delta = float(np.mean(delta[border]))
        metrics[f"{name} · mean linear delta vs clamp bilinear"] = all_delta
        metrics[f"{name} · border linear delta vs clamp bilinear"] = border_delta
        x, y = (index % columns) * thumb_w, (index // columns) * (thumb_h + label_h)
        draw.text((x + 5, y + 6), f"{name} · Δ {all_delta:.4f} · edge Δ {border_delta:.4f}", fill=(245, 245, 245), font=ImageFont.load_default())
        thumb = Image.fromarray(np.uint8(np.clip(result * 255, 0, 255))).resize((thumb_w, thumb_h), Image.Resampling.NEAREST)
        sheet.paste(thumb, (x, y + label_h))
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    return metrics


def make_color_space_fixture() -> Image.Image:
    width, height = 384, 192
    x = np.linspace(0.0, 1.0, width, dtype=np.float32)
    ramp = np.broadcast_to(x[None, :, None], (height // 2, width, 3)).copy()
    image = np.empty((height, width, 3), dtype=np.float32)
    image[:height // 2] = ramp
    image[height // 2:] = np.stack((
        np.broadcast_to(x[None, :], (height // 2, width)),
        np.broadcast_to(np.roll(x, width // 3)[None, :], (height // 2, width)),
        np.broadcast_to(np.roll(x, 2 * width // 3)[None, :], (height // 2, width)),
    ), axis=2)
    # Overlay flat swatches and alternating fine lines to expose local quantization and scanline interactions.
    swatches = ((24, 56, 96), (220, 72, 40), (32, 168, 120), (248, 208, 72))
    for index, color in enumerate(swatches):
        x0 = 12 + index * 92
        image[68:124, x0:x0 + 72] = np.asarray(color, dtype=np.float32) / 255.0
    image[124::2, :] *= 0.72
    return Image.fromarray(np.uint8(np.clip(image * 255, 0, 255)), "RGB")


def color_space_study(image: Image.Image, output: Path) -> dict[str, float]:
    source_srgb = np.asarray(image.convert("RGB"), dtype=np.float32) / 255.0
    source_linear = srgb_to_linear(source_srgb)
    results: list[tuple[str, np.ndarray]] = []
    levels = 4
    results.append(("posterize / sRGB code", np.round(source_srgb * (levels - 1)) / (levels - 1)))
    results.append(("posterize / linear light", linear_to_srgb(np.round(source_linear * (levels - 1)) / (levels - 1))))

    scanline_srgb = source_srgb.copy()
    scanline_srgb[::2] *= 0.65
    results.append(("scanline / sRGB code", scanline_srgb))
    scanline_linear = source_linear.copy()
    scanline_linear[::2] *= 0.65
    results.append(("scanline / linear light", linear_to_srgb(scanline_linear)))

    for domain, source in (("sRGB code", source_srgb), ("linear light", source_linear)):
        palette = PALETTES["pico8"]
        if domain == "linear light":
            quantized = nearest_palette(source, srgb_to_linear(palette))
            result = linear_to_srgb(quantized)
        else:
            result = nearest_palette(source, palette)
        results.append((f"PICO-8 nearest / {domain}", result))

    thumb_w = 384
    thumb_h = 192
    label_h = 28
    background = (34, 34, 38)
    sheet = Image.new("RGB", (thumb_w * 2, (thumb_h + label_h) * (len(results) // 2 + 1)), background)
    draw = ImageDraw.Draw(sheet)
    metrics: dict[str, float] = {}
    for index, (name, result) in enumerate(results):
        row, col = divmod(index, 2)
        linear_result = srgb_to_linear(np.clip(result, 0, 1))
        rmse = float(np.mean((linear_result - source_linear) ** 2) ** 0.5)
        metrics[name] = rmse
        x, y = col * thumb_w, row * (thumb_h + label_h)
        draw.text((x + 6, y + 6), f"{name} · linear RGB RMSE {rmse:.5f}", fill=(245, 245, 245), font=ImageFont.load_default())
        thumb = Image.fromarray(np.uint8(np.clip(result * 255, 0, 255))).resize((thumb_w, thumb_h), Image.Resampling.NEAREST)
        sheet.paste(thumb, (x, y + label_h))

    # Difference view isolates where code-space and linear-light pipelines diverge.
    difference = np.abs(results[0][1] - results[1][1])
    difference = np.clip(difference * 3.0, 0, 1)
    diff_image = Image.fromarray(np.uint8(difference * 255)).resize((thumb_w, thumb_h), Image.Resampling.NEAREST)
    sheet.paste(diff_image, (thumb_w, 3 * (thumb_h + label_h) + label_h))
    draw.text((6, 3 * (thumb_h + label_h) + 5), "Posterize domain difference ×3", fill=(245, 245, 245), font=ImageFont.load_default())
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    metrics["posterize domain mean absolute code difference"] = float(np.mean(difference))
    return metrics


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("--preset", choices=PRESETS, default="handheld")
    parser.add_argument("--colors", type=int)
    parser.add_argument("--scale", type=float)
    parser.add_argument("--dither", type=float)
    parser.add_argument("--scanlines", type=float)
    parser.add_argument("--aberration", type=float)
    parser.add_argument("--grain", type=float)
    parser.add_argument("--seed", type=int, default=7)
    parser.add_argument("--alpha-shift", type=float, default=3.5, help="pixel shift used by --alpha-chromatic-study")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--research-sheet", action="store_true", help="write a comparison sheet for quantization and fixed palettes")
    parser.add_argument("--alpha-study", action="store_true", help="write a transparent-edge filtering comparison using a synthetic fixture")
    parser.add_argument("--alpha-chromatic-study", action="store_true", help="compare hidden-RGB sensitivity for shifted color channels at transparent edges")
    parser.add_argument("--alpha-photo-sweep", action="store_true", help="sweep chromatic shift distances on a textured antialiased foliage cutout")
    parser.add_argument("--production-chromatic-alpha-study", action="store_true", help="model the current nearest/clamp radial effect on adversarial transparent RGB")
    parser.add_argument("--color-space-study", action="store_true", help="compare processing the input in sRGB-code and linear-light domains")
    parser.add_argument("--palette-domain-study", action="store_true", help="compare fixed-palette distance domains and Bayer strengths")
    parser.add_argument("--quantization-banding-study", action="store_true", help="measure ramp banding proxies across quantization domains and Bayer strengths")
    parser.add_argument("--natural-quantization-study", action="store_true", help="compare posterization and Bayer dither on a deterministic landscape/detail fixture")
    parser.add_argument("--dither-pattern-study", action="store_true", help="compare Bayer 4x4/8x8 with Floyd-Steinberg on a deterministic landscape fixture")
    parser.add_argument("--temporal-dither-study", action="store_true", help="measure dither shimmer across one-pixel translated frames")
    parser.add_argument("--chromatic-study", action="store_true", help="compare channel-shift interpolation, edge modes, and transfer domains")
    parser.add_argument("--radial-chromatic-study", action="store_true", help="compare radial channel-shift fields and CPU-style sampling")
    parser.add_argument("--grain-study", action="store_true", help="compare grain weighting, channel structure, spatial scale, and temporal stability")
    parser.add_argument("--pipeline-study", action="store_true", help="compare retro effect ordering and dither/scanline phase")
    parser.add_argument("--crop-phase-study", action="store_true", help="compare global and crop-local screen phase at odd sizes and scales")
    parser.add_argument("--crop-resize-phase-study", action="store_true", help="compare whole-frame and independently resized crop phase behavior")
    parser.add_argument("--opencv-crop-resize-study", action="store_true", help="compare OpenCV float resize filters and tile-local crops")
    parser.add_argument("--alpha-pixel-art-crop-study", action="store_true", help="compare crop-local pixelation and hidden RGB sensitivity on synthetic RGBA art")
    parser.add_argument("--alpha-resize-strategy-study", action="store_true", help="compare RGBA resize strategies through actual retro presets")
    parser.add_argument("--alpha-resize-multicolor-study", action="store_true", help="compare RGBA resize policies on multicolor gaps and translucent strands across every preset")
    parser.add_argument("--alpha-resize-gap-scale-study", action="store_true", help="isolate resize-policy leakage across scales and save enlarged gap/strand crops")
    parser.add_argument("--alpha-hidden-stage-study", action="store_true", help="attribute scale-1 hidden-RGB leakage across quantization, channel offset, scanlines, and grain")
    parser.add_argument("--alpha-chromatic-offset-sweep", action="store_true", help="sweep signed channel-offset amounts and measure hidden-RGB edge leakage")
    parser.add_argument("--alpha-resize-real-corpus", type=Path, help="compare alpha resize strategies on provenance-tracked PNG cutouts")
    parser.add_argument("--alpha-hidden-source-residual", type=Path, help="trace existing hidden RGB in vegetation_clover_02a.png through presets and alpha resize policies")
    parser.add_argument("--core-chromatic-alpha-model", type=Path, help="model Core integer/clamped chromatic RGB sampling against hidden-alpha colors")
    parser.add_argument("--rasterizer-chromatic-alpha-model", type=Path, help="model Artifact rasterizer radial chromatic CPU/HLSL RGB sampling against hidden-alpha colors")
    parser.add_argument("--alpha-hidden-distance-study", type=Path, help="measure CC0 clover hidden RGB residual by alpha-zero distance to visible support")
    parser.add_argument("--rasterizer-chromatic-transfer-domain", type=Path, help="compare sRGB-code and linear-light Rasterizer chromatic sampling on an ICC-profiled cutout")
    parser.add_argument("--image-layer-rasterizer-boundary", type=Path, help="model the ImageF32x4-to-QImage alpha-zero RGB cleanup before still-image Rasterizer effects")
    parser.add_argument("--rasterizer-alpha-sample-policy", action="store_true", help="compare current, alpha-zero/partial fallback, and coverage-blend radial sampling policies on the synthetic RGBA suite chart")
    parser.add_argument("--natural-alpha-sample-policy", type=Path, help="compare radial sample-alpha policies independently on provenance-tracked natural cutouts")
    parser.add_argument("--natural-alpha-sample-detail", type=Path, help="save a source-pixel 1:1 R8/B8 policy crop from a provenance-tracked corpus")
    parser.add_argument("--natural-alpha-detail-asset", default="feather_silver.png", help="asset filename for --natural-alpha-sample-detail")
    parser.add_argument("--natural-alpha-detail-box", type=int, nargs=4, default=(130, 120, 220, 220), metavar=("X", "Y", "W", "H"), help="source-pixel crop for --natural-alpha-sample-detail")
    parser.add_argument("--product-dither-source-audit", action="store_true", help="compare source-derived CPU, resident HLSL, and standalone HLSL Bayer formulas")
    parser.add_argument("--suite-corpus", type=Path, help="evaluate all fixed retro presets over user-provided images in this directory")
    parser.add_argument("--corpus-dir", type=Path, default=Path("eval-corpus"), help="destination for hash-named images and provenance manifest")
    parser.add_argument("--corpus-provenance", type=Path, help="JSON mapping source filenames to source_url_or_origin and license_or_permission")
    parser.add_argument("--make-suite-fixture", action="store_true", help="write license-free synthetic RGB/alpha evaluation input into the supplied corpus source directory")
    parser.add_argument("--alpha-hidden-suite-study", action="store_true", help="measure hidden-RGB leakage through the actual RetroImageLab presets and alpha compositing")
    parser.add_argument("--alpha-resize-suite-study", action="store_true", help="compare straight and premultiplied downsampling in the scale presets")
    parser.add_argument("--alpha-rgb-dilation-study", action="store_true", help="compare straight, premultiplied, and bounded hidden-RGB dilation at transparent edges")
    parser.add_argument("--alpha-dilation-stress-study", action="store_true", help="stress nearest-color RGB dilation across contrasting objects and 1-8px transparent gaps")
    parser.add_argument("--suite-detail-image", type=Path, help="image for a baseline plus actual-preset ROI sheet")
    parser.add_argument("--suite-detail-box", type=int, nargs=4, metavar=("X", "Y", "W", "H"), help="ROI rectangle in source pixels for --suite-detail-image")
    parser.add_argument("--texture-frequency-image", type=Path, help="source image for baseline/preset spatial-frequency redistribution metrics")
    parser.add_argument("--texture-frequency-box", type=int, nargs=4, metavar=("X", "Y", "W", "H"), help="ROI rectangle in source pixels for --texture-frequency-image")
    parser.add_argument("--video-grain-frames", type=Path, help="directory containing consecutively named frame-*.png images for temporal grain evaluation")
    parser.add_argument("--video-grain-space-frames", type=Path, help="compare screen-fixed and optical-flow-advected grain on consecutive frames")
    args = parser.parse_args()

    settings = PRESETS[args.preset].copy()
    for key in settings:
        value = getattr(args, key)
        if value is not None:
            settings[key] = value
    if not 0 < settings["scale"] <= 1:
        parser.error("--scale must be greater than 0 and at most 1")

    with Image.open(args.input) as source:
        source_for_sheet = source.copy()
        result = apply_retro(source, **settings, seed=args.seed)
    output_path = args.output or Path("output") / f"{args.input.stem}_{args.preset}.png"
    output_path.parent.mkdir(parents=True, exist_ok=True)
    result.save(output_path)
    print(f"Saved {output_path} ({result.width}x{result.height})")
    if args.research_sheet:
        sheet_path = output_path.with_name(f"{args.input.stem}_quantization-study.png")
        metrics = build_research_sheet(source_for_sheet, sheet_path)
        print(f"Saved {sheet_path}")
        for label, rmse in metrics.items():
            print(f"{label}: RGB RMSE {rmse:.5f}")
    if args.alpha_study:
        alpha_path = output_path.with_name("alpha-edge-study.png")
        metrics = alpha_edge_study(make_alpha_fixture(), alpha_path, args.seed)
        print(f"Saved {alpha_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.5f}")
    if args.alpha_chromatic_study:
        alpha_chromatic_path = output_path.with_name("alpha-chromatic-study.png")
        metrics = alpha_chromatic_study(make_alpha_fixture(), alpha_chromatic_path, args.alpha_shift)
        print(f"Saved {alpha_chromatic_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.5f}")
    if args.alpha_photo_sweep:
        sweep_path = output_path.with_name("alpha-photo-chromatic-sweep.png")
        fixture_path = Path("fixtures") / "photo-alpha-fixture.png"
        results = alpha_photo_sweep_study(sweep_path, fixture_path)
        print(f"Saved {sweep_path}")
        print(f"Saved {fixture_path}")
        for amount, metrics in results.items():
            print(f"shift {amount:g}px")
            for label, value in metrics.items():
                print(f"  {label}: {value:.5f}")
    if args.production_chromatic_alpha_study:
        study_path = output_path.with_name("production-chromatic-alpha-study.png")
        fixture_path = Path("fixtures") / "production-chromatic-alpha-fixture.png"
        results = production_chromatic_alpha_study(study_path, fixture_path)
        print(f"Saved {study_path}")
        print(f"Saved {fixture_path}")
        for amount, metrics in results.items():
            print(f"shift {amount:g}px")
            for label, value in metrics.items():
                print(f"  {label}: {value:.5f}")
    if args.color_space_study:
        color_path = output_path.with_name("color-space-study.png")
        metrics = color_space_study(source_for_sheet, color_path)
        print(f"Saved {color_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.5f}")
    if args.palette_domain_study:
        palette_path = output_path.with_name(f"{args.input.stem}_palette-domain-study.png")
        metrics = palette_domain_study(source_for_sheet, palette_path)
        print(f"Saved {palette_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.5f}")
    if args.quantization_banding_study:
        gray_path = output_path.with_name("quantization-banding-gray-study.png")
        color_path = output_path.with_name("quantization-banding-color-study.png")
        fixture_path = Path("fixtures") / "banding-fixture.png"
        metrics = quantization_banding_study(gray_path, color_path, fixture_path)
        print(f"Saved {gray_path}")
        print(f"Saved {color_path}")
        print(f"Saved {fixture_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.6f}")
    if args.natural_quantization_study:
        fixture_path = Path("fixtures") / "natural-quantization-fixture.png"
        sheet_path = output_path.with_name("natural-quantization-study.png")
        crop_path = output_path.with_name("natural-quantization-detail-crop.png")
        metrics = natural_quantization_study(fixture_path, sheet_path, crop_path)
        print(f"Saved {fixture_path}")
        print(f"Saved {sheet_path}")
        print(f"Saved {crop_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.6f}")
    if args.dither_pattern_study:
        fixture_path = Path("fixtures") / "natural-quantization-fixture.png"
        sheet_path = output_path.with_name("dither-pattern-study.png")
        crop_path = output_path.with_name("dither-pattern-detail-crop.png")
        metrics = dither_pattern_study(fixture_path, sheet_path, crop_path)
        print(f"Saved {fixture_path}")
        print(f"Saved {sheet_path}")
        print(f"Saved {crop_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.6f}")
    if args.temporal_dither_study:
        fixture_path = Path("fixtures") / "natural-quantization-fixture.png"
        sheet_path = output_path.with_name("temporal-dither-study.png")
        crop_path = output_path.with_name("temporal-dither-detail-strip.png")
        metrics = temporal_dither_study(fixture_path, sheet_path, crop_path)
        print(f"Saved {fixture_path}")
        print(f"Saved {sheet_path}")
        print(f"Saved {crop_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.6f}")
    if args.chromatic_study:
        chromatic_path = output_path.with_name("chromatic-shift-study.png")
        metrics = chromatic_study(source_for_sheet, chromatic_path)
        print(f"Saved {chromatic_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.5f}")
    if args.radial_chromatic_study:
        radial_path = output_path.with_name("radial-chromatic-study.png")
        metrics = radial_chromatic_study(source_for_sheet, radial_path)
        print(f"Saved {radial_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.5f}")
    if args.grain_study:
        grain_path = output_path.with_name("grain-study.png")
        metrics = grain_study(source_for_sheet, grain_path)
        print(f"Saved {grain_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.5f}")
    if args.pipeline_study:
        pipeline_path = output_path.with_name("pipeline-order-study.png")
        metrics = pipeline_order_study(source_for_sheet, pipeline_path, args.seed)
        print(f"Saved {pipeline_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.5f}")
    if args.crop_phase_study:
        crop_path = output_path.with_name("crop-phase-study.png")
        metrics = crop_phase_study(source_for_sheet, crop_path)
        print(f"Saved {crop_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.5f}")
    if args.crop_resize_phase_study:
        study_path = output_path.with_name("crop-resize-phase-study.png")
        metrics = crop_resize_phase_study(study_path, args.seed)
        print(f"Saved {study_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.6f}")
    if args.opencv_crop_resize_study:
        study_path = output_path.with_name("opencv-crop-resize-study.png")
        metrics = opencv_crop_resize_study(study_path)
        print(f"Saved {study_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.6f}")
    if args.alpha_pixel_art_crop_study:
        study_path = output_path.with_name("alpha-pixel-art-crop-study.png")
        fixture_path = Path(__file__).resolve().parent / "fixtures" / "alpha-pixel-art-crop-fixture.png"
        metrics = alpha_pixel_art_crop_study(study_path, fixture_path)
        print(f"Saved {fixture_path}")
        print(f"Saved {study_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.6f}")
    if args.alpha_resize_strategy_study:
        study_path = output_path.with_name("alpha-resize-strategy-study.png")
        fixture_path = Path(__file__).resolve().parent / "fixtures" / "alpha-pixel-art-crop-fixture.png"
        metrics = alpha_resize_strategy_study(fixture_path, study_path, args.seed)
        print(f"Saved {study_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.6f}")
    if args.alpha_resize_multicolor_study:
        study_path = output_path.with_name("alpha-resize-multicolor-study.png")
        fixture_path = Path(__file__).resolve().parent / "fixtures" / "alpha-resize-multicolor-fixture.png"
        metrics = alpha_resize_multicolor_study(study_path, fixture_path, args.seed)
        print(f"Saved {fixture_path}")
        print(f"Saved {study_path}")
        print(f"Fixture SHA256: {hashlib.sha256(fixture_path.read_bytes()).hexdigest()}")
        for label, value in metrics.items():
            print(f"{label}: {value:.6f}")
    if args.alpha_resize_gap_scale_study:
        study_path = output_path.with_name("alpha-resize-gap-scale-study.png")
        metrics = alpha_resize_gap_scale_study(study_path, args.seed)
        print(f"Saved {study_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.6f}")
    if args.alpha_hidden_stage_study:
        study_path = output_path.with_name("alpha-hidden-stage-study.png")
        metrics = alpha_hidden_stage_attribution(study_path, args.seed)
        print(f"Saved {study_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.6f}")
    if args.alpha_chromatic_offset_sweep:
        study_path = output_path.with_name("alpha-chromatic-offset-sweep.png")
        metrics = alpha_chromatic_offset_sweep(study_path, args.seed)
        print(f"Saved {study_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.6f}")
    if args.alpha_resize_real_corpus:
        study_path = output_path.with_name("alpha-resize-real-corpus-study.png")
        metrics = alpha_resize_real_corpus_study(args.alpha_resize_real_corpus, study_path, args.seed)
        print(f"Saved {study_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.6f}")
    if args.alpha_hidden_source_residual:
        study_path = output_path.with_name("alpha-hidden-source-residual-study.png")
        metrics = alpha_hidden_source_residual_study(args.alpha_hidden_source_residual, study_path, args.seed)
        print(f"Saved {study_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.6f}")
    if args.core_chromatic_alpha_model:
        study_path = output_path.with_name("core-chromatic-alpha-model-study.png")
        metrics = core_chromatic_alpha_source_model(args.core_chromatic_alpha_model, study_path)
        print(f"Saved {study_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.6f}")
    if args.rasterizer_chromatic_alpha_model:
        study_path = output_path.with_name("rasterizer-chromatic-alpha-model-study.png")
        metrics = rasterizer_chromatic_alpha_source_model(args.rasterizer_chromatic_alpha_model, study_path)
        print(f"Saved {study_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.6f}")
    if args.alpha_hidden_distance_study:
        study_path = output_path.with_name("alpha-hidden-distance-study.png")
        metrics = alpha_hidden_distance_residual_study(args.alpha_hidden_distance_study, study_path)
        print(f"Saved {study_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.6f}")
    if args.rasterizer_chromatic_transfer_domain:
        study_path = output_path.with_name("rasterizer-chromatic-transfer-domain.png")
        metrics = rasterizer_chromatic_transfer_domain_study(args.rasterizer_chromatic_transfer_domain, study_path)
        print(f"Saved {study_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.6f}")
    if args.image_layer_rasterizer_boundary:
        study_path = output_path.with_name("image-layer-rasterizer-boundary.png")
        metrics = image_layer_rasterizer_boundary_study(args.image_layer_rasterizer_boundary, study_path)
        print(f"Saved {study_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.6f}")
    if args.rasterizer_alpha_sample_policy:
        fixture_path = Path(__file__).resolve().parent / "fixtures" / "alpha-resize-suite-fixture.png"
        study_path = output_path.with_name("rasterizer-alpha-sample-policy.png")
        metrics = rasterizer_alpha_sample_policy_study(study_path, fixture_path)
        print(f"Saved {fixture_path}")
        print(f"Saved {study_path}")
        print(f"Fixture SHA256: {hashlib.sha256(fixture_path.read_bytes()).hexdigest()}")
        for label, value in metrics.items():
            print(f"{label}: {value:.6f}")
    if args.natural_alpha_sample_policy:
        study_path = output_path.with_name("natural-alpha-sample-policy.png")
        metrics = natural_alpha_sample_policy_study(args.natural_alpha_sample_policy, study_path)
        print(f"Saved {study_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.6f}")
    if args.natural_alpha_sample_detail:
        x, y, width, height = args.natural_alpha_detail_box
        study_path = output_path.with_name("natural-alpha-sample-detail.png")
        metrics = natural_alpha_sample_policy_detail_study(
            args.natural_alpha_sample_detail, args.natural_alpha_detail_asset,
            (x, y, width, height), study_path,
        )
        print(f"Saved {study_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.6f}")
    if args.product_dither_source_audit:
        fixture_path = Path("fixtures") / "natural-quantization-fixture.png"
        audit_path = output_path.with_name("product-dither-source-audit.png")
        metrics = product_dither_source_audit(fixture_path, audit_path)
        print(f"Saved {fixture_path}")
        print(f"Saved {audit_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.6f}")
    if args.make_suite_fixture:
        fixture_source = args.suite_corpus or Path("eval-inputs")
        fixture_source.mkdir(parents=True, exist_ok=True)
        fixture_path = fixture_source / "synthetic-retro-suite-fixture.png"
        make_retro_suite_fixture().save(fixture_path)
        print(f"Saved {fixture_path}")
    if args.suite_corpus:
        corpus_output = output_path.with_name("suite-corpus")
        metrics = retro_suite_corpus_study(args.suite_corpus, args.corpus_dir, corpus_output, args.seed, args.corpus_provenance)
        for label, value in metrics.items():
            print(f"{label}: {value:.6f}")
    if args.alpha_hidden_suite_study:
        study_path = output_path.with_name("alpha-hidden-suite-study.png")
        fixture_path = Path("fixtures") / "alpha-hidden-suite-fixture.png"
        metrics = alpha_hidden_rgb_suite_study(study_path, fixture_path)
        print(f"Saved {study_path}")
        print(f"Saved {fixture_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.6f}")
    if args.alpha_resize_suite_study:
        study_path = output_path.with_name("alpha-resize-suite-study.png")
        fixture_path = Path("fixtures") / "alpha-resize-suite-fixture.png"
        metrics = alpha_resize_suite_study(study_path, fixture_path)
        print(f"Saved {study_path}")
        print(f"Saved {fixture_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.6f}")
    if args.alpha_rgb_dilation_study:
        study_path = output_path.with_name("alpha-rgb-dilation-study.png")
        fixture_path = Path("fixtures") / "alpha-rgb-dilation-fixture.png"
        metrics = alpha_rgb_dilation_study(study_path, fixture_path)
        print(f"Saved {study_path}")
        print(f"Saved {fixture_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.6f}")
    if args.alpha_dilation_stress_study:
        study_path = output_path.with_name("alpha-dilation-stress-study.png")
        fixture_dir = Path("fixtures") / "alpha-dilation-stress"
        metrics = alpha_dilation_stress_study(study_path, fixture_dir)
        print(f"Saved {study_path}")
        print(f"Saved {fixture_dir}")
        for label, value in metrics.items():
            print(f"{label}: {value:.6f}")
    if args.suite_detail_image:
        if args.suite_detail_box is None:
            parser.error("--suite-detail-image requires --suite-detail-box X Y W H")
        x, y, width, height = args.suite_detail_box
        detail_path = output_path.with_name("suite-detail-crop.png")
        metrics = suite_detail_crop_study(args.suite_detail_image, (x, y, x + width, y + height), detail_path, args.seed)
        print(f"Saved {detail_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.6f}")
    if args.texture_frequency_image:
        if args.texture_frequency_box is None:
            parser.error("--texture-frequency-image requires --texture-frequency-box X Y W H")
        x, y, width, height = args.texture_frequency_box
        metrics_path = output_path.with_name("texture-frequency-suite-metrics.json")
        metrics = texture_frequency_suite_study(
            args.texture_frequency_image, (x, y, width, height), metrics_path, args.seed,
        )
        print(f"Saved {metrics_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.6f}")
    if args.video_grain_frames:
        study_path = output_path.with_name("video-grain-temporal-study.png")
        manifest_path = output_path.with_name("video-grain-temporal-manifest.json")
        metrics = video_grain_temporal_study(args.video_grain_frames, study_path, manifest_path, args.seed)
        print(f"Saved {study_path}")
        print(f"Saved {manifest_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.6f}")
    if args.video_grain_space_frames:
        study_path = output_path.with_name("video-grain-space-study.png")
        manifest_path = output_path.with_name("video-grain-space-manifest.json")
        metrics = video_grain_space_study(args.video_grain_space_frames, study_path, manifest_path, args.seed)
        print(f"Saved {study_path}")
        print(f"Saved {manifest_path}")
        for label, value in metrics.items():
            print(f"{label}: {value:.6f}")


if __name__ == "__main__":
    main()
