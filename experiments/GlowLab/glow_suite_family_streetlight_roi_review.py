"""Review local installed-Glow responses around the two CC0 street-light bulbs."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import cv2
import numpy as np


ROIS = {
    "left-practical": (0.195, 0.325, 0.315, 0.395),
    "right-practical": (0.585, 0.325, 0.705, 0.395),
}


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_rgba(path: Path) -> np.ndarray:
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
    rows: dict[str, list[np.ndarray]] = {name: [] for name in ROIS}
    records: list[dict[str, object]] = []
    suite_manifest = json.loads(
        (args.input_dir / "product-suite-manifest.json").read_text(encoding="utf-8"))

    for output_path in sorted(args.input_dir.glob("app-family-*-cpu.png")):
        if output_path.name.endswith("-fixture-input.png"):
            continue
        source_path = output_path.with_name(
            f"{output_path.stem}-fixture-input{output_path.suffix}")
        source, output = read_rgba(source_path), read_rgba(output_path)
        if source.shape != output.shape:
            raise RuntimeError(f"source/output size mismatch for {output_path.name}")
        if not np.array_equal(source[..., 3], output[..., 3]):
            raise RuntimeError(f"alpha changed for {output_path.name}")
        delta = output[..., :3].astype(np.int16) - source[..., :3].astype(np.int16)
        effect_id = output_path.name.rsplit("-", 2)[-2]
        # Match by the captured output path because effect identifiers can contain dots.
        entry = next((item for item in suite_manifest["entries"]
                      if item.get("product_output") == output_path.name), {})
        effect_id = str(entry.get("effect_id", effect_id))
        row: dict[str, object] = {
            "effect_id": effect_id,
            "profile_key": entry.get("profile_key"),
            "source": source_path.name,
            "output": output_path.name,
            "source_sha256": digest(source_path),
            "output_sha256": digest(output_path),
            "alpha_mae": 0.0,
        }
        height, width = source.shape[:2]
        for roi_name, bounds in ROIS.items():
            x0, y0, x1, y1 = (round(bounds[0] * width), round(bounds[1] * height),
                              round(bounds[2] * width), round(bounds[3] * height))
            source_crop = source[y0:y1, x0:x1]
            output_crop = output[y0:y1, x0:x1]
            delta_crop = delta[y0:y1, x0:x1]
            clipped = ((output_crop[..., :3] >= 254) &
                       (source_crop[..., :3] < 254))
            row[roi_name] = {
                "bounds_normalized": list(bounds),
                "pixels": int((x1 - x0) * (y1 - y0)),
                "rgb_mae_8bit": float(np.abs(delta_crop).mean()),
                "per_channel_mean_signed_delta_rgb": delta_crop.mean(axis=(0, 1)).tolist(),
                "newly_clipped_channel_fraction": float(clipped.mean()),
            }
            diff = np.clip(128.0 + delta_crop.astype(np.float32) * 5.0,
                           0.0, 255.0).astype(np.uint8)
            panels = [source_crop[..., :3], output_crop[..., :3], diff]
            panels = [cv2.resize(panel, None, fx=5, fy=5,
                                 interpolation=cv2.INTER_NEAREST)
                      for panel in panels]
            strip = cv2.cvtColor(np.concatenate(panels, axis=1), cv2.COLOR_RGB2BGR)
            strip = cv2.copyMakeBorder(strip, 30, 0, 0, 0,
                                       cv2.BORDER_CONSTANT, value=(26, 26, 26))
            cv2.putText(strip,
                        f"{row['profile_key']} | {effect_id} | new clip "
                        f"{clipped.mean():.2%}",
                        (12, 21), cv2.FONT_HERSHEY_SIMPLEX, 0.5,
                        (235, 235, 235), 1, cv2.LINE_AA)
            rows[roi_name].append(strip)
        records.append(row)

    for roi_name, panels in rows.items():
        if panels:
            cv2.imwrite(str(args.output_dir / f"{roi_name}-contact.png"),
                        np.concatenate(panels, axis=0))
    (args.output_dir / "streetlight-roi-metrics.json").write_text(
        json.dumps({
            "capture_class": "ArtifactUiTest installed-effect CPU product output",
            "input_plate": "CC0 street light night, provenance in assets/SOURCES.md",
            "roi_order": ["input", "output", "5x signed RGB difference on 128 gray"],
            "not_final_viewer_or_GPU_output": True,
            "records": records,
        }, indent=2) + "\n", encoding="utf-8")
    print(f"Reviewed {len(records)} captures into {args.output_dir}")


if __name__ == "__main__":
    main()
