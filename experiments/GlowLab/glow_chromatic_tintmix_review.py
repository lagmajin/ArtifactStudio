"""Review fixed-Dispersion Chromatic Glow Tint Mix product captures."""

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


def rgba(path: Path) -> np.ndarray:
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


def crop(image: np.ndarray, bounds: tuple[float, float, float, float]) -> np.ndarray:
    h, w = image.shape[:2]
    x0, y0, x1, y1 = bounds
    return image[round(y0*h):round(y1*h), round(x0*w):round(x1*w), :3]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    records: list[dict[str, object]] = []
    loaded: list[tuple[Path, np.ndarray, np.ndarray, float]] = []
    for output in sorted(args.input_dir.glob("*tintmix-*-chromatic_glow-cpu.png")):
        source_path = output.with_name(f"{output.stem}-fixture-input{output.suffix}")
        state_path = output.with_name(f"{output.name}.state.txt")
        source, result = rgba(source_path), rgba(output)
        if source.shape != result.shape or source.shape[:2] != (540, 960):
            raise RuntimeError(f"unexpected input/output dimensions: {output.name}")
        state = state_path.read_text(encoding="utf-8")
        properties_text = state.split("propertyOverrides=", 1)[-1].strip()
        properties = json.loads(properties_text)
        tint_mix = float(properties["Tint Mix"])
        if float(properties["Dispersion"]) != 2.0:
            raise RuntimeError(f"unexpected dispersion in {output.name}")
        if not np.array_equal(source[..., 3], result[..., 3]):
            raise RuntimeError(f"alpha changed in {output.name}")
        loaded.append((output, source, result, tint_mix))

    reference = next((entry[2] for entry in loaded if abs(entry[3] - 0.25) < 1e-6), None)
    if reference is None:
        raise RuntimeError("Tint Mix 0.25 reference capture is required")

    sheets_by_roi: dict[str, list[np.ndarray]] = {name: [] for name in ROIS}
    for output, source, result, tint_mix in loaded:
        source_delta = result[..., :3].astype(np.int16) - source[..., :3].astype(np.int16)
        pair_delta = result[..., :3].astype(np.int16) - reference[..., :3].astype(np.int16)
        record: dict[str, object] = {
            "tint_mix": tint_mix,
            "capture": output.name,
            "input_sha256": digest(output.with_name(f"{output.stem}-fixture-input{output.suffix}")),
            "output_sha256": digest(output),
            "alpha_mae_8bit": float(np.abs(result[..., 3].astype(np.int16) - source[..., 3].astype(np.int16)).mean()),
        }
        for name, bounds in ROIS.items():
            src_crop, out_crop = crop(source, bounds), crop(result, bounds)
            ref_crop = crop(reference, bounds)
            x0, y0, x1, y1 = bounds
            h, w = source.shape[:2]
            ys, xs = slice(round(y0*h), round(y1*h)), slice(round(x0*w), round(x1*w))
            delta_crop = source_delta[ys, xs]
            pair_crop = pair_delta[ys, xs]
            record[name] = {
                "source_delta_mean_abs_rgb_8bit": float(np.abs(delta_crop).mean()),
                "source_delta_mean_abs_rgb_by_channel": np.abs(delta_crop.astype(np.float32)).mean(axis=(0, 1)).tolist(),
                "new_clip_fraction": float(np.mean((out_crop >= 254) & (src_crop < 254))),
                "versus_tint_mix_0p25_mean_abs_rgb_8bit": float(np.abs(pair_crop).mean()),
                "versus_tint_mix_0p25_by_channel": np.abs(pair_crop.astype(np.float32)).mean(axis=(0, 1)).tolist(),
            }
            panels = [src_crop, out_crop,
                      np.clip(128 + delta_crop.astype(np.float32) * 6, 0, 255).astype(np.uint8)]
            panels = [cv2.resize(panel, None, fx=3, fy=3, interpolation=cv2.INTER_NEAREST)
                      for panel in panels]
            sheet = np.concatenate(panels, axis=1)
            band = np.zeros((32, sheet.shape[1], 3), dtype=np.uint8)
            cv2.putText(band, f"Tint Mix {tint_mix:g} | Dispersion 2.0 | input / output / 6x delta",
                        (8, 23), cv2.FONT_HERSHEY_SIMPLEX, 0.55, (220, 220, 220), 1, cv2.LINE_AA)
            sheets_by_roi[name].append(np.concatenate([band, sheet], axis=0))
        records.append(record)

    if not records:
        raise RuntimeError("no matching product captures")
    for name, rows in sheets_by_roi.items():
        cv2.imwrite(str(args.output_dir / f"chromatic-tintmix-{name}-contact-sheet.png"),
                    cv2.cvtColor(np.concatenate(rows, axis=0), cv2.COLOR_RGB2BGR))
    payload = {
        "capture_class": "ArtifactUiTest installed-effect CPU reference output",
        "plate": "synthetic neon, 960x540",
        "fixed_properties": {"Dispersion": 2.0, "Threshold": 0.6, "Angle": 0.0,
                             "Radius": 7.0, "Intensity": 0.65},
        "source_consistency": len({r["input_sha256"] for r in records}) == 1,
        "roi_bounds_normalized": ROIS,
        "records": records,
    }
    (args.output_dir / "chromatic-tintmix-roi-metrics.json").write_text(
        json.dumps(payload, indent=2) + "\n", encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
