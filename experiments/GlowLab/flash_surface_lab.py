"""Create a deterministic surface and cast-shadow chart for flash-fill R&D."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import cv2
import numpy as np

from flash_fill_lab import flash_fill
from glow_lab import linear_to_srgb, srgb_to_linear, to_u8


def make_surface_chart(width: int = 1280, height: int = 720) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Return a linear-light RGBA scene, relative inverse-depth and shadow matte."""
    y, x = np.mgrid[0:height, 0:width].astype(np.float32)
    xn, yn = x / max(width - 1, 1), y / max(height - 1, 1)
    depth = np.clip(0.20 + 0.64 * xn + 0.08 * np.sin(yn * np.pi), 0.0, 1.0)
    rgb = np.empty((height, width, 3), dtype=np.float32)
    checker = (((x // 56).astype(np.int32) + (y // 56).astype(np.int32)) & 1)
    floor_value = np.where(checker == 0, 0.075, 0.125).astype(np.float32)
    rgb[:] = np.stack((floor_value * 0.76, floor_value * 0.86, floor_value), axis=-1)

    # A raised matte panel with a bevel gives clear planar edges and a cast shadow.
    panel = np.zeros((height, width), dtype=np.uint8)
    cv2.rectangle(panel, (220, 212), (702, 604), 255, -1)
    shadow = cv2.warpAffine(panel, np.float32([[1, 0, 70], [0, 1, 35]]),
                            (width, height), flags=cv2.INTER_LINEAR)
    shadow_soft = cv2.GaussianBlur(shadow, (0, 0), 22).astype(np.float32) / 255.0
    shadow_matte = shadow_soft.copy()
    rgb *= (1.0 - shadow_soft[..., None] * 0.78)
    panel_f = panel.astype(np.float32) / 255.0
    edge = cv2.GaussianBlur(panel_f, (0, 0), 5)
    panel_albedo = np.stack((0.22 + 0.05 * xn, 0.25 + 0.04 * yn,
                             0.31 + 0.03 * (1.0 - xn)), axis=-1)
    rgb = rgb * (1.0 - panel_f[..., None]) + panel_albedo * panel_f[..., None]
    # Directional shading on the panel provides a normal-like cue without
    # claiming to reconstruct a scene normal from an arbitrary photo.
    panel_shade = np.clip(0.72 + 0.34 * (1.0 - xn) - 0.16 * yn, 0.42, 1.0)
    rgb = np.where(panel_f[..., None] > 0.0, rgb * panel_shade[..., None], rgb)
    bevel = np.clip(edge - panel_f * 0.55, 0.0, 1.0)
    rgb += bevel[..., None] * np.array([0.045, 0.05, 0.06], dtype=np.float32)
    depth = np.where(panel_f > 0.5, 0.32, depth)

    # A matte sphere with a soft terminator and a contact shadow.
    cx, cy, radius = 918.0, 380.0, 145.0
    sx, sy = (x - cx) / radius, (y - cy) / radius
    r2 = sx * sx + sy * sy
    sphere = r2 <= 1.0
    nz = np.sqrt(np.maximum(1.0 - r2, 0.0))
    light = np.array([-0.48, -0.62, 0.62], dtype=np.float32)
    light /= np.linalg.norm(light)
    lambert = np.clip((-sx * light[0] - sy * light[1] + nz * light[2]), 0.0, 1.0)
    sphere_rgb = np.stack((0.19, 0.115, 0.07), axis=0)[None, None, :] * (0.10 + 0.90 * lambert[..., None])
    sphere_rgb += (np.maximum(lambert, 0.0) ** 24)[..., None] * np.array([0.12, 0.10, 0.075])
    sphere_shadow = np.exp(-(((x - (cx + 92.0)) / 80.0) ** 2 +
                              ((y - (cy + 115.0)) / 25.0) ** 2)) * 0.7
    shadow_matte = np.maximum(shadow_matte, sphere_shadow.astype(np.float32))
    rgb *= (1.0 - sphere_shadow[..., None])
    rgb = np.where(sphere[..., None], sphere_rgb, rgb)
    depth = np.where(sphere, np.clip(0.18 + 0.16 * (1.0 - nz), 0.0, 1.0), depth)

    cv2.putText(rgb, "SURFACE / CAST SHADOW", (48, 72), cv2.FONT_HERSHEY_SIMPLEX,
                0.75, (0.42, 0.48, 0.58), 2, cv2.LINE_AA)
    cv2.putText(rgb, "MATTE PANEL", (290, 190), cv2.FONT_HERSHEY_SIMPLEX,
                0.52, (0.38, 0.43, 0.52), 1, cv2.LINE_AA)
    cv2.putText(rgb, "DIFFUSE SPHERE", (845, 570), cv2.FONT_HERSHEY_SIMPLEX,
                0.46, (0.40, 0.44, 0.50), 1, cv2.LINE_AA)
    rgba = np.dstack((np.clip(linear_to_srgb(np.maximum(rgb, 0.0)), 0.0, 1.0),
                      np.ones((height, width), dtype=np.float32)))
    return rgba, depth.astype(np.float32), shadow_matte.astype(np.float32)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parent / "captures" / "surface")
    parser.add_argument("--progress", type=float, default=0.58)
    parser.add_argument("--depth-front-strength", type=float, default=0.16)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    source, depth, shadow_matte = make_surface_chart()
    source_path = args.output / "surface-source.png"
    depth_path = args.output / "surface-depth.png"
    cv2.imwrite(str(source_path), cv2.cvtColor(to_u8(source), cv2.COLOR_RGBA2BGRA))
    cv2.imwrite(str(depth_path), np.clip(depth * 255.0, 0, 255).astype(np.uint8))
    cv2.imwrite(str(args.output / "surface-shadow-matte.png"),
                np.clip(shadow_matte * 255.0, 0, 255).astype(np.uint8))

    panels = [cv2.cvtColor(to_u8(source), cv2.COLOR_RGBA2BGRA)]
    panel_names = ["SOURCE"]
    metrics: dict[str, object] = {"progress": args.progress, "variants": []}
    for name, fill, softness, front, shadow_lift in (
            ("2D WIPE", 0.10, 0.19, 0.0, 1.0),
            ("DEPTH FRONT", 0.10, 0.19, args.depth_front_strength, 1.0),
            ("SHADOW RETENTION", 0.10, 0.19, args.depth_front_strength, 0.35)):
        result = flash_fill(source, args.progress, mode="radial", source_position=(0.04, 0.45),
                            strength=3.8, softness=softness, attack_duration=0.72,
                            peak_time=0.78, decay_rate=10.0, scatter_strength=0.8,
                            global_fill=fill, front_duration=0.72, radial_falloff=2.4,
                            depth_map=depth, depth_front_strength=front)
        if shadow_lift < 1.0:
            source_linear = srgb_to_linear(source[..., :3])
            result_linear = srgb_to_linear(result[..., :3])
            reduction = shadow_matte * (1.0 - shadow_lift) * 0.85
            result_linear = source_linear + (result_linear - source_linear) * (1.0 - reduction[..., None])
            result[..., :3] = np.clip(linear_to_srgb(np.maximum(result_linear, 0.0)), 0.0, 1.0)
        assert np.isfinite(result).all()
        assert np.array_equal(result[..., 3], source[..., 3])
        out_path = args.output / f"surface-{name.lower().replace(' ', '-')}.png"
        cv2.imwrite(str(out_path), cv2.cvtColor(to_u8(result), cv2.COLOR_RGBA2BGRA))
        panels.append(cv2.cvtColor(to_u8(result), cv2.COLOR_RGBA2BGRA))
        panel_names.append(name)
        linear = srgb_to_linear(result[..., :3])
        luma = linear[..., 0] * 0.2126 + linear[..., 1] * 0.7152 + linear[..., 2] * 0.0722
        metrics["variants"].append({
            "name": name, "global_fill": fill, "softness": softness,
            "depth_front_strength": front,
            "shadow_lift_retention": shadow_lift,
            "mean_linear_luma": float(luma.mean()),
            "near_white_fraction": float(np.mean(np.max(result[..., :3], axis=2) > 0.97)),
            "shadow_region_mean_luma": float(luma[300:520, 710:815].mean()),
            "sphere_region_mean_luma": float(luma[220:540, 780:1080].mean()),
        })
    thumbs = []
    for image, title in zip(panels, panel_names):
        image = cv2.resize(image, (640, 360), interpolation=cv2.INTER_AREA)
        header = np.full((44, 640, 4), (22, 27, 37, 255), dtype=np.uint8)
        cv2.putText(header, title, (18, 30), cv2.FONT_HERSHEY_SIMPLEX,
                    0.65, (245, 245, 245, 255), 1, cv2.LINE_AA)
        thumbs.append(np.vstack((header, image)))
    sheet = np.hstack(thumbs)
    cv2.imwrite(str(args.output / "surface-flash-comparison.png"), sheet)
    with (args.output / "surface-flash-metrics.json").open("w", encoding="utf-8", newline="\n") as stream:
        json.dump(metrics, stream, indent=2, ensure_ascii=False)
        stream.write("\n")
    print(f"Saved surface flash comparison to {args.output.resolve()}")


if __name__ == "__main__":
    main()
