from __future__ import annotations

import hashlib
import json
import math
import platform
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent.parent
PLAN = ROOT / "plans/physical-halation-highlight-cluster-edge-response-r1.json"
SOURCE = REPO / "ArtifactCore/include/ImageProcessing/Halation.ixx"
WRAPPER = REPO / "Artifact/src/Effects/Glow/PhysicalHalationEffect.cpp"
OUT = ROOT / "captures/physical-halation-highlight-cluster-edge-response-r1"
OUT.mkdir(parents=True, exist_ok=True)

SOURCE_TEXT = SOURCE.read_text(encoding="utf-8")
WRAPPER_TEXT = WRAPPER.read_text(encoding="utf-8")
MARKERS = {
    "source_rec601": "source[i].x * 0.114f + source[i].y * 0.587f + source[i].z * 0.299f" in SOURCE_TEXT,
    "soft_knee": "softContribution" in SOURCE_TEXT,
    "highlight_red_green_blue_mapping": "source[i].x * weight * 0.1f" in SOURCE_TEXT,
    "integer_radius_cast": "int r = static_cast<int>(radius);" in SOURCE_TEXT,
    "edge_clamp_horizontal": "std::clamp(x + dx, 0, w - 1)" in SOURCE_TEXT,
    "edge_clamp_vertical": "std::clamp(y + dy, 0, h - 1)" in SOURCE_TEXT,
    "green_blue_layer_mapping": "0.0f, 0.1f, 0.05f" in SOURCE_TEXT,
    "source_alpha_restored": "buffer[i].w = source[i].w" in SOURCE_TEXT,
    "wrapper_starts_from_source": "dst = src;" in WRAPPER_TEXT,
}
if not all(MARKERS.values()):
    raise RuntimeError(MARKERS)

DIMENSIONS = (128, 256, 512, 1024)
SPREADS = (15, 50, 100)
SIGMAS = (0, 1, 4, 12)
POSITIONS = ("center", "edge_left", "edge_right", "edge_top", "edge_bottom", "corner_tl", "corner_tr", "corner_bl", "corner_br")
THRESHOLD = np.float32(0.8)
SOFTNESS = np.float32(0.5)
RED_DIFFUSION = np.float32(2.0)
INTENSITY = np.float32(0.5)
PEAK = np.float32(4.0)
ALPHA = np.float32(1.0)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def make_source(length: int, sigma: int, position: str) -> tuple[np.ndarray, dict]:
    support_radius = int(math.ceil(2.0 * sigma)) if sigma else 0
    mid = length // 2
    centers = {
        "center": (mid, mid),
        "edge_left": (support_radius, mid),
        "edge_right": (length - 1 - support_radius, mid),
        "edge_top": (mid, support_radius),
        "edge_bottom": (mid, length - 1 - support_radius),
        "corner_tl": (support_radius, support_radius),
        "corner_tr": (length - 1 - support_radius, support_radius),
        "corner_bl": (support_radius, length - 1 - support_radius),
        "corner_br": (length - 1 - support_radius, length - 1 - support_radius),
    }
    cx, cy = centers[position]

    source = np.zeros((length, length, 4), dtype=np.float32)
    source[..., 3] = ALPHA
    mask = np.zeros((length, length), dtype=bool)
    offsets = np.arange(-support_radius, support_radius + 1)
    if sigma == 0:
        patch = np.array([[PEAK]], dtype=np.float32)
        mask_patch = np.ones((1, 1), dtype=bool)
    else:
        oy, ox = np.meshgrid(offsets, offsets, indexing="ij")
        radius_sq = ox * ox + oy * oy
        mask_patch = radius_sq <= support_radius * support_radius
        patch = np.zeros_like(radius_sq, dtype=np.float32)
        patch[mask_patch] = PEAK * np.exp(
            -radius_sq[mask_patch].astype(np.float32) / np.float32(2.0 * sigma * sigma)
        )

    x0, y0 = cx - support_radius, cy - support_radius
    y1, x1 = y0 + patch.shape[0], x0 + patch.shape[1]
    if x0 < 0 or y0 < 0 or x1 > length or y1 > length:
        raise RuntimeError(f"source cluster clipped: {length=} {sigma=} {position=}")
    source[y0:y1, x0:x1, :3] = patch[..., None]
    mask[y0:y1, x0:x1] = mask_patch
    return source, {
        "center_xy": [cx, cy],
        "support_radius_px": support_radius,
        "full_spot_pixel_count": int(mask_patch.sum()),
        "in_frame_spot_pixel_count": int(mask.sum()),
        "source_retention_fraction": float(mask.sum() / max(1, mask_patch.sum())),
    }


def extract_highlights(source: np.ndarray) -> tuple[np.ndarray, dict, np.ndarray]:
    b, g, r = source[..., 0], source[..., 1], source[..., 2]
    lum = b * np.float32(0.114) + g * np.float32(0.587) + r * np.float32(0.299)
    knee = np.clip(SOFTNESS, np.float32(0), np.float32(1))
    soft = np.clip(lum - THRESHOLD + knee, np.float32(0), np.float32(2) * knee)
    soft_contribution = soft * soft / (np.float32(4) * knee + np.float32(0.000001))
    contribution = np.maximum(lum - THRESHOLD, soft_contribution)
    weight = np.clip(
        contribution / (np.float32(1) - THRESHOLD + np.float32(0.001)),
        np.float32(0),
        np.float32(1),
    )
    weight = np.where(contribution > 0, weight, np.float32(0)).astype(np.float32)
    highlights = np.empty_like(source[..., :3])
    highlights[..., 0] = b * weight * np.float32(0.1)
    highlights[..., 1] = g * weight * np.float32(0.2)
    highlights[..., 2] = r * weight
    return highlights, {
        "positive_highlight_pixel_count": int(np.count_nonzero(weight > 0)),
        "red_input_highlight_mass": float(highlights[..., 2].sum(dtype=np.float64)),
        "green_effective_input_mass": float((highlights[..., 1] * np.float32(0.1)).sum(dtype=np.float64)),
        "blue_effective_input_mass": float((highlights[..., 0] * np.float32(0.05)).sum(dtype=np.float64)),
        "highlight_peak_weight": float(weight.max()),
    }, weight


def box_pass_axis(plane: np.ndarray, radius: int, axis: int) -> np.ndarray:
    """One normalized edge-replicated box pass along one axis."""
    length = plane.shape[axis]
    coords = np.arange(length)
    lo = np.maximum(coords - radius, 0)
    hi = np.minimum(coords + radius + 1, length)
    if axis == 1:
        prefix = np.pad(np.cumsum(plane, axis=1, dtype=np.float64), ((0, 0), (1, 0)))
        sums = prefix[:, hi] - prefix[:, lo]
        sums += np.maximum(radius - coords, 0)[None, :] * plane[:, :1]
        sums += np.maximum(coords + radius - (length - 1), 0)[None, :] * plane[:, -1:]
    else:
        prefix = np.pad(np.cumsum(plane, axis=0, dtype=np.float64), ((1, 0), (0, 0)))
        sums = prefix[hi, :] - prefix[lo, :]
        sums += np.maximum(radius - coords, 0)[:, None] * plane[:1, :]
        sums += np.maximum(coords + radius - (length - 1), 0)[:, None] * plane[-1:, :]
    return (sums / float(2 * radius + 1)).astype(np.float32)


def diffuse(plane: np.ndarray, radius: int) -> np.ndarray:
    horizontal = box_pass_axis(plane, radius, 1)
    return box_pass_axis(horizontal, radius, 0)


def stats(field: np.ndarray, origin: tuple[float, float]) -> dict:
    mass = float(field.sum(dtype=np.float64))
    yy, xx = np.nonzero(field > np.float32(1e-6))
    if mass > 0:
        xs = np.arange(field.shape[1], dtype=np.float64)
        ys = np.arange(field.shape[0], dtype=np.float64)
        profile_x = field.sum(axis=0, dtype=np.float64)
        profile_y = field.sum(axis=1, dtype=np.float64)
        cx = float(np.dot(xs, profile_x) / mass)
        cy = float(np.dot(ys, profile_y) / mass)
    else:
        cx, cy = 0.0, 0.0
    return {
        "energy": mass,
        "peak": float(field.max(initial=np.float32(0))),
        "centroid_xy": [cx, cy],
        "centroid_shift_from_source_center_xy": [cx - origin[0], cy - origin[1]],
        "support_pixels_gt_1e_6": int(len(xx)),
        "bbox_xyxy_exclusive": None if not len(xx) else [int(xx.min()), int(yy.min()), int(xx.max() + 1), int(yy.max() + 1)],
        "support_frame_fraction": float(len(xx) / field.size),
    }


rows = []
saved_contact = []

for dimension in DIMENSIONS:
    for spread in SPREADS:
        red_radius = int(np.float32(spread) * RED_DIFFUSION)
        gb_radius = int(spread)
        for sigma in SIGMAS:
            for position in POSITIONS:
                source, spot = make_source(dimension, sigma, position)
                highlights, highlight_metrics, highlight_weight = extract_highlights(source)
                red_input = highlights[..., 2]
                green_input = highlights[..., 1] * np.float32(0.1)
                blue_input = highlights[..., 0] * np.float32(0.05)
                red_field = diffuse(red_input, red_radius)
                green_field = diffuse(green_input, gb_radius)
                blue_field = diffuse(blue_input, gb_radius)
                origin = (float(spot["center_xy"][0]), float(spot["center_xy"][1]))
                red = stats(red_field, origin)
                green = stats(green_field, origin)
                blue = stats(blue_field, origin)
                red["energy_gain_over_unbounded_input_highlights"] = (
                    red["energy"] / highlight_metrics["red_input_highlight_mass"]
                    if highlight_metrics["red_input_highlight_mass"] else 0.0
                )
                green["energy_gain_over_unbounded_input_highlights"] = (
                    green["energy"] / highlight_metrics["green_effective_input_mass"]
                    if highlight_metrics["green_effective_input_mass"] else 0.0
                )
                blue["energy_gain_over_unbounded_input_highlights"] = (
                    blue["energy"] / highlight_metrics["blue_effective_input_mass"]
                    if highlight_metrics["blue_effective_input_mass"] else 0.0
                )
                output_rgb = source[..., :3].copy()
                output_rgb[..., 0] += blue_field * INTENSITY
                output_rgb[..., 1] += green_field * INTENSITY
                output_rgb[..., 2] += red_field * INTENSITY
                output_alpha = source[..., 3].copy()
                active_edge = bool(
                    np.any(highlight_weight[0, :] > 0)
                    or np.any(highlight_weight[-1, :] > 0)
                    or np.any(highlight_weight[:, 0] > 0)
                    or np.any(highlight_weight[:, -1] > 0)
                )
                row = {
                    "dimension": dimension,
                    "spread": spread,
                    "red_radius": red_radius,
                    "gb_radius": gb_radius,
                    "cluster_sigma_px": sigma,
                    "position": position,
                    "source_spot": spot,
                    "input_highlights": highlight_metrics,
                    "positive_highlight_reaches_frame_border": active_edge,
                    "red": red,
                    "green": green,
                    "blue": blue,
                    "output_alpha_exact": bool(np.array_equal(output_alpha, source[..., 3])),
                    "output_nonfinite_count": int(np.count_nonzero(~np.isfinite(output_rgb))),
                    "output_negative_rgb_count": int(np.count_nonzero(output_rgb < 0)),
                    "output_rgb_min": [float(output_rgb[..., c].min()) for c in range(3)],
                    "output_rgb_max": [float(output_rgb[..., c].max()) for c in range(3)],
                    "cluster_support_touches_red_kernel_boundary": (
                        red_radius + spot["support_radius_px"] >= dimension / 2
                    ),
                    "red_radius_to_frame_ratio": red_radius / float(dimension),
                }
                rows.append(row)
                if dimension == 256 and spread == 50:
                    saved_contact.append((sigma, position, red_field * INTENSITY, source))

# Compare each identical spot against center response and check reflection invariants.
for row in rows:
    center = next(
        x for x in rows
        if x["dimension"] == row["dimension"]
        and x["spread"] == row["spread"]
        and x["cluster_sigma_px"] == row["cluster_sigma_px"]
        and x["position"] == "center"
    )
    row["red_energy_gain_vs_center"] = row["red"]["energy"] / center["red"]["energy"] if center["red"]["energy"] else 0.0
    row["green_energy_gain_vs_center"] = row["green"]["energy"] / center["green"]["energy"] if center["green"]["energy"] else 0.0
    row["blue_energy_gain_vs_center"] = row["blue"]["energy"] / center["blue"]["energy"] if center["blue"]["energy"] else 0.0

mirror_checks = []
for dimension in DIMENSIONS:
    for spread in SPREADS:
        for sigma in SIGMAS:
            for a_name, b_name, reflect_x, reflect_y in (
                ("edge_left", "edge_right", True, False),
                ("edge_top", "edge_bottom", False, True),
                ("corner_tl", "corner_tr", True, False),
                ("corner_tl", "corner_bl", False, True),
                ("corner_tl", "corner_br", True, True),
            ):
                a = next(row for row in rows if row["dimension"] == dimension and row["spread"] == spread and row["cluster_sigma_px"] == sigma and row["position"] == a_name)
                b = next(row for row in rows if row["dimension"] == dimension and row["spread"] == spread and row["cluster_sigma_px"] == sigma and row["position"] == b_name)
                errors = []
                for channel in ("red", "green", "blue"):
                    av, bv = a[channel], b[channel]
                    relative_energy_error = abs(av["energy"] - bv["energy"]) / max(av["energy"], bv["energy"], 1e-12)
                    expected_x = dimension - 1 - av["centroid_xy"][0] if reflect_x else av["centroid_xy"][0]
                    expected_y = dimension - 1 - av["centroid_xy"][1] if reflect_y else av["centroid_xy"][1]
                    centroid_error = max(abs(expected_x - bv["centroid_xy"][0]), abs(expected_y - bv["centroid_xy"][1]))
                    errors.append(max(relative_energy_error, centroid_error / dimension))
                mirror_checks.append({
                    "dimension": dimension,
                    "spread": spread,
                    "cluster_sigma_px": sigma,
                    "pair": [a_name, b_name],
                    "max_relative_energy_or_normalized_centroid_error": max(errors),
                })

# One shared square-root display curve preserves cross-tile magnitude ordering
# while keeping broad low-amplitude halos visible beside the sharp point case.
tile = 128
label_h = 18
cols = 3
rows_per_sigma = len(SIGMAS)
sheet = Image.new("RGB", (cols * tile, rows_per_sigma * (tile + label_h)), (18, 18, 18))
draw = ImageDraw.Draw(sheet)
pos_order = ("center", "edge_tangent", "corner_tangent")
display_max = max(float(x[2].max()) for x in saved_contact)
display_labels = {"center": "center", "edge_tangent": "edge_left", "corner_tangent": "corner_tl"}
for row_idx, sigma in enumerate(SIGMAS):
    for col_idx, position in enumerate(pos_order):
        contact_position = display_labels[position]
        red_field = next(x[2] for x in saved_contact if x[0] == sigma and x[1] == contact_position)
        view = np.uint8(np.clip(np.sqrt(red_field / np.float32(display_max)), 0, 1) * 255 + 0.5)
        image = Image.fromarray(view, mode="L").convert("RGB")
        image = image.resize((tile, tile), Image.Resampling.NEAREST)
        x, y = col_idx * tile, row_idx * (tile + label_h)
        sheet.paste(image, (x, y))
        draw.text((x + 3, y + tile + 2), f"sigma={sigma} {contact_position}", fill=(240, 240, 240))
sheet.save(OUT / "cluster-red-diffusion-contact-spread-50-256.png")

metrics = {
    "study": "Physical Halation finite highlight-cluster edge response r1",
    "environment": {"python": platform.python_version(), "numpy": np.__version__},
    "input_sha256": {
        "plan": sha(PLAN),
        "core_source": sha(SOURCE),
        "wrapper_source": sha(WRAPPER),
        "runner": sha(Path(__file__)),
    },
    "source_markers": MARKERS,
    "conditions": {
        "dimensions": list(DIMENSIONS),
        "spreads": list(SPREADS),
        "cluster_sigma_px": list(SIGMAS),
        "positions": list(POSITIONS),
        "cases_total": len(rows),
        "peak_rgb": float(PEAK),
        "source_alpha": float(ALPHA),
        "threshold": float(THRESHOLD),
        "softness": float(SOFTNESS),
        "red_diffusion": float(RED_DIFFUSION),
        "intensity": float(INTENSITY),
        "spot_definition": "Gaussian intensity exp(-r^2/(2*sigma^2)), radial support <=2*sigma; sigma 0 is a single pixel. At the nonzero-sigma boundary, peak 4 gives luminance 4*exp(-2)=0.5413, above the positive-highlight threshold at threshold .8 / softness .5.",
        "box_accumulation": "Float64 prefix sums per axis, normalized once per axis, Float32 between horizontal and vertical passes.",
        "display_mapping": "Red diffusion delta divided by the global maximum across contact-sheet conditions, then square-root mapped to [0,1] for PNG display only.",
    },
    "reflection_checks": mirror_checks,
    "results": rows,
    "limitations": [
        "NumPy Float32 source-equation model with Float64 prefix-sum accumulation; not compiled C++ or application output.",
        "Synthetic neutral Gaussian HDR spots and opaque alpha only; no photographic plate, colored source, transparent edge or display transform.",
        "Contact sheet uses a fixed gain and clips for visualization; numeric metrics are authoritative.",
        "No GPU/preview/final parity, performance, allocation or physical-film validation.",
    ],
}
(OUT / "metrics.json").write_text(json.dumps(metrics, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
checksums = {
    path.name: sha(path)
    for path in sorted(OUT.iterdir())
    if path.is_file() and path.name != "checksums.json"
}
(OUT / "checksums.json").write_text(json.dumps(checksums, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
print(json.dumps({"cases": len(rows), "source_markers_all_true": all(MARKERS.values()), "all_alpha_exact": all(row["output_alpha_exact"] for row in rows), "all_source_retained": all(row["source_spot"]["source_retention_fraction"] == 1.0 for row in rows), "nonfinite_total": sum(row["output_nonfinite_count"] for row in rows), "outputs": len(checksums)}))
