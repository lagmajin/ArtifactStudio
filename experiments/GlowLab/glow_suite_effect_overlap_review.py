"""Measure exploratory similarity between installed Glow-suite effect deltas."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import cv2
import numpy as np


ROIS = {
    "full_frame": (0.0, 0.0, 1.0, 1.0),
    "magenta_ring": (0.15, 0.18, 0.40, 0.58),
    "emissive_text": (0.36, 0.35, 0.58, 0.54),
    "compact_cyan_practical": (0.70, 0.28, 0.82, 0.48),
    "compact_amber_practical": (0.74, 0.43, 0.84, 0.56),
    "horizontal_tube": (0.08, 0.58, 0.90, 0.69),
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_rgb(path: Path) -> np.ndarray:
    raw = cv2.imread(str(path), cv2.IMREAD_UNCHANGED)
    if raw is None or raw.ndim != 3 or raw.shape[2] < 3:
        raise RuntimeError(f"Could not read RGB image: {path}")
    return cv2.cvtColor(raw[..., :3], cv2.COLOR_BGR2RGB).astype(np.float32) / 255.0


def to_linear(rgb: np.ndarray) -> np.ndarray:
    return np.where(rgb <= 0.04045, rgb / 12.92,
                    ((rgb + 0.055) / 1.055) ** 2.4)


def cosine(a: np.ndarray, b: np.ndarray) -> float | None:
    av = a.reshape(-1).astype(np.float64)
    bv = b.reshape(-1).astype(np.float64)
    denom = float(np.linalg.norm(av) * np.linalg.norm(bv))
    if denom <= 1e-20:
        return None
    return float(np.dot(av, bv) / denom)


def crop(image: np.ndarray, rect: tuple[float, float, float, float]) -> np.ndarray:
    h, w = image.shape[:2]
    x0, y0, x1, y1 = rect
    return image[int(y0 * h):int(y1 * h), int(x0 * w):int(x1 * w)]


def run(capture_dir: Path, output_dir: Path) -> None:
    manifest_path = capture_dir / "role-candidates-contact-sheet.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest.get("capture_class") != "ArtifactUiTest installed-effect CPU reference output":
        raise RuntimeError("Unexpected capture class in role-candidate manifest")
    records = manifest.get("records", [])
    if len(records) != 12:
        raise RuntimeError(f"Expected 12 Glow-family captures, found {len(records)}")

    effect_ids = [str(record["effect_id"]) for record in records]
    source_hashes = {str(record["source_sha256"]) for record in records}
    dimensions = {tuple(record["dimensions"]) for record in records}
    if len(source_hashes) != 1 or len(dimensions) != 1:
        raise RuntimeError("The effect captures do not share one source and dimensions")

    deltas: dict[str, np.ndarray] = {}
    source_image: np.ndarray | None = None
    output_hashes: dict[str, str] = {}
    for record in records:
        effect_id = str(record["effect_id"])
        source_path = capture_dir / str(record["source"])
        output_path = capture_dir / str(record["output"])
        if sha256(source_path) != str(record["source_sha256"]):
            raise RuntimeError(f"Source hash mismatch for {effect_id}")
        source = to_linear(read_rgb(source_path))
        output = to_linear(read_rgb(output_path))
        if source.shape != output.shape:
            raise RuntimeError(f"Source/output dimensions differ for {effect_id}")
        if source_image is None:
            source_image = source
        elif not np.array_equal(source_image, source):
            raise RuntimeError("Per-effect fixture inputs are not pixel-identical")
        deltas[effect_id] = output - source
        output_hashes[effect_id] = sha256(output_path)

    per_roi: dict[str, object] = {}
    for roi_name, rect in ROIS.items():
        bands: dict[str, dict[str, np.ndarray]] = {}
        for effect_id, delta in deltas.items():
            delta_luma = delta @ np.asarray((0.2126, 0.7152, 0.0722), dtype=np.float32)
            broad_luma = cv2.GaussianBlur(delta_luma, (0, 0), 6.0)
            smooth_luma = cv2.GaussianBlur(delta_luma, (0, 0), 3.0)
            chroma_delta = np.stack((delta[..., 0] - delta[..., 1],
                                     delta[..., 2] - delta[..., 1]), axis=-1)
            bands.setdefault("raw_signed_rgb_delta", {})[effect_id] = crop(delta, rect)
            bands.setdefault("broad_halo_luma_sigma6", {})[effect_id] = crop(broad_luma, rect)
            bands.setdefault("fine_structure_luma_sigma3_residual", {})[effect_id] = crop(
                delta_luma - smooth_luma, rect)
            bands.setdefault("chroma_delta_rg_bg", {})[effect_id] = crop(chroma_delta, rect)

        band_results: dict[str, object] = {}
        for band_name, band_values in bands.items():
            matrix: list[list[float | None]] = []
            norms = {effect_id: float(np.linalg.norm(band_values[effect_id].reshape(-1)))
                     for effect_id in effect_ids}
            for left in effect_ids:
                matrix.append([cosine(band_values[left], band_values[right])
                               for right in effect_ids])
            pairs: list[dict[str, object]] = []
            for i, left in enumerate(effect_ids):
                for j in range(i + 1, len(effect_ids)):
                    right = effect_ids[j]
                    similarity = matrix[i][j]
                    if similarity is not None:
                        pairs.append({"effect_a": left, "effect_b": right,
                                      "signed_delta_cosine": similarity})
            pairs.sort(key=lambda pair: abs(float(pair["signed_delta_cosine"])), reverse=True)
            band_results[band_name] = {
                "effect_delta_l2_norm": norms,
                "signed_delta_cosine_matrix": matrix,
                "strongest_absolute_similarity_pairs": pairs[:8],
            }
        per_roi[roi_name] = {
            "normalized_rect": list(rect),
            "bands": band_results,
        }

    output_dir.mkdir(parents=True, exist_ok=True)
    output = {
        "capture_class": manifest["capture_class"],
        "comparison": "band-specific cosine similarity of linear-light output-minus-input deltas",
        "bands": {
            "raw_signed_rgb_delta": "full RGB change vector",
            "broad_halo_luma_sigma6": "linear-luma delta blurred at sigma 6 pixels",
            "fine_structure_luma_sigma3_residual": "linear-luma delta minus sigma 3 blur",
            "chroma_delta_rg_bg": "signed (R-G, B-G) change vector",
        },
        "interpretation": "exploratory overlap signal only; not a visual-quality score or redundancy verdict",
        "source_sha256": next(iter(source_hashes)),
        "dimensions": list(next(iter(dimensions))),
        "effect_ids": effect_ids,
        "output_sha256": output_hashes,
        "rois": per_roi,
        "limitations": [
            "single synthetic neon plate and exploratory per-effect parameter profiles",
            "high cosine can reflect shared source geometry without perceptual redundancy",
            "does not establish accepted presets, combined-stack quality, GPU parity, or Viewer output",
        ],
    }
    json_path = output_dir / "glow-suite-effect-overlap.json"
    json_path.write_text(json.dumps(output, indent=2) + "\n", encoding="utf-8")
    for roi_name, roi_data in per_roi.items():
        csv_path = output_dir / f"glow-suite-effect-overlap-{roi_name}.csv"
        for band_name, band_data in roi_data["bands"].items():
            matrix = band_data["signed_delta_cosine_matrix"]
            lines = ["," + ",".join(effect_ids)]
            lines.extend(effect_ids[i] + "," + ",".join(
                "" if value is None else f"{value:.6f}" for value in row)
                for i, row in enumerate(matrix))
            csv_path = output_dir / f"glow-suite-overlap-{roi_name}-{band_name}.csv"
            csv_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Saved {json_path}")
    for roi_name, roi_data in per_roi.items():
        print(f"{roi_name}:")
        for band_name, band_data in roi_data["bands"].items():
            strongest = band_data["strongest_absolute_similarity_pairs"][:2]
            summary = ", ".join(
                "{effect_a}/{effect_b} {signed_delta_cosine:.3f}".format(**pair)
                for pair in strongest)
            print(f"  {band_name}: {summary}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--captures", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.captures, args.output)


if __name__ == "__main__":
    main()
