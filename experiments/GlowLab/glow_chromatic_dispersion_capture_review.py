"""Review Chromatic Glow product captures against one photographic source plate."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import cv2
import numpy as np

from glow_lab import add_label


ROI_PROFILES = {
    "city": {
        "intersection_practicals": (0.36, 0.34, 0.58, 0.59),
        "upper_road_lights": (0.64, 0.08, 0.84, 0.34),
        "lower_signage": (0.29, 0.56, 0.48, 0.78),
    },
    "streetlight": {
        "paired_bulbs": (0.18, 0.32, 0.76, 0.49),
        "dark_sky": (0.12, 0.06, 0.88, 0.30),
        "colored_signage": (0.0, 0.56, 0.18, 0.78),
    },
    "probe": {
        "pin_lights": (0.06, 0.12, 0.44, 0.36),
        "bright_edges": (0.54, 0.14, 0.89, 0.45),
        "saturated_emitters": (0.06, 0.62, 0.44, 0.91),
        "dark_field_control": (0.56, 0.62, 0.90, 0.91),
    },
    "material": {
        "magenta_ring": (0.16, 0.22, 0.36, 0.49),
        "emissive_text": (0.36, 0.36, 0.55, 0.55),
        "cyan_point": (0.68, 0.28, 0.76, 0.40),
        "amber_point": (0.77, 0.43, 0.81, 0.50),
        "horizontal_tube": (0.10, 0.60, 0.89, 0.66),
        "brick_wall": (0.00, 0.00, 0.36, 0.59),
        "wet_road": (0.00, 0.66, 1.00, 1.00),
    },
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


def linear(rgb: np.ndarray) -> np.ndarray:
    value = rgb.astype(np.float32) / 255.0
    return np.where(value <= 0.04045, value / 12.92,
                    ((value + 0.055) / 1.055) ** 2.4)


def crop(image: np.ndarray, bounds: tuple[float, float, float, float]) -> np.ndarray:
    h, w = image.shape[:2]
    x0, y0, x1, y1 = bounds
    return image[round(y0*h):round(y1*h), round(x0*w):round(x1*w), :3]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--roi-profile", choices=tuple(ROI_PROFILES), default="city")
    args = parser.parse_args()
    rois = ROI_PROFILES[args.roi_profile]
    manifest_path = args.input_dir / "product-suite-manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest["capture_class"] != "ArtifactUiTest installed-effect product output":
        raise RuntimeError("unexpected capture class")
    entries = manifest["entries"]
    args.output_dir.mkdir(parents=True, exist_ok=True)

    loaded: list[tuple[float, Path, np.ndarray, np.ndarray, dict[str, float]]] = []
    source_hashes: set[str] = set()
    fixed_settings: set[tuple[tuple[str, float], ...]] = set()
    for entry in entries:
        properties = {key: float(value) for key, value in entry["properties"].items()}
        if entry["effect_id"] != "chromatic_glow" or entry["requested_compute"] != "cpu":
            raise RuntimeError("manifest includes a capture outside the expected CPU effect")
        dispersion = properties["Dispersion"]
        if not 0.0 <= dispersion <= 2.0:
            raise RuntimeError(f"Dispersion is outside the registered [0, 2] range: {dispersion}")
        output_path = args.input_dir / entry["product_output"]
        source_path = args.input_dir / entry["source_capture"]
        state_path = args.input_dir / entry["state_sidecar"]
        source, result = rgba(source_path), rgba(output_path)
        if source.shape != result.shape:
            raise RuntimeError(f"input/output shape mismatch for {output_path.name}")
        if digest(source_path) != entry["source_capture_sha256"]:
            raise RuntimeError(f"source capture hash mismatch for {source_path.name}")
        if digest(output_path) != entry["product_output_sha256"]:
            raise RuntimeError(f"product capture hash mismatch for {output_path.name}")
        state = state_path.read_text(encoding="utf-8")
        state_props = json.loads(state.split("propertyOverrides=", 1)[-1].strip())
        if state_props != entry["properties"]:
            raise RuntimeError(f"sidecar parameter readback mismatch for {output_path.name}")
        if "compute=0" not in state or "captureSource=Installed CPU reference" not in state:
            raise RuntimeError(f"not an installed CPU reference capture: {output_path.name}")
        if not np.array_equal(source[..., 3], result[..., 3]):
            raise RuntimeError(f"alpha changed for {output_path.name}")
        source_hashes.add(digest(source_path))
        fixed_settings.add(tuple(sorted((k, v) for k, v in properties.items()
                                        if k != "Dispersion")))
        loaded.append((dispersion, output_path, source, result, properties))

    if len(source_hashes) != 1:
        raise RuntimeError("dispersion sweep did not use an identical source capture")
    if len(fixed_settings) != 1:
        raise RuntimeError("settings other than Dispersion changed during the sweep")
    loaded.sort(key=lambda row: row[0])
    dispersions = tuple(row[0] for row in loaded)
    if not dispersions or dispersions[0] != 0.0 or len(set(dispersions)) != len(dispersions):
        raise RuntimeError("capture set must contain unique dispersion values and a zero baseline")
    source = loaded[0][2]
    baseline = loaded[0][3]
    h, w = source.shape[:2]

    overview_panels = []
    for dispersion, output_path, _, result, _ in loaded:
        panel = cv2.cvtColor(result[..., :3], cv2.COLOR_RGB2BGR)
        overview_panels.append(add_label(panel, f"Dispersion {dispersion:g}"))
    thumb_w = 640
    thumb_h = round(h * thumb_w / w)
    overview = np.hstack([cv2.resize(p, (thumb_w, thumb_h), interpolation=cv2.INTER_AREA)
                          for p in overview_panels])
    cv2.imwrite(str(args.output_dir / f"chromatic-dispersion-{args.roi_profile}-overview.jpg"),
                overview, [cv2.IMWRITE_JPEG_QUALITY, 94])

    records: list[dict[str, object]] = []
    for dispersion, output_path, _, result, properties in loaded:
        row: dict[str, object] = {
            "dispersion": dispersion,
            "properties": properties,
            "capture": output_path.name,
            "input_sha256": next(iter(source_hashes)),
            "output_sha256": digest(output_path),
            "alpha_mae_8bit": float(np.abs(result[..., 3].astype(np.int16) -
                                            source[..., 3].astype(np.int16)).mean()),
            "new_clipped_channel_fraction_full": float(np.mean(
                (result[..., :3] >= 254) & (source[..., :3] < 254))),
            "mean_abs_rgb_delta_from_source_8bit": float(np.abs(
                result[..., :3].astype(np.int16) - source[..., :3].astype(np.int16)).mean()),
            "mean_abs_rgb_delta_from_dispersion_zero_8bit": float(np.abs(
                result[..., :3].astype(np.int16) - baseline[..., :3].astype(np.int16)).mean()),
        }
        for name, bounds in rois.items():
            source_crop, result_crop = crop(source, bounds), crop(result, bounds)
            baseline_crop = crop(baseline, bounds)
            diff_from_source = np.abs(result_crop.astype(np.int16) -
                                      source_crop.astype(np.int16))
            diff_from_baseline = result_crop.astype(np.int16) - baseline_crop.astype(np.int16)
            new_clip = (result_crop >= 254) & (source_crop < 254)
            lin_delta = linear(result_crop) - linear(source_crop)
            luma_delta = lin_delta @ np.asarray((0.2126, 0.7152, 0.0722), dtype=np.float32)
            row[name] = {
                "mean_abs_rgb_delta_from_source_8bit": float(diff_from_source.mean()),
                "mean_abs_rgb_delta_from_zero_dispersion_8bit": float(
                    np.abs(diff_from_baseline).mean()),
                "mean_linear_luma_delta_from_source": float(luma_delta.mean()),
                "new_clipped_channel_fraction": float(new_clip.mean()),
                "mean_abs_red_minus_blue_delta_from_zero_dispersion": float(
                    np.abs(diff_from_baseline[..., 0] - diff_from_baseline[..., 2]).mean()),
            }
            panels = [source_crop, result_crop,
                      np.clip(128 + diff_from_source.astype(np.float32) * 5,
                              0, 255).astype(np.uint8),
                      np.clip(128 + diff_from_baseline.astype(np.float32) * 8,
                              0, 255).astype(np.uint8)]
            panels = [cv2.resize(p, None, fx=2, fy=2, interpolation=cv2.INTER_NEAREST)
                      for p in panels]
            sheet = np.concatenate(panels, axis=1)
            band = np.zeros((30, sheet.shape[1], 3), dtype=np.uint8)
            cv2.putText(band, f"Dispersion {dispersion:g} | source / output / 5x source delta / 8x vs D=0",
                        (8, 22), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (225, 225, 225), 1, cv2.LINE_AA)
            row.setdefault("_sheet_panels", {})[name] = np.concatenate([band, sheet], axis=0)
        records.append(row)

    for name in rois:
        panels = [row["_sheet_panels"][name] for row in records]
        sheet = np.vstack(panels)
        cv2.imwrite(str(args.output_dir / f"chromatic-dispersion-{args.roi_profile}-{name}.jpg"),
                    cv2.cvtColor(sheet, cv2.COLOR_RGB2BGR), [cv2.IMWRITE_JPEG_QUALITY, 95])
    for row in records:
        row.pop("_sheet_panels", None)
    payload = {
        "capture_class": "ArtifactUiTest installed-effect CPU reference output",
        "input": manifest["input"],
        "input_sha256": manifest["input_sha256"],
        "source_capture_sha256": next(iter(source_hashes)),
        "source_dimensions": [w, h],
        "fixed_properties": dict(next(iter(fixed_settings))),
        "roi_profile": args.roi_profile,
        "roi_bounds_normalized": rois,
        "records": records,
        "limitations": [
            "single CC0 night-city photograph with existing optical/color fringes",
            "manual ROI rectangles; metrics do not assess aesthetic quality",
            "installed CPU reference PNG capture, not final Viewer, GPU parity, or HDR",
        ],
    }
    metrics_path = args.output_dir / f"chromatic-dispersion-{args.roi_profile}-metrics.json"
    metrics_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8",
                            newline="\n")
    print(f"Saved {metrics_path}")
    first_roi = next(iter(rois))
    for row in records:
        print(f"D={row['dispersion']:g}: full delta={row['mean_abs_rgb_delta_from_source_8bit']:.3f} code, "
              f"new clip={row['new_clipped_channel_fraction_full']:.4%}, "
              f"alpha={row['alpha_mae_8bit']:.6f}, "
              f"{first_roi} ROI Δ={row[first_roi]['mean_linear_luma_delta_from_source']:.6f}")


if __name__ == "__main__":
    main()
