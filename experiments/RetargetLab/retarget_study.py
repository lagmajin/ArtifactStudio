"""Content-aware aspect-ratio retarget R&D (rd0).

Compares naive baselines (center crop, non-uniform stretch) against vertical seam
carving lanes on deterministic synthetic fixtures with a known ground-truth mask.

Scope: offline fixture generation, candidate execution, descriptive measurement.
No product code is imported. This script does not decide a visual-quality gate by
itself; thresholds live in EVALUATION_PLAN.md and are applied in review.
"""

from __future__ import annotations

import hashlib
import json
import time
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent
OUT = ROOT / "captures" / "rd0"
SEED = 20261116
W, H = 384, 216
TARGET_W = 216  # 16:9 -> 1:1 at fixed height


# ---------------------------------------------------------------- image helpers

def luma(img: np.ndarray) -> np.ndarray:
    return (0.2126 * img[..., 0] + 0.7152 * img[..., 1] + 0.0722 * img[..., 2]).astype(np.float32)


def srgb_encode(rgb: np.ndarray) -> np.ndarray:
    rgb = np.clip(rgb, 0.0, 1.0)
    return np.where(rgb <= 0.0031308, rgb * 12.92, 1.055 * np.maximum(rgb, 0.0) ** (1.0 / 2.4) - 0.055)


def srgb_decode(srgb: np.ndarray) -> np.ndarray:
    srgb = np.clip(srgb, 0.0, 1.0)
    return np.where(srgb <= 0.04045, srgb / 12.92, ((srgb + 0.055) / 1.055) ** 2.4)


def write_png(path: Path, rgb: np.ndarray) -> None:
    data = np.uint8(np.clip(srgb_encode(rgb) * 255.0 + 0.5, 0.0, 255.0))
    Image.fromarray(data, "RGB").save(path, format="PNG", optimize=False)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


# ------------------------------------------------------------------- fixtures

def build_fixtures() -> dict[str, dict]:
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    x = xx / (W - 1)
    y = yy / (H - 1)
    fixtures: dict[str, dict] = {}

    # 1. Off-center portrait subject (left third). A square crop must lose part of it.
    bg = np.stack((0.10 + 0.18 * x, 0.16 + 0.22 * y, 0.26 + 0.14 * x), axis=-1).astype(np.float32)
    img = bg.copy()
    cx, cy, rx, ry = 90.0, 108.0, 34.0, 58.0
    subject = ((xx - cx) / rx) ** 2 + ((yy - cy) / ry) ** 2 <= 1.0
    img[subject] = (0.95, 0.74, 0.56)
    fixtures["portrait"] = {
        "image": img,
        "mask": subject,
        "kind": "portrait",
        "subject_color": (0.95, 0.74, 0.56),
        "note": "off-center ellipse subject; measures retention and shape distortion",
    }

    # 2. Facade grid: straight vertical/horizontal lines must stay straight and countable.
    img = np.full((H, W, 3), 0.06, np.float32)
    for vx in range(16, W, 32):
        img[:, vx:vx + 3] = 0.72
    for hy in range(16, H, 27):
        img[hy:hy + 2, :] = 0.50
    fixtures["facade"] = {
        "image": img,
        "mask": np.zeros((H, W), bool),
        "kind": "facade",
        "subject_color": None,
        "note": "12 vertical + 8 horizontal lines; measures line survival and horizon bend",
    }

    # 3. Landscape horizon with a sun: crop loses the sun, carving may bend the horizon.
    hy = 140
    t_sky = yy / hy
    sky = np.stack((0.25 + 0.55 * t_sky, 0.42 + 0.43 * t_sky, 0.68 + 0.24 * t_sky), axis=-1)
    t_gr = (yy - hy) / (H - 1 - hy)
    ground = np.stack((0.22 + 0.06 * t_gr, 0.26 + 0.08 * t_gr, 0.14 + 0.05 * t_gr), axis=-1)
    img = np.where((yy < hy)[..., None], sky, ground).astype(np.float32)
    sun = ((xx - 300.0) ** 2 + (yy - 58.0) ** 2) <= 18.0 ** 2
    img[sun] = (1.0, 0.97, 0.62)
    fixtures["horizon"] = {
        "image": img,
        "mask": sun,
        "kind": "horizon",
        "subject_color": (1.0, 0.97, 0.62),
        "note": "sharp horizon plus sun near the right edge; measures bend and sun retention",
    }

    # 4. Thin vertical wires: seam carving classically deletes 1-px structures.
    img = np.stack((0.12 + 0.16 * x, 0.18 + 0.20 * y, 0.24 + 0.12 * x), axis=-1).astype(np.float32)
    wires = np.zeros((H, W), bool)
    for wx in (40, 96, 152, 208, 264, 320):
        img[:, wx] = 0.92
        wires[:, wx] = True
    fixtures["wires"] = {
        "image": img,
        "mask": wires,
        "kind": "wires",
        "subject_color": (0.92, 0.92, 0.92),
        "note": "six 1-px wires; measures thin-structure survival",
    }
    return fixtures


# --------------------------------------------------------------- seam carving

def backward_energy(img: np.ndarray) -> np.ndarray:
    g = luma(img)
    ex = np.zeros_like(g)
    ey = np.zeros_like(g)
    ex[:, 1:-1] = np.abs(g[:, 2:] - g[:, :-2])
    ey[1:-1, :] = np.abs(g[2:, :] - g[:-2, :])
    return ex + ey


def find_seam_backward(energy: np.ndarray) -> np.ndarray:
    h, w = energy.shape
    m = energy.astype(np.float64)
    back = np.zeros((h, w), np.int8)
    cols = np.arange(w)
    for i in range(1, h):
        prev = m[i - 1]
        left = np.full(w, np.inf)
        left[1:] = prev[:-1]
        right = np.full(w, np.inf)
        right[:-1] = prev[1:]
        cand = np.stack((left, prev, right), axis=0)
        choice = np.argmin(cand, axis=0).astype(np.int8)
        m[i] = m[i] + cand[choice, cols]
        back[i] = choice
    seam = np.empty(h, np.int32)
    seam[-1] = int(np.argmin(m[-1]))
    for i in range(h - 1, 0, -1):
        seam[i - 1] = seam[i] + int(back[i, seam[i]]) - 1
    return seam


def find_seam_forward(img: np.ndarray, protect_mask: np.ndarray | None, protect_weight: float) -> np.ndarray:
    g = luma(img)
    h, w = g.shape
    ex = np.zeros_like(g)
    ey = np.zeros_like(g)
    ex[:, 1:-1] = np.abs(g[:, 2:] - g[:, :-2])
    ey[1:-1, :] = np.abs(g[2:, :] - g[:-2, :])
    e = ex + ey
    g_up = np.vstack((g[:1], g[:-1]))
    g_left = np.hstack((g[:, :1], g[:, :-1]))
    g_right = np.hstack((g[:, 1:], g[:, -1:]))
    c_l = np.abs(g_right - g_left)
    c_u = np.abs(g_up - g_left)
    c_r = np.abs(g_up - g_right)
    if protect_mask is not None and protect_weight > 0.0:
        e = e + protect_mask.astype(np.float32) * np.float32(protect_weight)
    m = e.astype(np.float64)
    back = np.zeros((h, w), np.int8)
    cols = np.arange(w)
    for i in range(1, h):
        prev = m[i - 1]
        a = np.full(w, np.inf)
        a[1:] = prev[:-1] + c_u[i, 1:]
        b = prev + c_l[i]
        c = np.full(w, np.inf)
        c[:-1] = prev[1:] + c_r[i, :-1]
        cand = np.stack((a, b, c), axis=0)
        choice = np.argmin(cand, axis=0).astype(np.int8)
        m[i] = e[i] + cand[choice, cols]
        back[i] = choice
    seam = np.empty(h, np.int32)
    seam[-1] = int(np.argmin(m[-1]))
    for i in range(h - 1, 0, -1):
        seam[i - 1] = seam[i] + int(back[i, seam[i]]) - 1
    return seam


def carve_columns(img: np.ndarray, gt_mask: np.ndarray, target_w: int, mode: str,
                  protect_mask: np.ndarray | None = None, protect_weight: float = 0.0):
    """Remove columns one seam at a time.

    The ground-truth mask is sliced together with the image so that removed pixels keep
    their original identity; seam coordinates are only valid inside the current frame.
    """
    img = img.astype(np.float32).copy()
    gt = gt_mask.copy()
    total = int(gt.sum())
    lost = 0
    protect = None if protect_mask is None else protect_mask.copy()
    removed: list[np.ndarray] = []
    while img.shape[1] > target_w:
        if mode == "backward":
            seam = find_seam_backward(backward_energy(img))
        else:
            seam = find_seam_forward(img, protect, protect_weight)
        removed.append(seam.copy())
        keep = np.ones(gt.shape, bool)
        keep[np.arange(gt.shape[0]), seam] = False
        lost += int(gt[~keep].sum())
        rows, new_w = gt.shape[0], gt.shape[1] - 1
        img = img[keep].reshape(rows, new_w, 3)
        gt = gt[keep].reshape(rows, new_w)
        if protect is not None:
            protect = protect[keep].reshape(rows, new_w)
    retention = 1.0 if total == 0 else 1.0 - lost / float(total)
    return img, retention, removed


# -------------------------------------------------------------- measurements

def column_structures(img: np.ndarray, threshold: float, row_ratio: float):
    """Group adjacent columns carrying a vertical structure.

    Returns the groups and the per-column row-coverage score. A structure that a seam
    shifts by a pixel for part of its height keeps its group but loses coverage, so the
    score exposes the jaggy/doubled-column artifact that a pure count would hide.
    """
    g = luma(img)
    score = (g > threshold).sum(axis=0) / float(g.shape[0])
    groups: list[list[int]] = []
    current: list[int] = []
    for index, value in enumerate(score >= row_ratio):
        if value:
            current.append(index)
        elif current:
            groups.append(current)
            current = []
    if current:
        groups.append(current)
    return groups, score


def structure_integrity(groups: list[list[int]], score: np.ndarray) -> float:
    if not groups:
        return 0.0
    return float(np.mean([max(float(score[c]) for c in group) for group in groups]))


def subject_bbox(img: np.ndarray, color, tol: float):
    if color is None:
        return None
    d = np.abs(img - np.array(color, np.float32)).max(axis=2)
    m = d < tol
    if not m.any():
        return None
    ys, xs = np.nonzero(m)
    return int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())


def resize_stretch(img: np.ndarray, width: int, height: int) -> np.ndarray:
    """Non-uniform LANCZOS resize, decoded back to linear float32."""
    data = np.uint8(np.clip(srgb_encode(img) * 255.0 + 0.5, 0.0, 255.0))
    out = Image.fromarray(data, "RGB").resize((width, height), Image.LANCZOS)
    return srgb_decode(np.asarray(out).astype(np.float32) / 255.0).astype(np.float32)


# ------------------------------------------------------------------- runner

def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    fixtures = build_fixtures()
    candidates = ["crop_center", "stretch", "seam_backward", "seam_forward", "seam_forward_oracle"]
    metrics: dict[str, dict] = {}
    file_index: dict[str, str] = {}
    tiles: list[list[np.ndarray]] = []

    for name, fx in fixtures.items():
        img = fx["image"]
        mask = fx["mask"]
        kind = fx["kind"]
        write_png(OUT / f"{name}_source.png", img)
        file_index[f"{name}_source"] = sha256(OUT / f"{name}_source.png")
        row_tiles: list[np.ndarray] = []
        metrics[name] = {"kind": kind, "note": fx["note"], "candidates": {}}

        for cand in candidates:
            t0 = time.perf_counter()
            total = int(mask.sum())
            retention: float | None = None
            if cand == "crop_center":
                left = (W - TARGET_W) // 2
                keep_cols = np.zeros(W, bool)
                keep_cols[left:left + TARGET_W] = True
                out = img[:, keep_cols]
                retention = 1.0 if total == 0 else float(mask[:, keep_cols].sum()) / total
            elif cand == "stretch":
                out = resize_stretch(img, TARGET_W, H)
            elif cand == "seam_backward":
                out, retention, _ = carve_columns(img, mask, TARGET_W, "backward")
            elif cand == "seam_forward":
                out, retention, _ = carve_columns(img, mask, TARGET_W, "forward")
            else:  # oracle: forward energy with the ground-truth subject weighted out
                out, retention, _ = carve_columns(
                    img, mask, TARGET_W, "forward",
                    protect_mask=mask if mask.any() else None, protect_weight=1.0e3,
                )
            elapsed_ms = (time.perf_counter() - t0) * 1000.0

            png = OUT / f"{name}_{cand}.png"
            write_png(png, out)
            file_index[f"{name}_{cand}"] = sha256(png)

            entry: dict[str, object] = {
                "size": [int(out.shape[1]), int(out.shape[0])],
                "runtime_ms": round(elapsed_ms, 1),
                "subject_retention": None if retention is None else round(retention, 4),
            }
            if kind in ("portrait", "horizon"):
                bbox = subject_bbox(out, fx["subject_color"], 0.15)
                if bbox is None:
                    entry["subject_bbox"] = None
                    entry["subject_aspect_error"] = None
                else:
                    x0, y0, x1, y1 = bbox
                    aspect = (x1 - x0 + 1) / float(y1 - y0 + 1)
                    gt = (2 * 34.0) / (2 * 58.0) if kind == "portrait" else 1.0
                    entry["subject_bbox"] = bbox
                    entry["subject_aspect"] = round(aspect, 4)
                    entry["subject_aspect_error"] = round(abs(aspect - gt) / gt, 4)
            if kind == "facade":
                groups, score = column_structures(out, 0.45, 0.10)
                entry["vertical_lines"] = len(groups)
                entry["vertical_line_width_mean"] = round(
                    float(np.mean([len(g) for g in groups])), 3) if groups else 0.0
                entry["vertical_line_integrity"] = round(structure_integrity(groups, score), 4)
            if kind == "wires":
                groups, score = column_structures(out, 0.45, 0.10)
                entry["wires_surviving"] = len(groups)
                entry["wire_width_mean"] = round(
                    float(np.mean([len(g) for g in groups])), 3) if groups else 0.0
                entry["wire_line_integrity"] = round(structure_integrity(groups, score), 4)
                entry["wire_pixel_retention"] = 1.0 if retention is None else round(retention, 4)
            metrics[name]["candidates"][cand] = entry
            row_tiles.append(out)
        tiles.append(row_tiles)

    # Contact sheet: rows = fixtures, columns = candidates, in the same order.
    sheet = np.concatenate([np.concatenate(row, axis=1) for row in tiles], axis=0)
    write_png(OUT / "contact_sheet.png", sheet)
    file_index["contact_sheet"] = sha256(OUT / "contact_sheet.png")

    report = {
        "rd": "rd0",
        "seed": SEED,
        "source_size": [W, H],
        "target_size": [TARGET_W, H],
        "generator_sha256": sha256(Path(__file__).resolve()),
        "fixtures": {
            name: {"kind": fx["kind"], "subject_pixels": int(fx["mask"].sum())}
            for name, fx in fixtures.items()
        },
        "candidates": candidates,
        "metrics": metrics,
        "outputs": file_index,
        "notes": [
            "Synthetic fixtures only; no real photography and no perceptual study.",
            "stretch quantizes through uint8 sRGB before LANCZOS resize, so it is not a float reference.",
            "subject_retention is exact for crop/seam lanes (removed columns are tracked) and not applicable to stretch.",
            "seam_forward_oracle uses the ground-truth mask as an upper bound, not a deployable candidate.",
            "Column-only transforms preserve row geometry exactly, so horizontal-line bend cannot discriminate lanes; continuity of vertical structures is measured instead.",
            "Gate thresholds are declared in EVALUATION_PLAN.md; this script reports measurements only.",
        ],
    }
    (OUT / "metrics.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps(report["metrics"], indent=2))


if __name__ == "__main__":
    main()
