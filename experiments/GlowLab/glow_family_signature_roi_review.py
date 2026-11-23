"""Create fixed ROI comparisons for Chromatic and Liquid Glow product captures."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import cv2
import numpy as np


ROIS = {
    "emissive_text": (0.37, 0.34, 0.57, 0.57),
    "horizontal_tube": (0.10, 0.59, 0.89, 0.68),
    "magenta_ring": (0.16, 0.22, 0.37, 0.55),
}


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_rgba(path: Path) -> np.ndarray:
    image = cv2.imread(str(path), cv2.IMREAD_UNCHANGED)
    if image is None:
        raise RuntimeError(f"cannot read {path}")
    if image.ndim == 2:
        image = cv2.cvtColor(image, cv2.COLOR_GRAY2BGRA)
    elif image.shape[2] == 3:
        image = cv2.cvtColor(image, cv2.COLOR_BGR2BGRA)
    if image.dtype != np.uint8:
        image = np.clip(image, 0, 255).astype(np.uint8)
    return cv2.cvtColor(image, cv2.COLOR_BGRA2RGBA)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    records: list[dict[str, object]] = []

    for family, effect_id, patterns in (
        ("chromatic", "chromatic_glow", ("dispersion-0p0", "dispersion-0p75", "dispersion-1p5", "dispersion-1p75", "dispersion-2p0", "dispersion-2p5")),
        ("liquid", "liquid_glow", ("distortion-0p0", "distortion-1p5", "distortion-3p0", "distortion-6p0", "distortion-12p0")),
    ):
        family_rows: dict[str, list[np.ndarray]] = {key: [] for key in ROIS}
        for pattern in patterns:
            matches = sorted(args.input_dir.glob(
                f"*{pattern}-{effect_id}-cpu.png"))
            for output in matches:
                source_path = output.with_name(
                    f"{output.stem}-fixture-input{output.suffix}")
                source, result = load_rgba(source_path), load_rgba(output)
                if source.shape != result.shape or source.shape[1] != 960 or source.shape[0] != 540:
                    raise RuntimeError(f"unexpected input/output size for {output.name}")
                if not np.array_equal(source[..., 3], result[..., 3]):
                    raise RuntimeError(f"alpha changed for {output.name}")
                prop_text = output.with_name(f"{output.name}.state.txt").read_text(encoding="utf-8")
                delta = result[..., :3].astype(np.int16) - source[..., :3].astype(np.int16)
                record: dict[str, object] = {
                    "family": family,
                    "effect_id": effect_id,
                    "capture": output.name,
                    "source_sha256": digest(source_path),
                    "output_sha256": digest(output),
                    "state_properties": prop_text.split("propertyOverrides=", 1)[-1].strip(),
                    "alpha_mae": 0.0,
                }
                for roi_name, bounds in ROIS.items():
                    x0, y0, x1, y1 = (round(bounds[0] * 960), round(bounds[1] * 540),
                                      round(bounds[2] * 960), round(bounds[3] * 540))
                    src_crop, out_crop = source[y0:y1, x0:x1], result[y0:y1, x0:x1]
                    delta_crop = delta[y0:y1, x0:x1]
                    channel_energy = np.abs(delta_crop.astype(np.float32)).mean(axis=(0, 1))
                    record[roi_name] = {
                        "normalized_roi": list(bounds),
                        "mean_abs_rgb_delta_8bit": float(np.abs(delta_crop).mean()),
                        "per_channel_mean_abs_delta_rgb": channel_energy.tolist(),
                        "newly_clipped_channel_fraction": float(
                            np.mean((out_crop[..., :3] >= 254) & (src_crop[..., :3] < 254))),
                    }
                    scale = 3
                    panels = [src_crop[..., :3], out_crop[..., :3],
                              np.clip(128 + delta_crop.astype(np.float32) * 6, 0, 255).astype(np.uint8)]
                    panels = [cv2.resize(p, None, fx=scale, fy=scale,
                                         interpolation=cv2.INTER_NEAREST) for p in panels]
                    family_rows[roi_name].append(
                        cv2.cvtColor(np.concatenate(panels, axis=1), cv2.COLOR_RGB2BGR))
                records.append(record)
        for roi_name, rows in family_rows.items():
            if rows:
                sheet = np.concatenate(rows, axis=0)
                cv2.imwrite(str(args.output_dir / f"{family}-{roi_name}-signature.png"), sheet)

    payload = {
        "capture_class": "ArtifactUiTest installed-effect CPU product output",
        "plate": "synthetic neon 960x540",
        "source_consistency": len({r["source_sha256"] for r in records}) == 1 if records else False,
        "roi_order": ["input", "output", "6x difference"],
        "records": records,
    }
    (args.output_dir / "signature-roi-metrics.json").write_text(
        json.dumps(payload, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
