"""Generate a deterministic RGBA chart for flash-fill look development."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import cv2
import numpy as np


def make_chart(width: int = 1280, height: int = 720) -> np.ndarray:
    if width < 800 or height < 500:
        raise ValueError("Flash calibration chart must be at least 800x500 pixels")
    image = np.zeros((height, width, 4), dtype=np.float32)
    y, x = np.mgrid[0:height, 0:width].astype(np.float32)
    vignette = np.clip(1.0 - 0.25 * (((x - width * 0.5) / width) ** 2 +
                                    ((y - height * 0.5) / height) ** 2), 0.0, 1.0)
    image[..., 0] = 0.018 * vignette
    image[..., 1] = 0.022 * vignette
    image[..., 2] = 0.035 * vignette
    image[..., 3] = 1.0

    cv2.putText(image, "FLASH FILL / CALIBRATION CHART", (36, 80),
                cv2.FONT_HERSHEY_SIMPLEX, 0.9, (0.82, 0.88, 1.0, 1.0), 2, cv2.LINE_AA)
    cv2.putText(image, "REFLECTANCE STEPS  |  CHROMA  |  POINT SOURCES", (38, 108),
                cv2.FONT_HERSHEY_SIMPLEX, 0.42, (0.48, 0.57, 0.72, 1.0), 1, cv2.LINE_AA)

    gray_steps = (0.025, 0.045, 0.08, 0.14, 0.24, 0.40, 0.62, 0.86)
    patch_y0, patch_y1 = 142, 232
    patch_width = 90
    for index, value in enumerate(gray_steps):
        x0 = 38 + index * (patch_width + 10)
        x1 = x0 + patch_width
        cv2.rectangle(image, (x0, patch_y0), (x1, patch_y1),
                      (value, value, value, 1.0), -1, cv2.LINE_AA)
        cv2.putText(image, f"{value:.3f}", (x0 + 5, patch_y1 + 23),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.38, (0.75, 0.79, 0.88, 1.0), 1, cv2.LINE_AA)

    color_steps = (
        (0.72, 0.12, 0.08), (0.95, 0.34, 0.09), (0.92, 0.70, 0.12),
        (0.12, 0.66, 0.25), (0.06, 0.56, 0.78), (0.24, 0.23, 0.86),
        (0.72, 0.14, 0.68), (0.78, 0.70, 0.62),
    )
    for index, color in enumerate(color_steps):
        x0 = 38 + index * (patch_width + 10)
        x1 = x0 + patch_width
        cv2.rectangle(image, (x0, 290), (x1, 380), (*color, 1.0), -1, cv2.LINE_AA)
        cv2.rectangle(image, (x0, 388), (x1, 411), (*tuple(c * 0.28 for c in color), 1.0),
                      -1, cv2.LINE_AA)
        cv2.rectangle(image, (x0, 415), (x1, 438), (*tuple(c * 0.62 for c in color), 1.0),
                      -1, cv2.LINE_AA)

    cv2.putText(image, "POINT SOURCES / SCATTER SEEDS", (40, 480),
                cv2.FONT_HERSHEY_SIMPLEX, 0.46, (0.58, 0.65, 0.8, 1.0), 1, cv2.LINE_AA)
    sources = [((150, 545), (1.0, 0.94, 0.82), 5),
               ((320, 545), (0.28, 0.72, 1.0), 4),
               ((490, 545), (1.0, 0.20, 0.08), 6),
               ((660, 545), (0.82, 0.24, 1.0), 5),
               ((830, 545), (1.0, 0.82, 0.22), 4)]
    for (cx, cy), color, radius in sources:
        cv2.circle(image, (cx, cy), radius * 5, (*tuple(c * 0.12 for c in color), 1.0), -1, cv2.LINE_AA)
        cv2.circle(image, (cx, cy), radius, (*color, 1.0), -1, cv2.LINE_AA)
    for index, label in enumerate(("WARM", "CYAN", "RED", "VIOLET", "AMBER")):
        cv2.putText(image, label, (111 + index * 170, 588),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.34, (0.54, 0.59, 0.71, 1.0), 1, cv2.LINE_AA)

    # Transparent test region: opaque matte shapes and soft alpha edges sit on
    # zero-alpha pixels so RGB scatter cannot be mistaken for alpha expansion.
    transparent_x = 900
    image[:, transparent_x:, :] = 0.0
    cv2.circle(image, (1015, 300), 70, (0.95, 0.20, 0.08, 0.82), -1, cv2.LINE_AA)
    cv2.circle(image, (1160, 300), 70, (0.12, 0.58, 1.0, 0.48), -1, cv2.LINE_AA)
    cv2.circle(image, (1088, 300), 46, (1.0, 0.92, 0.75, 1.0), 5, cv2.LINE_AA)
    cv2.putText(image, "ALPHA PROBE  >", (700, 642),
                cv2.FONT_HERSHEY_SIMPLEX, 0.40, (0.62, 0.68, 0.8, 1.0), 1, cv2.LINE_AA)

    # Restore an opaque backing to the chart's lower left; the alpha probe is
    # deliberately isolated to its own right-hand strip.
    image[:, :transparent_x, 3] = 1.0
    return np.clip(image, 0.0, 1.0)


def write_alpha_comparison(source_path: Path, output_path: Path, preview_path: Path) -> None:
    source = cv2.imread(str(source_path), cv2.IMREAD_UNCHANGED)
    effected = cv2.imread(str(output_path), cv2.IMREAD_UNCHANGED)
    if source is None or effected is None:
        raise ValueError("Could not read source and effected RGBA images")
    if source.shape != effected.shape or source.ndim != 3 or source.shape[2] != 4:
        raise ValueError("Alpha comparison requires matching RGBA images")
    height, width = source.shape[:2]
    y, x = np.mgrid[0:height, 0:width]
    checker_index = ((x // 40 + y // 40) % 2).astype(np.float32)
    checker = np.where(checker_index[..., None] > 0.5, 0.30, 0.16).astype(np.float32)

    def composite(bgra: np.ndarray) -> np.ndarray:
        rgb = cv2.cvtColor(bgra[..., :3], cv2.COLOR_BGR2RGB).astype(np.float32) / 255.0
        alpha = bgra[..., 3:4].astype(np.float32) / 255.0
        return cv2.cvtColor(np.round(np.clip(rgb * alpha + checker * (1.0 - alpha),
                                              0.0, 1.0) * 255.0).astype(np.uint8),
                            cv2.COLOR_RGB2BGR)

    panels = []
    for pixels, label in ((source, "SOURCE / CHECKERBOARD"),
                          (effected, f"FLASH / {output_path.name}")):
        panel = composite(pixels)
        cv2.rectangle(panel, (0, 0), (width, 42), (8, 10, 16), -1)
        cv2.putText(panel, label, (16, 28), cv2.FONT_HERSHEY_SIMPLEX,
                    0.52, (238, 240, 245), 1, cv2.LINE_AA)
        panels.append(cv2.resize(panel, None, fx=0.5, fy=0.5, interpolation=cv2.INTER_AREA))
    preview_path.parent.mkdir(parents=True, exist_ok=True)
    if not cv2.imwrite(str(preview_path), np.hstack(panels)):
        raise OSError(f"Could not write alpha comparison preview: {preview_path}")
    alpha_diff = np.abs(source[..., 3].astype(np.int16) - effected[..., 3].astype(np.int16))
    report = {
        "source": str(source_path.resolve()),
        "effected": str(output_path.resolve()),
        "width": width,
        "height": height,
        "alpha_identical": bool(np.count_nonzero(alpha_diff) == 0),
        "alpha_changed_pixel_count": int(np.count_nonzero(alpha_diff)),
        "max_alpha_delta_u8": int(alpha_diff.max()),
        "preview": preview_path.name,
    }
    report_path = preview_path.with_suffix(".json")
    with report_path.open("w", encoding="utf-8", newline="\n") as stream:
        json.dump(report, stream, indent=2, ensure_ascii=False)
        stream.write("\n")
    print(f"Alpha identical: {report['alpha_identical']} "
          f"({report['alpha_changed_pixel_count']} changed pixels)")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parent / "assets" / "flash-calibration-chart.png")
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    parser.add_argument("--compare", type=Path,
                        help="effected RGBA image to compare with the generated source over checkerboard")
    parser.add_argument("--compare-output", type=Path,
                        help="comparison PNG path (default: assets/flash-calibration-alpha-checkerboard.png)")
    args = parser.parse_args()
    chart = make_chart(args.width, args.height)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    bgra = cv2.cvtColor(np.round(chart * 255.0).astype(np.uint8), cv2.COLOR_RGBA2BGRA)
    if not cv2.imwrite(str(args.output), bgra):
        raise OSError(f"Could not write calibration chart: {args.output}")
    print(f"Saved flash calibration chart to {args.output.resolve()}")
    if args.compare is not None:
        preview_path = args.compare_output or args.output.parent.parent / "captures" / \
            "flash-calibration-alpha-checkerboard.png"
        write_alpha_comparison(args.output, args.compare, preview_path)


if __name__ == "__main__":
    main()
