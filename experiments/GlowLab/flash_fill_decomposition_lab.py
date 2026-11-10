"""Render and measure the additive image components of a traveling flash."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import cv2
import numpy as np

from flash_fill_lab import flash_fill, load_image, save_image, srgb_to_linear, to_u8


def luma(rgb: np.ndarray) -> np.ndarray:
    linear = srgb_to_linear(rgb)
    return linear[..., 0] * 0.2126 + linear[..., 1] * 0.7152 + linear[..., 2] * 0.0722


def main() -> None:
    default_input = Path(__file__).resolve().parent / "assets" / "flash-photo-plate.png"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=default_input)
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parent / "captures" / "fill-decomposition-rd1")
    parser.add_argument("--source-position", nargs=2, type=float, default=(0.08, 0.5))
    parser.add_argument("--strength", type=float, default=2.4)
    parser.add_argument("--scatter", type=float, default=2.0)
    parser.add_argument("--scatter-front-coupling", type=float, default=0.0)
    parser.add_argument("--base-veil-gain", type=float, default=0.025,
                        help="veil gain used for the component timeline")
    parser.add_argument("--highlight-threshold", type=float, default=0.10)
    parser.add_argument("--highlight-range", type=float, default=0.42)
    parser.add_argument("--highlight-rolloff", type=float, default=0.0)
    parser.add_argument("--global-fill", type=float, default=0.015)
    parser.add_argument("--front-duration", type=float, default=0.72)
    parser.add_argument("--veil-gains", nargs="+", type=float,
                        default=(0.0, 0.00625, 0.0125, 0.025))
    parser.add_argument("--veil-sweep-progress", type=float, default=0.54)
    parser.add_argument("--samples", nargs="+", type=float, default=(0.18, 0.36, 0.54, 0.72, 0.90))
    args = parser.parse_args()
    if not args.input.is_file():
        parser.error(f"input image does not exist: {args.input}")
    if any(not 0.0 <= t <= 1.0 for t in args.samples) or not 0.0 <= args.veil_sweep_progress <= 1.0:
        parser.error("all sample times must be between 0 and 1")
    if any(gain < 0.0 or gain > 0.05 for gain in args.veil_gains):
        parser.error("veil gains must be between 0 and 0.05")
    args.output.mkdir(parents=True, exist_ok=True)

    source = load_image(args.input)
    source_luma = luma(source[..., :3])
    components = ("source_plus_exposure", "source_plus_scatter", "source_plus_veil",
                  "combined_pre_rolloff", "result")
    rows: list[np.ndarray] = []
    measurements: list[dict[str, float]] = []
    for progress in args.samples:
        layers = flash_fill(
            source, progress, mode="radial", source_position=tuple(args.source_position),
            strength=args.strength, softness=0.19, attack_duration=0.78,
            peak_time=0.78, decay_rate=2.4, radial_falloff=2.7,
            scatter_strength=args.scatter,
            scatter_front_coupling=args.scatter_front_coupling,
            highlight_threshold=args.highlight_threshold,
            highlight_range=args.highlight_range,
            global_fill=args.global_fill, front_duration=args.front_duration,
            highlight_rolloff=args.highlight_rolloff,
            veil_gain=args.base_veil_gain, return_components=True)
        assert isinstance(layers, dict)
        layers["source_plus_exposure"] = layers.pop("exposure")
        layers["source_plus_scatter"] = layers.pop("scatter")
        layers["source_plus_veil"] = layers.pop("veil")
        cells = []
        for name in components:
            image = source if name == "source" else layers[name]
            if not np.isfinite(image).all() or not np.array_equal(image[..., 3], source[..., 3]):
                raise ValueError(f"{name} violated finite-value or alpha-preservation contract")
            save_image(args.output / f"{name}-p{progress:.2f}.png", image)
            image_luma = luma(image[..., :3])
            delta = image_luma - source_luma
            measurements.append({
                "progress": float(progress),
                "component": name,
                "mean_signed_linear_luma_delta": float(delta.mean()),
                "mean_absolute_linear_luma_delta": float(np.abs(delta).mean()),
                "p95_linear_luma": float(np.percentile(image_luma, 95)),
                "newly_clipped_channel_fraction": float(np.mean(
                    (image[..., :3] >= 1.0) & (source[..., :3] < 1.0))),
            })
            thumb = cv2.resize(to_u8(image), (512, 341), interpolation=cv2.INTER_AREA)
            thumb = cv2.cvtColor(thumb, cv2.COLOR_RGBA2BGR)
            header = np.full((36, 512, 3), (25, 28, 36), np.uint8)
            cv2.putText(header, f"{name.replace('_', ' ').upper()} / {progress:.0%}",
                        (10, 24), cv2.FONT_HERSHEY_SIMPLEX, 0.48,
                        (245, 245, 245), 1, cv2.LINE_AA)
            cells.append(np.vstack((header, thumb)))
        rows.append(np.hstack(cells))

    veil_rows: list[np.ndarray] = []
    veil_sweep_metrics = []
    progress = args.veil_sweep_progress
    for gain in args.veil_gains:
        layers = flash_fill(
            source, progress, mode="radial", source_position=tuple(args.source_position),
            strength=args.strength, softness=0.19, attack_duration=0.78,
            peak_time=0.78, decay_rate=2.4, radial_falloff=2.7,
            scatter_strength=args.scatter,
            scatter_front_coupling=args.scatter_front_coupling,
            highlight_threshold=args.highlight_threshold,
            highlight_range=args.highlight_range,
            global_fill=args.global_fill, front_duration=args.front_duration,
            highlight_rolloff=args.highlight_rolloff,
            veil_gain=gain, return_components=True)
        assert isinstance(layers, dict)
        panels = []
        for name, label in (("veil", "SOURCE + VEIL"), ("result", "COMBINED RESULT")):
            image = layers[name]
            save_image(args.output / f"veil-gain-{gain:.5f}-{name}-p{progress:.2f}.png", image)
            thumb = cv2.resize(to_u8(image), (640, 360), interpolation=cv2.INTER_AREA)
            thumb = cv2.cvtColor(thumb, cv2.COLOR_RGBA2BGR)
            header = np.full((36, 640, 3), (25, 28, 36), np.uint8)
            cv2.putText(header, f"VEIL GAIN {gain:.5f} / {label}", (10, 24),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.52, (245, 245, 245), 1, cv2.LINE_AA)
            panels.append(np.vstack((header, thumb)))
        veil_rows.append(np.hstack(panels))
        result_luma = luma(layers["result"][..., :3])
        veil_luma = luma(layers["veil"][..., :3])
        veil_sweep_metrics.append({
            "progress": progress,
            "veil_gain": gain,
            "veil_mean_linear_luma_delta": float((veil_luma - source_luma).mean()),
            "result_mean_linear_luma_delta": float((result_luma - source_luma).mean()),
            "result_p95_linear_luma": float(np.percentile(result_luma, 95)),
            "result_newly_clipped_channel_fraction": float(np.mean(
                (layers["result"][..., :3] >= 1.0) & (source[..., :3] < 1.0))),
        })

    cv2.imwrite(str(args.output / "veil-gain-sweep.png"), np.vstack(veil_rows))

    cv2.imwrite(str(args.output / "flash-fill-decomposition.png"), np.vstack(rows))
    report = {
        "experiment": "traveling flash additive component decomposition",
        "input_image": str(args.input.resolve()),
        "input_dimensions": {"width": int(source.shape[1]), "height": int(source.shape[0])},
        "source_position": list(args.source_position),
        "strength": args.strength,
        "scatter_strength": args.scatter,
        "scatter_front_coupling": args.scatter_front_coupling,
        "highlight_threshold": args.highlight_threshold,
        "highlight_range": args.highlight_range,
        "highlight_rolloff": args.highlight_rolloff,
        "global_fill": args.global_fill,
        "front_duration": args.front_duration,
        "base_veil_gain": args.base_veil_gain,
        "sample_times": list(args.samples),
        "measurements": measurements,
        "veil_gain_sweep": {
            "progress": progress,
            "gains": list(args.veil_gains),
            "measurements": veil_sweep_metrics,
        },
        "limits": [
            "Exposure, scatter, and veil panels show each layer added independently over the source.",
            "Combined pre-rolloff is the additive linear-light composition; final result may differ when highlight rolloff is enabled.",
            "The source plate is not a calibrated camera capture, and its depth/occlusion is not reconstructed.",
        ],
    }
    with (args.output / "flash-fill-decomposition-metrics.json").open(
            "w", encoding="utf-8", newline="\n") as stream:
        json.dump(report, stream, indent=2, ensure_ascii=False)
        stream.write("\n")
    print(f"Saved flash component decomposition to {args.output.resolve()}")


if __name__ == "__main__":
    main()
