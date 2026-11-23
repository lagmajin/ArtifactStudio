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
PLAN = ROOT / "plans/physical-halation-frame-size-scaling-r1.json"
SOURCE = REPO / "ArtifactCore/include/ImageProcessing/Halation.ixx"
WRAPPER = REPO / "Artifact/src/Effects/Glow/PhysicalHalationEffect.cpp"
OUT = ROOT / "captures/physical-halation-frame-size-scaling-r1"
OUT.mkdir(parents=True, exist_ok=True)

SOURCE_TEXT = SOURCE.read_text(encoding="utf-8")
WRAPPER_TEXT = WRAPPER.read_text(encoding="utf-8")
MARKERS = {
    "source_rec601": "source[i].x * 0.114f + source[i].y * 0.587f + source[i].z * 0.299f" in SOURCE_TEXT,
    "soft_knee": "softContribution" in SOURCE_TEXT,
    "red_radius_integer_cast": "int r = static_cast<int>(radius);" in SOURCE_TEXT,
    "edge_clamp_x": "std::clamp(x + dx, 0, w - 1)" in SOURCE_TEXT,
    "edge_clamp_y": "std::clamp(y + dy, 0, h - 1)" in SOURCE_TEXT,
    "red_radius": "settings.spread * settings.redDiffusion" in SOURCE_TEXT,
    "alpha_restored": "buffer[i].w = source[i].w" in SOURCE_TEXT,
    "wrapper_starts_from_source": "dst = src;" in WRAPPER_TEXT,
}
if not all(MARKERS.values()):
    raise RuntimeError(MARKERS)

DIMENSIONS = (128, 256, 512, 1024)
SPREADS = (5, 15, 50, 100)
SOURCE_VALUE = np.float32(4.0)
THRESHOLD = np.float32(0.8)
SOFTNESS = np.float32(0.5)
RED_DIFFUSION = np.float32(2.0)
INTENSITY = np.float32(0.5)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_weight() -> np.float32:
    # Match the source's order of operations for the fixed neutral HDR point.
    lum = np.float32(SOURCE_VALUE * np.float32(0.114 + 0.587 + 0.299))
    contribution = np.maximum(
        np.float32(lum - THRESHOLD),
        np.float32(np.float32(lum - THRESHOLD + SOFTNESS) ** 2 / np.float32(4 * SOFTNESS + 0.000001)),
    )
    return np.clip(
        np.float32(contribution / np.float32(1 - THRESHOLD + 0.001)),
        np.float32(0),
        np.float32(1),
    )


def clamped_box_impulse(length: int, position: int, radius: int) -> np.ndarray:
    field = np.zeros(length, dtype=np.float32)
    field[position] = np.float32(1)
    result = np.zeros(length, dtype=np.float32)
    coords = np.arange(length)
    count = np.float32(2 * radius + 1)
    for offset in range(-radius, radius + 1):
        result += field[np.clip(coords + offset, 0, length - 1)]
    result /= count
    return result


def layer_response(length: int, x: int, y: int, radius: int, amplitude: float) -> dict:
    px = clamped_box_impulse(length, x, radius)
    py = clamped_box_impulse(length, y, radius)
    sx = float(px.sum(dtype=np.float64))
    sy = float(py.sum(dtype=np.float64))
    total = amplitude * sx * sy
    cx = float(np.dot(px.astype(np.float64), np.arange(length)) / sx)
    cy = float(np.dot(py.astype(np.float64), np.arange(length)) / sy)
    peak = amplitude * float(px.max()) * float(py.max())
    support_x = int(np.count_nonzero(px > 1e-6))
    support_y = int(np.count_nonzero(py > 1e-6))
    return {
        "energy": total,
        "energy_gain_over_unbounded_input": total / amplitude if amplitude else 0.0,
        "peak": peak,
        "centroid_xy": [cx, cy],
        "centroid_shift_xy": [cx - x, cy - y],
        "support_wh": [support_x, support_y],
        "support_frame_fraction": (support_x * support_y) / float(length * length),
        "radius_to_frame_ratio": radius / float(length),
    }


def source_positions(length: int) -> dict[str, tuple[int, int]]:
    mid = length // 2
    return {
        "center": (mid, mid),
        "edge_left": (0, mid),
        "edge_right": (length - 1, mid),
        "edge_top": (mid, 0),
        "edge_bottom": (mid, length - 1),
        "corner_tl": (0, 0),
        "corner_tr": (length - 1, 0),
        "corner_bl": (0, length - 1),
        "corner_br": (length - 1, length - 1),
    }


weight = source_weight()
highlight_red = float(SOURCE_VALUE * weight)
highlight_green = float(SOURCE_VALUE * weight * np.float32(0.2) * np.float32(0.1))
highlight_blue = float(SOURCE_VALUE * weight * np.float32(0.1) * np.float32(0.05))
rows = []
lookup = {}

for length in DIMENSIONS:
    for spread in SPREADS:
        red_radius = int(np.float32(spread) * RED_DIFFUSION)
        gb_radius = int(spread)
        for label, (x, y) in source_positions(length).items():
            red = layer_response(length, x, y, red_radius, highlight_red)
            green = layer_response(length, x, y, gb_radius, highlight_green)
            blue = layer_response(length, x, y, gb_radius, highlight_blue)
            row = {
                "dimension": length,
                "spread": spread,
                "red_radius": red_radius,
                "gb_radius": gb_radius,
                "position": label,
                "source_xy": [x, y],
                "red": red,
                "green": green,
                "blue": blue,
                "red_gain_over_unbounded_input": red["energy_gain_over_unbounded_input"],
                "gb_gain_over_unbounded_input": (
                    green["energy_gain_over_unbounded_input"] + blue["energy_gain_over_unbounded_input"]
                ) / 2.0,
                "gb_combined_energy": green["energy"] + blue["energy"],
                "alpha_exact": True,
                "finite_nonnegative": all(
                    math.isfinite(v) and v >= 0
                    for v in (red["energy"], red["peak"], green["energy"], blue["energy"])
                ),
            }
            rows.append(row)
            lookup[(length, spread, label)] = row

# Compare boundary energy with the center in the same frame. A center that is
# itself clipped by the boundary remains explicitly identified in the metrics.
for row in rows:
    center = lookup[(row["dimension"], row["spread"], "center")]
    row["red_gain_vs_center"] = row["red"]["energy"] / center["red"]["energy"]
    row["gb_gain_vs_center"] = row["gb_combined_energy"] / center["gb_combined_energy"]
    row["center_kernel_touches_boundary"] = (
        row["red_radius"] >= row["dimension"] // 2
    )

# Validate reflection symmetry on complete 2D fields using the exact separable
# Frobenius norm, without allocating a full frame for every matrix comparison.
symmetry = []
pairs = (
    ("edge_left", "edge_right", "x"),
    ("edge_top", "edge_bottom", "y"),
    ("corner_tl", "corner_tr", "x"),
    ("corner_tl", "corner_bl", "y"),
    ("corner_tl", "corner_br", "xy"),
)
for length in DIMENSIONS:
    for spread in SPREADS:
        for first, second, axis in pairs:
            a = lookup[(length, spread, first)]
            b = lookup[(length, spread, second)]
            xa, ya = a["source_xy"]
            xb, yb = b["source_xy"]
            error_by_channel = []
            for channel in ("red", "green", "blue"):
                radius = int(spread * 2) if channel == "red" else spread
                amplitude = {
                    "red": highlight_red,
                    "green": highlight_green,
                    "blue": highlight_blue,
                }[channel]
                ax = clamped_box_impulse(length, xa, radius)
                ay = clamped_box_impulse(length, ya, radius)
                bx = clamped_box_impulse(length, xb, radius)
                by = clamped_box_impulse(length, yb, radius)
                if "x" in axis:
                    bx = bx[::-1]
                if "y" in axis:
                    by = by[::-1]
                field_a = np.multiply.outer(ay, ax) * np.float32(amplitude)
                field_b = np.multiply.outer(by, bx) * np.float32(amplitude)
                error_by_channel.append(float(np.sqrt(np.mean(np.square(field_a - field_b), dtype=np.float64))))
            symmetry.append({"dimension": length, "spread": spread, "pair": [first, second], "mirrored_rgb_rmse": max(error_by_channel)})

# Produce compact amplification heatmaps with dimension columns and spread rows.
def heatmap(position: str, key: str, filename: str) -> None:
    cell_w, cell_h, left, top = 145, 52, 110, 42
    image = Image.new("RGB", (left + cell_w * len(DIMENSIONS), top + cell_h * len(SPREADS) + 12), "white")
    draw = ImageDraw.Draw(image)
    draw.text((left, 12), "Frame width (px)", fill="black")
    for col, length in enumerate(DIMENSIONS):
        draw.text((left + col * cell_w + 44, 28), str(length), fill="black")
    values = [
        lookup[(length, spread, position)][key]
        for spread in SPREADS
        for length in DIMENSIONS
    ]
    vmax = max(values) if values else 1.0
    for row_idx, spread in enumerate(SPREADS):
        y = top + row_idx * cell_h
        draw.text((10, y + 18), f"spread {spread}", fill="black")
        for col, length in enumerate(DIMENSIONS):
            value = lookup[(length, spread, position)][key]
            norm = math.log1p(value) / math.log1p(vmax) if vmax > 0 else 0.0
            color = (255, int(245 - 195 * norm), int(245 - 220 * norm))
            x = left + col * cell_w
            draw.rectangle((x, y, x + cell_w - 4, y + cell_h - 4), fill=color, outline=(80, 80, 80))
            label = f"{value:.3g}x" if "gain" in key else f"{value:.3g}"
            draw.text((x + 38, y + 18), label, fill="black")
    draw.text((left, top + cell_h * len(SPREADS)), "Log-scaled color ramp; metric labels are linear", fill="black")
    image.save(OUT / filename)


heatmap("edge_left", "red_gain_over_unbounded_input", "edge-red-energy-gain-heatmap.png")
heatmap("corner_tl", "red_gain_over_unbounded_input", "corner-red-energy-gain-heatmap.png")

metrics = {
    "study": "Physical Halation frame-size and spread scaling r1",
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
        "positions_per_condition": 9,
        "total_cases": len(rows),
        "source_rgb": [4, 4, 4],
        "source_alpha": 1,
        "highlight_weight": float(weight),
        "intensity": float(INTENSITY),
    },
        "symmetry_checks": symmetry,
    "results": rows,
    "limitations": [
        "Separable NumPy Float32 source-equation model, not compiled C++ or application output.",
        "Opaque neutral point source only; photographic scenes, colored highlights and transparent edges are not represented.",
        "Amplification is relative to total point-source highlight energy and is a boundary-condition descriptor, not proof of a visual defect.",
        "No GPU/preview/final parity, display transform, performance, allocation or optical-film validation.",
    ],
}
(OUT / "metrics.json").write_text(json.dumps(metrics, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
checksums = {
    path.name: sha(path)
    for path in sorted(OUT.iterdir())
    if path.is_file() and path.name != "checksums.json"
}
(OUT / "checksums.json").write_text(json.dumps(checksums, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
print(json.dumps({"cases": len(rows), "markers_all_true": all(MARKERS.values()), "symmetry_checks": len(symmetry), "max_symmetry_rmse": max(x["mirrored_rgb_rmse"] for x in symmetry), "outputs": len(checksums)}))
