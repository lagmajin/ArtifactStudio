"""Review multi-level registered Glow effect intensity captures."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import cv2
import numpy as np

from glow_lab import add_label


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


def capture_stem(effect_id: str, property_name: str, value: float) -> str:
    effect_key = effect_id.replace(".", "_")
    property_key = property_name.lower().replace(" ", "-")
    value_key = str(value).replace(".", "p")
    return f"sweep-{effect_key}-{property_key}-{value_key}-{effect_id}-cpu"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-dir", type=Path, required=True)
    parser.add_argument("--profile", type=Path,
                        default=Path("experiments/GlowLab/glow_role_intensity_sweep_neon.json"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--difference-gain", type=float, default=4.0)
    args = parser.parse_args()

    profile = json.loads(args.profile.read_text(encoding="utf-8"))
    records: list[dict[str, object]] = []
    panels: list[np.ndarray] = []
    for effect_id, sweep in profile["profiles"].items():
        property_name = sweep["varied_property"]
        for value in sweep["values"]:
            stem = capture_stem(effect_id, property_name, value)
            source_path = args.input_dir / f"{stem}-fixture-input.png"
            output_path = args.input_dir / f"{stem}.png"
            source = read_rgba(source_path)
            result = read_rgba(output_path)
            if source.shape != result.shape:
                raise RuntimeError(
                    f"size mismatch for {effect_id} {property_name}={value}: "
                    f"{source.shape} != {result.shape}"
                )

            source_rgb = source[..., :3].astype(np.float32) / 255.0
            result_rgb = result[..., :3].astype(np.float32) / 255.0
            delta = linear_luma(result[..., :3]) - linear_luma(source[..., :3])
            newly_clipped = ((result[..., :3] >= 254) &
                             (source[..., :3] < 254))
            changed = np.abs(result[..., :3].astype(np.int16) -
                             source[..., :3].astype(np.int16)) > 1
            record = {
                "effect_id": effect_id,
                "property": property_name,
                "value": value,
                "input": source_path.name,
                "output": output_path.name,
                "dimensions": [int(source.shape[1]), int(source.shape[0])],
                "input_sha256": digest(source_path),
                "output_sha256": digest(output_path),
                "encoded_rgb_mae": float(np.abs(result_rgb - source_rgb).mean()),
                "linear_luma_delta_mean": float(delta.mean()),
                "linear_luma_delta_p95": float(np.percentile(delta, 95)),
                "linear_luma_delta_max": float(delta.max()),
                "changed_channel_fraction_gt_1_255": float(changed.mean()),
                "newly_clipped_channel_fraction": float(newly_clipped.mean()),
                "alpha_mae": float(np.abs(result[..., 3].astype(np.int16) -
                                          source[..., 3].astype(np.int16)).mean() / 255.0),
            }
            records.append(record)

            small_source = cv2.resize(source, (320, 180), interpolation=cv2.INTER_AREA)
            small_result = cv2.resize(result, (320, 180), interpolation=cv2.INTER_AREA)
            difference = np.clip(
                np.abs(result.astype(np.float32) - source.astype(np.float32)) *
                args.difference_gain, 0, 255).astype(np.uint8)
            small_difference = cv2.resize(
                difference, (320, 180), interpolation=cv2.INTER_AREA)
            panel = np.hstack((
                add_label(small_source, "INPUT"),
                add_label(small_result, "PRODUCT OUTPUT"),
                add_label(small_difference,
                          f"ABS DIFF x{args.difference_gain:g}"),
            ))
            title = (f"{effect_id} {property_name}={value:g} "
                     f"MAE={record['encoded_rgb_mae']:.4f} "
                     f"clip={record['newly_clipped_channel_fraction']:.2%}")
            panels.append(add_label(panel, title))

    if not panels:
        raise RuntimeError("the intensity profile contains no captures")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    sheet = np.vstack(panels)
    if not cv2.imwrite(str(args.output), cv2.cvtColor(sheet, cv2.COLOR_RGBA2BGRA)):
        raise RuntimeError(f"could not write contact sheet: {args.output}")
    metrics_path = args.output.with_suffix(".json")
    metrics_path.write_text(json.dumps({
        "capture_class": "ArtifactUiTest installed-effect CPU reference output",
        "not_final_viewer_or_GPU_output": True,
        "difference_gain": args.difference_gain,
        "records": records,
    }, indent=2) + "\n", encoding="utf-8")
    print(f"Saved {args.output} and {metrics_path}")
    for record in records:
        print(f"{record['effect_id']} {record['property']}={record['value']}: "
              f"MAE={record['encoded_rgb_mae']:.5f}, "
              f"clip={record['newly_clipped_channel_fraction']:.3%}, "
              f"alphaMAE={record['alpha_mae']:.5f}")


if __name__ == "__main__":
    main()
