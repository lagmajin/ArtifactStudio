"""Deterministic vector-defined fixtures for the VectorTraceLab R&D.

All geometry is authored at a 512x320 base and scaled for resolution cases.
Pixels are sRGB encoded 8-bit values scaled to 0..1, with straight alpha.
No product code, QImage, QPainter, or GPU path is involved.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from dataclasses import dataclass, field
from pathlib import Path

import cv2
import numpy as np

SEED = 20261120
SS = 4  # supersample factor
BASE_W, BASE_H = 512, 320


@dataclass
class Region:
    name: str
    color: tuple[int, int, int]
    alpha: float
    kind: str  # fill | thin | semi
    outer: list  # list of polygons, each a list of [x, y] pixel coords
    holes: list = field(default_factory=list)


@dataclass
class Fixture:
    name: str
    w: int
    h: int
    regions: list[Region]
    rois: dict
    description: str = ""
    scale: tuple[float, float] = (1.0, 1.0)


def scale_poly(poly, sx, sy):
    return [[x * sx, y * sy] for x, y in poly]


def rect(x, y, w, h):
    return [[x, y], [x + w, y], [x + w, y + h], [x, y + h]]


def regular(cx, cy, r, n=64, rot=-math.pi / 2):
    return [
        [cx + r * math.cos(rot + 2 * math.pi * i / n),
         cy + r * math.sin(rot + 2 * math.pi * i / n)]
        for i in range(n)
    ]


def star(cx, cy, r_outer, r_inner, n=5):
    pts = []
    for i in range(2 * n):
        r = r_outer if i % 2 == 0 else r_inner
        a = -math.pi / 2 + math.pi * i / n
        pts.append([cx + r * math.cos(a), cy + r * math.sin(a)])
    return pts


def rounded_rect(x, y, w, h, r, steps=10):
    corners = [
        (x + r, y + r, math.pi, 1.5 * math.pi),
        (x + w - r, y + r, 1.5 * math.pi, 2 * math.pi),
        (x + w - r, y + h - r, 0.0, 0.5 * math.pi),
        (x + r, y + h - r, 0.5 * math.pi, math.pi),
    ]
    pts = []
    for cx, cy, a0, a1 in corners:
        for i in range(steps + 1):
            a = a0 + (a1 - a0) * i / steps
            pts.append([cx + r * math.cos(a), cy + r * math.sin(a)])
    return pts


def _scaled(regions, sx, sy):
    return [
        Region(r.name, r.color, r.alpha, r.kind,
               [scale_poly(p, sx, sy) for p in r.outer],
               [scale_poly(p, sx, sy) for p in r.holes])
        for r in regions
    ]


def _scaled_rois(rois, sx, sy):
    return {name: [v[0] * sx, v[1] * sy, v[2] * sx, v[3] * sy]
            for name, v in rois.items()}


def _base_definitions():
    """Return (regions, rois) authored at 512x320."""
    defs = {}

    defs["logo_badge"] = (
        [
            Region("plate", (38, 86, 196), 1.0, "fill",
                   [rounded_rect(60, 40, 392, 240, 40)]),
            Region("ring", (244, 246, 250), 1.0, "fill",
                   [regular(256, 160, 86)], [regular(256, 160, 54)]),
            Region("star", (240, 138, 38), 1.0, "fill",
                   [star(256, 160, 50, 22, 5)]),
            Region("dots", (28, 30, 44), 1.0, "fill",
                   [regular(160, 246, 10, 32), regular(352, 246, 10, 32)]),
        ],
        {
            "flat_interior": [95, 65, 60, 40],
            "corner": [62, 42, 50, 45],
            "hole": [205, 110, 102, 100],
            "star": [220, 125, 72, 70],
        },
    )

    defs["flat_illustration"] = (
        [
            Region("red_disc", (220, 70, 70), 1.0, "fill",
                   [regular(150, 160, 95)]),
            Region("green_tri", (60, 180, 110), 1.0, "fill",
                   [[[220, 70], [420, 230], [180, 250]]]),
            Region("yellow_rect", (245, 200, 60), 1.0, "fill",
                   [rect(200, 110, 170, 110)]),
            Region("purple_dot", (130, 80, 190), 1.0, "fill",
                   [regular(390, 80, 14, 32)]),
        ],
        {
            "flat_interior": [215, 125, 80, 70],
            "overlap": [190, 130, 130, 90],
        },
    )

    defs["nested_holes"] = (
        [
            Region("plate", (40, 120, 140), 1.0, "fill",
                   [rect(60, 40, 392, 240)]),
            Region("ring", (230, 235, 240), 1.0, "fill",
                   [regular(256, 160, 110)], [regular(256, 160, 70)]),
            Region("core", (235, 150, 80), 1.0, "fill",
                   [regular(256, 160, 40, 48)]),
        ],
        {
            "hole": [188, 92, 136, 136],
            "flat_interior": [70, 50, 70, 60],
        },
    )

    defs["thin_strokes"] = (
        [
            Region("hline", (30, 30, 40), 1.0, "thin",
                   [rect(80, 120, 352, 2)]),
            Region("vline", (30, 30, 40), 1.0, "thin",
                   [rect(250, 50, 2, 220)]),
            Region("dots", (200, 60, 60), 1.0, "thin",
                   [regular(120, 70, 3, 16), regular(380, 250, 3, 16),
                    regular(120, 250, 3, 16)]),
            Region("wedge", (30, 30, 40), 1.0, "thin",
                   [[[330, 60], [336, 60], [420, 150]]]),
        ],
        {
            "thin_features": [70, 45, 370, 240],
        },
    )

    defs["alpha_logo"] = (
        [
            Region("plate", (35, 45, 70), 1.0, "fill",
                   [rounded_rect(80, 50, 352, 220, 30)]),
            Region("glass", (230, 120, 220), 0.5, "semi",
                   [regular(256, 160, 90)]),
            Region("mark", (245, 245, 245), 1.0, "fill",
                   [star(256, 160, 45, 20, 4)]),
        ],
        {
            "semi_transparent": [170, 75, 172, 170],
            "flat_interior": [90, 60, 60, 40],
        },
    )

    return defs


def build_fixture(name: str, w: int = BASE_W, h: int = BASE_H) -> Fixture:
    defs = _base_definitions()
    if name not in defs:
        raise KeyError(name)
    regions, rois = defs[name]
    sx, sy = w / BASE_W, h / BASE_H
    return Fixture(name, w, h, _scaled(regions, sx, sy),
                   _scaled_rois(rois, sx, sy),
                   description=f"vector-defined synthetic fixture: {name}",
                   scale=(sx, sy))


def build_all() -> list[Fixture]:
    fixtures = [build_fixture(n) for n in _base_definitions()]
    small = build_fixture("logo_badge", 256, 160)
    small.name = "logo_badge_256"
    large = build_fixture("logo_badge", 1024, 640)
    large.name = "logo_badge_1024"
    fixtures.extend([small, large])
    return fixtures


def rasterize_region(region: Region, w: int, h: int, ss: int = SS) -> np.ndarray:
    """Return fractional coverage (h, w) float32 for one vector region."""
    H, W = h * ss, w * ss
    mask = np.zeros((H, W), np.uint8)
    for poly in region.outer:
        pts = np.array([[round(x * ss), round(y * ss)]
                        for x, y in poly], np.int32)
        cv2.fillPoly(mask, [pts], 1, lineType=cv2.LINE_8)
    for poly in region.holes:
        pts = np.array([[round(x * ss), round(y * ss)]
                        for x, y in poly], np.int32)
        cv2.fillPoly(mask, [pts], 0, lineType=cv2.LINE_8)
    return cv2.resize(mask.astype(np.float32), (w, h),
                      interpolation=cv2.INTER_AREA)


def render_fixture(fx: Fixture, ss: int = SS):
    """Return (rgba float32 (h,w,4), coverage (R,h,w), labels (h,w), alpha (h,w))."""
    covs = np.stack([rasterize_region(r, fx.w, fx.h, ss) for r in fx.regions])
    rgb = np.zeros((fx.h, fx.w, 3), np.float32)
    alpha = np.zeros((fx.h, fx.w), np.float32)
    trans = np.ones((fx.h, fx.w), np.float32)
    # Painter's order: definitions are back-to-front, so fold front-to-back.
    # The frontmost layer contributes fully; layers behind are attenuated by
    # the transmittance of everything already in front of them.
    for idx in reversed(range(len(fx.regions))):
        r = fx.regions[idx]
        a = covs[idx] * r.alpha
        color = np.array(r.color, np.float32) / 255.0
        rgb += color[None, None, :] * (a * trans)[..., None]
        alpha += a * trans
        trans = trans * (1.0 - a)
    straight = np.zeros_like(rgb)
    valid = alpha > 1e-6
    straight[valid] = rgb[valid] / alpha[valid, None]
    rgba = np.concatenate([straight, alpha[..., None]], axis=2).astype(np.float32)

    # Labels: frontmost region with coverage >= 0.5 owns the pixel.
    labels = np.zeros((fx.h, fx.w), np.int16)
    for idx in reversed(range(len(fx.regions))):
        own = (covs[idx] >= 0.5) & (labels == 0)
        labels[own] = idx + 1
    return rgba, covs.astype(np.float32), labels, alpha.astype(np.float32)


def region_definitions_json(fx: Fixture) -> dict:
    return {
        "name": fx.name,
        "w": fx.w,
        "h": fx.h,
        "scale": fx.scale,
        "supersample": SS,
        "description": fx.description,
        "regions": [
            {
                "name": r.name, "color_srgb8": r.color, "alpha": r.alpha,
                "kind": r.kind, "outer": r.outer, "holes": r.holes,
            }
            for r in fx.regions
        ],
        "rois_rect": fx.rois,
        "computed_rois": {
            "edge_band": "pixels where any region coverage is in (0.02, 0.98)",
            "semi_transparent": "pixels where composed alpha is in (0.4, 0.6)",
        },
    }


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def review_png(rgba: np.ndarray, bg=(0.5, 0.5, 0.5)) -> np.ndarray:
    a = rgba[..., 3:4]
    comp = rgba[..., :3] * a + np.array(bg, np.float32)[None, None, :] * (1 - a)
    return np.clip(comp * 255.0, 0, 255).astype(np.uint8)


def write_fixtures(out_dir: Path) -> dict:
    out_dir.mkdir(parents=True, exist_ok=True)
    records = []
    for fx in build_all():
        rgba, coverage, labels, alpha = render_fixture(fx)
        np.savez_compressed(
            out_dir / f"{fx.name}__source.npz", rgba=rgba)
        np.savez_compressed(
            out_dir / f"{fx.name}__gt.npz",
            coverage=coverage, labels=labels, alpha=alpha)
        (out_dir / f"{fx.name}__regions.json").write_text(
            json.dumps(region_definitions_json(fx), indent=2), encoding="utf-8")
        from PIL import Image
        Image.fromarray(review_png(rgba)).save(out_dir / f"{fx.name}__source_review.png")
        records.append({
            "name": fx.name, "w": fx.w, "h": fx.h,
            "region_count": len(fx.regions),
            "files": {
                "source_npz": f"{fx.name}__source.npz",
                "gt_npz": f"{fx.name}__gt.npz",
                "regions_json": f"{fx.name}__regions.json",
                "review_png": f"{fx.name}__source_review.png",
            },
            "hashes": {
                "source_npz": sha256(out_dir / f"{fx.name}__source.npz"),
                "gt_npz": sha256(out_dir / f"{fx.name}__gt.npz"),
                "regions_json": sha256(out_dir / f"{fx.name}__regions.json"),
            },
        })
    return {"seed": SEED, "supersample": SS, "fixtures": records}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="captures/rd0/fixtures")
    args = ap.parse_args()
    rec = write_fixtures(Path(args.out))
    print(json.dumps({"fixtures": len(rec["fixtures"]), "out": args.out}, indent=2))


if __name__ == "__main__":
    main()
