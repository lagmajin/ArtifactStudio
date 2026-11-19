"""Measure a Playground shot and produce review composites from saved PNGs."""

import argparse
import hashlib
import json
from math import ceil
from pathlib import Path
from statistics import median

from PIL import Image, ImageChops


REVIEW_FRAMES = (30, 59, 90, 120, 149)
# Fixed rectangles in source pixels so framing is comparable over time.
REVIEW_REGIONS = {
    "title": {"letter_counters": (810, 390, 1110, 690)},
    "sand": {"stream_split": (600, 300, 1300, 900)},
}


def analyze(directory, shot, output):
    capture = json.loads((directory / "capture_measurements.json").read_text(encoding="utf-8"))
    if (capture["shot"], capture["width"], capture["height"], capture["fps"]) != (
        shot, 1920, 1080, 30
    ):
        raise ValueError("Capture metadata does not match the requested shot")
    if [item["frame"] for item in capture["frames"]] != list(range(150)):
        raise ValueError("Expected timing records for frames 0 through 149")
    stop_frame = capture["emission_stop_frame"]
    if not 0 < stop_frame < 149:
        raise ValueError("Emission stop frame is outside the capture")
    timings = [item["offscreen_render_and_readback_ns"] for item in capture["frames"]]
    if any(type(value) is not int or value < 0 for value in timings):
        raise ValueError("Expected nonnegative integer render times")

    def summarize(values):
        ordered = sorted(values)
        return {
            "frames": len(ordered),
            "median_ms": median(ordered) / 1e6,
            "p95_ms": ordered[ceil(len(ordered) * 0.95) - 1] / 1e6,
        }

    performance = {
        "first_frame_ms": timings[0] / 1e6,
        "emission_stop_frame": stop_frame,
        "emission_stop_frame_ms": timings[stop_frame] / 1e6,
        "during_emission": summarize(timings[1:stop_frame]) if stop_frame > 1 else None,
        "after_emission": summarize(timings[stop_frame + 1:]),
    }
    output.mkdir(parents=True, exist_ok=True)
    frames = []
    previous_alpha = None
    previous_rgb = None
    background_rgb = None
    for frame in range(150):
        path = directory / f"{shot}_{frame:04}.png"
        with Image.open(path) as source:
            if source.size != (1920, 1080) or source.mode != "RGBA":
                raise ValueError(f"{path}: expected 1920x1080 RGBA PNG")
            image = source.copy()
        alpha = image.getchannel("A")
        rgb = image.convert("RGB")
        if background_rgb is None:
            background_rgb = rgb.getpixel((0, 0))
        rgb_difference = ImageChops.difference(
            rgb, Image.new("RGB", rgb.size, background_rgb)
        )
        red_difference, green_difference, blue_difference = rgb_difference.split()
        max_channel_difference = ImageChops.lighter(
            ImageChops.lighter(red_difference, green_difference), blue_difference
        )
        above_threshold = max_channel_difference.point(
            lambda value: 255 if value > 10 else 0
        )
        threshold_histogram = above_threshold.histogram()
        threshold_pixels = threshold_histogram[255]
        threshold_bbox = above_threshold.getbbox()
        threshold_bbox_width_fraction = (
            (threshold_bbox[2] - threshold_bbox[0]) / 1920
            if threshold_bbox else 0.0
        )
        threshold_bbox_height_fraction = (
            (threshold_bbox[3] - threshold_bbox[1]) / 1080
            if threshold_bbox else 0.0
        )
        histogram = alpha.histogram()
        difference = None
        if previous_alpha is not None:
            delta = ImageChops.difference(previous_alpha, alpha).histogram()
            difference = sum(value * count for value, count in enumerate(delta))
            difference /= 1920 * 1080
        frames.append({
            "frame": frame,
            "offscreen_render_and_readback_ns": timings[frame],
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "alpha_bbox": alpha.getbbox(),
            "rgb_bbox": rgb.getbbox(),
            "background_rgb": background_rgb,
            "rgb_difference_bbox": rgb_difference.getbbox(),
            "rgb_difference_histogram": rgb_difference.histogram(),
            "rgb_difference_pixels_gt10": threshold_pixels,
            "rgb_difference_fraction_gt10": threshold_pixels / (1920 * 1080),
            "rgb_difference_bbox_gt10": threshold_bbox,
            "rgb_difference_bbox_width_fraction_gt10": threshold_bbox_width_fraction,
            "rgb_difference_bbox_height_fraction_gt10": threshold_bbox_height_fraction,
            "alpha_histogram": histogram,
            "previous_frame_mean_absolute_alpha_delta_u8": difference,
        })
        previous_alpha = alpha
        previous_rgb = rgb
        if frame in REVIEW_FRAMES:
            image.convert("RGB").save(output / f"{shot}_{frame:04}_source.png")
            for region, bounds in REVIEW_REGIONS[shot].items():
                crop = image.crop(bounds).convert("RGB")
                stem = f"{shot}_{frame:04}_{region}"
                crop.save(output / f"{stem}_100.png")
                crop.resize(
                    (crop.width * 4, crop.height * 4), Image.Resampling.NEAREST
                ).save(output / f"{stem}_400.png")
    gate_summary = None
    if shot == "sand":
        core = frames[59]
        tail = frames[149]
        tail_bottom_clearance_px = (
            1080 - tail["rgb_difference_bbox_gt10"][3]
            if tail["rgb_difference_bbox_gt10"] else 1080
        )
        gate_summary = {
            "threshold": "max absolute RGB channel difference > 10/255 from upper-left background pixel",
            "core_frame_59": {
                "pass": (
                    0.08 <= core["rgb_difference_fraction_gt10"] <= 0.20
                    and core["rgb_difference_bbox_width_fraction_gt10"] >= 0.70
                    and core["rgb_difference_bbox_height_fraction_gt10"] >= 0.60
                ),
                "fraction": core["rgb_difference_fraction_gt10"],
                "bbox": core["rgb_difference_bbox_gt10"],
                "bbox_width_fraction": core["rgb_difference_bbox_width_fraction_gt10"],
                "bbox_height_fraction": core["rgb_difference_bbox_height_fraction_gt10"],
            },
            "tail_frame_149": {
                "pass": (
                    0.005 <= tail["rgb_difference_fraction_gt10"] <= 0.03
                    and tail["rgb_difference_bbox_gt10"] is not None
                    and tail_bottom_clearance_px >= 24
                ),
                "fraction": tail["rgb_difference_fraction_gt10"],
                "bbox": tail["rgb_difference_bbox_gt10"],
                "bottom_clearance_px": tail_bottom_clearance_px,
                "required_bottom_clearance_px": 24,
            },
            "manual_visual_review_required": True,
        }

    (output / "sequence_report.json").write_text(
        json.dumps({
            "shot": shot,
            "fps": 30,
            "review_regions_xyxy": REVIEW_REGIONS[shot],
            "offscreen_render_and_readback": performance,
            "predeclared_acceptance_gates": gate_summary,
            "frames": frames,
        }, indent=2) + "\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("shot", choices=("sand", "title"))
    parser.add_argument("directory", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    analyze(args.directory, args.shot, args.output)
