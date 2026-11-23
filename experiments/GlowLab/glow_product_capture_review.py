"""Review registered Glow-family product captures from capture_ui_test.py."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import cv2
import numpy as np

from glow_lab import add_label


EFFECT_IDS = (
    "glow", "edge_bloom", "reactive_glow", "chromatic_glow", "liquid_glow",
    "residual_glow", "directional_glow", "physical_halation",
    "luminescence_caustics", "builtin.optical_glow", "builtin.volumetric_shine",
    "builtin.glint_star_filter",
)


def read_rgba(path: Path) -> np.ndarray:
    image = cv2.imread(str(path), cv2.IMREAD_UNCHANGED)
    if image is None:
        raise RuntimeError(f"could not read capture: {path}")
    if image.ndim == 2:
        image = cv2.cvtColor(image, cv2.COLOR_GRAY2BGRA)
    elif image.shape[2] == 3:
        image = cv2.cvtColor(image, cv2.COLOR_BGR2BGRA)
    if image.dtype == np.uint16:
        image = np.rint(image.astype(np.float32) / 257.0).astype(np.uint8)
    elif image.dtype != np.uint8:
        image = np.clip(image, 0.0, 255.0).astype(np.uint8)
    return cv2.cvtColor(image, cv2.COLOR_BGRA2RGBA)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def linear_luma(rgb: np.ndarray) -> np.ndarray:
    srgb = rgb.astype(np.float32) / 255.0
    linear = np.where(srgb <= 0.04045, srgb / 12.92,
                      ((srgb + 0.055) / 1.055) ** 2.4)
    return linear @ np.asarray((0.2126, 0.7152, 0.0722), dtype=np.float32)


def contain_preview(image: np.ndarray, width: int = 320,
                    height: int = 213) -> np.ndarray:
    """Fit an RGBA capture into a fixed contact-sheet cell without distorting it."""
    source_height, source_width = image.shape[:2]
    scale = min(width / source_width, height / source_height)
    scaled_width = max(1, round(source_width * scale))
    scaled_height = max(1, round(source_height * scale))
    resized = cv2.resize(image, (scaled_width, scaled_height),
                         interpolation=cv2.INTER_AREA)
    canvas = np.empty((height, width, 4), dtype=np.uint8)
    canvas[..., :3] = 8
    canvas[..., 3] = 255
    x = (width - scaled_width) // 2
    y = (height - scaled_height) // 2
    canvas[y:y + scaled_height, x:x + scaled_width] = resized
    return canvas


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--capture-prefix", default="",
                        help="limit review to captures whose filenames start with this prefix")
    parser.add_argument("--gain-diff", type=float, default=4.0)
    args = parser.parse_args()
    panels: list[np.ndarray] = []
    records: list[dict[str, object]] = []
    for effect_id in EFFECT_IDS:
        matches = sorted(args.input_dir.glob(
            f"{args.capture_prefix}*-{effect_id}-cpu.png"))
        if not matches:
            continue
        for output in matches:
            source_path = output.with_name(f"{output.stem}-fixture-input{output.suffix}")
            source, result = read_rgba(source_path), read_rgba(output)
            if source.shape != result.shape:
                raise RuntimeError(
                    f"size mismatch for {effect_id}: {source.shape} != {result.shape}")
            source_rgb = source[..., :3].astype(np.float32) / 255.0
            result_rgb = result[..., :3].astype(np.float32) / 255.0
            delta = linear_luma(result[..., :3]) - linear_luma(source[..., :3])
            encoded_mae = float(np.abs(result_rgb - source_rgb).mean())
            source_clipped = source[..., :3] >= 254
            result_clipped = result[..., :3] >= 254
            newly_clipped = result_clipped & ~source_clipped
            changed = float(np.count_nonzero(np.abs(result[..., :3].astype(np.int16) -
                                                    source[..., :3].astype(np.int16)) > 1) /
                            (source.shape[0] * source.shape[1] * 3))
            record = {
                "effect_id": effect_id,
                "source": source_path.name,
                "output": output.name,
                "dimensions": [int(source.shape[1]), int(source.shape[0])],
                "source_sha256": digest(source_path),
                "output_sha256": digest(output),
                "encoded_rgb_mae": encoded_mae,
                "linear_luma_delta_mean": float(delta.mean()),
                "linear_luma_delta_p95": float(np.percentile(delta, 95)),
                "linear_luma_delta_max": float(delta.max()),
                "changed_channel_fraction_gt_1_255": changed,
                "newly_clipped_channel_fraction": float(newly_clipped.mean()),
                "alpha_mae": float(np.abs(result[..., 3].astype(np.int16) -
                                          source[..., 3].astype(np.int16)).mean() / 255.0),
            }
            records.append(record)

            width, height = 320, 213
            source_small = contain_preview(source, width, height)
            result_small = contain_preview(result, width, height)
            delta_vis = np.clip(
                np.abs(result.astype(np.float32) - source.astype(np.float32)) *
                args.gain_diff, 0, 255).astype(np.uint8)
            delta_small = contain_preview(delta_vis, width, height)
            capture_label = output.stem.removesuffix(f"-{effect_id}-cpu")
            label = (f"{capture_label}  MAE={encoded_mae:.4f} dL={delta.mean():+.4f} "
                     f"clip={newly_clipped.mean():.3%}")
            panels.append(np.hstack((add_label(source_small, "INPUT"),
                                     add_label(result_small, "PRODUCT OUTPUT"),
                                     add_label(delta_small,
                                               f"ABS DIFF x{args.gain_diff:g}")))
                          )
            panels[-1] = add_label(panels[-1], label)

    if not panels:
        raise RuntimeError(f"no product captures found under {args.input_dir}")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    sheet = np.vstack(panels)
    if not cv2.imwrite(str(args.output), cv2.cvtColor(sheet, cv2.COLOR_RGBA2BGRA)):
        raise RuntimeError(f"could not write contact sheet: {args.output}")
    metrics_path = args.output.with_suffix(".json")
    metrics_path.write_text(json.dumps({
        "capture_class": "ArtifactUiTest installed-effect CPU reference output",
        "not_final_viewer_or_GPU_output": True,
        "records": records,
    }, indent=2) + "\n", encoding="utf-8")
    print(f"Saved {args.output} and {metrics_path}")
    for record in records:
        print(f"{record['effect_id']}: MAE={record['encoded_rgb_mae']:.5f}, "
              f"ΔL={record['linear_luma_delta_mean']:+.5f}, "
              f"changed={record['changed_channel_fraction_gt_1_255']:.2%}, "
              f"clip={record['newly_clipped_channel_fraction']:.4%}, "
              f"alphaMAE={record['alpha_mae']:.5f}")


if __name__ == "__main__":
    main()
