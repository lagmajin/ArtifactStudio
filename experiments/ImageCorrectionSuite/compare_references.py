from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter


ROOT = Path(__file__).resolve().parent
CAPTURE = ROOT / "captures" / "rd0"


def read(name: str) -> np.ndarray:
    return np.load(CAPTURE / f"{name}.npy", allow_pickle=False).astype(np.float32, copy=False)


def save_preview(name: str, rgb: np.ndarray) -> str:
    path = CAPTURE / f"reference_{name}.png"
    srgb = np.where(rgb <= 0.0031308, rgb * 12.92, 1.055 * np.maximum(rgb, 0) ** (1 / 2.4) - 0.055)
    Image.fromarray(np.uint8(np.clip(srgb * 255 + 0.5, 0, 255)), "RGB").save(path, format="PNG", optimize=False)
    return hashlib.sha256(path.read_bytes()).hexdigest()


def stats(a: np.ndarray, b: np.ndarray) -> dict[str, float]:
    d = np.abs(a.astype(np.float64) - b.astype(np.float64)).reshape(-1)
    return {"mae": float(d.mean()), "rmse": float(np.sqrt(np.mean(d * d))), "p95": float(np.percentile(d, 95)), "max": float(d.max())}


def to_u8(rgb: np.ndarray) -> np.ndarray:
    return np.uint8(np.clip(np.rint(np.clip(rgb, 0, 1) ** (1 / 2.2) * 255), 0, 255))


def from_u8(rgb: np.ndarray) -> np.ndarray:
    return (rgb.astype(np.float32) / 255.0) ** 2.2


def main() -> None:
    results: list[dict[str, object]] = []

    # 2: classical scratch repair references. The damage mask is known; local methods
    # are intentionally modest and serve as baselines, not restoration claims.
    clean, damaged = read("repair_clean"), read("repair_damaged")
    mask = np.asarray(Image.open(CAPTURE / "repair_damage_mask.png")) > 0
    damaged8 = to_u8(damaged)
    for radius in (3, 7, 13):
        result8 = np.asarray(Image.fromarray(damaged8).filter(ImageFilter.MedianFilter(radius | 1)))
        result = from_u8(result8)
        name = f"repair_median_{radius}"
        results.append({"candidate": name, "theme": "2-auto-repair", "status": "offline_reference_only",
                        "masked_error": stats(result[mask], clean[mask]),
                        "outside_mask_max_abs": float(np.max(np.abs(result[~mask] - damaged[~mask]))),
                        "preview_sha256": save_preview(name, result)})

    # 3: global Fourier low-pass references. Report both target-band reduction and
    # retained high-frequency energy; no automatic quality verdict.
    moire, truth = read("moire_corrupted"), read("moire_reference")
    luminance = moire.mean(axis=2)
    spectrum = np.fft.fftshift(np.fft.fft2(luminance))
    cy, cx = np.array(luminance.shape) // 2
    fy, fx = np.ogrid[:luminance.shape[0], :luminance.shape[1]]
    radius = np.sqrt(((fx - cx) / max(cx, 1)) ** 2 + ((fy - cy) / max(cy, 1)) ** 2)
    for cutoff in (0.08, 0.16, 0.28):
        filtered = np.fft.ifft2(np.fft.ifftshift(spectrum * (radius <= cutoff))).real.astype(np.float32)
        delta = filtered - luminance
        result = np.clip(moire + delta[..., None], 0, 1)
        name = f"moire_lowpass_{cutoff:.2f}"
        high = radius > cutoff
        results.append({"candidate": name, "theme": "3-moire", "status": "offline_reference_only",
                        "full_image_error_vs_truth": stats(result, truth),
                        "high_frequency_energy_ratio": float(np.sum(np.abs(spectrum[high]) ** 2) / max(1e-12, np.sum(np.abs(spectrum) ** 2))),
                        "preview_sha256": save_preview(name, result)})

    # 5: inverse radial maps for known barrel/pincushion coefficient.
    chart = read("distortion_reference")
    yy, xx = np.mgrid[:chart.shape[0], :chart.shape[1]]
    h, w = chart.shape[:2]
    cx, cy = (w - 1) / 2, (h - 1) / 2
    xn, yn = (xx - cx) / cx, (yy - cy) / cy
    r2 = xn * xn + yn * yn
    for kind, coeff in (("barrel", 0.08), ("pincushion", -0.08)):
        warped = read(f"distortion_{kind}")
        # Fixed-point invert the radial polynomial for a known synthetic coefficient.
        target_r = np.sqrt(r2)
        source_r = target_r.copy()
        for _ in range(8):
            source_r = target_r / np.maximum(1e-6, 1.0 + coeff * source_r * source_r)
        scale = np.divide(source_r, target_r, out=np.ones_like(source_r), where=target_r > 1e-8)
        map_x = (cx + xn * scale * cx).astype(np.float32)
        map_y = (cy + yn * scale * cy).astype(np.float32)
        import cv2
        result = cv2.remap(warped, map_x, map_y, cv2.INTER_LINEAR, borderMode=cv2.BORDER_CONSTANT)
        valid = (map_x >= 1) & (map_x < w - 1) & (map_y >= 1) & (map_y < h - 1)
        name = f"distortion_inverse_{kind}"
        results.append({"candidate": name, "theme": "5-lens-distortion", "status": "known-coefficient-offline-reference",
                        "valid_interior_error": stats(result[valid], chart[valid]),
                        "valid_pixel_fraction": float(np.mean(valid)),
                        "preview_sha256": save_preview(name, result)})

    # 10: channel-wise mean/std reference, explicitly distinct from Core Lab/Lab matcher.
    source, reference = read("color_match_source"), read("color_match_reference")
    matched = source.copy()
    source_stats = (source.mean(axis=(0, 1)), source.std(axis=(0, 1)))
    reference_stats = (reference.mean(axis=(0, 1)), reference.std(axis=(0, 1)))
    matched = (source - source_stats[0]) * (reference_stats[1] / np.maximum(source_stats[1], 1e-6)) + reference_stats[0]
    matched = np.clip(matched, 0, 1)
    name = "color_match_channel_mean_std"
    results.append({"candidate": name, "theme": "10-color-match", "status": "offline_reference_only_not_Core_AutoColorMatcher",
                    "source_to_reference_error_before": stats(source, reference),
                    "source_to_reference_error_after": stats(matched, reference),
                    "preview_sha256": save_preview(name, matched)})

    output = {
        "rd": "rd0-reference",
        "purpose": "exercise fixture-to-metric pipeline and expose candidate tradeoffs; no quality gates or ranking",
        "input_manifest_sha256": hashlib.sha256((CAPTURE / "manifest.json").read_bytes()).hexdigest(),
        "color_contract": "float32 linear RGB for metrics; previews encoded with fixed gamma 2.2",
        "results": results,
        "excluded": {
            "4-chromatic": "Core/RetroImageLab source mirror and runtime parity not added; existing RetroImageLab studies are referenced",
            "7-palette": "Core SmartPaletteAnalyzer and RetroImageLab adaptive palette studies exist; candidate extraction not duplicated",
            "8-dithering": "RetroImageLab Bayer/Floyd-Steinberg studies exist",
            "9-pixel-art": "RetroImageLab pixelation/palette/alpha resize studies exist",
        },
    }
    (CAPTURE / "reference_metrics.json").write_text(json.dumps(output, indent=2) + "\n", encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
