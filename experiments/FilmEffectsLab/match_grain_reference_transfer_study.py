#!/usr/bin/env python3
"""Exploratory source-equation model for ArtifactMatchGrainEffect."""
from __future__ import annotations

import hashlib
import json
import platform
from pathlib import Path

import cv2
import numpy as np

ROOT = Path(__file__).resolve().parents[2]
PLAN = ROOT / "experiments/FilmEffectsLab/plans/match-grain-reference-transfer-r1.json"
SOURCE = ROOT / "Artifact/src/Effect/ArtifactCreativeEffects.cpp"
OUT = ROOT / "experiments/FilmEffectsLab/captures/match-grain-reference-transfer-r1"
SEED = 20261112
DEFAULTS = {
    "grainSize": 1.15, "amount": 1.0, "lumaAmount": 1.0,
    "chromaAmount": 0.65, "shadowResponse": 0.25,
    "highlightResponse": -0.2, "temporalJitter": 1.0, "mix": 1.0,
}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def creative_hash(x: int, y: int, seed: int) -> float:
    value = ((x & 0xFFFFFFFF) * 0x8DA6B343) ^ ((y & 0xFFFFFFFF) * 0xD8163841) ^ ((seed & 0xFFFFFFFF) * 0xCB1AB31F)
    value &= 0xFFFFFFFF
    value ^= value >> 13
    value = (value * 0x85EBCA6B) & 0xFFFFFFFF
    value ^= value >> 16
    return (value & 0xFFFF) / 65535.0


def make_source(width: int, height: int) -> np.ndarray:
    yy, xx = np.mgrid[0:height, 0:width].astype(np.float32)
    u, v = xx / max(1, width - 1), yy / max(1, height - 1)
    r = 0.13 + 0.72 * u
    g = 0.12 + 0.68 * v
    b = 0.2 + 0.3 * u + 0.22 * v
    image = np.stack([b, g, r], axis=-1).astype(np.float32)

    # Flat patches, a high-frequency checker, and narrow line details.
    image[:, width // 16:width // 5] = (0.08, 0.12, 0.16)
    image[:, width // 5:width * 2 // 5] = (0.42, 0.48, 0.55)
    image[:, width * 2 // 5:width * 3 // 5] = (0.8, 0.86, 0.92)
    checker = ((xx.astype(np.int32) // max(2, width // 128) + yy.astype(np.int32) // max(2, height // 64)) % 2) == 0
    image[checker & (u > 0.66) & (u < 0.9)] = (0.72, 0.28, 0.14)
    for col in range(width // 8, width, max(8, width // 32)):
        image[:, max(0, col - 1):col + 1] = (0.96, 0.96, 0.96)
    for row in range(height // 8, height, max(8, height // 16)):
        image[max(0, row - 1):row + 1, :] = (0.03, 0.03, 0.03)

    # Straight-alpha soft ellipse over a transparent exterior.
    cx, cy = width * 0.5, height * 0.5
    radius = np.sqrt(((xx - cx) / (width * 0.38)) ** 2 + ((yy - cy) / (height * 0.39)) ** 2)
    alpha = np.clip((1.08 - radius) * 10.0, 0.0, 1.0).astype(np.float32)
    rgba = np.dstack((image, alpha))
    return rgba.astype(np.float32)


def make_reference(width: int, height: int) -> np.ndarray:
    rng = np.random.default_rng(SEED)
    coarse = rng.normal(0, 0.06, (height, width, 3)).astype(np.float32)
    fine = rng.normal(0, 0.045, (height, width, 3)).astype(np.float32)
    mid = cv2.GaussianBlur(coarse, (0, 0), 2.2)
    texture = 0.5 + fine + mid
    texture[..., 0] *= 0.92
    texture[..., 2] *= 1.04
    alpha = np.ones((height, width, 1), dtype=np.float32)
    return np.concatenate((np.clip(texture, 0.0, 1.0), alpha), axis=2)


def residual_field(reference_bgra: np.ndarray, grain_size: float) -> np.ndarray:
    sigma = max(0.1, float(grain_size))
    low = cv2.GaussianBlur(reference_bgra[..., :3], (0, 0), sigmaX=sigma)
    return reference_bgra[..., :3] - low


def jitter_for(frame: int, width: int, height: int, amount: float) -> tuple[int, int]:
    jx = int((creative_hash(frame, 17, 9127) - 0.5) * width * amount)
    jy = int((creative_hash(31, frame, 1777) - 0.5) * height * amount)
    return jx, jy


def apply_model(source_bgra: np.ndarray, reference_bgra: np.ndarray,
                params: dict[str, float], frame: int) -> tuple[np.ndarray, np.ndarray, tuple[int, int]]:
    height, width = source_bgra.shape[:2]
    residual = residual_field(reference_bgra, params["grainSize"])
    jitter_x, jitter_y = jitter_for(frame, width, height, params["temporalJitter"])
    ys = (np.arange(height) + jitter_y) % height
    xs = (np.arange(width) + jitter_x) % width
    shifted = residual[ys[:, None], xs[None, :], :]

    # Source accesses are BGRA; weights below follow the inspected BGRA indices.
    src_luma = (source_bgra[..., 2] * 0.2126 + source_bgra[..., 1] * 0.7152 +
                source_bgra[..., 0] * 0.0722)
    residual_luma = (shifted[..., 2] * 0.2126 + shifted[..., 1] * 0.7152 +
                     shifted[..., 0] * 0.0722)
    luma_clamped = np.clip(src_luma, 0.0, 1.0)
    response = np.maximum(0.0, 1.0 + (1.0 - luma_clamped) * params["shadowResponse"] +
                          luma_clamped * params["highlightResponse"])
    output = source_bgra.copy()
    for channel in range(3):
        chroma = shifted[..., channel] - residual_luma
        grain = (residual_luma * params["lumaAmount"] + chroma * params["chromaAmount"])
        output[..., channel] = source_bgra[..., channel] + grain * params["amount"] * response * params["mix"]
    output[..., 3] = source_bgra[..., 3]
    return output, shifted, (jitter_x, jitter_y)


def corr(a: np.ndarray, b: np.ndarray) -> float:
    av = a.astype(np.float64).ravel()
    bv = b.astype(np.float64).ravel()
    av -= av.mean(); bv -= bv.mean()
    den = np.sqrt(np.mean(av * av) * np.mean(bv * bv))
    return float(np.mean(av * bv) / den) if den else 0.0


def stats(source: np.ndarray, candidate: np.ndarray, mask: np.ndarray,
          reference_residual: np.ndarray) -> dict[str, float | int]:
    before = source[mask, :3]
    after = candidate[mask, :3]
    delta = after - before
    alpha_delta = candidate[mask, 3] - source[mask, 3]
    return {
        "pixel_count": int(mask.sum()),
        "rgb_mean_abs_delta": float(np.mean(np.abs(delta))) if delta.size else 0.0,
        "rgb_rmse": float(np.sqrt(np.mean(delta * delta))) if delta.size else 0.0,
        "rgb_p99_abs_delta": float(np.quantile(np.abs(delta), 0.99)) if delta.size else 0.0,
        "rgb_max_abs_delta": float(np.max(np.abs(delta))) if delta.size else 0.0,
        "candidate_rgb_outside_0_1_fraction": float(np.mean((after < 0.0) | (after > 1.0))) if after.size else 0.0,
        "alpha_max_abs_delta": float(np.max(np.abs(alpha_delta))) if alpha_delta.size else 0.0,
        "candidate_residual_reference_corr": corr(delta, reference_residual[mask]),
    }


def write_preview(path: Path, bgra: np.ndarray) -> None:
    # Inspection-only 16-bit PNG; preserve channel order BGRA.
    encoded = np.rint(np.clip(bgra, 0.0, 1.0) * 65535.0).astype(np.uint16)
    cv2.imwrite(str(path), encoded)


def make_contact_sheet(path: Path, cases: list[tuple[str, np.ndarray]],
                       width: int, height: int) -> None:
    cell_w, cell_h = 384, 192
    columns = ["Source", "Reference"] + [label for label, _ in cases[2:]]
    source, reference = cases[0][1], cases[1][1]
    rows = []
    for label in ("source", "reference") + tuple(case[0] for case in cases[2:]):
        if label == "source":
            bgra = source
        elif label == "reference":
            bgra = reference
        else:
            bgra = dict(cases)[label]
        alpha = bgra[..., 3:4]
        gray = np.full_like(bgra[..., :3], 0.18)
        display = np.clip(bgra[..., :3] * alpha + gray * (1.0 - alpha), 0.0, 1.0)
        cell = cv2.resize(display, (cell_w, cell_h), interpolation=cv2.INTER_AREA)
        cell = np.rint(cell * 255.0).astype(np.uint8)
        cv2.putText(cell, label, (8, 22), cv2.FONT_HERSHEY_SIMPLEX, 0.55,
                    (255, 255, 255), 1, cv2.LINE_AA)
        rows.append(cell)
    sheet = np.vstack(rows)
    cv2.imwrite(str(path), sheet)


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    plan = json.loads(PLAN.read_text(encoding="utf-8"))
    source_markers = SOURCE.read_text(encoding="utf-8")
    markers = {
        "named_input_sampled_at_current_frame": "sampleNamedInput(impl_->referenceInput, impl_->frame" in source_markers,
        "reference_residual_uses_gaussian_blur": "cv::GaussianBlur(channels[channel], lowFrequency" in source_markers,
        "source_alpha_copied": "output[offset + 3] = source[offset + 3];" in source_markers,
        "frame_hash_jitter": "creativeHash(\n        static_cast<int>(impl_->frame), 17, 9127)" in source_markers,
        "additive_unclamped_rgb": "output[offset + channel] = source[offset + channel] +" in source_markers,
    }
    if not all(markers.values()):
        raise SystemExit(f"Source marker verification failed: {markers}")

    reports = {}
    preview_paths = []
    for width, height in ((512, 256), (1024, 512)):
        source = make_source(width, height)
        reference = make_reference(width, height)
        yy, xx = np.mgrid[0:height, 0:width]
        roi = {
            "whole": np.ones((height, width), bool),
            "transparent": source[..., 3] < 0.01,
            "edge": (source[..., 3] >= 0.01) & (source[..., 3] < 0.99),
            "opaque": source[..., 3] >= 0.99,
            "fine_detail": (xx > width * 0.65) & (xx < width * 0.91) & (yy > height * 0.08) & (yy < height * 0.92),
            "flat_patch": (xx > width * 0.08) & (xx < width * 0.19) & (yy > height * 0.08) & (yy < height * 0.92),
        }
        default_residual = residual_field(reference, DEFAULTS["grainSize"])
        frame_models = {}
        size_rows = {}
        display_cases: list[tuple[str, np.ndarray]] = [
            ("Source", source), ("Reference", reference),
        ]

        for grain_size in (0.5, 1.15, 3.0, 8.0):
            params = DEFAULTS | {"grainSize": grain_size, "temporalJitter": 0.0}
            candidate, shifted, jitter = apply_model(source, reference, params, 0)
            key = f"grain_size_{grain_size:g}"
            size_rows[key] = {
                "grain_size": grain_size,
                "jitter_xy": list(jitter),
                "regions": {name: stats(source, candidate, mask, residual_field(reference, grain_size)) for name, mask in roi.items()},
            }
            preview_path = OUT / f"match-grain-{width}x{height}-{key}.png"
            write_preview(preview_path, candidate)
            preview_paths.append(preview_path)
            display_cases.append((f"Size {grain_size:g}", candidate))

        mix_rows = {}
        for label, overrides in (
            ("mix_0", {"mix": 0.0}), ("mix_half", {"mix": 0.5}),
            ("mix_1", {"mix": 1.0}), ("luma_only", {"chromaAmount": 0.0}),
            ("chroma_only", {"lumaAmount": 0.0}),
            ("neutral_luma_response", {"shadowResponse": 0.0, "highlightResponse": 0.0}),
        ):
            params = DEFAULTS | overrides | {"temporalJitter": 0.0}
            candidate, shifted, jitter = apply_model(source, reference, params, 0)
            mix_rows[label] = {
                "parameters": params,
                "jitter_xy": list(jitter),
                "regions": {name: stats(source, candidate, mask, default_residual) for name, mask in roi.items()},
            }
            preview_path = OUT / f"match-grain-{width}x{height}-{label}.png"
            write_preview(preview_path, candidate)
            preview_paths.append(preview_path)
            if label in ("mix_half", "luma_only", "chroma_only"):
                display_cases.append((label, candidate))

            if label == "mix_1":
                default_candidate = candidate

        temporal_rows = {}
        for jitter_amount in (0.0, 0.5, 1.0):
            params = DEFAULTS | {"temporalJitter": jitter_amount}
            previous = None
            frames = []
            for frame in range(4):
                candidate, shifted, offset = apply_model(source, reference, params, frame)
                current_delta = candidate[..., :3] - source[..., :3]
                row = {"frame": frame, "jitter_xy": list(offset)}
                if previous is not None:
                    diff = current_delta - previous
                    opaque = roi["opaque"]
                    row["opaque_frame_delta_rmse"] = float(np.sqrt(np.mean(diff[opaque] ** 2)))
                    row["opaque_residual_temporal_corr"] = corr(current_delta[opaque], previous[opaque])
                frames.append(row)
                previous = current_delta
                if frame == 0:
                    preview_path = OUT / f"match-grain-{width}x{height}-jitter_{jitter_amount:g}-frame_{frame}.png"
                    write_preview(preview_path, candidate)
                    preview_paths.append(preview_path)
            temporal_rows[f"jitter_{jitter_amount:g}"] = frames

        make_contact_sheet(OUT / f"contact-sheet-{width}x{height}.png", display_cases, width, height)
        # Native-size crops from deterministic source regions, including a translucent edge.
        crop_centers = {
            "alpha-edge": (int(width * 0.88), int(height * 0.5)),
            "flat-patch": (int(width * 0.14), int(height * 0.55)),
            "fine-detail": (int(width * 0.77), int(height * 0.5)),
        }
        half_w, half_h = max(12, width // 32), max(12, height // 16)
        for crop_name, (center_x, center_y) in crop_centers.items():
            x0, x1 = max(0, center_x - half_w), min(width, center_x + half_w)
            y0, y1 = max(0, center_y - half_h), min(height, center_y + half_h)
            write_preview(OUT / f"crop-{width}x{height}-{crop_name}-source.png", source[y0:y1, x0:x1])
            write_preview(OUT / f"crop-{width}x{height}-{crop_name}-candidate.png", default_candidate[y0:y1, x0:x1])

        reports[f"{width}x{height}"] = {
            "roi_pixel_counts": {name: int(mask.sum()) for name, mask in roi.items()},
            "grain_size_sweep": size_rows,
            "response_sweep": mix_rows,
            "temporal_jitter": temporal_rows,
        }

        write_preview(OUT / f"source-{width}x{height}.png", source)
        write_preview(OUT / f"reference-{width}x{height}.png", reference)

    metrics = {
        "plan_id": plan["plan_id"],
        "plan_sha256": sha(PLAN),
        "script_sha256": sha(Path(__file__)),
        "source_sha256": sha(SOURCE),
        "python": platform.python_version(),
        "numpy": np.__version__,
        "opencv": cv2.__version__,
        "seed": SEED,
        "channel_model": "BGRA float32 array; source luma weights use B=.0722, G=.7152, R=.2126",
        "source_markers_verified": markers,
        "parameters": DEFAULTS,
        "measurements": reports,
        "fixture_preview_sha256": {p.name: sha(p) for p in sorted(OUT.glob("*.png"))},
        "limits": "Independent source-equation mirror only. Does not execute Artifact, validate auxiliary input descriptors, expose production dispatch, or establish aesthetic acceptance/performance.",
    }
    metrics_path = OUT / "metrics.json"
    metrics_path.write_text(json.dumps(metrics, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    (OUT / "metrics.sha256").write_text(f"{sha(metrics_path)}  metrics.json\n", encoding="utf-8")
    print(json.dumps({"metrics": str(metrics_path), "preview_count": len(preview_paths) + 4,
                      "plan_sha256": metrics["plan_sha256"], "script_sha256": metrics["script_sha256"],
                      "source_markers_verified": markers}, indent=2))


if __name__ == "__main__":
    main()
