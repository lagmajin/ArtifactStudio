"""Review Glow suite alpha and background-composite behavior from product captures."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import cv2
import numpy as np

from surface_alpha_contract import (composite_linear_premultiplied,
                                    premultiply_linear)


BACKGROUNDS = {
    "black": np.array((0, 0, 0), dtype=np.float32),
    "mid_gray": np.array((128, 128, 128), dtype=np.float32) / 255.0,
    "white": np.array((1, 1, 1), dtype=np.float32),
}


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_rgba(path: Path) -> np.ndarray:
    image = cv2.imread(str(path), cv2.IMREAD_UNCHANGED)
    if image is None:
        raise RuntimeError(f"cannot read {path}")
    if image.ndim != 3:
        raise RuntimeError(f"expected color image: {path}")
    if image.shape[2] == 3:
        image = cv2.cvtColor(image, cv2.COLOR_BGR2BGRA)
    if image.dtype != np.uint8:
        image = np.clip(image, 0, 255).astype(np.uint8)
    return cv2.cvtColor(image, cv2.COLOR_BGRA2RGBA)


def composite(rgba: np.ndarray, background: np.ndarray) -> np.ndarray:
    straight_srgb = rgba.astype(np.float32) / 255.0
    premultiplied_linear = premultiply_linear(straight_srgb)
    composited_srgb = composite_linear_premultiplied(
        premultiplied_linear, background)
    return np.clip(np.rint(composited_srgb[..., :3] * 255.0), 0, 255).astype(np.uint8)


def title_strip(width: int, title: str) -> np.ndarray:
    image = np.zeros((38, width, 4), dtype=np.uint8)
    image[..., :3] = (7, 9, 15)
    image[..., 3] = 255
    cv2.putText(image, title, (14, 25), cv2.FONT_HERSHEY_SIMPLEX, 0.56,
                (230, 235, 245, 255), 1, cv2.LINE_AA)
    return image


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--fixture", type=Path, required=True)
    parser.add_argument("--capture-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    fixture_path, capture_dir, output_dir = (
        args.fixture.resolve(), args.capture_dir.resolve(), args.output_dir.resolve())
    fixture = read_rgba(fixture_path)
    manifest = json.loads((capture_dir / "product-suite-manifest.json")
                          .read_text(encoding="utf-8"))
    output_dir.mkdir(parents=True, exist_ok=True)
    records: list[dict[str, object]] = []
    sheets: dict[str, list[np.ndarray]] = {name: [] for name in BACKGROUNDS}

    for entry in manifest["entries"]:
        if "product_output" not in entry:
            continue
        output_path = capture_dir / entry["product_output"]
        source_path = capture_dir / entry["source_capture"]
        source, result = read_rgba(source_path), read_rgba(output_path)
        if source.shape != fixture.shape or result.shape != fixture.shape:
            raise RuntimeError(f"dimension mismatch for {entry['effect_id']}")
        if not np.array_equal(source, fixture):
            raise RuntimeError(f"captured source pixels differ from alpha fixture: {entry['effect_id']}")

        alpha_delta = result[..., 3].astype(np.int16) - source[..., 3].astype(np.int16)
        record: dict[str, object] = {
            "effect_id": entry["effect_id"],
            "properties": entry["properties"],
            "source_sha256": digest(source_path),
            "output_sha256": digest(output_path),
            "alpha_mae_8bit": float(np.abs(alpha_delta).mean()),
            "alpha_max_abs_delta_8bit": int(np.abs(alpha_delta).max()),
            "alpha_changed_pixel_fraction": float(np.mean(alpha_delta != 0)),
            "output_zero_alpha_nonzero_rgb_fraction": float(np.mean(
                (result[..., 3] == 0) & np.any(result[..., :3] != 0, axis=2))),
        }
        boundary = (source[..., 3] > 0) & (source[..., 3] < 255)
        record["partial_alpha_pixel_fraction"] = float(boundary.mean())
        for bg_name, background in BACKGROUNDS.items():
            src_comp = composite(source, background)
            out_comp = composite(result, background)
            diff = np.abs(out_comp.astype(np.int16) - src_comp.astype(np.int16))
            record[bg_name] = {
                "composite_rgb_mae_8bit": float(diff.mean()),
                "composite_rgb_p95_abs_delta_8bit": float(np.percentile(diff, 95)),
                "composite_rgb_max_abs_delta_8bit": int(diff.max()),
                "boundary_rgb_mae_8bit": float(diff[boundary].mean()) if np.any(boundary) else 0.0,
            }
            panels = (src_comp, out_comp,
                      np.clip(128 + (out_comp.astype(np.float32) -
                                     src_comp.astype(np.float32)) * 4.0,
                              0, 255).astype(np.uint8))
            rgba_panels = [np.dstack((panel, np.full(panel.shape[:2], 255, dtype=np.uint8)))
                           for panel in panels]
            resized = [cv2.resize(panel, (360, 203), interpolation=cv2.INTER_AREA)
                       for panel in rgba_panels]
            row = np.hstack(resized)
            sheets[bg_name].extend((title_strip(row.shape[1],
                f"{entry['effect_id']} | alpha MAE {record['alpha_mae_8bit']:.3f} | "
                f"composite MAE {diff.mean():.3f}"), row))
        records.append(record)

    for bg_name, rows in sheets.items():
        if rows:
            image = np.vstack(rows)
            cv2.imwrite(str(output_dir / f"alpha-suite-on-{bg_name}.png"),
                        cv2.cvtColor(image, cv2.COLOR_RGBA2BGRA))
    payload = {
        "capture_class": "ArtifactUiTest installed-effect CPU product output",
        "composite_assumption": "straight sRGB capture converted to linear-premultiplied, composited, then converted to sRGB via surface_alpha_contract.py",
        "fixture_sha256": digest(fixture_path),
        "dimensions": [int(fixture.shape[1]), int(fixture.shape[0])],
        "background_srgb": {name: bg.tolist() for name, bg in BACKGROUNDS.items()},
        "records": records,
    }
    (output_dir / "alpha-suite-metrics.json").write_text(
        json.dumps(payload, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
