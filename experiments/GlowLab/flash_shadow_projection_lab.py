"""Compare image-space flash fill with geometry-driven cast-shadow projection.

The chart has known 2.5D geometry. It deliberately does not claim that an
arbitrary photograph can provide this geometry automatically.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import cv2
import numpy as np

from flash_surface_lab import make_surface_chart
from glow_lab import linear_to_srgb, srgb_to_linear, to_u8


def projected_shadow(width: int, height: int, light_xy: tuple[float, float],
                     height_px: float, softness_px: float) -> np.ndarray:
    """Project a raised-object silhouette onto a horizontal image-space plane."""
    silhouette = np.zeros((height, width), np.uint8)
    cv2.rectangle(silhouette, (220, 212), (702, 604), 255, -1)
    # Cast down onto the checkerboard floor, away from the light's horizontal side.
    horizontal_sign = -1.0 if light_xy[0] < 461.0 else 1.0
    shift = np.array([horizontal_sign * float(height_px) * 0.55,
                      float(height_px)], np.float32)
    matrix = np.array([[1.0, 0.0, shift[0]], [0.0, 1.0, shift[1]]], np.float32)
    mask = cv2.warpAffine(silhouette, matrix, (width, height), flags=cv2.INTER_LINEAR,
                          borderMode=cv2.BORDER_CONSTANT, borderValue=0)
    softness = max(0.1, float(softness_px))
    return cv2.GaussianBlur(mask.astype(np.float32) / 255.0, (0, 0), softness)


def shade_known_surfaces(rgb: np.ndarray, light_xy: tuple[float, float],
                         strength: float, progress: float) -> tuple[np.ndarray, dict[str, float]]:
    """Apply a simple Lambert response to the chart's known panel/sphere surfaces."""
    height, width = rgb.shape[:2]
    y, x = np.mgrid[0:height, 0:width].astype(np.float32)
    panel = (x >= 220) & (x <= 702) & (y >= 212) & (y <= 604)
    light = np.array([light_xy[0] - 461.0, 408.0 - light_xy[1], 420.0], np.float32)
    light /= max(float(np.linalg.norm(light)), 1.0e-6)
    # A broad, gently curved normal field stands in for the chart's bevelled panel.
    nx = np.clip((461.0 - x) / 300.0, -0.48, 0.48)
    ny = np.clip((408.0 - y) / 270.0, -0.32, 0.32)
    nz = np.sqrt(np.maximum(1.0 - nx * nx - ny * ny, 0.1))
    ndotl = np.maximum(nx * light[0] + ny * light[1] + nz * light[2], 0.0)
    ambient = 0.32
    panel_response = ambient + (1.0 - ambient) * ndotl
    sphere_x, sphere_y, radius = 918.0, 380.0, 145.0
    sx, sy = (x - sphere_x) / radius, (y - sphere_y) / radius
    r2 = sx * sx + sy * sy
    sphere = r2 <= 1.0
    sz = np.sqrt(np.maximum(1.0 - r2, 0.0))
    sphere_ndotl = np.maximum(-sx * light[0] - sy * light[1] + sz * light[2], 0.0)
    sphere_response = ambient + (1.0 - ambient) * sphere_ndotl
    response = np.ones((height, width), np.float32)
    response[panel] = panel_response[panel]
    response[sphere] = sphere_response[sphere]
    blend = float(np.clip(progress / 0.72, 0.0, 1.0))
    radial = np.sqrt(((x - light_xy[0]) / width) ** 2 +
                     ((y - light_xy[1]) / height) ** 2)
    attenuation = 1.0 / (1.0 + radial * 2.2)
    flash_gain = np.exp2(max(strength, 0.0) / 2.0)
    effective_response = (ambient + (response - ambient) * attenuation) * blend
    lit = rgb * (1.0 + effective_response[..., None] * (flash_gain - 1.0))
    metrics = {
        "panel_mean_response": float(panel_response[panel].mean()),
        "sphere_mean_response": float(sphere_response[sphere].mean()),
        "panel_lighting_variation_std": float(panel_response[panel].std()),
        "light_direction_z": float(light[2]),
    }
    return lit, metrics


def draw_source(rgb: np.ndarray, light_xy: tuple[float, float], tint: tuple[float, float, float]) -> None:
    """Add a compact visible source marker and restrained halo to linear RGB."""
    height, width = rgb.shape[:2]
    x = int(np.clip(light_xy[0], 0, width - 1))
    y = int(np.clip(light_xy[1], 0, height - 1))
    marker = np.zeros((height, width), np.float32)
    cv2.circle(marker, (x, y), 13, 1.0, -1, cv2.LINE_AA)
    halo = cv2.GaussianBlur(marker, (0, 0), 30)
    tint_array = np.asarray(tint, np.float32)
    rgb += halo[..., None] * tint_array[None, None, :] * 0.55
    rgb += marker[..., None] * tint_array[None, None, :] * 3.0


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parent / "captures" / "shadow-projection")
    parser.add_argument("--progress", type=float, default=0.76)
    parser.add_argument("--strength", type=float, default=2.8)
    parser.add_argument("--shadow-height", type=float, default=105.0,
                        help="shadow displacement for the fixed-height test panel")
    parser.add_argument("--shadow-softness", type=float, default=13.0)
    parser.add_argument("--shadow-opacity", type=float, default=0.62)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    source, _, existing_shadow = make_surface_chart()
    height, width = source.shape[:2]
    source_linear = srgb_to_linear(source[..., :3])
    light_positions = ((72.0, 150.0), (1000.0, 115.0))
    tint = (1.0, 0.82, 0.62)
    panels: list[np.ndarray] = []
    results: dict[str, object] = {
        "experiment": "known-geometry directional flash, surface response, and projected shadow",
        "progress": args.progress,
        "light_positions": [list(point) for point in light_positions],
        "shadow_height_px": args.shadow_height,
        "shadow_softness_px": args.shadow_softness,
        "shadow_opacity": args.shadow_opacity,
        "limitations": [
            "The panel geometry and height are supplied by this synthetic scene.",
            "The sphere keeps a fixed baked shading and shadow in this experiment.",
            "This is an image-space projection cue, not a world-space renderer.",
        ],
        "variants": [],
    }
    for index, light_xy in enumerate(light_positions):
        distance = max(float(np.linalg.norm(np.asarray(light_xy) - np.array([461.0, 408.0]))), 1.0)
        height_scale = float(np.clip(args.shadow_height / distance, 0.05, 0.65))
        # Use only the panel's top edge as the occluder; its projected shadow
        # lands on the open floor immediately behind it.
        shadow = projected_shadow(width, height, light_xy,
                                  args.shadow_height, args.shadow_softness)
        shadow[:604, :] = 0.0
        shadow[:, :130] = 0.0
        shadow[:, 850:] = 0.0

        # Treat normalized distance from the source as a coarse direct-light field.
        y, x = np.mgrid[0:height, 0:width].astype(np.float32)
        lit, surface_metrics = shade_known_surfaces(
            source_linear, light_xy, args.strength, args.progress)
        lit *= np.asarray(tint, np.float32)[None, None, :]
        # Preserve the original plate shadow while adding the projected one.
        cast_factor = np.clip(shadow * args.shadow_opacity, 0.0, 0.95)
        with_shadow = lit * (1.0 - cast_factor[..., None])
        no_shadow = lit.copy()
        draw_source(no_shadow, light_xy, tint)
        draw_source(with_shadow, light_xy, tint)

        for title, image in ((f"LIGHT {index + 1} / NO PROJECTED SHADOW", no_shadow),
                             (f"LIGHT {index + 1} / PROJECTED SHADOW", with_shadow)):
            srgb = np.clip(linear_to_srgb(np.maximum(image, 0.0)), 0.0, 1.0)
            rgba = np.dstack((srgb, source[..., 3]))
            path = args.output / f"shadow-projection-{index + 1}-{'shadow' if 'PROJECTED' in title else 'no-shadow'}.png"
            cv2.imwrite(str(path), cv2.cvtColor(to_u8(rgba), cv2.COLOR_RGBA2BGRA))
            thumb = cv2.resize(cv2.cvtColor(to_u8(rgba), cv2.COLOR_RGBA2BGR),
                               (640, 360), interpolation=cv2.INTER_AREA)
            header = np.full((42, 640, 3), (22, 27, 37), np.uint8)
            cv2.putText(header, title, (12, 28), cv2.FONT_HERSHEY_SIMPLEX,
                        0.48, (245, 245, 245), 1, cv2.LINE_AA)
            panels.append(np.vstack((header, thumb)))
        panel_region = (x >= 220) & (x <= 702) & (y >= 212) & (y <= 604)
        caster_region = (shadow > 0.2) & ~panel_region & (y >= 604)
        unshadowed_region = (shadow < 0.02) & (x > 710) & (x < 850) & (y >= 604)
        luma = with_shadow[..., 0] * 0.2126 + with_shadow[..., 1] * 0.7152 + with_shadow[..., 2] * 0.0722
        results["variants"].append({
            "light_xy": list(light_xy),
            "shadow_centroid_xy": [float((shadow * x).sum() / max(shadow.sum(), 1.0e-6)),
                                   float((shadow * y).sum() / max(shadow.sum(), 1.0e-6))],
            "shadow_strength_scale": height_scale,
            "cast_region_mean_linear_luma": float(luma[caster_region].mean()),
            "adjacent_region_mean_linear_luma": float(luma[unshadowed_region].mean()),
            "source_shadow_mean_difference": float(luma[unshadowed_region].mean() - luma[caster_region].mean()),
            "inherited_shadow_mean": float(existing_shadow.mean()),
            **surface_metrics,
        })
    cv2.imwrite(str(args.output / "shadow-projection-comparison.png"), np.vstack(panels))
    with (args.output / "shadow-projection-metrics.json").open("w", encoding="utf-8", newline="\n") as stream:
        json.dump(results, stream, indent=2, ensure_ascii=False)
        stream.write("\n")
    print(f"Saved projected-shadow comparison to {args.output.resolve()}")


if __name__ == "__main__":
    main()
