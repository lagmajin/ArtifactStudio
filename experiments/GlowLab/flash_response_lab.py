"""Compare hard clipping and a linear-light flash highlight shoulder."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import cv2
import numpy as np

from flash_calibration_chart import make_chart
from flash_fill_lab import flash_fill, load_image
from glow_lab import srgb_to_linear, to_u8


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parent / "captures" / "response")
    parser.add_argument("--progress", type=float, default=0.64)
    parser.add_argument("--strength", type=float, default=6.0)
    parser.add_argument("--input", type=Path,
                        help="optional photo-style test plate; defaults to the calibration chart")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    source = load_image(args.input) if args.input else make_chart()
    panels: list[np.ndarray] = []
    metrics: dict[str, object] = {"progress": args.progress, "exposure_strength": args.strength,
                                  "variants": []}
    metrics["input_image"] = str(args.input.resolve()) if args.input else "<flash calibration chart>"
    gray_steps = (0.025, 0.045, 0.08, 0.14, 0.24, 0.40, 0.62, 0.86)
    patch_width, patch_y0, patch_y1 = 90, 142, 232
    for rolloff in (0.0, 0.45, 1.0):
        result = flash_fill(source, args.progress, mode="radial", source_position=(0.04, 0.5),
                            strength=args.strength, softness=0.20, attack_duration=0.72,
                            peak_time=0.78, decay_rate=8.0, scatter_strength=1.4,
                            global_fill=0.08, front_duration=0.72, radial_falloff=1.8,
                            highlight_rolloff=rolloff)
        assert np.isfinite(result).all()
        assert np.array_equal(result[..., 3], source[..., 3])
        rgb8 = to_u8(result)
        title = f"flash-rolloff-{rolloff:.2f}"
        path = args.output / f"{title}.png"
        cv2.imwrite(str(path), cv2.cvtColor(rgb8, cv2.COLOR_RGBA2BGRA))
        thumb_height = max(1, int(round(rgb8.shape[0] * 640 / rgb8.shape[1])))
        thumb = cv2.resize(cv2.cvtColor(rgb8, cv2.COLOR_RGBA2BGRA), (640, thumb_height),
                           interpolation=cv2.INTER_AREA)
        header = np.full((44, 640, 4), (22, 27, 37, 255), dtype=np.uint8)
        label = "HARD CLIP" if rolloff == 0.0 else f"SHOULDER {rolloff:.2f}"
        cv2.putText(header, label, (18, 30), cv2.FONT_HERSHEY_SIMPLEX,
                    0.65, (245, 245, 245, 255), 1, cv2.LINE_AA)
        panels.append(np.vstack((header, thumb)))

        linear = srgb_to_linear(result[..., :3])
        luma = linear[..., 0] * 0.2126 + linear[..., 1] * 0.7152 + linear[..., 2] * 0.0722
        variant_metrics: dict[str, object] = {
            "highlight_rolloff": rolloff,
            "mean_linear_luma": float(luma.mean()),
            "near_white_fraction": float(np.mean(np.max(result[..., :3], axis=2) > 0.97)),
            "clipped_channel_fraction": float(np.mean(result[..., :3] >= 1.0)),
        }
        if args.input is None:
            patches = []
            for index, nominal in enumerate(gray_steps):
                x0 = 38 + index * (patch_width + 10)
                sample = luma[patch_y0 + 20:patch_y1 - 15, x0 + 15:x0 + patch_width - 15]
                patches.append({"input_code_value": nominal, "mean_output_linear_luma": float(sample.mean()),
                                "std_output_linear_luma": float(sample.std()),
                                "near_white_fraction": float(np.mean(sample > 0.97))})
            variant_metrics["gray_patches"] = patches
        else:
            source_linear = srgb_to_linear(source[..., :3])
            source_luma = (source_linear[..., 0] * 0.2126 + source_linear[..., 1] * 0.7152 +
                           source_linear[..., 2] * 0.0722)
            highlight_mask = (source_luma > 0.18) & (source[..., 3] > 0.01)
            chroma = (lambda rgb: (np.max(rgb, axis=2) - np.min(rgb, axis=2)) /
                      np.maximum(np.max(rgb, axis=2), 1.0e-6))
            output_chroma = chroma(linear)
            variant_metrics["source_highlight_pixel_fraction"] = float(highlight_mask.mean())
            variant_metrics["source_highlight_mean_output_luma"] = float(luma[highlight_mask].mean())
            variant_metrics["source_highlight_clipped_channel_fraction"] = float(
                np.mean(result[..., :3][highlight_mask] >= 1.0))
            variant_metrics["source_highlight_mean_linear_chroma"] = float(
                output_chroma[highlight_mask].mean())
        metrics["variants"].append(variant_metrics)
    cv2.imwrite(str(args.output / "flash-highlight-rolloff-comparison.png"), np.hstack(panels))
    with (args.output / "flash-highlight-rolloff-metrics.json").open(
            "w", encoding="utf-8", newline="\n") as stream:
        json.dump(metrics, stream, indent=2, ensure_ascii=False)
        stream.write("\n")
    print(f"Saved highlight rolloff comparison to {args.output.resolve()}")


if __name__ == "__main__":
    main()
