"""Compare global highlight bloom with scatter gated by a traveling flash front."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import cv2
import numpy as np

from flash_fill_lab import flash_fill, load_image, save_image, srgb_to_linear, to_u8


def main() -> None:
    default_input = Path(__file__).resolve().parent / "assets" / "flash-photo-plate.png"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=default_input)
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parent / "captures" / "scatter-front-rd2")
    parser.add_argument("--source-position", nargs=2, type=float, default=(0.08, 0.5))
    parser.add_argument("--strength", type=float, default=2.4)
    parser.add_argument("--scatter", type=float, default=2.0)
    parser.add_argument("--veil-gain", type=float, default=0.025)
    parser.add_argument("--highlight-threshold", type=float, default=0.10)
    parser.add_argument("--highlight-range", type=float, default=0.42)
    parser.add_argument("--front-duration", type=float, default=0.72)
    parser.add_argument("--samples", nargs="+", type=float, default=(0.25, 0.45, 0.65, 0.85))
    args = parser.parse_args()
    if not args.input.is_file():
        parser.error(f"input image does not exist: {args.input}")
    if any(not 0.0 <= t <= 1.0 for t in args.samples):
        parser.error("all sample times must be between 0 and 1")
    args.output.mkdir(parents=True, exist_ok=True)

    source = load_image(args.input)
    couplings = (0.0, 0.5, 1.0)
    rows: list[np.ndarray] = []
    measurements: list[dict[str, float]] = []
    source_linear = srgb_to_linear(source[..., :3])
    source_luma = (source_linear[..., 0] * 0.2126 + source_linear[..., 1] * 0.7152 +
                   source_linear[..., 2] * 0.0722)
    for coupling in couplings:
        cells = []
        for progress in args.samples:
            result = flash_fill(
                source, progress, mode="radial",
                source_position=tuple(args.source_position),
                strength=args.strength, softness=0.19,
                attack_duration=0.78, peak_time=0.78, decay_rate=2.4,
                radial_falloff=2.7, scatter_strength=args.scatter,
                highlight_threshold=args.highlight_threshold,
                highlight_range=args.highlight_range,
                global_fill=0.015, front_duration=args.front_duration,
                highlight_rolloff=0.78, veil_gain=args.veil_gain,
                scatter_front_coupling=coupling)
            if not np.isfinite(result).all() or not np.array_equal(result[..., 3], source[..., 3]):
                raise ValueError("flash output violated finite-value or alpha-preservation contract")
            linear = srgb_to_linear(result[..., :3])
            luma = linear[..., 0] * 0.2126 + linear[..., 1] * 0.7152 + linear[..., 2] * 0.0722
            delta = luma - source_luma
            measurements.append({
                "scatter_front_coupling": coupling,
                "progress": float(progress),
                "mean_linear_luma_delta": float(delta.mean()),
                "p95_linear_luma": float(np.percentile(luma, 95)),
                "newly_clipped_channel_fraction": float(np.mean(
                    (result[..., :3] >= 1.0) & (source[..., :3] < 1.0))),
                "mean_absolute_linear_luma_delta_from_global": 0.0,
            })
            save_image(args.output / f"coupling-{coupling:.1f}-p{progress:.2f}.png", result)
            thumb = cv2.resize(to_u8(result), (640, 360), interpolation=cv2.INTER_AREA)
            thumb = cv2.cvtColor(thumb, cv2.COLOR_RGBA2BGR)
            header = np.full((38, 640, 3), (25, 28, 36), np.uint8)
            cv2.putText(header, f"SCATTER FRONT COUPLING {coupling:.1f} / {progress:.0%}",
                        (12, 26), cv2.FONT_HERSHEY_SIMPLEX, 0.58, (245, 245, 245), 1, cv2.LINE_AA)
            cells.append(np.vstack((header, thumb)))
        rows.append(np.hstack(cells))

    # Compare each gated result to the uncoupled global bloom at the same time.
    by_key = {(m["scatter_front_coupling"], m["progress"]): m for m in measurements}
    for progress in args.samples:
        reference = load_image(args.output / f"coupling-0.0-p{progress:.2f}.png")
        reference_luma = srgb_to_linear(reference[..., :3])
        reference_luma = (reference_luma[..., 0] * 0.2126 + reference_luma[..., 1] * 0.7152 +
                          reference_luma[..., 2] * 0.0722)
        for coupling in couplings[1:]:
            candidate = load_image(args.output / f"coupling-{coupling:.1f}-p{progress:.2f}.png")
            candidate_luma = srgb_to_linear(candidate[..., :3])
            candidate_luma = (candidate_luma[..., 0] * 0.2126 + candidate_luma[..., 1] * 0.7152 +
                              candidate_luma[..., 2] * 0.0722)
            by_key[(coupling, progress)]["mean_absolute_linear_luma_delta_from_global"] = float(
                np.abs(candidate_luma - reference_luma).mean())

    contact = np.vstack(rows)
    cv2.imwrite(str(args.output / "scatter-front-comparison.png"), contact)
    report = {
        "experiment": "highlight sources gated by traveling front before optical PSF spread",
        "input_image": str(args.input.resolve()),
        "input_dimensions": {"width": int(source.shape[1]), "height": int(source.shape[0])},
        "source_position": list(args.source_position),
        "strength": args.strength,
        "scatter": args.scatter,
        "veil_gain": args.veil_gain,
        "highlight_threshold": args.highlight_threshold,
        "highlight_range": args.highlight_range,
        "front_duration": args.front_duration,
        "couplings": list(couplings),
        "sample_times": list(args.samples),
        "measurements": measurements,
        "limits": [
            "The portrait plate is an R&D image, not a calibrated or licensed camera test chart.",
            "Coupling gates highlight sources by the image-space front before PSF spread; it does not model scene occlusion.",
            "SDR clipping and luma metrics do not establish perceptual realism or HDR behavior.",
        ],
    }
    with (args.output / "scatter-front-metrics.json").open("w", encoding="utf-8", newline="\n") as stream:
        json.dump(report, stream, indent=2, ensure_ascii=False)
        stream.write("\n")
    print(f"Saved scatter-front comparison to {args.output.resolve()}")


if __name__ == "__main__":
    main()
